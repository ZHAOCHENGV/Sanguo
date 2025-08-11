// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "AntUtil.h"
#include "AntSubsystem.h"
#include "AntGrid.h"
#include "AntMath.h"
#include "AntRecastNavMesh.h"
#include "NavigationSystem.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"

struct FCirclePackNode
{
	// circle attributes 
	float x = 0;
	float y = 0;
	float radius = 0;
	// link chain
	FCirclePackNode *next = nullptr;
	FCirclePackNode *prev = nullptr;
	// insertion order 
	FCirclePackNode *insertNext = nullptr;

	static void Place(FCirclePackNode *a, FCirclePackNode *b, FCirclePackNode *c)
	{
		float da = b->radius + c->radius;
		float db = a->radius + c->radius;
		float dx = b->x - a->x;
		float dy = b->y - a->y;
		float dc = sqrt(dx * dx + dy * dy);
		if (dc > 0.0f)
		{
			float cos = (db * db + dc * dc - da * da) / (2 * db * dc);
			float theta = FMath::Acos(cos);
			float x = cos * db;
			float h = FMath::Sin(theta) * db;
			dx /= dc;
			dy /= dc;
			c->x = a->x + x * dx + h * dy;
			c->y = a->y + x * dy - h * dx;
		}
		else
		{
			c->x = a->x + db;
			c->y = a->y;
		}
	}

	static void Splice(FCirclePackNode *a, FCirclePackNode *b)
	{
		a->next = b;
		b->prev = a;
	}

	static void Insert(FCirclePackNode *a, FCirclePackNode *b)
	{
		auto *c = a->next;
		a->next = b;
		b->prev = a;
		b->next = c;
		if (c)
			c->prev = b;
	}
};

/** Initialize and build an empty path. (thread safe) */
static ENavigationQueryResult::Type BuildPath(UWorld *World, AAntRecastNavMesh *NavMesh, FNavMeshPath &Path, FAntHandle PathHandle, const FVector &Start, const FVector &End, float PathWidth, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = World->GetSubsystem<UAntSubsystem>();

	// prepare path
	Path.SetNavigationDataUsed(NavMesh);
	Path.SetTimeStamp(NavMesh->GetWorldTimeStamp());

	// query path
	const auto queryFilter = FilterClass ? UNavigationQueryFilter::GetQueryFilter(*NavMesh, nullptr, FilterClass) : NavMesh->GetDefaultQueryFilter();
	const auto result = NavMesh->AntFindPath(Start, End, TNumericLimits<FVector::FReal>::Max(), true, Path, *queryFilter);

	// check results
	if (result != ENavigationQueryResult::Type::Success)
		return result;

	/*TArray <FVector> curvedPath;
	ConvertLinearPathToCurvePath(results[idx].Path.GetPathPoints(), 50, curvedPath);*/
	/*results[idx].Corridor.Portals.Reset(curvedPath.Num());
	for (int32 pidx = 0; pidx < curvedPath.Num(); ++pidx)
		results[idx].Corridor.Portals.Add(FNavCorridorPortal{ curvedPath[pidx], curvedPath[pidx], curvedPath[pidx], pidx, false });*/

	// build corridor
	FNavCorridor corridor;
	FNavCorridorParams params;
	params.SetFromWidth(PathWidth);
	corridor.BuildFromPath(Path, queryFilter, params);

	// convert corridor data to ant path data
	auto &antPath = ant->GetMutablePathData(PathHandle);
	auto &pathData = antPath.GetMutableData();
	pathData.SetNum(corridor.Portals.Num());
	uint8 linkCounter = 0;
	for (int32 pidx = 0; pidx < corridor.Portals.Num(); ++pidx)
	{
		memcpy(&pathData[pidx], &corridor.Portals[pidx], sizeof(FNavCorridorPortal));

		// because of buggy UE implenetation, we can't rely on the navigation API (IsPathSegmentANavLink) to find out which point is a NavLink, 
		// so we have to check portal left-right to make sure this is a nav link.
		const auto isLink = Path.IsPathSegmentANavLink(pidx) || pathData[pidx].Left == pathData[pidx].Right;
		if (linkCounter == 0 && isLink)
			linkCounter = 4;

		if (linkCounter > 0 || isLink)
		{
			pathData[pidx].Type = FAntPathData::Portal::Link;
			pathData[pidx].Left = pathData[pidx].Right = pathData[pidx].Location;
			--linkCounter;
		}

		// fix incorrect portals location
		if (!isLink && (pathData[pidx].Location == pathData[pidx].Left || pathData[pidx].Location == pathData[pidx].Right))
			pathData[pidx].Location = FMath::Lerp(pathData[pidx].Left, pathData[pidx].Right, 0.5f);
	}

	//pathData.NavLinkWidth = NavLinkWidth;
	antPath.ReplanCostThreshold = ReplanCostThreshold;

	// rebuild distances
	antPath.RebuildDistanceList();

	return result;
}

