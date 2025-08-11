// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "AntFunctionLibrary.h"
#include "NavigationSystem.h"
#include "Components/InstancedStaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AntFunctionLibrary)

UAntSubsystem *UAntFunctionLibrary::GetAntSubsystem(const UObject *WorldContextObject)
{
	auto *world = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return world ? world->GetSubsystem<UAntSubsystem>() : nullptr;
}

void UAntFunctionLibrary::SortAgentsBySquare(const UObject *WorldContextObject, const FVector &DestLocation, const TArray<FAntHandle> &Agents, float SplitSpace, float &SquareDimension,
	TArray<FAntHandle> &SortedAgents, TArray<FVector> &ResultLocations)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgents(Agents))
	{
		SortedAgents = Agents;
		UAntUtil::SortAgentsBySquare(WorldContextObject->GetWorld(), DestLocation, SortedAgents, SplitSpace, SquareDimension, ResultLocations);
	}
}

FAntHandle UAntFunctionLibrary::AddAgentAdvanced(const UObject *WorldContextObject, const FVector &NavLocation, float Radius, float ExtraQueryRadius, float Height,
	float FaceAngle, float TurnRate, float MoveAngleThreshold,
	bool TurnByPreferred, bool CanPierce, bool UseNavigation, float MaxOverlapForce, int32 Flags, int32 IgnoreFlag, EAntAvoidanceTypes CollisionAvoidance, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *navSys = UNavigationSystemV1::GetCurrent(WorldContextObject->GetWorld());
	if (!navSys || !navSys->GetDefaultNavDataInstance())
		return FAntHandle();

	auto *navData = navSys->GetDefaultNavDataInstance();

	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
	{
		const auto handle = ant->AddAgent(NavLocation, Radius, Height, FaceAngle, Flags);
		auto &agentData = ant->GetMutableAgentData(handle);
		agentData.bTurnByPreferred = TurnByPreferred;
		agentData.bCanPierce = CanPierce;
		agentData.MaxOverlapForce = MaxOverlapForce;
		agentData.IgnoreFlag = IgnoreFlag;
		agentData.TurnRate = TurnRate;
		agentData.MoveAngleThreshold = MoveAngleThreshold;
		agentData.bUseNavigation = UseNavigation;
		agentData.AvoidanceType = CollisionAvoidance;
		agentData.ExtraQueryRadius = ExtraQueryRadius;
		agentData.QueryFilterClass = UNavigationQueryFilter::GetQueryFilter(*navData, nullptr, FilterClass);
		return handle;
	}

	return FAntHandle();
}

FAntHandle UAntFunctionLibrary::AddAgent(const UObject *WorldContextObject, const FVector &Location, float Radius, float Height, float FaceAngle, int32 Flags)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	return ant ? ant->AddAgent(Location, Radius, Height, FaceAngle, Flags) : FAntHandle();
}

void UAntFunctionLibrary::RemoveAgent(const UObject *WorldContextObject, FAntHandle Handle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(Handle))
		ant->RemoveAgent(Handle);
}

void UAntFunctionLibrary::RemoveAgents(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
		for (const auto &it : AgentsHandle)
			if (ant->IsValidAgent(it))
				ant->RemoveAgent(it);
}

void UAntFunctionLibrary::SetAgentLocation(const UObject *WorldContextObject, FAntHandle Handle, const FVector &Location)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(Handle))
		ant->SetAgentLocation(Handle, Location);
}

void UAntFunctionLibrary::GetAgentLocation(const UObject *WorldContextObject, FAntHandle AgentHandle, FVector3f &Location)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		Location = ant->GetAgentData(AgentHandle).GetLocationLerped();
}

void UAntFunctionLibrary::GetAgentRotation(const UObject * WorldContextObject, FAntHandle AgentHandle, float & FaceAngle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
    if (ant && ant->IsValidAgent(AgentHandle))
         FaceAngle = ant->GetAgentData(AgentHandle).FaceAngle;
}

void UAntFunctionLibrary::SetAgentRotation(const UObject * WorldContextObject, FAntHandle AgentHandle, float FaceAngle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
    if (ant && ant->IsValidAgent(AgentHandle))
         ant->GetMutableAgentData(AgentHandle).FaceAngle = FaceAngle;
}

