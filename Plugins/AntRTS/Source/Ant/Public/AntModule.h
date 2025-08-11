// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleManager.h"

// Ant log category
DECLARE_LOG_CATEGORY_EXTERN(LogAnt, Log, All);
// Ant profile category
DECLARE_STATS_GROUP(TEXT("Ant"), STATGROUP_ANT, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Ant - TotalQPS"), STAT_ANT_QPS, STATGROUP_ANT, ANT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Ant - CollisionSolver"), STAT_ANT_PxUpdate, STATGROUP_ANT, ANT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Ant - AsyncPathfinding"), STAT_ANT_Pathfinding, STATGROUP_ANT, ANT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Ant - AsyncQueries"), STAT_ANT_Queries, STATGROUP_ANT, ANT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Ant - BroadPhase"), STAT_ANT_BFUpdate, STATGROUP_ANT, ANT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Ant - Movements"), STAT_ANT_MovementsUpdate, STATGROUP_ANT, ANT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Ant - PostPxUpdate"), STAT_ANT_PostPX, STATGROUP_ANT, ANT_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Ant - TotalAntFrame"), STAT_ANT_TotalFrame, STATGROUP_ANT, ANT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Ant - NumAgents"), STAT_ANT_NumAgents, STATGROUP_ANT, ANT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Ant - NumMovingAgents"), STAT_ANT_NumMovingAgents, STATGROUP_ANT, ANT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Ant - NumPaths"), STAT_ANT_NumPaths, STATGROUP_ANT, ANT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Ant - NumAsyncQueries"), STAT_ANT_NumAsyncQueries, STATGROUP_ANT, ANT_API);

class FAntModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};