bool UAntUtil::MoveAgentsToLocations(UWorld *World, const TArray<FAntHandle> &AgentsToMove, const TArray<FVector> &Locations,
	const TArray<float> &MaxSpeeds, const TArray<float> &Accelerations, const TArray<float> &Decelerations,
	float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float PathWidth,
	float MissingVelocityTimeout, bool PathReplan, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = World->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");
	check((AgentsToMove.Num() <= Locations.Num()) && "Size of agents must be equal or lower than to the size of the locations arrays.");

	if (AgentsToMove.IsEmpty() || AgentsToMove.Num() > Locations.Num() || MaxSpeeds.IsEmpty() || Accelerations.IsEmpty() || Decelerations.IsEmpty())
		return false;

	UNavigationSystemV1 *navSys = UNavigationSystemV1::GetCurrent(World);
	if (!navSys || !navSys->GetDefaultNavDataInstance())
		return false;

	// path query filter
	auto *navData = navSys->GetDefaultNavDataInstance();
	auto *antNavMesh = Cast<AAntRecastNavMesh>(navData);
	check(antNavMesh && "AntRecastNavMesh is not set.");
	if (!antNavMesh)
		return false;

	const auto queryFilter = FilterClass ? UNavigationQueryFilter::GetQueryFilter(*navData, nullptr, FilterClass) : antNavMesh->GetDefaultQueryFilter();
	struct FData { FNavMeshPath Path; ENavigationQueryResult::Type Result = ENavigationQueryResult::Type::Invalid; FAntHandle PathHandle; };
	bool anyFail = false;
	TArray<FData> results;
	results.SetNum(AgentsToMove.Num());

	// step 1: create path
	for (int32 idx = 0; idx < results.Num(); ++idx)
		if (ant->IsValidAgent(AgentsToMove[idx]))
			results[idx].PathHandle = ant->AddEmptyPath(PathWidth, queryFilter, AgentsToMove[idx]);

	// step 2: path-finding and corridor building
	ParallelFor(AgentsToMove.Num(), [&](int32 idx)
		{
			if (!ant->IsValidAgent(AgentsToMove[idx]))
				return;

			const auto &agentData = ant->GetAgentData(AgentsToMove[idx]);
			results[idx].Result = BuildPath(World, antNavMesh, results[idx].Path, results[idx].PathHandle, FVector(agentData.Location), Locations[idx], PathWidth, ReplanCostThreshold, FilterClass);

			if (results[idx].Result != ENavigationQueryResult::Type::Success)
				anyFail = true;
		});

	// step 3: move agents with valid paths and remove invalid paths 
	for (int32 idx = 0; idx < AgentsToMove.Num(); ++idx)
	{
		// remove invalid results
		if (ant->IsValidAgent(AgentsToMove[idx]))
			if (results[idx].Result != ENavigationQueryResult::Type::Success || ant->GetMutablePathData(results[idx].PathHandle).GetData().IsEmpty())
			{
				ant->RemovePath(results[idx].PathHandle);
				ant->RemoveAgentMovement(AgentsToMove[idx]);
				continue;
			}

		// path replaning
		if (PathReplan)
			antNavMesh->AddPathToReplanList(results[idx].PathHandle, &results[idx].Path);

		// move agent through the path
		const auto maxSpeed = MaxSpeeds.Num() > idx ? MaxSpeeds[idx] : MaxSpeeds[0];
		const auto accel = Accelerations.Num() > idx ? Accelerations[idx] : Accelerations[0];
		const auto decel = Decelerations.Num() > idx ? Decelerations[idx] : Decelerations[0];
		ant->MoveAgentByPath(AgentsToMove[idx], results[idx].PathHandle, PathFollowerType, maxSpeed, accel, decel, TargetAcceptanceRadius, PathAcceptanceRadius, 0, MissingVelocityTimeout);
	}

	return !anyFail;
}

