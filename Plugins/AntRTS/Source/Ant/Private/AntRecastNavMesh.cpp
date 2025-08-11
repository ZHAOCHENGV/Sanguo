// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "AntRecastNavMesh.h"
#include "NavigationSystem.h"
#include "NavigationDataHandler.h"
#include "AI/Navigation/NavigationElement.h"
#include "NavMesh/PImplRecastNavMesh.h"
#include "NavMesh/RecastNavMeshGenerator.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"

// a little trick to get access to protected members.
class UNavSysAccessor : public UNavigationSystemV1
{
	friend class AAntRecastNavMesh;
};

void UAntNavRelevant::GetNavigationData(FNavigationRelevantData &Data) const
{
	FAreaNavModifier modifier(Radius, Height, FTransform(FRotator::ZeroRotator, FVector(Location), FVector::OneVector), AreaClass);
	//modifier.SetApplyMode(ENavigationAreaMode::Type::Replace);
	//Data.Modifiers.SetMaskFillCollisionUnderneathForNavmesh(true);
	Data.Modifiers.Add(modifier);
	Data.Modifiers.SetNavMeshResolution(ENavigationDataResolution::Default);
}

FBox UAntNavRelevant::GetNavigationBounds() const
{
	FAreaNavModifier modifier(Radius, Height, FTransform(FRotator::ZeroRotator, FVector(Location), FVector::OneVector), AreaClass);
	return modifier.GetBounds();
}

void AAntRecastNavMesh::OnNavMeshTilesUpdated(const TArray<FNavTileRef> &ChangedTiles)
{
	Super::OnNavMeshTilesUpdated(ChangedTiles);

	if (GetWorld()->WorldType != EWorldType::PIE && GetWorld()->WorldType != EWorldType::Game)
		return;

	// allocate grid
	const auto totalTiles = GetRecastNavMeshImpl()->GetNavMeshTilesCount();
	if (TileGrid.Num() < totalTiles)
	{
		TileGrid.Reserve(totalTiles);
		for (int32 idx = TileGrid.Num(); idx < totalTiles; ++idx)
			TileGrid.Add(INDEX_NONE);
	}

	// decode tile ids from tile refs and store them for later process
	for (const auto it : ChangedTiles)
		UpdatedTiles.AddUnique(GetRecastNavMeshImpl()->GetTileIndexFromPolyRef((NavNodeRef)it));

	// update path replan list
	UpdateAntPaths(UpdatedTiles);

	// reset list
	UpdatedTiles.Reset();
}

void AAntRecastNavMesh::AddAgentNavModifier(FAntHandle Agent, TSubclassOf<UNavAreaBase> AreaClass)
{
	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	auto *navSys = FNavigationSystem::GetCurrent<UNavSysAccessor>(GetWorld());
	auto &agent = ant->GetMutableAgentData(Agent);

	// add a new modifier
	if (agent.NavModifierIdx == INDEX_NONE)
	{
		const auto idx = NavModifiers.Add(NewObject<UAntNavRelevant>());
		NavModifiers[idx]->AreaClass = AreaClass;
		NavModifiers[idx]->AntAgent = Agent;
		NavModifiers[idx]->Radius = agent.Radius;
		NavModifiers[idx]->Height = agent.Height;
		NavModifiers[idx]->Location = agent.Location;
		agent.NavModifierIdx = idx;

		// add to navigation octree
		auto navElement = FNavigationElement::CreateFromNavRelevantInterface(*NavModifiers[idx]);
		const auto result = navSys->RegisterNavigationElementWithNavOctree(navElement, FNavigationOctreeController::EOctreeUpdateMode::OctreeUpdate_Modifiers);
		NavModifiers[idx]->ElementHandle = navElement->GetHandle();
		//const auto result = navSys->RegisterNavOctreeElement(NavModifiers[idx], NavModifiers[idx], FNavigationOctreeController::EOctreeUpdateMode::OctreeUpdate_Modifiers);

		// cleanup if registration failed
		if (!result.IsValidId())
		{
			NavModifiers[idx]->ConditionalBeginDestroy();
			NavModifiers.Pop();
			agent.NavModifierIdx = INDEX_NONE;
		}
	}

	// update existing modifier
	else
	{
		const auto idx = agent.NavModifierIdx;
		NavModifiers[idx]->AreaClass = AreaClass;
		NavModifiers[idx]->AntAgent = Agent;
		NavModifiers[idx]->Radius = agent.Radius;
		NavModifiers[idx]->Height = agent.Height;
		NavModifiers[idx]->Location = agent.Location;

		// update navigation octree
		auto navElement = FNavigationElement::CreateFromNavRelevantInterface(*NavModifiers[idx]);
		navSys->UpdateNavOctreeElement(NavModifiers[idx]->ElementHandle, navElement, FNavigationOctreeController::EOctreeUpdateMode::OctreeUpdate_Modifiers);
		//navSys->UpdateNavOctreeElement(NavModifiers[idx], NavModifiers[idx], FNavigationOctreeController::EOctreeUpdateMode::OctreeUpdate_Modifiers);
	}
}