void UAntFunctionLibrary::GetAgentVelocity(const UObject * WorldContextObject, FAntHandle AgentHandle, FVector3f & Velocity)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
    if (ant && ant->IsValidAgent(AgentHandle))
		Velocity = ant->GetAgentData(AgentHandle).GetVelocity();
}

void UAntFunctionLibrary::SetAgentPreferredVelocity(const UObject *WorldContextObject, FAntHandle Handle, const FVector3f &Velocity)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(Handle))
		ant->GetMutableAgentData(Handle).PreferredVelocity = Velocity;
}

void UAntFunctionLibrary::SetAgentOverlapForce(const UObject *WorldContextObject, FAntHandle Handle, float OverlapForce)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(Handle))
		ant->GetMutableAgentData(Handle).MaxOverlapForce = OverlapForce;
}

void UAntFunctionLibrary::SetAgentFlag(const UObject *WorldContextObject, FAntHandle AgentHandle, int32 Flags)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		ant->SetAgentFlag(AgentHandle, Flags);
}

void UAntFunctionLibrary::SetAgentsFlag(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle, int32 Flags)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
		for (const auto &handle : AgentsHandle)
			if (ant->IsValidAgent(handle))
				ant->SetAgentFlag(handle, Flags);
}

void UAntFunctionLibrary::SetAgentIgnoreFlag(const UObject *WorldContextObject, FAntHandle AgentHandle, int32 Flags)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		ant->GetMutableAgentData(AgentHandle).IgnoreFlag = Flags;
}

void UAntFunctionLibrary::SetAgentsIgnoreFlag(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle, int32 Flags)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
		for (const auto &handle : AgentsHandle)
			if (ant->IsValidAgent(handle))
				ant->GetMutableAgentData(handle).IgnoreFlag = Flags;
}

void UAntFunctionLibrary::SetAgentIgnoreWakeupFlag(const UObject *WorldContextObject, FAntHandle AgentHandle, int32 Flags)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		ant->GetMutableAgentData(AgentHandle).IgnoreButWakeUpFlag = Flags;
}

void UAntFunctionLibrary::SetAgentsIgnoreWakeupFlag(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle, int32 Flags)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
		for (const auto &handle : AgentsHandle)
			if (ant->IsValidAgent(handle))
				ant->GetMutableAgentData(handle).IgnoreButWakeUpFlag = Flags;
}

void UAntFunctionLibrary::SetAgentCustomInstancedStruct(const UObject *WorldContextObject, FAntHandle AgentHandle, const FInstancedStruct &CustomStruct)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		ant->GetAgentUserData(AgentHandle) = CustomStruct;
}

void UAntFunctionLibrary::GetAgentCustomInstancedStruct(const UObject *WorldContextObject, FAntHandle AgentHandle, FInstancedStruct &OutStruct)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		OutStruct = ant->GetAgentUserData(AgentHandle);
}

void UAntFunctionLibrary::GetAgentCustomStruct(const UObject *WorldContextObject, FAntHandle AgentHandle, bool &ExecResult, int32 &OutStruct)
{
	checkNoEntry();
}

void UAntFunctionLibrary::SetAgentCustomStruct(const UObject *WorldContextObject, FAntHandle AgentHandle, const int32 &InStruct)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UAntFunctionLibrary::execGetAgentCustomStruct)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT_REF(FAntHandle, AgentHandle);
	P_GET_UBOOL_REF(ExecResult);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty *valueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	uint8 *valuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ExecResult = false;
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (!ant || !ant->IsValidAgent(AgentHandle))
		return;

	if (!valueProp || !valuePtr)
	{
		//FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AbortExecution, INVTEXT("Failed to resolve the Value")));
		return;
	}

	const auto *scriptStruct = ant->GetAgentUserData(AgentHandle).GetScriptStruct();
	const bool bCompatible = scriptStruct && scriptStruct->IsChildOf(valueProp->Struct);
	if (!bCompatible)
		return;

	{
		P_NATIVE_BEGIN;
		valueProp->Struct->CopyScriptStruct(valuePtr, ant->GetAgentUserData(AgentHandle).GetMemory());
		ExecResult = true;
		P_NATIVE_END;
	}
}