FAntHandle UAntUtil::CreateSharedPath(UWorld *World, const FVector &Start, const FVector &End, float PathWidth, bool PathReplan, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = World->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	UNavigationSystemV1 *navSys = UNavigationSystemV1::GetCurrent(World);
	if (!navSys || !navSys->GetDefaultNavDataInstance())
		return FAntHandle();

	auto *navData = navSys->GetDefaultNavDataInstance();
	auto *antNavMesh = Cast<AAntRecastNavMesh>(navData);
	check(antNavMesh && "AntRecastNavMesh is not set.");
	if (!antNavMesh)
		return FAntHandle();

	FNavMeshPath path;
	const auto queryFilter = FilterClass ? UNavigationQueryFilter::GetQueryFilter(*navData, nullptr, FilterClass) : antNavMesh->GetDefaultQueryFilter();
	const auto antPath = ant->AddEmptyPath(PathWidth, queryFilter);
	const auto result = BuildPath(World, antNavMesh, path, antPath, Start, End, PathWidth, ReplanCostThreshold, FilterClass);

	if (result != ENavigationQueryResult::Type::Success)
	{
		ant->RemovePath(antPath);
		return FAntHandle();
	}

	// path replaning
	if (PathReplan)
		antNavMesh->AddPathToReplanList(antPath, &path);

	return antPath;
}

FAntHandle UAntUtil::CreateSharedPathBySpline(UWorld *World, const USplineComponent *Spline, float PathWidth, bool PathReplan, float ReplanCostThreshold, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	auto *ant = World->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	// retrieve navigation system and check spline component
	auto *navSys = UNavigationSystemV1::GetCurrent(World);
	if (!navSys || !navSys->GetDefaultNavDataInstance() || !Spline || Spline->GetNumberOfSplinePoints() <= 1)
		return FAntHandle();

	auto *navData = navSys->GetDefaultNavDataInstance();
	auto *antNavMesh = Cast<AAntRecastNavMesh>(navData);
	check(antNavMesh && "AntRecastNavMesh is not set.");
	if (!antNavMesh)
		return FAntHandle();

	const auto queryFilter = FilterClass ? UNavigationQueryFilter::GetQueryFilter(*navData, nullptr, FilterClass) : antNavMesh->GetDefaultQueryFilter();

	// iterate over each point and make its corridor
	const auto spNumPoints = Spline->GetNumberOfSplinePoints();
	FNavPathSharedPtr finalPath;
	for (int32 idx = 0; idx < spNumPoints - 1; ++idx)
	{
		// find path between 2 spline points
		const FPathFindingQuery query(nullptr, *navData, Spline->GetLocationAtSplinePoint(idx, ESplineCoordinateSpace::World), Spline->GetLocationAtSplinePoint(idx + 1, ESplineCoordinateSpace::World), queryFilter);
		const auto result = navSys->FindPathSync(query, EPathFindingMode::Regular);

		// there is no valid path through the spline
		if (!result.IsSuccessful())
			return FAntHandle();

		// store first path for merging the rest into it
		if (idx == 0)
		{
			finalPath = result.Path;
			continue;
		}

		finalPath->GetPathPoints().Pop();
		finalPath->GetPathPoints().Append(result.Path->GetPathPoints());
	}

	// build corridor
	FNavCorridor corridor;
	FNavCorridorParams corridorParam;
	corridorParam.SetFromWidth(PathWidth);
	corridor.BuildFromPath(*finalPath, queryFilter, corridorParam);
	if (corridor.Portals.IsEmpty())
		return FAntHandle();

	// add it as a permanent shared Ant path
	const auto pathHandle = ant->AddEmptyPath(PathWidth, queryFilter);

	// fill path data
	auto &antPath = ant->GetMutablePathData(pathHandle);
	auto &pathData = antPath.GetMutableData();
	pathData.SetNum(corridor.Portals.Num());
	for (int32 pidx = 0; pidx < corridor.Portals.Num(); ++pidx)
		memcpy(&pathData[pidx], &corridor.Portals[pidx], sizeof(FNavCorridorPortal));

	//pathData.NavLinkWidth = NavLinkWidth;
	antPath.ReplanCostThreshold = ReplanCostThreshold;

	antPath.RebuildDistanceList();

	// path replaning
	if (PathReplan)
		antNavMesh->AddPathToReplanList(pathHandle, finalPath.Get());

	return pathHandle;
}

