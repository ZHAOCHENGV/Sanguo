// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "AntSubsystem.h"
#include "AntModule.h"
#include "AntGrid.h"
#include "AntMath.h"
#include "AntUtil.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GeomUtils.h"

int32 Ant_DebugDraw = 0;
int32 Ant_DebugHeightSamp = 0;
FAutoConsoleVariableRef CVar_DebugDraw(TEXT("ant.DebugDraw"), Ant_DebugDraw, TEXT(""), ECVF_Default);
FAutoConsoleVariableRef CVar_DebugHeightSamp(TEXT("ant.DebugHeightSampling"), Ant_DebugHeightSamp, TEXT(""), ECVF_Default);

// user data slot index in its storage
constexpr uint8 SlotUserData = 1;

// path slot index in its storage
constexpr uint8 SlotPath = 0;

// agents slot index in its storage
constexpr uint8 SlotAgt = 0;
// movement slot index in its storage
constexpr uint8 SlotMov = 2;

// query slot index in its storage
constexpr uint8 SlotQry = 0;

//template<typename T>
//void CreateTasksRange(const TSparseArray<T> &Container, TArray<TPair<int32, int32>> &RangeList, uint32 MinPerThread, uint8 NumThreads)
//{
//	// prepare and scale resources for parallel tasks
//	const auto neededThread = (Container.Num() / MinPerThread) + (Container.Num() % MinPerThread != 0 ? 1 : 0);
//	const auto availThreads = FMath::Clamp(neededThread, 0, NumThreads);
//	const auto agentPerThread = availThreads > 0 ? Container.Num() / availThreads : 0;
//
//	uint8 threadInd = 0;
//	int32 offset = 0;
//	while (threadInd++ < availThreads)
//	{
//		// collect a range of agents which need process
//		int32 agentCount = 0;
//		const auto start = offset;
//		for (; offset < Container.GetMaxIndex(); ++offset)
//			if (Container.IsValidIndex(offset) && ++agentCount >= agentPerThread && ++offset)
//				break;
//
//		const auto isLastRange = threadInd == availThreads;
//		const auto count = isLastRange ? Container.GetMaxIndex() - start : offset - start;
//		RangeList.Add({ start, count });
//	}
//}

FAntCorridorLocation FAntPathData::FindNearestLocationOnCorridor(const FVector &NavLocation, int32 StartIndex) const
{
	using FReal = FVector::FReal;

	if (Data.Num() < 2)
	{
		return FAntCorridorLocation();
	}

	FReal NearestDistanceSq = MAX_dbl;
	FAntCorridorLocation Result;

	for (int32 PortalIndex = StartIndex; PortalIndex < Data.Num() - 1; PortalIndex++)
	{
		const auto &CurrPortal = Data[PortalIndex];
		const auto &NextPortal = Data[PortalIndex + 1];

		const auto UV = UE::AI::InvBilinear2D(NavLocation, CurrPortal.Left, CurrPortal.Right, NextPortal.Right, NextPortal.Left);
		const FVector NearestSectionLocation = UE::AI::Bilinear(UV.ClampAxes(0.0, 1.0), CurrPortal.Left, CurrPortal.Right, NextPortal.Right, NextPortal.Left);
		const FReal SectionDistanceSq = FVector::DistSquared(NavLocation, NearestSectionLocation);

		if (SectionDistanceSq < NearestDistanceSq)
		{
			NearestDistanceSq = SectionDistanceSq;
			Result.VertT = static_cast<float>(UV.Y);
			Result.HoriT = static_cast<float>(UV.X);
			Result.NavLocation = NearestSectionLocation;
			Result.PortalIndex = PortalIndex;

			if (NearestDistanceSq < UE_KINDA_SMALL_NUMBER)
				break;
		}
	}

	return Result;
}

void FAntPathData::RebuildDistanceList()
{
	// calculate distance of the each waypoint on the path (reverse)
	float traveledDist = 0.0f;
	for (int32 idx = Data.Num() - 1; idx >= 0; --idx)
	{
		Data[idx].Distance = traveledDist;

		// calculate traveled distance
		if (idx > 0)
			traveledDist += FVector::Distance(Data[idx - 1].Location, Data[idx].Location);
	}
}

UAntSubsystem::UAntSubsystem()
{
}

void UAntSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);

	this->Settings = Cast<AAntWorldSettings>(this->GetWorld()->GetWorldSettings(false, false));
	if (!Settings)
	{
		this->Settings = GetDefault<AAntWorldSettings>();
		check(this->Settings);
	}

	// retrieve number of the available threads
	NumAvailThreads = FGenericPlatformMisc::NumberOfWorkerThreadsToSpawn();

	// create broadphase grid
	BroadphaseGrid = new FAntGrid;
	ShiftSize = Settings->CollisionCellSize * (Settings->NumCells / 2);
	BroadphaseGrid->Reset(Settings->NumCells, Settings->CollisionCellSize, ShiftSize);
}

void UAntSubsystem::Deinitialize()
{
	Super::Deinitialize();

	// free grid memory
	if (BroadphaseGrid)
	{
		delete BroadphaseGrid;
		BroadphaseGrid = nullptr;
	}
}

void UAntSubsystem::Tick(float Delta)
{
	Super::Tick(Delta);

	// checking for collsion update rate
	// increase elapsed delta time
	PxDeltaTime += Delta;
	const bool collisionNeedUpdate = PxDeltaTime >= Settings->CollisionTickInterval;
	float deltaMul = 1.0f;

	if (collisionNeedUpdate)
	{
		// calculate multiplicative delta value
		deltaMul = PxDeltaTime / Settings->CollisionTickInterval;

		// pre-collision handler
		OnPxPreUpdate.Broadcast(deltaMul);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ANT_TotalFrame);
		// update movements at collisiion tick interval
		// updating movements out of this scope is redundant
		if (collisionNeedUpdate)
			UpdateMovements(PxDeltaTime, deltaMul);

		// update collisions and async queries
		UpdateCollisionsAndQueries(Delta, collisionNeedUpdate);
	}

	// post-collision handler
	{
		SCOPE_CYCLE_COUNTER(STAT_ANT_PostPX);
		if (collisionNeedUpdate)
			OnPxPostUpdate.Broadcast(deltaMul);
	}

	DispatchAsyncQueries(Delta);

	// reset elapsed time
	if (collisionNeedUpdate)
		PxDeltaTime = 0;

	// 
	if (Ant_DebugDraw > 0 || Ant_DebugHeightSamp > 0)
		DebugDraw();
}

bool UAntSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
}

FAntHandle UAntSubsystem::AddAgent(const FVector &Location, float Radius, float Height, float FaceAngle, int32 Flags)
{
	// add new agent to the list
	const auto dataIdx = Agents.Add({});
	Agents[dataIdx].Location = Agents[dataIdx].LocationLerped = FVector3f(Location);
	Agents[dataIdx].Radius = Radius;
	Agents[dataIdx].Flag = Flags;
	Agents[dataIdx].Height = Height;
	Agents[dataIdx].FaceAngle = Agents[dataIdx].FinalFaceAngle = FaceAngle;

	// set it to the storage
	const auto handle = AgentStorage.Add();
	AgentStorage.Set(handle, SlotAgt, dataIdx);
	Agents[dataIdx].Handle = handle;

	// add new agent to the grid
	Agents[dataIdx].GridHandle = BroadphaseGrid->AddCylinder(Agents[dataIdx].Handle, FVector3f(Location), Radius, Height, Flags);
	check(Agents[dataIdx].GridHandle != INDEX_NONE);

	return handle;
}