DEFINE_FUNCTION(UAntFunctionLibrary::execSetAgentCustomStruct)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT_REF(FAntHandle, AgentHandle);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const auto *valueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	uint8 *valuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!valueProp || !valuePtr)
	{
		//FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AbortExecution, INVTEXT("Failed to resolve the Value")));
		return;
	}

	auto *ant = GetAntSubsystem(WorldContextObject);
	if (!ant || !ant->IsValidAgent(AgentHandle))
		return;

	{
		P_NATIVE_BEGIN;
		ant->GetAgentUserData(AgentHandle).InitializeAs(valueProp->Struct, valuePtr);
		P_NATIVE_END;
	}
}

bool UAntFunctionLibrary::IsValidAgent(const UObject *WorldContextObject, FAntHandle Handle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	return (ant && ant->IsValidAgent(Handle));
}

bool UAntFunctionLibrary::IsValidAgents(const UObject *WorldContextObject, const TArray<FAntHandle> Handles)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	return (ant && ant->IsValidAgents(Handles));
}

bool UAntFunctionLibrary::FindAgentsByFlag(const UObject *WorldContextObject, int32 Flags, TArray<FAntHandle> &Result)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	const auto resultSize = Result.Num();
	if (ant)
		for (const auto &it : ant->GetUnderlyingAgentsList())
			if (CHECK_BIT_ANY(it.GetFlag(), Flags))
				Result.Add(it.GetHandle());

	return resultSize < Result.Num();
}

FAntHandle UAntFunctionLibrary::CreateSharedPath(const UObject *WorldContextObject, const FVector &Start, const FVector &End, float PathWidth, bool PathReplan, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
		return UAntUtil::CreateSharedPath(WorldContextObject->GetWorld(), Start, End, PathWidth, PathReplan, ReplanCostThreshold, FilterClass);

	return FAntHandle();
}

FAntHandle UAntFunctionLibrary::CreateSharedPathBySpline(const UObject *WorldContextObject, const USplineComponent *Spline, float PathWidth, bool PathReplan, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && Spline)
		return UAntUtil::CreateSharedPathBySpline(WorldContextObject->GetWorld(), Spline, PathWidth, PathReplan, ReplanCostThreshold, FilterClass);

	return FAntHandle();
}

void UAntFunctionLibrary::SetPathCustomInstancedStruct(const UObject *WorldContextObject, FAntHandle PathHandle, const FInstancedStruct &CustomStruct)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidPath(PathHandle))
		ant->GetPathUserData(PathHandle) = CustomStruct;
}

void UAntFunctionLibrary::GetPathCustomInstancedStruct(const UObject *WorldContextObject, FAntHandle PathHandle, FInstancedStruct &OutStruct)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidPath(PathHandle))
		OutStruct = ant->GetPathUserData(PathHandle);
}

void UAntFunctionLibrary::GetPathCustomStruct(const UObject *WorldContextObject, FAntHandle PathHandle, bool &ExecResult, int32 &OutStruct)
{
	checkNoEntry();
}

void UAntFunctionLibrary::SetPathCustomStruct(const UObject *WorldContextObject, FAntHandle PathHandle, const int32 &InStruct)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UAntFunctionLibrary::execGetPathCustomStruct)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT_REF(FAntHandle, PathHandle);
	P_GET_UBOOL_REF(ExecResult);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty *valueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	uint8 *valuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ExecResult = false;
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (!ant || !ant->IsValidPath(PathHandle))
		return;

	if (!valueProp || !valuePtr)
	{
		//FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AbortExecution, INVTEXT("Failed to resolve the Value")));
		return;
	}

	const auto *scriptStruct = ant->GetPathUserData(PathHandle).GetScriptStruct();
	const bool bCompatible = scriptStruct && scriptStruct->IsChildOf(valueProp->Struct);
	if (!bCompatible)
		return;

	{
		P_NATIVE_BEGIN;
		valueProp->Struct->CopyScriptStruct(valuePtr, ant->GetPathUserData(PathHandle).GetMemory());
		ExecResult = true;
		P_NATIVE_END;
	}
}