void UAntUtil::QueryOnScreenAgents(APlayerController *PC, const FBox2f &ScreenArea, float FrustumDepth, ECollisionChannel FloorChannel, int32 Flags, TArray<FAntHandle> &ResultUnits)
{
	// Ant is a world subsystem
	auto *ant = PC->GetWorld()->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	// calibrate ScreenArea in case of incorrect Min, Max points
	const auto minx = FMath::Min(ScreenArea.Min.X, ScreenArea.Max.X);
	const auto miny = FMath::Min(ScreenArea.Min.Y, ScreenArea.Max.Y);
	const auto maxx = FMath::Max(ScreenArea.Min.X, ScreenArea.Max.X);
	const auto maxy = FMath::Max(ScreenArea.Min.Y, ScreenArea.Max.Y);

	// if given ScreenArea size is too small, we can do a simple point query
	const auto areaWidth = (maxx - minx);
	const auto areaHeight = (maxy - miny);
	
	if (areaWidth <= 5 && areaHeight <= 5)
	{
		// cast a ray at the center of the given ScreenArea to find actual ground location
		FHitResult hitResult;
		const int32 xCenter = minx + areaWidth / 2;
		const int32 yCenter = miny + areaHeight / 2;

		// point query at the ground location
		TArray<FAntContactInfo> contactList;
		if (PC->GetHitResultAtScreenPosition(FVector2D(xCenter, yCenter), FloorChannel, false, hitResult))
			ant->QueryCylinder(FVector3f(hitResult.ImpactPoint), 7, 100, Flags, false, contactList);

		// add to the result list
		for (const auto &contact : contactList)
			ResultUnits.Add(contact.Handle);
		
		return;
	}

	// find wolrd coordinates of the given points (near plane)
	TStaticArray<FVector, 4> worldOrigin;
	TStaticArray<FVector, 4> worldDirection;
	UGameplayStatics::DeprojectScreenToWorld(PC, FVector2D(minx, miny), worldOrigin[0], worldDirection[0]);
	UGameplayStatics::DeprojectScreenToWorld(PC, { maxx, miny }, worldOrigin[1], worldDirection[1]);
	UGameplayStatics::DeprojectScreenToWorld(PC, FVector2D(maxx, maxy), worldOrigin[2], worldDirection[2]);
	UGameplayStatics::DeprojectScreenToWorld(PC, { minx, maxy }, worldOrigin[3], worldDirection[3]);

	// convert directions to offsets (far plane)
	for (auto &it : worldDirection)
		it *= FrustumDepth;

	TArray <FAntContactInfo> result;
	ant->QueryConvexVolume(worldOrigin, worldDirection, Flags, result);

	for (const auto &it : result)
		ResultUnits.Add(it.Handle);
}

void UAntUtil::ConvertLinearPathToCurvePath(TArray<FVector> &PathToConvert, float MaxSquareDistanceFromSpline)
{
	// add path points to the spline
	FAntSplineCurve splineCurve;
	for (const auto &point : PathToConvert)
		splineCurve.AddSplinePoint(point, false);

	// build the curve
	splineCurve.UpdateSpline();

	// convert curve to to path
	splineCurve.ConvertSplineToPolyLine(MaxSquareDistanceFromSpline, PathToConvert);
}

void UAntUtil::ConvertLinearPathToCurvePath(TArray<FNavPathPoint> &Path, float MaxSquareDistanceFromSpline)
{
	TArray<FVector> resultPath;
	resultPath.Reserve(Path.Num());
	for (auto idx = 0; idx < Path.Num(); ++idx)
		resultPath.Add(Path[idx].Location);

	ConvertLinearPathToCurvePath(resultPath, MaxSquareDistanceFromSpline);

	Path.Reset(resultPath.Num());
	for (const auto &p : resultPath)
		Path.Add(FNavPathPoint(p));
}