void AAntRecastNavMesh::RemoveAgentNavModifier(FAntHandle AgentHandle)
{
	OnAgentRemoved(AgentHandle);
}

void AAntRecastNavMesh::AddPathToReplanList(FAntHandle PathHandle, const FNavigationPath *Path)
{
	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	if (!ant->IsValidPath(PathHandle) || !Path)
		return;

	// check if the path is already in the plan list, we remove it first
	auto &antPathData = ant->GetMutablePathData(PathHandle);
	if (antPathData.TileNodeIdx != INDEX_NONE)
		OnPathRemoved(PathHandle);

	// register path in the list by its tiles
	const FNavMeshPath *path = Path ? Path->CastPath<FNavMeshPath>() : nullptr;
	int32 prevPathIdx = INDEX_NONE;
	for (auto it : path->PathCorridor)
	{
		const auto tileID = GetRecastNavMeshImpl()->GetTileIndexFromPolyRef(it);

		FTileData nodeData;
		nodeData.Path = PathHandle;
		nodeData.TileIDX = tileID;
		nodeData.NextPathNodeIdx = INDEX_NONE;
		nodeData.PrevIdx = INDEX_NONE;
		nodeData.NextIdx = TileGrid[tileID];

		// add to path list
		const auto nextPathIdx = TileData.Add(nodeData);

		// link prev node to this node
		if (prevPathIdx != INDEX_NONE && TileData[prevPathIdx].NextPathNodeIdx == INDEX_NONE)
			TileData[prevPathIdx].NextPathNodeIdx = nextPathIdx;

		// move first node to the second place
		if (TileGrid[tileID] != INDEX_NONE)
			TileData[TileGrid[tileID]].PrevIdx = nextPathIdx;

		// add this node at first place
		TileGrid[tileID] = nextPathIdx;

		// store index for next run of the loop
		prevPathIdx = nextPathIdx;

		// set first index to the path data
		if (antPathData.TileNodeIdx == INDEX_NONE)
			antPathData.TileNodeIdx = prevPathIdx;
	}
}

void AAntRecastNavMesh::UpdateAntPaths(const TArray<uint32> &ChangedTiles)
{
	if (GetWorld()->WorldType != EWorldType::PIE && GetWorld()->WorldType != EWorldType::Game)
		return;

	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	static TArray<FAntHandle> uniqList;
	uniqList.Reset();

	// collect path on changed tiles
	for (const auto it : ChangedTiles)
	{
		// coollect paths on this tile
		int32 firstIndex = TileGrid[it];
		while (firstIndex != INDEX_NONE)
		{
			uniqList.AddUnique(TileData[firstIndex].Path);
			firstIndex = TileData[firstIndex].NextIdx;
		}
	}

	// rebuild affected paths
	struct FData { FAntHandle Handle; FNavMeshPath Path; ENavigationQueryResult::Type Result = ENavigationQueryResult::Type::Invalid; };
	TArray<FData> results;
	results.SetNum(uniqList.Num());
	ParallelFor(uniqList.Num(), [&](int32 idx)
		{
			auto &pathData = ant->GetMutablePathData(uniqList[idx]);
			if (pathData.Data.IsEmpty())
				return;

			const auto startLoc = ant->IsValidAgent(pathData.GetOwner()) ? FVector(ant->GetAgentData(pathData.GetOwner()).GetLocation()) : pathData.Data[0].Location;

			results[idx].Handle = uniqList[idx];
			// prepare path
			results[idx].Path.SetNavigationDataUsed(this);
			results[idx].Path.SetTimeStamp(GetWorldTimeStamp());

			// query path
			results[idx].Result = AntFindPath(startLoc, pathData.Data.Last().Location, TNumericLimits<FVector::FReal>::Max(), true, results[idx].Path, *pathData.FilterClass);

			// path blocked
			pathData.Status = results[idx].Result == ENavigationQueryResult::Type::Success ? EAntPathStatus::Ready : EAntPathStatus::Blocked;

			// increase path version
			++pathData.Ver;

			// replan cost threshold
			if (results[idx].Result == ENavigationQueryResult::Type::Success && pathData.ReplanCostThreshold >= 0.0f && pathData.ReplanCostThreshold < results[idx].Path.GetLength())
			{
				pathData.Status = EAntPathStatus::HighCost;
				results[idx].Result = ENavigationQueryResult::Type::Fail;
			}

			if (results[idx].Result != ENavigationQueryResult::Type::Success)
				return;

			// build corridor
			FNavCorridor corridor;
			FNavCorridorParams params;
			params.SetFromWidth(pathData.Width);
			corridor.BuildFromPath(results[idx].Path, pathData.FilterClass, params);

			// convert corridor data to ant path data
			pathData.Data.SetNum(corridor.Portals.Num());
			uint8 linkCounter = 0;
			for (int32 pidx = 0; pidx < corridor.Portals.Num(); ++pidx)
			{
				memcpy(&pathData.Data[pidx], &corridor.Portals[pidx], sizeof(FNavCorridorPortal));

				// because of buggy UE implenetation, we can't rely on the navigation API (IsPathSegmentANavLink) to find out which point is a NavLink, 
				// so we have to check portal left-right to make sure this is a nav link.
				const auto isLink = results[idx].Path.IsPathSegmentANavLink(pidx) || pathData.Data[pidx].Left == pathData.Data[pidx].Right;
				if (linkCounter == 0 && isLink)
					linkCounter = 4;

				if (linkCounter > 0 || isLink)
				{
					pathData.Data[pidx].Type = FAntPathData::Portal::Link;
					pathData.Data[pidx].Left = pathData.Data[pidx].Right = pathData.Data[pidx].Location;
					--linkCounter;
				}

				// fix incorrect portals location
				if (!isLink && (pathData.Data[pidx].Location == pathData.Data[pidx].Left || pathData.Data[pidx].Location == pathData.Data[pidx].Right))
					pathData.Data[pidx].Location = FMath::Lerp(pathData.Data[pidx].Left, pathData.Data[pidx].Right, 0.5f);
			}

			// re-build distance list
			pathData.RebuildDistanceList();
		});

	// re-add path to the replan list
	for (const auto &it : results)
		if (it.Result == ENavigationQueryResult::Type::Success)
			AddPathToReplanList(it.Handle, &it.Path);
}