DEFINE_FUNCTION(UAntFunctionLibrary::execSetPathCustomStruct)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT_REF(FAntHandle, PathHandle);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const auto *valueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	uint8 *valuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!valueProp || !valuePtr)
	{
		//FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AbortExecution, INVTEXT("Failed to resolve the Value")));
		return;
	}

	auto *ant = GetAntSubsystem(WorldContextObject);
	if (!ant || !ant->IsValidPath(PathHandle))
		return;

	{
		P_NATIVE_BEGIN;
		ant->GetPathUserData(PathHandle).InitializeAs(valueProp->Struct, valuePtr);
		P_NATIVE_END;
	}
}

void UAntFunctionLibrary::QueryOnScreenAgents(APlayerController *PC, const FBox2f &ScreenArea, float FrustumDepth, ECollisionChannel FloorChannel, int32 Flags, TArray<FAntHandle> &ResultUnits)
{
	// Ant is a world subsystem
	if (PC && GetAntSubsystem(PC->GetWorld()))
		UAntUtil::QueryOnScreenAgents(PC, ScreenArea, FrustumDepth, FloorChannel, Flags, ResultUnits);
}

void UAntFunctionLibrary::QueryCylinder(const UObject *WorldContextObject, const FVector3f &Base, float Radius, float Height, int32 Flags, bool MustIncludeCenter, TArray<FAntHandle> &AgentsHandle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
	{
		TArray<FAntContactInfo> contacts;
		ant->QueryCylinder(Base, Radius, Height, Flags, MustIncludeCenter, contacts);
		for (const auto &contact : contacts)
			AgentsHandle.Add(contact.Handle);
	}
}

void UAntFunctionLibrary::QueryConvexVolume(const UObject* WorldContextObject,
	const TStaticArray<FVector, 4>& NearPlane, const TStaticArray<FVector, 4>& FarPlane, int32 Flags,
	TArray<FAntHandle>& AgentsHandle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
	{
		TArray<FAntContactInfo> contacts;
		ant->QueryConvexVolume(NearPlane, FarPlane, Flags, contacts);
		for (const auto &contact : contacts)
			AgentsHandle.Add(contact.Handle);
	}
}

FAntHandle UAntFunctionLibrary::QueryCylinderAttachedAsync(const UObject *WorldContextObject, FAntHandle AgentHandle, float Radius, float Height, int32 Flags, bool MustIncludeCenter, float Interval)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		return ant->QueryCylinderAttachedAsync(AgentHandle, Radius, Height, Flags, MustIncludeCenter, Interval);

	return FAntHandle();
}

void UAntFunctionLibrary::GetAsyncQueryCustomStruct(const UObject *WorldContextObject, FAntHandle QueryHandle, bool &ExecResult, int32 &OutStruct)
{
	checkNoEntry();
}

void UAntFunctionLibrary::SetAsyncQueryCustomStruct(const UObject *WorldContextObject, FAntHandle QueryHandle, const int32 &InStruct)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UAntFunctionLibrary::execGetAsyncQueryCustomStruct)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT_REF(FAntHandle, QueryHandle);
	P_GET_UBOOL_REF(ExecResult);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty *valueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	uint8 *valuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ExecResult = false;
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (!ant || !ant->IsValidAsyncQuery(QueryHandle))
		return;

	if (!valueProp || !valuePtr)
	{
		//FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AbortExecution, INVTEXT("Failed to resolve the Value")));
		return;
	}

	const auto *scriptStruct = ant->GetAsyncQueryUserData(QueryHandle).GetScriptStruct();
	const bool bCompatible = scriptStruct && scriptStruct->IsChildOf(valueProp->Struct);
	if (!bCompatible)
		return;

	{
		P_NATIVE_BEGIN;
		valueProp->Struct->CopyScriptStruct(valuePtr, ant->GetAsyncQueryUserData(QueryHandle).GetMemory());
		ExecResult = true;
		P_NATIVE_END;
	}
}

DEFINE_FUNCTION(UAntFunctionLibrary::execSetAsyncQueryCustomStruct)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT_REF(FAntHandle, QueryHandle);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const auto *valueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	uint8 *valuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!valueProp || !valuePtr)
	{
		//FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AbortExecution, INVTEXT("Failed to resolve the Value")));
		return;
	}

	auto *ant = GetAntSubsystem(WorldContextObject);
	if (!ant || !ant->IsValidAsyncQuery(QueryHandle))
		return;

	{
		P_NATIVE_BEGIN;
		ant->GetAsyncQueryUserData(QueryHandle).InitializeAs(valueProp->Struct, valuePtr);
		P_NATIVE_END;
	}
}