void UAntUtil::ConvertLinearPathToCurvePath(const TArray<FNavPathPoint> &Path, float MaxSquareDistanceFromSpline, TArray<FVector> &ResultPath)
{
	ResultPath.Reserve(Path.Num());
	for (auto idx = 0; idx < Path.Num(); ++idx)
		ResultPath.Add(Path[idx].Location);

	ConvertLinearPathToCurvePath(ResultPath, MaxSquareDistanceFromSpline);
}

void UAntUtil::SortAgentsBySquare(UWorld *World, const FVector &DestLocation, TArray<FAntHandle> &Agents, float SplitSpace, float &SquareDimension, TArray<FVector> &ResultLocations)
{
	auto *ant = World->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	ResultLocations.Empty(Agents.Num());

	// 
	if (Agents.IsEmpty() || !ant->IsValidAgents(Agents))
		return;

	// compute average radius of the agents
	float averageRadius = 0.f;
	for (const auto &handle : Agents)
		averageRadius += ant->GetAgentData(handle).GetRadius();

	averageRadius /= Agents.Num();

	// build quad formation
	const float agentDiameter = averageRadius * 2 + SplitSpace;
	const auto numRows = FMath::RoundToInt(FMath::Sqrt(static_cast<float>(Agents.Num())));
	const auto adjustment = (numRows / 2) * agentDiameter;
	for (int32 row = 0; row <= numRows; ++row)
		for (int32 col = 0; col < numRows; ++col)
			if (ResultLocations.Num() < Agents.Num())
				ResultLocations.Add(FVector(row * agentDiameter - adjustment, col * agentDiameter - adjustment, 0) + DestLocation);

	// sort agents based thier X axis
	Agents.Sort([ant](const FAntHandle &First, const FAntHandle &Second) -> bool
		{
			return ant->GetAgentData(First).GetLocationLerped().X < ant->GetAgentData(Second).GetLocationLerped().X;
		});

	// sort agents based their Y axis per row
	for (int32 offset = 0; offset < Agents.Num(); offset += numRows)
		MakeArrayView(&Agents[offset], FMath::Min(numRows, Agents.Num() - offset)).Sort([ant](const FAntHandle &First, const FAntHandle &Second) -> bool
			{
				return ant->GetAgentData(First).GetLocationLerped().Y < ant->GetAgentData(Second).GetLocationLerped().Y;
			});

	SquareDimension = (agentDiameter * numRows);
}

void UAntUtil::SortAgentsByCurrentLocation(UWorld *World, const FVector &DestLocation, const TArray<FAntHandle> &Agents, TArray<FVector> &ResultLocations)
{
	auto *ant = World->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	ResultLocations.Empty(Agents.Num());

	// 
	if (Agents.IsEmpty() || !ant->IsValidAgents(Agents))
		return;

	// find center of the crowd at the source area
	FVector centerOfCrowd;
	for (const auto &handle : Agents)
		centerOfCrowd += FVector(ant->GetAgentData(handle).GetLocation());

	centerOfCrowd /= Agents.Num();

	// transform agent locations to the destination locations by center of crowd
	for (const auto &handle : Agents)
	{
		const auto delta = FVector(ant->GetAgentData(handle).GetLocation()) - centerOfCrowd;
		ResultLocations.Add(DestLocation + delta);
	}
}