void AAntRecastNavMesh::OnAgentRemoved(FAntHandle Agent)
{
	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();

	// skip non-registered agents
	auto &agentData = ant->GetMutableAgentData(Agent);
	if (agentData.NavModifierIdx == INDEX_NONE)
		return;

	// remove from navigation octree
	auto *navSys = FNavigationSystem::GetCurrent<UNavSysAccessor>(GetWorld());
	auto navElement = FNavigationElement::CreateFromNavRelevantInterface(*NavModifiers[agentData.NavModifierIdx]);
	navSys->UnregisterNavigationElementWithOctree(navElement, FNavigationOctreeController::EOctreeUpdateMode::OctreeUpdate_Modifiers);
	//navSys->UnregisterNavOctreeElement(NavModifiers[agentData.NavModifierIdx], NavModifiers[agentData.NavModifierIdx], FNavigationOctreeController::EOctreeUpdateMode::OctreeUpdate_Modifiers);

	// remove from modifiers list
	ant->GetMutableAgentData(NavModifiers.Last()->AntAgent).NavModifierIdx = agentData.NavModifierIdx;
	NavModifiers[agentData.NavModifierIdx]->ConditionalBeginDestroy();
	NavModifiers[agentData.NavModifierIdx] = NavModifiers.Last();
	NavModifiers.Pop();
	agentData.NavModifierIdx = INDEX_NONE;
}

void AAntRecastNavMesh::OnPathRemoved(FAntHandle Path)
{
	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	// skip non-replaned path
	auto &pathData = ant->GetMutablePathData(Path);
	if (pathData.TileNodeIdx == INDEX_NONE)
		return;

	auto firstIdx = pathData.TileNodeIdx;
	while (firstIdx != INDEX_NONE)
	{
		// unlink from prev node
		if (TileData[firstIdx].PrevIdx != INDEX_NONE)
			TileData[TileData[firstIdx].PrevIdx].NextIdx = TileData[firstIdx].NextIdx;

		// unlink from next node
		if (TileData[firstIdx].NextIdx != INDEX_NONE)
			TileData[TileData[firstIdx].NextIdx].PrevIdx = TileData[firstIdx].PrevIdx;

		// reset tile if it was first node
		if (TileData[firstIdx].PrevIdx == INDEX_NONE)
			TileGrid[TileData[firstIdx].TileIDX] = TileData[firstIdx].NextIdx;

		const auto tempIdx = firstIdx;
		firstIdx = TileData[firstIdx].NextPathNodeIdx;
		// remove from data list
		TileData.RemoveAt(tempIdx);
	}

	pathData.TileNodeIdx = INDEX_NONE;
}