bool UAntFunctionLibrary::IsValidAsyncQuery(const UObject *WorldContextObject, FAntHandle Handle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	return (ant && ant->IsValidAsyncQuery(Handle));
}

void UAntFunctionLibrary::GetAsyncQueryResult(const UObject *WorldContextObject, FAntHandle QueryHandle, TArray<FAntHandle> &QueryResult)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAsyncQuery(QueryHandle))
		for (const auto &it : ant->GetAsyncQueryData(QueryHandle).Result)
			QueryResult.Add(it.Handle);
}

void UAntFunctionLibrary::QueryRay(const UObject *WorldContextObject, const FVector3f &Start, const FVector3f &End, int32 Flags, TArray<FAntHandle> &AgentsHandle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
	{
		TArray<FAntContactInfo> contacts;
		ant->QueryRay(Start, End, Flags, contacts);
		for (const auto &contact : contacts)
			AgentsHandle.Add(contact.Handle);
	}
}

void UAntFunctionLibrary::MoveAgentByPath(const UObject *WorldContextObject, FAntHandle AgentHandle, FAntHandle PathHandle, float MaxSpeed, float Acceleration, float Deceleration,
	float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float StartDelay, float MissingVelocityTimeout)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		ant->MoveAgentByPath(AgentHandle, PathHandle, PathFollowerType, MaxSpeed, Acceleration, Deceleration, TargetAcceptanceRadius, PathAcceptanceRadius, StartDelay, MissingVelocityTimeout);
}

void UAntFunctionLibrary::FollowAgent(const UObject *WorldContextObject, FAntHandle FollowerHandle, FAntHandle FolloweeHandle, float MaxSpeed, float Acceleration, float Deceleration,
	float FolloweeAcceptanceRadius, float StartDelay, float MissingVelocityTimeout)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(FollowerHandle) && ant->IsValidAgent(FolloweeHandle))
		ant->FollowAgent(FollowerHandle, FolloweeHandle, MaxSpeed, Acceleration, Deceleration, FolloweeAcceptanceRadius, StartDelay, MissingVelocityTimeout);
}

bool UAntFunctionLibrary::MoveAgentToLocation(const UObject *WorldContextObject, FAntHandle AgentHandle, const FVector &Destination, float MaxSpeed, float Acceleration, float Deceleration,
	float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float PathWidth,
	float MissingVelocityTimeout, bool PathReplan, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(AgentHandle))
		return UAntUtil::MoveAgentsToLocations(WorldContextObject->GetWorld(), { AgentHandle }, { Destination }, { MaxSpeed }, {Acceleration}, {Deceleration},
			TargetAcceptanceRadius, PathAcceptanceRadius, PathFollowerType, PathWidth,
			MissingVelocityTimeout, PathReplan, ReplanCostThreshold, FilterClass);

	return false;
}

bool UAntFunctionLibrary::MoveAgentsToLocationByFlag(const UObject *WorldContextObject, int32 Flags, const FVector &Destination, float MaxSpeed, float Acceleration, float Deceleration,
	float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float PathWidth, 
	float MissingVelocityTimeout, bool PathReplan, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
	{
		// collect agents with the proper flga
		TArray<FAntHandle> agents;
		agents.Reserve(ant->GetUnderlyingAgentsList().Num());
		for (const auto &it : ant->GetUnderlyingAgentsList())
			if (CHECK_BIT_ANY(it.GetFlag(), Flags))
				agents.Add(it.GetHandle());

		// prepare locations and speeds
		TArray<FVector> locations;
		TArray<float> speeds;
		TArray<float> acc;
		TArray<float> dec;
		locations.Init(Destination, agents.Num());
		speeds.Init(MaxSpeed, agents.Num());
		acc.Init(Acceleration, agents.Num());
		dec.Init(Deceleration, agents.Num());

		//
		return UAntUtil::MoveAgentsToLocations(WorldContextObject->GetWorld(), agents, locations, speeds, acc, dec, TargetAcceptanceRadius, PathAcceptanceRadius, PathFollowerType, PathWidth,
			MissingVelocityTimeout, PathReplan, ReplanCostThreshold, FilterClass);
	}
		
	return false;
}