void UAntUtil::SortAgentsByCirclePack(UWorld *World, const FVector &DestLocation, TArray<FAntHandle> &Agents, float SplitSpace, TArray<FVector> &ResultLocations)
{
	auto *ant = World->GetSubsystem<UAntSubsystem>();
	check(ant && "Ant is not available.");

	ResultLocations.Empty();

	// 
	if (Agents.IsEmpty() || !ant->IsValidAgents(Agents))
		return;

	if (Agents.Num() <= 2)
	{
		for (const auto &it : Agents)
			ResultLocations.Add(DestLocation);

		return;
	}

	// find center of the crowd at the source area
	FVector centerOfCrowd;
	for (const auto &handle : Agents)
		centerOfCrowd += FVector(ant->GetAgentData(handle).GetLocation());

	centerOfCrowd /= Agents.Num();

	// sort agnets according to its distance to the center
	//Agents.Sort([ant, centerOfCrowd](const FAntHandle &First, const FAntHandle &Second) -> bool
	//	{
	//		return FVector2f::DistSquared(FVector2f(ant->GetAgentData(First).GetLocationLerped()), FVector2f(centerOfCrowd.X, centerOfCrowd.Y)) 
	//			< FVector2f::DistSquared(FVector2f(ant->GetAgentData(Second).GetLocationLerped()), FVector2f(centerOfCrowd.X, centerOfCrowd.Y));
	//	});

	// allocate and init node list
	TArray<FCirclePackNode> nodeList;
	nodeList.SetNum(Agents.Num());
	for (int32 idx = 0; idx < Agents.Num(); ++idx)
	{
		const auto it = Agents[idx];
		nodeList[idx].radius = ant->GetAgentData(it).Radius + SplitSpace;
		nodeList[idx].insertNext = idx < nodeList.Num() - 1 ? &nodeList[idx + 1] : nullptr;
	}

	/* Create first circle. */
	FCirclePackNode *a = &nodeList[0];
	FCirclePackNode *b = NULL;
	FCirclePackNode *c = NULL;

	a->x = -1 * a->radius;
	//bound(a, bb_topright, bb_bottomleft);

	// Create second circle. */
	b = a->insertNext;
	b->x = b->radius;
	b->y = 0;
	//bound(b, bb_topright, bb_bottomleft);

	// Create third circle. */
	c = b->insertNext;
	FCirclePackNode::Place(a, b, c);
	//bound(c, bb_topright, bb_bottomleft);

	// make initial chain of a <-> b <-> c
	a->next = c;
	a->prev = b;
	b->next = a;
	b->prev = c;
	c->next = b;
	c->prev = a;
	b = c;

	/* add remaining nodes */
	int skip = 0;
	c = c->insertNext;
	while (c)
	{
		// Determine the node a in the chain, which is nearest to the center
		// The new node c will be placed next to a (unless overlap occurs)
		// NB: This search is only done the first time for each new node, i.e.
		// not again after splicing
		if (!skip)
		{
			FCirclePackNode *n = a;
			FCirclePackNode *nearestnode = n;
			float nearestdist = FLT_MAX;
			do
			{
				float dist_n = FVector2f(n->x, n->y).Size();
				if (dist_n < nearestdist)
				{
					nearestdist = dist_n;
					nearestnode = n;
				}
				n = n->next;
			} while (n != a);

			a = nearestnode;
			b = nearestnode->next;
			skip = 0;
		}

		// a corresponds to C_m, and b corresponds to C_n in the paper
		FCirclePackNode::Place(a, b, c);

		// for debugging: initial placement of c that may ovelap
		int32 isect = 0;
		auto *j = b->next;
		auto *k = a->prev;
		float sj = b->radius;
		float sk = a->radius;
		//j = b.next, k = a.previous, sj = b._.r, sk = a._.r;
		do {
			if (sj <= sk) {
				if (FAntMath::CircleCircleSq(FVector2f(j->x, j->y), j->radius, FVector2f(c->x, c->y), c->radius) > 0)
				{
					FCirclePackNode::Splice(a, j);
					b = j;
					skip = 1;
					isect = 1;
					break;
				}
				sj += j->radius;
				j = j->next;
			}
			else
			{
				if (FAntMath::CircleCircleSq(FVector2f(k->x, k->y), k->radius, FVector2f(c->x, c->y), c->radius) > 0)
				{
					FCirclePackNode::Splice(k, b);
					a = k;
					skip = 1;
					isect = 1;
					break;
				}
				sk += k->radius;
				k = k->prev;
			}
		} while (j != k->next);

		if (isect == 0) {
			/* Update node chain. */
			FCirclePackNode::Insert(a, c);
			b = c;
			//bound(c, bb_topright, bb_bottomleft);
			skip = 0;
			c = c->insertNext;
		}
	}

	// sort by Y
	//Agents.Sort([ant](const FAntHandle &First, const FAntHandle &Second) -> bool
	//	{
	//		return ant->GetAgentData(First).GetLocation().Y < ant->GetAgentData(Second).GetLocation().Y;
	//	});

	for (int32 idx = 0; idx < nodeList.Num(); ++idx)
		ResultLocations.Add(FVector(nodeList[idx].x + DestLocation.X, nodeList[idx].y + DestLocation.Y, DestLocation.Z));
}