FInstancedStruct &UAntSubsystem::GetAgentUserData(FAntHandle Handle)
{
	check(AgentStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	auto dataIdx = AgentStorage.Get(Handle, SlotUserData);

	// create new custom data if it is not exist
	if (dataIdx == INDEX_NONE)
	{
		dataIdx = UserData.Add({});
		AgentStorage.Set(Handle, SlotUserData, dataIdx);
	}

	return UserData[dataIdx];
}

void UAntSubsystem::RemoveAgent(FAntHandle Handle)
{
	check(IsValidAgent(Handle) && "Invalid handle!");

	// cancel/remove any in-progress movement
	RemoveAgentMovement(Handle);

	// retrieve index
	const auto dataIdx = AgentStorage.Get(Handle, SlotAgt);
	const auto userIdx = AgentStorage.Get(Handle, SlotUserData);

	// notify
	OnAgentRemoved.Broadcast(Handle);

	// remove agent from the grid
	BroadphaseGrid->Remove(Agents[dataIdx].GridHandle);

	// remove agent from the list
	Agents.RemoveAt(dataIdx);

	// remove user data
	if (userIdx != INDEX_NONE)
		UserData.RemoveAt(userIdx);

	// remove agent from the storage
	AgentStorage.Remove(Handle);
}

void UAntSubsystem::ClearAgents()
{
	for (int32 idx = 0; idx < Agents.GetMaxIndex(); ++idx)
		if (Agents.IsValidIndex(idx))
			RemoveAgent(Agents[idx].Handle);

	Agents.Reset();
}

const FAntAgentData &UAntSubsystem::GetAgentData(FAntHandle Handle) const
{
	check(AgentStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = AgentStorage.Get(Handle, SlotAgt);

	return Agents[dataIdx];
}

FAntAgentData &UAntSubsystem::GetMutableAgentData(FAntHandle Handle)
{
	check(AgentStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = AgentStorage.Get(Handle, SlotAgt);

	return Agents[dataIdx];
}

void UAntSubsystem::SetAgentLocation(FAntHandle Handle, const FVector &Location)
{
	check(AgentStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = AgentStorage.Get(Handle, SlotAgt);

	// remove agent from current place
	auto &agent = Agents[dataIdx];
	BroadphaseGrid->Remove(agent.GridHandle);

	// add agent to the new place
	agent.Location = agent.LocationLerped = FVector3f(Location);
	agent.NodeRef = INVALID_NAVNODEREF;
	agent.LerpAlpha = 1.f;
	agent.GridHandle = BroadphaseGrid->AddCylinder(Handle, agent.Location, agent.Radius, agent.Height, agent.Flag);
}

void UAntSubsystem::SetAgentRadius(FAntHandle Handle, float Radius)
{
	check(AgentStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = AgentStorage.Get(Handle, SlotAgt);

	// remove agent with old
	auto &agent = Agents[dataIdx];
	BroadphaseGrid->Remove(agent.GridHandle);

	// add agent with new radius
	agent.Radius = Radius;
	agent.GridHandle = BroadphaseGrid->AddCylinder(Handle, agent.Location, agent.Radius, agent.Height, agent.Flag);
}

void UAntSubsystem::SetAgentHeight(FAntHandle Handle, float Height)
{
	check(AgentStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = AgentStorage.Get(Handle, SlotAgt);


	auto &agent = Agents[dataIdx];

	// invalidate current navigation location
	agent.NodeRef = agent.Height != Height ? INVALID_NAVNODEREF : agent.NodeRef;

	// update height
	agent.Height = Height;
	BroadphaseGrid->UpdateHeight(agent.GridHandle, Height);
}

void UAntSubsystem::SetAgentFlag(FAntHandle Handle, int32 Flags)
{
	check(AgentStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = AgentStorage.Get(Handle, SlotAgt);

	// remove agent flag
	auto &agent = Agents[dataIdx];
	agent.Flag = Flags;
	BroadphaseGrid->UpdateFlag(agent.GridHandle, Flags);
}

bool UAntSubsystem::IsValidAgent(FAntHandle Handle) const
{
	return (AgentStorage.IsValid(Handle) && AgentStorage.Get(Handle, SlotAgt) != INDEX_NONE);
}

bool UAntSubsystem::IsValidAgents(const TArray<FAntHandle> Handles) const
{
	if (Handles.IsEmpty())
		return false;

	for (const auto &handle : Handles)
		if (!IsValidAgent(handle))
			return false;

	return true;
}

void UAntSubsystem::MoveAgentByPath(FAntHandle AgentHandle, FAntHandle PathHandle, EAntPathFollowerType PathFollowerType, float MaxSpeed, float Acceleration, float Deceleration,
	float TargetLocationRadius, float PathNodeRadius, float StartDelay, float MissingVelocityTimeout)
{
	check(IsValidAgent(AgentHandle) && "Invalid agent handle!");
	check(IsValidPath(PathHandle) && "Invalid path handle!");

	// check curretn in-progress movement
	if (IsValidMovement(AgentHandle))
	{
		// same path
		if (GetAgentMovement(AgentHandle).Path == PathHandle)
			return;

		// cancel current movement
		RemoveAgentMovement(AgentHandle);
	}

	// add e new movements record
	const auto dataIdx = Movements.Add({});
	Movements[dataIdx].TargetRadiusSquared = TargetLocationRadius * TargetLocationRadius;
	Movements[dataIdx].MissingVelocityTimeout = MissingVelocityTimeout;
	Movements[dataIdx].PathNodeRadiusSquared = PathNodeRadius * PathNodeRadius;
	Movements[dataIdx].MaxSpeed = MaxSpeed;
	Movements[dataIdx].Acceleration = Acceleration;
	Movements[dataIdx].Deceleration = Deceleration;
	Movements[dataIdx].Speed = Acceleration;
	Movements[dataIdx].StartDelay = StartDelay;
	Movements[dataIdx].Path = PathHandle;
	Movements[dataIdx].PathVer = GetPathData(PathHandle).Ver;
	Movements[dataIdx].PathFollowerType = PathFollowerType;
	Movements[dataIdx].Destination = !GetPathData(PathHandle).Data.IsEmpty() ? GetPathData(PathHandle).Data.Last().Location : FVector(GetAgentData(AgentHandle).Location);

	// set it to the storage
	AgentStorage.Set(AgentHandle, SlotMov, dataIdx);
	Movements[dataIdx].Handle = AgentHandle;
	Movements[dataIdx].UpdateResult = EAntMoveResult::NeedUpdate;
}

void UAntSubsystem::FollowAgent(FAntHandle FollowerHandle, FAntHandle FolloweeHandle, float MaxSpeed, float Acceleration, float Deceleration,
	int32 FolloweeRadius, float StartDelay, float MissingVelocityTimeout)
{
	check(IsValidAgent(FollowerHandle) && "Invalid handle!");
	check(IsValidAgent(FolloweeHandle) && "Invalid handle!");

	// cancel current movement
	if (IsValidMovement(FollowerHandle))
		RemoveAgentMovement(FollowerHandle);

	// add e new movements record
	const auto dataIdx = Movements.Add({});
	Movements[dataIdx].TargetRadiusSquared = FolloweeRadius * FolloweeRadius;
	Movements[dataIdx].MissingVelocityTimeout = MissingVelocityTimeout;
	Movements[dataIdx].MaxSpeed = MaxSpeed;
	Movements[dataIdx].Acceleration = Acceleration;
	Movements[dataIdx].Deceleration = Deceleration;
	Movements[dataIdx].Speed = Acceleration;
	Movements[dataIdx].Followee = FolloweeHandle;
	Movements[dataIdx].StartDelay = StartDelay;
	Movements[dataIdx].Destination = FVector(GetAgentData(FolloweeHandle).Location);

	// set it to the storage
	AgentStorage.Set(FollowerHandle, SlotMov, dataIdx);
	Movements[dataIdx].Handle = FollowerHandle;
}

void UAntSubsystem::RemoveAgentMovement(FAntHandle Handle, bool ShouldNotify)
{
	if (!IsValidMovement(Handle))
		return;

	// reset agent velocity
	auto &agent = GetMutableAgentData(Handle);
	agent.PreferredVelocity = FVector3f::ZeroVector;

	// reset index
	const auto dataIdx = AgentStorage.Get(Handle, SlotMov);
	AgentStorage.Set(Handle, SlotMov, INDEX_NONE);

	// remove path
	if (IsValidPath(Movements[dataIdx].Path) && GetPathData(Movements[dataIdx].Path).Owner == Handle)
		RemovePath(Movements[dataIdx].Path);

	// remove movement data
	Movements.RemoveAt(dataIdx);

	// notify 
	if (ShouldNotify)
	{
		OnMovementCanceled.Broadcast({ Handle });
		OnMovementCanceled_BP.Broadcast({ Handle });
	}
}

const FAntMovementData &UAntSubsystem::GetAgentMovement(FAntHandle Handle) const
{
	check(IsValidMovement(Handle));
	const auto dataIdx = AgentStorage.Get(Handle, SlotMov);
	return Movements[dataIdx];
}

FAntMovementData &UAntSubsystem::GetMutableAgentMovement(FAntHandle Handle)
{
	check(IsValidMovement(Handle));
	const auto dataIdx = AgentStorage.Get(Handle, SlotMov);
	return Movements[dataIdx];
}

void UAntSubsystem::ClearMovements()
{
	// collect handles for notifying phase
	TArray<FAntHandle> handleList;
	handleList.Reserve(Movements.Num());
	for (const auto &it : Movements)
		handleList.Add(it.Handle);

	// 
	for (const auto &it : Movements)
	{
		auto &agent = GetMutableAgentData(it.Handle);
		agent.PreferredVelocity = FVector3f::ZeroVector;

		// reset index
		const auto dataIdx = AgentStorage.Get(it.Handle, SlotMov);
		AgentStorage.Set(it.Handle, SlotMov, INDEX_NONE);

		// remove movement data
		Movements.RemoveAt(dataIdx);
	}

	// notify 
	if (handleList.IsEmpty())
	{
		OnMovementCanceled.Broadcast(handleList);
		OnMovementCanceled_BP.Broadcast(handleList);
	}
}

bool UAntSubsystem::IsValidMovement(FAntHandle Handle) const
{
	return (IsValidAgent(Handle) && AgentStorage.Get(Handle, SlotMov) != INDEX_NONE);
}

FAntHandle UAntSubsystem::AddEmptyPath(float Width, FSharedConstNavQueryFilter QueryFilter, FAntHandle Owner)
{
	// add new path to the list
	const auto dataIdx = Paths.Add({});
	Paths[dataIdx].Owner = Owner;
	Paths[dataIdx].Width = Width;
	Paths[dataIdx].FilterClass = QueryFilter;

	// set it to the storage
	const auto handle = PathStorage.Add();
	PathStorage.Set(handle, SlotPath, dataIdx);
	Paths[dataIdx].Handle = handle;

	return handle;
}

void UAntSubsystem::RemovePath(FAntHandle Handle)
{
	check(IsValidPath(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = PathStorage.Get(Handle, SlotPath);
	const auto userDataIdx = PathStorage.Get(Handle, SlotUserData);

	// notify
	OnPathRemoved.Broadcast(Handle);

	// remove user data
	if (userDataIdx != INDEX_NONE)
		UserData.RemoveAt(userDataIdx);

	// remove path from the list
	Paths.RemoveAt(dataIdx);

	// remove path from the storage
	PathStorage.Remove(Handle);
}

const FAntPathData &UAntSubsystem::GetPathData(FAntHandle Handle) const
{
	check(PathStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = PathStorage.Get(Handle, SlotPath);

	return Paths[dataIdx];
}

FInstancedStruct &UAntSubsystem::GetPathUserData(FAntHandle Handle)
{
	check(PathStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	auto dataIdx = PathStorage.Get(Handle, SlotPath);

	// create new custom data if it is not exist
	if (dataIdx == INDEX_NONE)
	{
		dataIdx = UserData.Add({});
		PathStorage.Set(Handle, SlotPath, dataIdx);
	}

	return UserData[dataIdx];
}

FAntPathData &UAntSubsystem::GetMutablePathData(FAntHandle Handle)
{
	check(PathStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = PathStorage.Get(Handle, SlotPath);

	return Paths[dataIdx];
}

bool UAntSubsystem::IsValidPath(FAntHandle Handle) const
{
	return (PathStorage.IsValid(Handle) && PathStorage.Get(Handle, SlotPath) != INDEX_NONE);
}

bool UAntSubsystem::IsValidLocation(const FVector2f &NavLocation)
{
	const auto shiftedLoc = NavLocation + ShiftSize;
	return (shiftedLoc.X >= 0 && shiftedLoc.Y >= 0 && shiftedLoc.X < ShiftSize && shiftedLoc.Y < ShiftSize);
}

void UAntSubsystem::QueryCylinder(const FVector3f &Base, float Radius, float Height, int32 Flags, bool MustIncludeCenter, TArray<FAntContactInfo> &OutCollided) const
{
	BroadphaseGrid->QueryCylinder(Base, Radius, Height, Flags, MustIncludeCenter, OutCollided);
}

FAntHandle UAntSubsystem::QueryCylinderAsync(const FVector3f &Base, float Radius, float Height, int32 Flags, bool MustIncludeCenter, float Interval)
{
	// add new query to the list
	const auto dataIdx = Queries.Add({});
	Queries[dataIdx].Type = EAntQueryType::Cylinder;
	Queries[dataIdx].Interval = Interval;
	Queries[dataIdx].Flag = Flags;
	Queries[dataIdx].MustIncludeCenter = MustIncludeCenter;
	Queries[dataIdx].Data.Cylinder.Base = Base;
	Queries[dataIdx].Data.Cylinder.Radius = Radius;

	// set it to the storage
	const auto handle = QueryStorage.Add();
	QueryStorage.Set(handle, SlotQry, dataIdx);
	Queries[dataIdx].Handle = handle;

	//
	return handle;
}

FAntHandle UAntSubsystem::QueryCylinderAttachedAsync(FAntHandle Handle, float Radius, float Height, int32 Flags, bool MustIncludeCenter, float Interval)
{
	check(IsValidAgent(Handle) && "invalid handle!");

	// add new query to the list
	const auto dataIdx = Queries.Add({});
	Queries[dataIdx].Type = EAntQueryType::CylinderAttached;
	Queries[dataIdx].Interval = Interval;
	Queries[dataIdx].Flag = Flags;
	Queries[dataIdx].MustIncludeCenter = MustIncludeCenter;
	Queries[dataIdx].Data.CylinderAttached.Handle = Handle;
	Queries[dataIdx].Data.CylinderAttached.Base = GetAgentData(Handle).Location;
	Queries[dataIdx].Data.CylinderAttached.Radius = Radius;
	Queries[dataIdx].Data.CylinderAttached.Height = Height;

	// set it to the storage
	const auto handle = QueryStorage.Add();
	QueryStorage.Set(handle, SlotQry, dataIdx);
	Queries[dataIdx].Handle = handle;

	//
	return handle;
}

void UAntSubsystem::QueryRay(const FVector3f &Start, const FVector3f &End, int32 Flags, TArray<FAntContactInfo> &OutCollided) const
{
	BroadphaseGrid->QueryRay(Start, End, Flags, OutCollided);
}

FAntHandle UAntSubsystem::QueryRayAsync(const FVector3f &Start, const FVector3f &End, int32 Flags, float Interval)
{
	// add new query to the list
	const auto dataIdx = Queries.Add({});
	Queries[dataIdx].Type = EAntQueryType::Ray;
	Queries[dataIdx].Interval = Interval;
	Queries[dataIdx].Flag = Flags;
	Queries[dataIdx].Data.Ray.Start = Start;
	Queries[dataIdx].Data.Ray.End = End;

	// set it to the storage
	const auto handle = QueryStorage.Add();
	QueryStorage.Set(handle, SlotQry, dataIdx);
	Queries[dataIdx].Handle = handle;

	//
	return handle;
}

void UAntSubsystem::QueryConvexVolume(const TStaticArray<FVector, 4> &NearPlane, const TStaticArray<FVector, 4> &FarPlane, int32 Flags, TArray<FAntContactInfo> &OutCollided) const
{
	BroadphaseGrid->QueryConvexVolume(NearPlane, FarPlane, Flags, OutCollided);
}

const FAntQueryData &UAntSubsystem::GetAsyncQueryData(FAntHandle Handle) const
{
	check(QueryStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = QueryStorage.Get(Handle, SlotQry);

	return Queries[dataIdx];
}

FAntQueryData &UAntSubsystem::GetMutableAsyncQueryData(FAntHandle Handle)
{
	check(QueryStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = QueryStorage.Get(Handle, SlotQry);

	return Queries[dataIdx];
}

FInstancedStruct &UAntSubsystem::GetAsyncQueryUserData(FAntHandle Handle)
{
	check(QueryStorage.IsValid(Handle) && "Invalid handle!");

	// retrieve index
	auto dataIdx = QueryStorage.Get(Handle, SlotUserData);

	// create new custom data if it is not exist
	if (dataIdx == INDEX_NONE)
	{
		dataIdx = UserData.Add({});
		QueryStorage.Set(Handle, SlotUserData, dataIdx);
	}

	return UserData[dataIdx];
}

void UAntSubsystem::RemoveAsyncQuery(FAntHandle Handle)
{
	check(IsValidAsyncQuery(Handle) && "Invalid handle!");

	// retrieve index
	const auto dataIdx = QueryStorage.Get(Handle, SlotQry);
	const auto userIdx = QueryStorage.Get(Handle, SlotUserData);

	// remove query from the list
	Queries.RemoveAt(dataIdx);

	// remove user data
	if (userIdx != INDEX_NONE)
		UserData.RemoveAt(userIdx);

	// remove query from the storage
	QueryStorage.Remove(Handle);
}

void UAntSubsystem::ClearAsyncQueries()
{
	for (int32 idx = 0; idx < Queries.GetMaxIndex(); ++idx)
		if (Queries.IsValidIndex(idx))
			RemoveAsyncQuery(Queries[idx].Handle);

	Queries.Reset();
}

bool UAntSubsystem::IsValidAsyncQuery(FAntHandle Handle)
{
	return QueryStorage.IsValid(Handle);
}

EAntHandleTypes UAntSubsystem::GetHandleType(FAntHandle Handle)
{
	switch (Handle.Type)
	{
	case static_cast<uint8>(EAntHandleTypes::Agent):
		return EAntHandleTypes::Agent;

	case static_cast<uint8>(EAntHandleTypes::AsyncQuery):
		return EAntHandleTypes::AsyncQuery;

	case static_cast<uint8>(EAntHandleTypes::Path):
		return EAntHandleTypes::Path;
	}

	return EAntHandleTypes::Invalid;
}

FVector3f UAntSubsystem::DefaultSolver(FAntAgentData &Agent)
{
	const uint32 allFlags = -1 & ~(Agent.IgnoreFlag);
	// add other forces to the final velocity
	const auto overlapForce = Agent.OverlapForce.GetSafeNormal() * Agent.MaxOverlapForce;
	const auto prefVel = FVector2f(Agent.PreferredVelocity) + overlapForce;
	auto prefLen = prefVel == FVector2f::ZeroVector ? 0.0f : prefVel.Length();
	const auto prefNorm = prefVel.GetSafeNormal();
	const auto affectedRadius = Agent.Radius + FMath::RoundToInt32(prefLen) + 1;
	const FVector2f pos2D(Agent.Location.X, Agent.Location.Y);
	auto finalNorm = prefNorm;
	auto finalZ = Agent.Location.Z + Agent.PreferredVelocity.Z;

	// consume forces
	Agent.OverlapForce = FVector2f::ZeroVector;

	struct FNormal { FVector2f Norm; bool Segment = false; };
	static thread_local TArray<FAntContactInfo> overlap;
	static thread_local TArray<FNormal> overlapNormals;
	overlap.Reset();
	overlapNormals.Reset();

	// query affected area at maximum speed
	BroadphaseGrid->QueryCylinder(Agent.Location, affectedRadius, Agent.Height, allFlags, false, overlap);

	// step 1: collect normals from overlapped objects
	for (auto &it : overlap)
	{
		bool overlapped = false;

		// agents overlap solver
		if (it.Handle != Agent.Handle)
		{
			const auto sumRadiusEx = Agent.Radius + it.Cylinder.Radius + 1;
			const auto sqRadiusEx = sumRadiusEx * sumRadiusEx;
			if (it.SqDist < sqRadiusEx)
			{
				// mark collided agent to wake it up for next frame
				auto &collidedAgent = GetMutableAgentData(it.Handle);

				// stacking
				const auto distDelta = sqRadiusEx - it.SqDist;
				//Agent.bIsStacked = false;
				//if (/*CHECK_BIT_ANY(collidedAgent.Flag, Agent.StackFlag) && collidedAgent.StackPriority < Agent.StackPriority*/ Agent.Handle.Idx > collidedAgent.Handle.Idx
				//	&& distDelta >= Settings->MinOverlapStackSizeSq && (it.Cylinder.Base.Z + it.Cylinder.Height) <= (Agent.Location.Z + Agent.MaxStackStepHeight))
				//{
				//	const auto zalpha = it.SqDist / (sqRadiusEx - Settings->MinOverlapStackSizeSq);					
				//	finalZ = FMath::Max(finalZ, FMath::Lerp(it.Cylinder.Base.Z, it.Cylinder.Base.Z + it.Cylinder.Height, 1.0f - zalpha));
				//	Agent.bIsStacked = true;
				//}
				// non-stacking 
				//else if (!CHECK_BIT_ANY(Agent.Flag, collidedAgent.StackFlag) || Agent.StackPriority == collidedAgent.StackPriority)
				{
					collidedAgent.bCollided = true;

					// skip ignored agents
					if (CHECK_BIT_ANY(collidedAgent.Flag, Agent.IgnoreButWakeUpFlag))
					{
						it.bIgnored = true;
						continue;
					}

					// a collision happened, so we try to get away from collided agent
					// in case of equal centers, we use owner.idx to produce a unique normal
					auto norm = pos2D == FVector2f(it.Cylinder.Base) ? FVector2f(FMath::Sin(static_cast<float>(it.Handle.Idx)), FMath::Cos(static_cast<float>(Agent.Handle.Idx))).GetSafeNormal()
						: (pos2D - FVector2f(it.Cylinder.Base)).GetSafeNormal(0.0f);

					Agent.OverlapForce += norm;

					// ignore agent overlap normals at knocked state
					if (!Agent.bKnocked)
						overlapNormals.Push({ -norm, false });
				}

				overlapped = true;
			}
		}

		// ignore self, oerlapped, knocked agents
		it.bIgnored = (it.Handle == Agent.Handle || overlapped || Agent.bKnocked);
	}

	// step 2: merge overlapped normals and also checking piercing angle
	FVector2f finalCW = FVector2f::ZeroVector;
	FVector2f finalCCW = FVector2f::ZeroVector;
	TPair<float, FVector2f> pierceCW = { -2.0f, FVector2f::ZeroVector };
	TPair<float, FVector2f> pierceCCW = { -2.0f, FVector2f::ZeroVector };
	bool fullyBlocked = false;
	bool canPierce = Agent.bCanPierce;
	for (const auto &it : overlapNormals)
	{
		// define each axis
		const FVector2f normCW(-it.Norm.Y, it.Norm.X);
		const FVector2f normCCW(it.Norm.Y, -it.Norm.X);

		// merge cw axis
		// in case of duplication in the overlapNormals, we have to check the value first (normCW != finalCW)
		const auto resultCW = normCW != finalCW ? FAntMath::IsInsideCW(normCCW, normCW, finalCW) : 0;
		finalCW = resultCW >= 0 ? normCW : finalCW;

		// merge ccw axis
		// in case of duplication in the overlapNormals, we have to check the value first (normCCW != finalCCW)
		const auto resultCCW = normCCW != finalCCW ? FAntMath::IsInsideCW(normCCW, normCW, finalCCW) : 0;
		finalCCW = resultCCW >= 0 ? normCCW : finalCCW;

		// fully blocked
		if (resultCW > 0 && resultCCW > 0)
			fullyBlocked = true;

		// checking for piercing angle.
		// there is some sichuation that the agent is blocked but its neighbor agents are close enough to 90 degree of the preferred direction.
		// so we can pierece them and it will avoid stucking of multiple agents while trying to move through a tight bottleneck.
		if (canPierce)
		{
			// positive result means its against our finalNorm
			const auto result = FVector2f::CrossProduct({ finalNorm.Y, -finalNorm.X }, it.Norm);
			// positive results means its at CCW (left) side of our finalNorm
			const auto ccwOrCW = FVector2f::CrossProduct({ -finalNorm.X, -finalNorm.Y }, it.Norm);

			// pierce range (left)
			if (result > pierceCCW.Key && ccwOrCW >= 0)
				pierceCCW = { result, it.Norm };

			// pierce range (right)
			if (result > pierceCW.Key && ccwOrCW < 0)
				pierceCW = { result, it.Norm };

			// there is a blocking obstacle segment and agents can't pierce obstacles!
			if (result > 0 && it.Segment)
				canPierce = false;
		}

		// no way!
		if (!canPierce && fullyBlocked)
			break;
	}

	// step 3: check final allowed range against preferred direction
	if (fullyBlocked)
		finalNorm = FVector2f::ZeroVector;

	if (!fullyBlocked)
	{
		// we can slide to closest axis (cw or ccw) if we are close enoughe to one of them
		const auto result = FAntMath::IsInsideCW(finalCCW, finalCW, finalNorm);
		if (result > 0 && finalNorm != finalCW && finalNorm != finalCCW)
		{
			const auto ccwAngle = FAntMath::Angle(finalCCW, finalNorm);
			const auto cwAngle = FAntMath::Angle(finalCW, finalNorm);
			finalNorm = FVector2f::ZeroVector;

			// close to ccw
			if (ccwAngle <= cwAngle && ccwAngle < RAD_90)
			{
				// cap speed to avoid pendulum effect
				float t = 0;
				FAntMath::ClosestPointSegment({ FVector2f::ZeroVector, finalCCW * prefLen }, prefNorm * prefLen, t);
				prefLen *= t;

				// project on ccw
				finalNorm = finalCCW;
			}

			// close to cw
			if (cwAngle < ccwAngle && cwAngle <= RAD_90)
			{
				// cap speed to avoid pendulum effect
				float t = 0;
				FAntMath::ClosestPointSegment({ FVector2f::ZeroVector, finalCW * prefLen }, prefNorm * prefLen, t);
				prefLen *= t;

				// project on cw
				finalNorm = finalCW;
			}
		}
	}

	if (canPierce && pierceCCW.Key >= -1.0f && pierceCW.Key >= -1 && finalNorm == FVector2f::ZeroVector)
		if (FAntMath::Angle(pierceCCW.Value, pierceCW.Value) >= Settings->PierceAngle)
			finalNorm = prefNorm;

	// there is no allowed path!
	if (finalNorm == FVector2f::ZeroVector)
		return FVector3f(Agent.Location.X, Agent.Location.Y, finalZ);

	// 
	const FVector2f finalVel = finalNorm * prefLen;
	FVector2f hitPoint = pos2D + finalVel;
	FAntContactInfo::FindClosestCollision({ pos2D, finalNorm }, prefLen, Agent.Radius, overlap, hitPoint);
	return FVector3f(hitPoint.X, hitPoint.Y, finalZ);
}

FVector3f UAntSubsystem::ORCASolver(FAntAgentData &Agent)
{
	const uint32 allFlags = -1 & ~(Agent.IgnoreFlag);
	// add other forces to the final velocity
	const auto overlapForce = Agent.OverlapForce.GetSafeNormal() * Agent.MaxOverlapForce;
	const auto prefVel = FVector2f(Agent.PreferredVelocity) + overlapForce;
	const auto prefLen = prefVel == FVector2f::ZeroVector ? 0.0f : prefVel.Length();
	const auto affectedRadius = Agent.Radius + FMath::RoundToInt32(prefLen) + 1.f;
	const FVector2f pos2D(Agent.Location);
	const FVector2f vel2D(Agent.Velocity);

	// consume forces
	Agent.OverlapForce = FVector2f::ZeroVector;

	struct FNormal { FVector2f Norm; bool Segment = false; };
	static thread_local TArray<FAntContactInfo> neighbours;
	static thread_local TArray<FAntRay> orcaLines;
	orcaLines.Reset();
	neighbours.Reset();

	// query affected area at maximum speed
	BroadphaseGrid->QueryCylinder(Agent.Location, affectedRadius + Agent.ExtraQueryRadius, Agent.Height, allFlags, false, neighbours);

	// Create obstacle ORCA lines.
	const auto numObstLines = orcaLines.Num();
	const float invTimeHorizon = 1.0F / Settings->RVOTimeHorizon;

	for (auto &it : neighbours)
	{
		if (it.Handle != Agent.Handle)
		{
			const auto &neighbourAgent = GetAgentData(it.Handle);
			const auto relativePosition = FVector2f(it.Cylinder.Base) - pos2D;
			const auto relativeVelocity = vel2D - FVector2f(neighbourAgent.Velocity);
			const auto distSq = it.SqDist;
			const float combinedRadius = Agent.Radius + it.Cylinder.Radius;
			const float combinedRadiusSq = combinedRadius * combinedRadius;

			FAntRay line;
			FVector2f u;

			// no collison
			if (distSq > combinedRadiusSq)
			{
				const auto w = relativeVelocity - invTimeHorizon * relativePosition;
				/* Vector from cutoff center to relative velocity. */
				const float wLengthSq = w.SizeSquared();

				const float dotProduct = FVector2f::DotProduct(w, relativePosition);
				if (dotProduct < 0.0F && dotProduct * dotProduct > combinedRadiusSq * wLengthSq)
				{
					/* Project on cut-off circle. */
					const float wLength = FMath::Sqrt(wLengthSq);
					const auto unitW = w / wLength;

					line.Dir = FVector2f(unitW.Y, -unitW.X);
					u = (combinedRadius * invTimeHorizon - wLength) * unitW;

					//DrawDebugLine(GetWorld(), FVector(Agent.Location.X, Agent.Location.Y, Agent.Location.Z + 10),
						//FVector(Agent.Location.X, Agent.Location.Y, Agent.Location.Z + 30) + FVector(line.Dir.X, line.Dir.Y, 0) * 150, FColor::Red, false, 1, 0, 10);
				}
				else {
					/* Project on legs. */
					const float leg = FMath::Sqrt(distSq - combinedRadiusSq);

					if (FAntMath::Determinant(relativePosition, w) > 0.0F)
					{
						/* Project on left leg. */
						line.Dir = FVector2f(relativePosition.X * leg -
							relativePosition.Y * combinedRadius,
							relativePosition.X * combinedRadius +
							relativePosition.Y * leg) /
							distSq;
					}
					else
					{
						/* Project on right leg. */
						line.Dir = -FVector2f(relativePosition.X * leg +
							relativePosition.Y * combinedRadius,
							-relativePosition.X * combinedRadius +
							relativePosition.Y * leg) /
							distSq;
					}

					//DrawDebugLine(GetWorld(), FVector(Agent.Location.X, Agent.Location.Y, Agent.Location.Z + 40),
						//FVector(Agent.Location.X, Agent.Location.Y, Agent.Location.Z + 40) + FVector(line.Dir.X, line.Dir.Y, 0) * 150, FColor::Green, false, 1, 0, 10);

					u = (relativeVelocity * line.Dir) * line.Dir -
						relativeVelocity;
				}
			}

			// collision
			else
			{
				/* Collision. Project on cut-off circle of time timeStep. */
				const float invTimeStep = 1.0F;// / timeStep;

				/* Vector from cutoff center to relative velocity. */
				const auto w = relativeVelocity - invTimeStep * relativePosition;

				const auto wLength = w.Size();
				const auto unitW = w / wLength;

				line.Dir = FVector2f(unitW.Y, -unitW.X);
				u = (combinedRadius * invTimeStep - wLength) * unitW;

				// mark collided agent to wake it up for next frame
				auto &collidedAgent = GetMutableAgentData(it.Handle);
				collidedAgent.bCollided = true;

				// skip ignored agents
				if (!CHECK_BIT_ANY(collidedAgent.Flag, Agent.IgnoreButWakeUpFlag))
				{
					// a collision happened, so we try to get away from collided agent
					// in case of equal centers, we use owner.idx to produce a unique normal
					auto norm = pos2D == FVector2f(it.Cylinder.Base) ? FVector2f(FMath::Sin(static_cast<float>(it.Handle.Idx)), FMath::Cos(static_cast<float>(Agent.Handle.Idx))).GetSafeNormal()
						: (pos2D - FVector2f(it.Cylinder.Base)).GetSafeNormal(0.0f);

					Agent.OverlapForce += norm;
				}
			}

			line.Start = vel2D + 0.5F * u;
			//DrawDebugLine(GetWorld(), FVector(Agent.Location), FVector(Agent.Location) + FVector(line.Dir.X, line.Dir.Y, 0) * 200, FColor::Blue, false, 1, 0, 10);
			orcaLines.Add(line);
		}
	}

	FVector2f resultVel = FVector2f::ZeroVector;
	const auto lineFail = RVOProgram2(orcaLines, prefLen, prefVel, false, resultVel);

	if (lineFail < orcaLines.Num())
		RVOProgram3(orcaLines, numObstLines, lineFail, prefLen, resultVel);

	//DrawDebugLine(GetWorld(), FVector(Agent.Location), FVector(Agent.Location) + FVector(resultVel.X, resultVel.Y, 0).GetSafeNormal() * 300, FColor::Orange, false, 1, 0, 10);
	return FVector3f(resultVel.X + Agent.Location.X, resultVel.Y + Agent.Location.Y, Agent.Location.Z + Agent.PreferredVelocity.Z);
}

bool UAntSubsystem::RVOProgram1(const TArray<FAntRay> &Lines, int32 LineNo, float Radius, const FVector2f &OptVelocity, bool DirectionOpt, FVector2f &Result)
{
	const float dotProduct = Lines[LineNo].Start.Dot(Lines[LineNo].Dir);
	const float discriminant = dotProduct * dotProduct + Radius * Radius - Lines[LineNo].Start.SizeSquared();

	// Max speed circle fully invalidates line lineNo.
	if (discriminant < 0.0F)
		return false;

	const float sqrtDiscriminant = FMath::Sqrt(discriminant);
	float tLeft = -dotProduct - sqrtDiscriminant;
	float tRight = -dotProduct + sqrtDiscriminant;

	for (int32 idx = 0; idx < LineNo; ++idx)
	{
		const float denominator = FAntMath::Determinant(Lines[LineNo].Dir, Lines[idx].Dir);
		const float numerator = FAntMath::Determinant(Lines[idx].Dir, Lines[LineNo].Start - Lines[idx].Start);

		if (FMath::Abs(denominator) <= FLT_EPSILON)
		{
			// Lines lineNo and i are (almost) parallel. 
			if (numerator < 0.0F)
				return false;

			continue;
		}

		const float t = numerator / denominator;

		// Line i bounds line lineNo on the right.
		if (denominator >= 0.0F)
			tRight = FMath::Min(tRight, t);

		// Line i bounds line lineNo on the left.
		else
			tLeft = FMath::Max(tLeft, t);

		if (tLeft > tRight)
			return false;
	}

	if (DirectionOpt)
	{
		// Optimize direction. Take right extreme. 
		if (OptVelocity.Dot(Lines[LineNo].Dir) > 0.0F)
			Result = Lines[LineNo].Start + tRight * Lines[LineNo].Dir;

		// Take left extreme
		else
			Result = Lines[LineNo].Start + tLeft * Lines[LineNo].Dir;
	}
	else
	{
		// Optimize closest point.
		const float t = Lines[LineNo].Dir.Dot(OptVelocity - Lines[LineNo].Start);

		if (t < tLeft)
			Result = Lines[LineNo].Start + tLeft * Lines[LineNo].Dir;

		if (t > tRight)
			Result = Lines[LineNo].Start + tRight * Lines[LineNo].Dir;
		else
			Result = Lines[LineNo].Start + t * Lines[LineNo].Dir;
	}

	return true;
}

int32 UAntSubsystem::RVOProgram2(const TArray<FAntRay> &Lines, float Radius, const FVector2f &OptVelocity, bool DirectionOpt, FVector2f &Result)
{
	// Optimize direction. Note that the optimization velocity is of unit length in this case.
	if (DirectionOpt)
		Result = OptVelocity * Radius;

	// Optimize closest point and outside circle. 
	if (OptVelocity.SizeSquared() > Radius * Radius)
		Result = OptVelocity.GetSafeNormal(0.0f) * Radius;

	// Optimize closest point and inside circle.
	else
		Result = OptVelocity;

	for (int32 i = 0U; i < Lines.Num(); ++i)
		if (FAntMath::Determinant(Lines[i].Dir, Lines[i].Start - Result) > 0.0F)
		{
			/* Result does not satisfy constraint i. Compute new optimal result. */
			const auto tempResult = Result;

			if (!RVOProgram1(Lines, i, Radius, OptVelocity, DirectionOpt, Result))
			{
				Result = tempResult;
				return i;
			}
		}

	return Lines.Num();
}

void UAntSubsystem::RVOProgram3(const TArray<FAntRay> &Lines, int32 NumObstLines, int32 BeginLine, float Radius, FVector2f &Result)
{
	float distance = 0.0F;

	for (int32 idx = BeginLine; idx < Lines.Num(); ++idx)
	{
		if (FAntMath::Determinant(Lines[idx].Dir, Lines[idx].Start - Result) > distance)
		{
			// Result does not satisfy constraint of line idx.
			TArray<FAntRay> projLines(&Lines[0], NumObstLines);
			for (int32 idx2 = NumObstLines; idx2 < idx; ++idx2)
			{
				FAntRay line;
				const float determinant = FAntMath::Determinant(Lines[idx].Dir, Lines[idx2].Dir);

				if (FMath::Abs(determinant) <= FLT_EPSILON)
				{
					// Line idx and line idx2 are parallel.
					if (Lines[idx].Dir.Dot(Lines[idx2].Dir) > 0.0F)
						continue;

					// Line idx and line idx2 point in opposite direction.
					line.Start = 0.5F * (Lines[idx].Start + Lines[idx2].Start);
				}
				else
				{
					line.Start = Lines[idx].Start + (FAntMath::Determinant(Lines[idx2].Dir, Lines[idx].Start - Lines[idx2].Start) / determinant) * Lines[idx].Dir;
				}

				line.Dir = (Lines[idx2].Dir - Lines[idx].Dir).GetSafeNormal(0.0f);
				projLines.Add(line);
			}

			const auto tempResult = Result;

			// This should in principle not happen. The result is by definition
			// already in the feasible region of this linear program. If it fails,
			// it is due to small floating point error, and the current result is kept.
			if (RVOProgram2(projLines, Radius, FVector2f(-Lines[idx].Dir.Y, Lines[idx].Dir.X), true, Result) < projLines.Num())
				Result = tempResult;

			distance = FAntMath::Determinant(Lines[idx].Dir, Lines[idx].Start - Result);
		}
	}
}

void UAntSubsystem::UpdateCollisionsAndQueries(float Delta, bool CollisionCanTick)
{
	const auto lerpAlpha = Delta != 0 ? Settings->LerpTickInterval / Delta : 0;

	// fill each thread with the specific number of the agents to process.
	{
		INC_DWORD_STAT_BY(STAT_ANT_NumAgents, Agents.Num());
		INC_DWORD_STAT_BY(STAT_ANT_NumAsyncQueries, Queries.Num());
		SCOPE_CYCLE_COUNTER(STAT_ANT_QPS);

		// run colllison solver tasks
		ParallelFor(Agents.GetMaxIndex(), [&](int32 idx)
			{
				SCOPE_CYCLE_COUNTER(STAT_ANT_PxUpdate);

				if (!Agents.IsValidIndex(idx))
					return;

				auto &agent = Agents[idx];
				// skip idle sleep units
				if (CollisionCanTick && (agent.PreferredVelocity != FVector3f::ZeroVector || agent.OverlapForce != FVector2f::ZeroVector || !agent.bSleep || agent.bIsOnNavLink))
				{
					auto newPos = agent.Location;

					// swip to final position
					if (!agent.bDisabled)
					{
						newPos = agent.AvoidanceType == EAntAvoidanceTypes::AntDefault ? DefaultSolver(agent) : ORCASolver(agent);

						// reset lerp alpha after each new swip
						agent.LerpAlpha = 0.0f;
					}

					// collison phase velocity
					agent.Velocity = newPos - agent.Location;

					// current face angle from preferred velocity
					if (agent.bTurnByPreferred && FVector2f(agent.PreferredVelocity) != FVector2f::ZeroVector)
					{
						const auto norm = agent.PreferredVelocity.GetSafeNormal();
						agent.FinalFaceAngle = FMath::Atan2(norm.Y, norm.X);
					}

					// current face angle from current velocity
					if (!agent.bTurnByPreferred && agent.Velocity.X != 0 && agent.Velocity.Y != 0)
					{
						const auto norm = FVector2f(agent.Velocity).GetSafeNormal();
						agent.FinalFaceAngle = FMath::Atan2(norm.Y, norm.X);
					}

					// in case of turn before move we will reset the location to its old location if agent is not at the final face angle
					// note: if you changed that radian threshold, you have to update it in the movementUpdate() also.
					if (FMath::Abs(agent.FinalFaceAngle - agent.FaceAngle) > (agent.MoveAngleThreshold + RAD_5))
						agent.Velocity = FVector3f::ZeroVector;

					agent.bKnocked = false;
				}

				// update face direction interpolation
				UpdateFaceDirection(agent);

				// lerp alpha
				agent.LerpAlpha = FMath::Min(1.0f, agent.LerpAlpha + lerpAlpha);
			});

		// run query tasks
		ParallelFor(Queries.GetMaxIndex(), [&](int32 idx)
			{
				SCOPE_CYCLE_COUNTER(STAT_ANT_Queries);

				if (!Queries.IsValidIndex(idx))
					return;

				auto &queryData = Queries[idx];

				// check interval
				queryData.ElapsedTime += Delta;
				if (queryData.ElapsedTime >= queryData.Interval)
				{
					// clear old query result
					queryData.Result.Reset(0);

					// 
					queryData.ElapsedTime = 0;

					// agent position query
					if (queryData.Type == EAntQueryType::CylinderAttached)
					{
						// agent refrence is invalid so we mark this query for destroy
						if (!IsValidAgent(queryData.Data.CylinderAttached.Handle))
						{
							queryData.Interval = -1;
							return;
						}

						// 
						BroadphaseGrid->QueryCylinder(queryData.Data.CylinderAttached.Base, queryData.Data.CylinderAttached.Radius, queryData.Data.CylinderAttached.Height,
							queryData.Flag, queryData.MustIncludeCenter, queryData.Result);
						return;
					}

					// point query
					if (queryData.Type == EAntQueryType::Cylinder)
					{
						BroadphaseGrid->QueryCylinder(queryData.Data.Cylinder.Base, queryData.Data.Cylinder.Radius, queryData.Data.Cylinder.Height,
							queryData.Flag, queryData.MustIncludeCenter, queryData.Result);
						return;
					}

					// ray query
					if (queryData.Type == EAntQueryType::Ray)
					{
						BroadphaseGrid->QueryRay(queryData.Data.Ray.Start, queryData.Data.Ray.End, queryData.Flag, queryData.Result);
						return;
					}
				}
			});

		// run navigation tasks
		if (CollisionCanTick)
			OnUpdateNav.ExecuteIfBound();
	}

	// post update agents in the broadphase grid
	{
		SCOPE_CYCLE_COUNTER(STAT_ANT_BFUpdate);
		if (CollisionCanTick)
			for (auto &agent : Agents)
			{
				// in case of collision from other thread with this agent, we have to wake it up for the next frame
				agent.bSleep = agent.bCollided || agent.bIsOnNavLink ? false : agent.bSleep;
				agent.bCollided = false;

				// update agent in the grid
				if (agent.bUpdateGrid)
				{
					// remove agent from current place
					BroadphaseGrid->Remove(agent.GridHandle);
					agent.GridHandle = BroadphaseGrid->AddCylinder(agent.Handle, agent.Location, agent.Radius, agent.Height, agent.Flag);
					agent.bUpdateGrid = false;
				}
			}
	}
}

void UAntSubsystem::UpdateFaceDirection(FAntAgentData &CylinderAttached)
{
	// very first velocity
	if (CylinderAttached.FaceAngle == CylinderAttached.FinalFaceAngle)
		return;

	// update turn angle
	const auto delta = FMath::Abs(CylinderAttached.FinalFaceAngle - CylinderAttached.FaceAngle);
	const auto needNear = delta > PI;
	const auto rate = FMath::Min((needNear ? RAD_360 - delta : delta), CylinderAttached.TurnRate);

	if (!needNear)
		CylinderAttached.FaceAngle = CylinderAttached.FaceAngle > CylinderAttached.FinalFaceAngle ? CylinderAttached.FaceAngle - rate : CylinderAttached.FaceAngle + rate;

	if (needNear)
		CylinderAttached.FaceAngle = CylinderAttached.FaceAngle < CylinderAttached.FinalFaceAngle ? CylinderAttached.FaceAngle - rate : CylinderAttached.FaceAngle + rate;

	// round
	if (FMath::Abs(CylinderAttached.FaceAngle) > PI)
		CylinderAttached.FaceAngle = CylinderAttached.FaceAngle > 0.0f ? -PI + (CylinderAttached.FaceAngle - PI) : PI - (FMath::Abs(CylinderAttached.FaceAngle) - PI);
}

void UAntSubsystem::UpdateMovements(float Delta, float DeltaMul)
{
	// fill each thread with the specific number of agents to process.
	INC_DWORD_STAT_BY(STAT_ANT_NumMovingAgents, Movements.Num());
	ParallelFor(Movements.GetMaxIndex(), [&](int32 idx)
		{
			SCOPE_CYCLE_COUNTER(STAT_ANT_MovementsUpdate);
			// skip invalid indexes
			if (!Movements.IsValidIndex(idx))
				return;

			auto &moveData = Movements[idx];
			auto &agent = GetMutableAgentData(moveData.Handle);

			// delayed movement
			moveData.StartDelay -= FMath::Min(Delta, moveData.StartDelay);
			if (moveData.StartDelay > 0.0f)
				return;

			// follower agent with invalid followee, invalid or blocked path, all count as canceled movement
			const auto followAgent = IsValidAgent(moveData.Followee);
			if (!followAgent && (!IsValidPath(moveData.Path) || GetPathData(moveData.Path).GetData().IsEmpty() || GetPathData(moveData.Path).Status == EAntPathStatus::Blocked))
			{
				moveData.UpdateResult = EAntMoveResult::Canceled;
				agent.PreferredVelocity = FVector3f::ZeroVector;
				return;
			}

			const FVector3f currentPos = agent.GetLocation();
			const FVector3f targetPos = followAgent ? GetAgentData(moveData.Followee).Location : FVector3f(GetPathData(moveData.Path).Data.Last().Location);
			FVector3f dist(targetPos - currentPos);
			FVector3f dir = FVector3f::ZeroVector;
			auto distToEnd = 0.0f;
			auto acc = moveData.Acceleration;

			// calculate deceleration distance
			const auto decDist = moveData.Deceleration > 0.0f ? (moveData.Speed * moveData.Speed) / (2 * moveData.Deceleration) : 0;

			// 
			if (moveData.UpdateResult != EAntMoveResult::NeedUpdate)
			{
				agent.PreferredVelocity = FVector3f::ZeroVector;
				return;
			}

			// check distance to the target position
			if (currentPos == targetPos || dist.SizeSquared() <= moveData.TargetRadiusSquared + 2.0f)
			{
				moveData.UpdateResult = EAntMoveResult::GoalReached;
				agent.PreferredVelocity = FVector3f::ZeroVector;
				return;
			}

			// check missing velocity threshold
			if (moveData.MissingVelocityTimeout >= 0.0f && (FMath::Abs(agent.FinalFaceAngle - agent.FaceAngle) <= (agent.MoveAngleThreshold + RAD_5)))
			{
				const auto missVel = FVector2f(agent.PreferredVelocity - agent.GetVelocity());
				moveData.MissingVelocitySum += FMath::Abs(missVel.X) + FMath::Abs(missVel.Y);

				// reset missing velocity sum if we are moving at the preferred velocity
				if (missVel == FVector2f::ZeroVector || agent.PreferredVelocity == FVector3f::ZeroVector)
					moveData.MissingVelocitySum = 0;

				// 
				if (moveData.MissingVelocitySum > moveData.MissingVelocityTimeout)
				{
					moveData.UpdateResult = EAntMoveResult::VelocityTimedout;
					moveData.MissingVelocitySum = 0;
					return;
				}
			}

			// follow agent
			if (followAgent)
			{
				const auto distLen = dist.Size();
				dir = dist / distLen;
				distToEnd = distLen;
			}

			// move through each waypoint on the portals until we get correct waypoint to follow
			if (moveData.PathFollowerType == EAntPathFollowerType::Waypoint && !followAgent)
			{
				const auto &pathData = GetPathData(moveData.Path);

				// in case of replanning, we reset the curretn path index
				moveData.PathIndex = moveData.PathVer != pathData.Ver ? 0 : moveData.PathIndex;
				moveData.PathVer = pathData.Ver;

				auto portalLoc = pathData.Data[moveData.PathIndex].Location;
				for (int32 pidx = moveData.PathIndex; pidx < pathData.Data.Num(); ++pidx)
				{
					moveData.PathIndex = pidx;
					portalLoc = pathData.Data[pidx].Location;
					if (FVector3f::DistSquared(currentPos, FVector3f(portalLoc)) > moveData.PathNodeRadiusSquared)
						break;
				}

				dist = FVector3f(portalLoc) - currentPos;
				const auto distLen = dist.Size();
				dir = dist / distLen;
				distToEnd = distLen + pathData.Data[moveData.PathIndex].Distance;
			}

			// move through corridor with flow velocity
			if (moveData.PathFollowerType == EAntPathFollowerType::FlowField && !followAgent)
			{
				const auto &pathData = GetPathData(moveData.Path);

				// in case of replanning, we reset the curretn path index
				moveData.PathIndex = moveData.PathVer != pathData.Ver ? 0 : moveData.PathIndex;
				moveData.PathVer = pathData.Ver;

				// find nearet point to the corrdior area
				auto nearestLoc = pathData.FindNearestLocationOnCorridor(FVector(currentPos), moveData.PathIndex);

				// nav link.
				const auto isLink = pathData.Data[nearestLoc.PortalIndex].Type == FAntPathData::Portal::Link;
				// mark as changed/updated
				agent.bNavLinkUpdated = isLink != agent.bIsOnNavLink;
				agent.bIsOnNavLink = isLink;

				// make sure we are inside the corridor area
				const auto isInsideCorridor = (nearestLoc.HoriT <= 1.0f && nearestLoc.HoriT >= 0.0f);
				if (isInsideCorridor)
				{
					nearestLoc.VertT = FMath::Clamp(nearestLoc.VertT, 0.0f, 1.0f);
					nearestLoc.HoriT = FMath::Clamp(nearestLoc.HoriT, 0.0f, 1.0f);

					distToEnd = FMath::Lerp(pathData.Data[nearestLoc.PortalIndex].Distance, pathData.Data[nearestLoc.PortalIndex + 1].Distance, nearestLoc.VertT);

					if (nearestLoc.VertT >= 0.99f && nearestLoc.PortalIndex < pathData.Data.Num() - 2)
						++nearestLoc.PortalIndex;

					// debug draw purpose
					moveData.PathIndex = nearestLoc.PortalIndex;

					// we are in the last corridor sector, so we move toward the target pos
					if (nearestLoc.PortalIndex == pathData.Data.Num() - 2)
					{
						dist = FVector3f(pathData.Data.Last().Location) - currentPos;
						const auto distLen = dist.Size();
						dir = dist / distLen;
						distToEnd = distLen;
					}
					// we are in one of middle corridor sector, so we can move by flow
					else
					{
						const auto backPortalPos = FMath::Lerp(pathData.Data[nearestLoc.PortalIndex].Left, pathData.Data[nearestLoc.PortalIndex].Right, nearestLoc.HoriT);
						const auto frontPortalPos = FMath::Lerp(pathData.Data[nearestLoc.PortalIndex + 1].Left, pathData.Data[nearestLoc.PortalIndex + 1].Right, nearestLoc.HoriT);
						dist = FVector3f(frontPortalPos) - currentPos;
						//dist = moveData.MaxSpeed * FVector3f::OneVector;

						// we can compute direction by (frontPortalPos - currentPos), but to avoid inaccurate normal due to very samll distance, we find it with back and front portals.
						dir = FVector3f(frontPortalPos - backPortalPos).GetSafeNormal();
						//dir = FMath::Lerp(agent.Velocity, FVector3f(frontPortalPos - backPortalPos), 0.001f).GetSafeNormal();
					}
				}
				// we are pushed out of the flow path, try to move to the next waypoint
				else if (!isInsideCorridor)
				{
					const auto portalCenter = FMath::Lerp(pathData.Data[nearestLoc.PortalIndex + 1].Left, pathData.Data[nearestLoc.PortalIndex + 1].Right, 0.5f);
					dist = FVector3f(portalCenter) - currentPos;
					const auto distLen = dist.Size();
					dir = dist / distLen;
					distToEnd = distLen + pathData.Data[nearestLoc.PortalIndex + 1].Distance;
				}
			}

			// acceleration/deceleration
			if (distToEnd <= decDist)
			{
				acc = FMath::Min((moveData.Speed * moveData.Speed) / (2 * distToEnd), moveData.Speed);
				acc = -acc;
			}
			moveData.Speed = FMath::Min(FMath::Max(moveData.Speed + (acc * DeltaMul), 0.0f), moveData.MaxSpeed);

			// calculate max velocity
			const auto maxVel = dir * moveData.Speed;

			// cap the max velocity with the current distance
			agent.PreferredVelocity.X = FMath::Abs(maxVel.X) > FMath::Abs(dist.X) ? dist.X : maxVel.X;
			agent.PreferredVelocity.Y = FMath::Abs(maxVel.Y) > FMath::Abs(dist.Y) ? dist.Y : maxVel.Y;
			agent.PreferredVelocity.Z = FMath::Abs(maxVel.Z) > FMath::Abs(dist.Z) ? dist.Z : maxVel.Z;
		});

	// checking update output
	static TArray<FAntHandle> canceledList;
	static TArray<FAntHandle> reachedList;
	static TArray<FAntHandle> velTimeoutList;

	canceledList.Reset(0);
	reachedList.Reset(0);
	velTimeoutList.Reset(0);

	for (int32 idx = 0; idx < Movements.GetMaxIndex(); ++idx)
	{
		if (!Movements.IsValidIndex(idx))
			continue;

		const auto updateResult = Movements[idx].UpdateResult;
		const auto owner = Movements[idx].Handle;

		// agent get canceled
		if (updateResult == EAntMoveResult::Canceled)
		{
			// remove data and its index
			RemoveAgentMovement(owner, false);

			// add to the deffered notify list
			if (updateResult == EAntMoveResult::Canceled)
				canceledList.Add(owner);

			continue;
		}

		// agent missing velocity timeout or reached its goal location
		if (updateResult == EAntMoveResult::VelocityTimedout || updateResult == EAntMoveResult::GoalReached)
		{
			// put it back again to the updating state
			Movements[idx].UpdateResult = EAntMoveResult::NeedUpdate;

			// add to the deffered notify list
			if (updateResult == EAntMoveResult::VelocityTimedout)
				velTimeoutList.Add(owner);

			// add to the deffered notify list
			if (updateResult == EAntMoveResult::GoalReached)
				reachedList.Add(owner);
		}
	}

	// notify
	if (!reachedList.IsEmpty())
	{
		OnMovementGoalReached.Broadcast(reachedList);
		OnMovementGoalReached_BP.Broadcast(reachedList);
	}

	// notify
	if (!velTimeoutList.IsEmpty())
	{
		OnMovementMissingVelocity.Broadcast(velTimeoutList);
		OnMovementMissingVelocity_BP.Broadcast(velTimeoutList);
	}

	// notify
	if (!canceledList.IsEmpty())
	{
		OnMovementCanceled.Broadcast(canceledList);
		OnMovementCanceled_BP.Broadcast(canceledList);
	}
}

void UAntSubsystem::DispatchAsyncQueries(float Delta)
{
	// proceed query list
	static TArray <FAntHandle> proceedQueries;
	static TArray <FAntHandle> tempQueries;
	proceedQueries.Reset(0);
	tempQueries.Reset(0);

	// collect proceed queries
	for (const auto &it : Queries)
	{
		if (it.ElapsedTime == 0)
			proceedQueries.Add(it.Handle);

		if (it.Interval < 0)
			tempQueries.Add(it.Handle);
	}

	// notify proceed queries
	if (!proceedQueries.IsEmpty())
	{
		OnQueryFinished.Broadcast(proceedQueries);
		OnQueryFinished_BP.Broadcast(proceedQueries);
	}

	// remove temporary queries
	for (const auto &it : tempQueries)
		if (IsValidAsyncQuery(it))
			RemoveAsyncQuery(it);

	// update attached queries
	for (auto &it : Queries)
		if (it.Type == EAntQueryType::CylinderAttached && IsValidAgent(it.Data.CylinderAttached.Handle))
			it.Data.CylinderAttached.Base = GetAgentData(it.Data.CylinderAttached.Handle).Location;
}

void UAntSubsystem::DebugDraw()
{
	if (Ant_DebugDraw == 0)
		return;

	// draw agents
	for (const auto &agent : Agents)
	{
		// simple light weight debug draw using points
		const FVector center(agent.LocationLerped.X, agent.LocationLerped.Y, agent.LocationLerped.Z + Settings->DebugDrawHeight + 25);
		if (Ant_DebugDraw == 1 || Ant_DebugDraw == 2 || Ant_DebugDraw == 3)
		{
			DrawDebugPoint(GetWorld(), center, 7, agent.bSleep ? FColor::Blue : FColor::Red);
			continue;
		}

		// debug draw location, current and final face angle 
		const FVector faceNorm(FMath::Cos(agent.FaceAngle), FMath::Sin(agent.FaceAngle), 0);
		const FVector finalFaceNorm(FMath::Cos(agent.FinalFaceAngle), FMath::Sin(agent.FinalFaceAngle), 0);

		DrawDebugCircle(GetWorld(), center, agent.Radius, 20, agent.bSleep ? FColor::Blue : FColor::Red, false, -1, 0, 10, FVector::XAxisVector, FVector::YAxisVector, false);
		DrawDebugLine(GetWorld(), center, FVector(center.X, center.Y, center.Z + agent.Height), agent.bSleep ? FColor::Blue : FColor::Red, false, -1, 0, 20);
		DrawDebugLine(GetWorld(), center, center + faceNorm * agent.Radius, FColor(100, 100, 100, 100), false, -1, 0, 5);
		DrawDebugLine(GetWorld(), center, center + finalFaceNorm * agent.Radius, FColor(255, 255, 255, 50), false, -1, 0, 5);
	}

	// draw queries
	if (Ant_DebugDraw >= 2)
		for (const auto &qry : Queries)
		{
			if (qry.Type == EAntQueryType::Cylinder)
			{
				DrawDebugCircle(GetWorld(), { qry.Data.Cylinder.Base.X, qry.Data.Cylinder.Base.Y, qry.Data.Cylinder.Base.Z + Settings->DebugDrawHeight + 25 }, qry.Data.Cylinder.Radius, 30, FColor::Orange,
					false, -1, 0, 15, FVector::XAxisVector, FVector::YAxisVector, false);

				DrawDebugCircle(GetWorld(), { qry.Data.Cylinder.Base.X, qry.Data.Cylinder.Base.Y, qry.Data.Cylinder.Base.Z + qry.Data.Cylinder.Height + Settings->DebugDrawHeight + 25 },
					qry.Data.Cylinder.Radius, 30, FColor::Orange, false, -1, 0, 15, FVector::XAxisVector, FVector::YAxisVector, false);
			}

			if (qry.Type == EAntQueryType::CylinderAttached)
			{
				DrawDebugCircle(GetWorld(), { qry.Data.CylinderAttached.Base.X, qry.Data.CylinderAttached.Base.Y, Settings->DebugDrawHeight + 25 }, qry.Data.CylinderAttached.Radius, 30, FColor::Orange,
					false, -1, 0, 15, FVector::XAxisVector, FVector::YAxisVector, false);

				DrawDebugCircle(GetWorld(), { qry.Data.CylinderAttached.Base.X, qry.Data.CylinderAttached.Base.Y, qry.Data.CylinderAttached.Base.Z + qry.Data.CylinderAttached.Height + Settings->DebugDrawHeight + 25 },
					qry.Data.CylinderAttached.Radius, 30, FColor::Orange, false, -1, 0, 15, FVector::XAxisVector, FVector::YAxisVector, false);
			}

			if (qry.Type == EAntQueryType::Ray)
				DrawDebugLine(GetWorld(), { qry.Data.Ray.Start.X, qry.Data.Ray.Start.Y, Settings->DebugDrawHeight + 25 }, { qry.Data.Ray.End.X, qry.Data.Ray.End.Y, Settings->DebugDrawHeight + 25 },
					FColor::Orange, false, -1, 0, 30);
		}

	// draw in-progress movements and path
	if (Ant_DebugDraw == 2 || Ant_DebugDraw == 3 || Ant_DebugDraw == 5 || Ant_DebugDraw == 6)
	{
		// draw paths
		for (const auto &it : Paths)
			for (int32 idx = 0; idx < it.Data.Num(); ++idx)
			{
				DrawDebugPoint(GetWorld(), it.Data[idx].Location + FVector(0, 0, Settings->DebugDrawHeight + 15), 7, FColor::Yellow, false, -1, 0);
				if (idx < it.Data.Num() - 1)
				{
					DrawDebugLine(GetWorld(), it.Data[idx].Location + FVector(0, 0, Settings->DebugDrawHeight + 15),
						it.Data[idx + 1].Location + FVector(0, 0, Settings->DebugDrawHeight + 15), FColor::Cyan, false, -1, 0, 5);

					// draw whole corridor
					if (Ant_DebugDraw == 3 || Ant_DebugDraw == 6)
					{
						auto color = it.Data[idx].Type == FAntPathData::Portal::Link ? FColor::Black : FColor::Green;
						DrawDebugLine(GetWorld(), it.Data[idx].Left + FVector(0, 0, Settings->DebugDrawHeight + 15),
							it.Data[idx].Right + FVector(0, 0, Settings->DebugDrawHeight + 15), color, false, -1, 0, 5);

						color = it.Data[idx + 1].Type == FAntPathData::Portal::Link ? FColor::Black : FColor::Green;
						DrawDebugLine(GetWorld(), it.Data[idx].Left + FVector(0, 0, Settings->DebugDrawHeight + 15),
							it.Data[idx + 1].Left + FVector(0, 0, Settings->DebugDrawHeight + 15), color, false, -1, 0, 5);

						DrawDebugLine(GetWorld(), it.Data[idx].Right + FVector(0, 0, Settings->DebugDrawHeight + 15),
							it.Data[idx + 1].Right + FVector(0, 0, Settings->DebugDrawHeight + 15), color, false, -1, 0, 5);

						DrawDebugLine(GetWorld(), it.Data[idx + 1].Left + FVector(0, 0, Settings->DebugDrawHeight + 15),
							it.Data[idx + 1].Right + FVector(0, 0, Settings->DebugDrawHeight + 15), color, false, -1, 0, 5);
					}
				}
			}

		// movements
		for (const auto &movement : Movements)
		{
			if (IsValidAgent(movement.Followee) || (IsValidPath(movement.Path) && !GetPathData(movement.Path).Data.IsEmpty()))
			{
				const auto &agentData = GetAgentData(movement.Handle);
				const FVector agentPos(agentData.LocationLerped.X, agentData.LocationLerped.Y, agentData.LocationLerped.Z + Settings->DebugDrawHeight + 20);
				if (movement.PathFollowerType == EAntPathFollowerType::Waypoint)
				{
					const FVector nearestPath = IsValidAgent(movement.Followee) ? FVector(GetAgentData(movement.Followee).LocationLerped)
						: GetPathData(movement.Path).Data[movement.PathIndex].Location;
					DrawDebugLine(GetWorld(), agentPos, nearestPath + FVector(0, 0, Settings->DebugDrawHeight + 20), FColor::Purple, false, -1, 0, 7);
				}

				//if (movement.PathFollowerType == EAntPathFollowerType::FlowField && IsValidPath(movement.Path) && !GetPathData(movement.Path).Data.Portals.IsEmpty())
					//DrawDebugLine(GetWorld(), GetPathData(movement.Path).Data.Portals[movement.PathIndex].Left + FVector(0, 0, Settings->DebugDrawHeight + 20),
						//GetPathData(movement.Path).Data.Portals[movement.PathIndex].Right + FVector(0, 0, Settings->DebugDrawHeight + 20), FColor::Green, false, -1, 0, 25);
			}
		}
	}

	// debug draw landscape size
	DrawDebugSolidBox(GetWorld(), FVector::Zero(), FVector(ShiftSize, ShiftSize, Settings->DebugDrawHeight), FColor(0, 0, 255, 25), false);

	// debug draw collision grid (broadphase grid)
	for (int32 i = 0; i <= Settings->NumCells; ++i)
	{
		DrawDebugLine(GetWorld(), FVector(-ShiftSize, i * Settings->CollisionCellSize - ShiftSize, Settings->DebugDrawHeight + 10),
			FVector(Settings->CollisionCellSize * Settings->NumCells - ShiftSize, i * Settings->CollisionCellSize - ShiftSize, Settings->DebugDrawHeight + 10), FColor(0, 255, 0, 25), false, -1, 0, 2);

		DrawDebugLine(GetWorld(), FVector(i * Settings->CollisionCellSize - ShiftSize, -ShiftSize, Settings->DebugDrawHeight + 10),
			FVector(i * Settings->CollisionCellSize - ShiftSize, Settings->CollisionCellSize * Settings->NumCells - ShiftSize, Settings->DebugDrawHeight + 10), FColor(255, 0, 0, 25), false, -1, 0, 2);
	}
}