bool UAntFunctionLibrary::MoveAgentsToLocations(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsToMove, const TArray<FVector> &Locations,
	const TArray<float> &MaxSpeeds, const TArray<float> &Accelerations, const TArray<float> &Decelerations,
	float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float PathWidth,
	float MissingVelocityTimeout, bool PathReplan, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
		return UAntUtil::MoveAgentsToLocations(WorldContextObject->GetWorld(), AgentsToMove, Locations, MaxSpeeds, Accelerations, Decelerations,
			TargetAcceptanceRadius, PathAcceptanceRadius, PathFollowerType, PathWidth,
			MissingVelocityTimeout, PathReplan, ReplanCostThreshold, FilterClass);

	return false;
}

FVector UAntFunctionLibrary::GetAgentMovementLocation(const UObject *WorldContextObject, FAntHandle AgentHandle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidMovement(AgentHandle) && ant->IsValidPath(ant->GetAgentMovement(AgentHandle).GetPath()))
		return ant->GetPathData(ant->GetAgentMovement(AgentHandle).GetPath()).GetData().Last().Location;

	return FVector();
}

float UAntFunctionLibrary::GetAgentMovementSpeed(const UObject *WorldContextObject, FAntHandle AgentHandle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidMovement(AgentHandle))
		return ant->GetAgentMovement(AgentHandle).MaxSpeed;

	return 0.0f;
}

void UAntFunctionLibrary::RemoveAgentsMovement(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant)
		for (const auto &handle : AgentsHandle)
			if (ant->IsValidAgent(handle) && ant->IsValidMovement(handle))
				ant->RemoveAgentMovement(handle);
}

bool UAntFunctionLibrary::IsValidMovement(const UObject * WorldContextObject, FAntHandle AgentHandle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	return (ant && ant->IsValidMovement(AgentHandle));
}

void UAntFunctionLibrary::UpdateAgentNavModifier(const UObject *WorldContextObject, FAntHandle Agent, TSubclassOf<UNavAreaBase> AreaClass)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(Agent))
		UAntUtil::UpdateAgentNavModifier(WorldContextObject->GetWorld(), Agent, AreaClass);
}

void UAntFunctionLibrary::RemoveAgentNavModifier(const UObject * WorldContextObject, FAntHandle Agent)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	if (ant && ant->IsValidAgent(Agent))
		UAntUtil::RemoveAgentNavModifier(WorldContextObject->GetWorld(), Agent);
}

void UAntFunctionLibrary::CopyAgentsTransformToISMC(UInstancedStaticMeshComponent *InstancedMeshComp, int32 Flags)
{
	auto *ant = GetAntSubsystem(InstancedMeshComp);
	if (!ant)
		return;

	InstancedMeshComp->ClearInstances();

	// iterate over agents
	for (const auto &agent : ant->GetUnderlyingAgentsList())
	{
		if (!CHECK_BIT_ANY(agent.GetFlag(), Flags))
			continue;

		const FVector center(agent.GetLocationLerped());
		InstancedMeshComp->AddInstance(FTransform(FRotator::ZeroRotator, center, FVector::One()));
	}
}

int UAntFunctionLibrary::GetAntGroupByIndex(int32 Index)
{
	return 1 << Index;
}

int UAntFunctionLibrary::GetAllAntGroupsExceptThese(int32 Flags)
{
	const uint32 result = -1 & ~Flags;
	return result;
}

EAntHandleTypes UAntFunctionLibrary::GetHandleType(const UObject *WorldContextObject, FAntHandle Handle)
{
	auto *ant = GetAntSubsystem(WorldContextObject);
	return ant ? ant->GetHandleType(Handle) : EAntHandleTypes::Invalid;
}

//FAntHeightData UAntFunctionLibrary::GetHeightAtLocation(const UObject *WorldContextObject, const FVector2f &Location, bool CalcNormal)
//{
//	auto *ant = GetAntSubsystem(WorldContextObject);
//	return FAntHeightData();
//}