void UAntUtil::GetCorridorPortalAlpha(const TArray<FVector> &SourceLocations, const FVector &DestLocation, TArray<float> &ResultAlpha)
{
	ResultAlpha.SetNum(SourceLocations.Num());

	// ignore SourceLocation list with single element
	if (SourceLocations.Num() == 1)
	{
		ResultAlpha[0] = -1;
		return;
	}

	// compute average direction from agents to the destination location
	// this way we can find out which side of the destination has more agent
	struct RequiredData { int32 Ind = INDEX_NONE; FVector2f Dir = FVector2f::ZeroVector; float Radian = 0.0f; };
	const FVector2f worldLoc2D(DestLocation.X, DestLocation.Y);
	int32 ind = 0;
	FVector2f averageDir = FVector2f::Zero();
	TArray<RequiredData> dataList;
	dataList.Reserve(SourceLocations.Num());
	for (const auto &loc : SourceLocations)
	{
		const auto deltaDist = FVector2f(loc.X, loc.Y) - worldLoc2D;
		const auto dir = deltaDist.GetSafeNormal();
		dataList.Add({ ind++, dir });
		averageDir += dir;
	}

	// in case of fully symmetrical directions we set all alpha to -1
	if (averageDir == FVector2f::ZeroVector)
		for (auto &alpha : ResultAlpha)
			alpha = -1;

	//						^
	//						|
	//						|
	//					averageDir
	//						|
	// <---ccwEdge----------|----------cwEdge--->
	// 
	averageDir = averageDir.GetSafeNormal();
	// clockwise edge relative to the averageDir
	const FVector2f cwEdge(-averageDir.Y, averageDir.X);
	// counter clockwise edge relative to the averageDir
	const FVector2f ccwEdge(averageDir.Y, -averageDir.X);
	// finding all facing agents in a 180 range of the average direction
	for (int32 idx = 0; idx < dataList.Num(); /* custom */)
	{
		const auto ret = FAntMath::IsInsideCW(ccwEdge, cwEdge, dataList[idx].Dir);

		// it's inside the range, so we need to compute its radians for next phase (sorting phase)
		if (ret > 0)
		{
			dataList[idx].Radian = FMath::Atan2(dataList[idx].Dir.Y, dataList[idx].Dir.X);
			++idx;
			continue;
		}

		// out of range
		if (ret <= 0)
		{
			ResultAlpha[dataList[idx].Ind] = -1;
			dataList.RemoveAtSwap(idx);
		}
	}

	// sort the list according to the ccwEdge
	const auto ccwEdgeRadian = FMath::Atan2(ccwEdge.Y, ccwEdge.X);
	dataList.Sort([ccwEdgeRadian](const RequiredData &First, const RequiredData &Second) -> bool
		{
			const auto firstDelta = FMath::FindDeltaAngleRadians(ccwEdgeRadian, First.Radian);
			const auto secondDelta = FMath::FindDeltaAngleRadians(ccwEdgeRadian, Second.Radian);
			return firstDelta > secondDelta;
		});

	// compute final alpha and put it in the result list
	for (int32 idx = 0; idx < dataList.Num(); ++idx)
		ResultAlpha[dataList[idx].Ind] = static_cast<float>(idx) / static_cast<float>(dataList.Num());
}

void UAntUtil::UpdateAgentNavModifier(UWorld *World, FAntHandle Agent, TSubclassOf<UNavAreaBase> AreaClass)
{
	UNavigationSystemV1 *navSys = UNavigationSystemV1::GetCurrent(World);
	if (!navSys || !navSys->GetDefaultNavDataInstance())
		return;

	// path query filter
	auto *navData = navSys->GetDefaultNavDataInstance();
	auto *antNavMesh = Cast<AAntRecastNavMesh>(navData);
	check(antNavMesh && "AntRecastNavMesh is not set.");
	if (!antNavMesh)
		return;

	antNavMesh->AddAgentNavModifier(Agent, AreaClass);
}

void UAntUtil::RemoveAgentNavModifier(UWorld *World, FAntHandle Agent)
{
	UNavigationSystemV1 *navSys = UNavigationSystemV1::GetCurrent(World);
	if (!navSys || !navSys->GetDefaultNavDataInstance())
		return;

	// path query filter
	auto *navData = navSys->GetDefaultNavDataInstance();
	auto *antNavMesh = Cast<AAntRecastNavMesh>(navData);
	check(antNavMesh && "AntRecastNavMesh is not set.");
	if (!antNavMesh)
		return;

	antNavMesh->RemoveAgentNavModifier(Agent);
}