void AAntRecastNavMesh::OnAntPxPostUpdate()
{
	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	ParallelFor(ant->GetMutableUnderlyingAgentsList().GetMaxIndex(), [&](int32 idx)
		{
			auto &Agents = ant->GetMutableUnderlyingAgentsList();

			if (!Agents.IsValidIndex(idx))
				return;

			auto &agentData = Agents[idx];
			auto oldLoc = agentData.Location;

			if (agentData.bDisabled)
				return;

			if (agentData.Velocity != FVector3f::ZeroVector || agentData.NodeRef == INVALID_NAVNODEREF)
			{
				FNavLocation newLoc(FVector(agentData.Location + agentData.Velocity));

				// agent is using navigation
				if (agentData.bUseNavigation && !agentData.bIsOnNavLink)
				{
					// we need to get a valid navigation node ref at the first time
					FNavLocation currentLoc(FVector(agentData.Location), agentData.NodeRef);
					if (agentData.NodeRef == INVALID_NAVNODEREF
						&& GetRecastNavMeshImpl()->ProjectPointToNavMesh(FVector(agentData.Location), currentLoc, ant->Settings->NavLocationExtent,
							agentData.QueryFilterClass ? *agentData.QueryFilterClass : *GetDefaultQueryFilter(), nullptr))
						{
							agentData.NodeRef = currentLoc.NodeRef;
							agentData.Location = oldLoc = FVector3f(currentLoc.Location);
						}


					// find new location
					if (!GetRecastNavMeshImpl()->FindMoveAlongSurface(currentLoc, FVector(agentData.Location + agentData.Velocity), newLoc,
						agentData.QueryFilterClass ? *agentData.QueryFilterClass : *GetDefaultQueryFilter(), nullptr))
					{
						newLoc.NodeRef = INVALID_NAVNODEREF;
						newLoc.Location = FVector(agentData.Location + agentData.Velocity);

						// apply gravity
						//newLoc.Location.Z -= ant->Settings->Gravity;
					}
				}

				agentData.NodeRef = newLoc.NodeRef;
				agentData.Location = FVector3f(newLoc.Location);
				agentData.Velocity = agentData.Location - oldLoc;
			}

			agentData.bUpdateGrid = oldLoc != agentData.Location;
			agentData.bSleep = agentData.OverlapForce == FVector2f::ZeroVector && !agentData.bUpdateGrid;
			agentData.LocationLerped = ant->Settings->EnableLerp ? FMath::Lerp(agentData.LocationLerped, agentData.Location, agentData.LerpAlpha) : agentData.Location;
		});
}

bool AAntRecastNavMesh::GetHeightAt(const FVector &NavLocation, const FNavigationQueryFilter &QueryFilter, const FVector &Extent, FVector &Result) const
{
	FNavLocation result;
	const auto ret = GetRecastNavMeshImpl()->ProjectPointToNavMesh(NavLocation, result, Extent, QueryFilter, nullptr);
	Result = result.Location;
	return ret;
}

ENavigationQueryResult::Type AAntRecastNavMesh::AntFindPath(const FVector &StartLoc, const FVector &EndLoc, const FVector::FReal CostLimit, bool bRequireNavigableEndLocation,
	FNavMeshPath &Path, const FNavigationQueryFilter &InQueryFilter) const
{
	const auto adjustedEndLocation = InQueryFilter.GetAdjustedEndLocation(EndLoc);
	return GetRecastNavMeshImpl()->FindPath(StartLoc, adjustedEndLocation, CostLimit, bRequireNavigableEndLocation, Path, InQueryFilter, nullptr);
}

void AAntRecastNavMesh::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	TileData.Reset();
	TileGrid.Reset();
	NavModifiers.Reset();
}

void AAntRecastNavMesh::BeginPlay()
{
	Super::BeginPlay();

	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	// allocate grid
	const auto totalTiles = GetRecastNavMeshImpl()->GetNavMeshTilesCount();
	if (TileGrid.Num() < totalTiles)
	{
		TileGrid.Reserve(totalTiles);
		for (int32 idx = TileGrid.Num(); idx < totalTiles; ++idx)
			TileGrid.Add(INDEX_NONE);
	}

	// bind to get notify about paths
	if (!ant->OnPathRemoved.IsBoundToObject(this))
		ant->OnPathRemoved.AddUObject(this, &AAntRecastNavMesh::OnPathRemoved);

	// bind to get notify about navigation update
	if (!ant->OnUpdateNav.IsBoundToObject(this))
		ant->OnUpdateNav.BindUObject(this, &AAntRecastNavMesh::OnAntPxPostUpdate);
}