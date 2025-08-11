// Copyright 2024 Lazy Marmot Games. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AntSubsystem.h"
#include "AntUtil.h"
#include "AntTest/AntTest.h"
#include "AntTest/AntTest.h"
#include "AntFunctionLibrary.generated.h"

class UInstancedStaticMeshComponent;

/** This calss is a higher-level and safe version of Ant for use in bluperint. */
UCLASS()
class ANT_API UAntFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static UAntSubsystem *GetAntSubsystem(const UObject *WorldContextObject);

	/**
	 * Sort and formation given agnets by a square. it works best when agents have same radius.
	 * This function may alter order of the Agents array.
	 * @param World Current active world.
	 * @param DestLocation Destination location (center of the quad).
	 * @param SplitSpace Space between each agent
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SortAgentsBySquare(const UObject *WorldContextObject, const FVector &DestLocation, const TArray<FAntHandle> &Agents, float SplitSpace, float &SquareDimension,
		TArray<FAntHandle> &SortedAgents, TArray<FVector> &ResultLocations);

	/**
	 * Add a new agent at the given location.
	 * @param Location Agent location.
	 * @param Radius Agent radius.
	 * @param ExtraRVORadius Extra radius for RVO neighbour query.
	 * @param FaceAngle Agent face angle.
	 * @param Flags Agent flag.
	 * @return Agent handle.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static FAntHandle AddAgentAdvanced(const UObject *WorldContextObject, const FVector &Location, float Radius, float ExtraQueryRadius, float Height,
		float FaceAngle = 0.0f, float TurnRate = 0.05f, float MoveAngleThreshold = 7.0f,
		bool TurnByPreferred = false, bool CanPierce = true, bool UseNavigation = true, float MaxOverlapForce = 1.0, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags = 0,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 IgnoreFlag = 0, EAntAvoidanceTypes CollisionAvoidance = EAntAvoidanceTypes::AntDefault, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/**
	 * Add a new agent at the given location.
	 * @param Location Agent location.
	 * @param Radius Agent radius.
	 * @param FaceAngle Agent face angle.
	 * @param Flags Agent flag.
	 * @return Agent handle.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static FAntHandle AddAgent(const UObject *WorldContextObject, const FVector &Location, float Radius, float Height, float FaceAngle, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	/**
	 * @brief Destroy an agent.
	 * @param Handle Agent handle.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void RemoveAgent(const UObject *WorldContextObject, FAntHandle Handle);

	/**
	 * @brief Destroy agents.
	 * @param AgentsHandle Agents handle list to destroy.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void RemoveAgents(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle);

	/**
	 * Set agent location directly (teleport).
	 * If you need to move an agent use MoveAgent() instead.
	 * @param Handle Agent handle.
	 * @param Location Agent new location.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentLocation(const UObject *WorldContextObject, FAntHandle Handle, const FVector &Location);

	/** Get agent current location. */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void GetAgentLocation(const UObject *WorldContextObject, FAntHandle AgentHandle, FVector3f &Location);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void GetAgentRotation(const UObject *WorldContextObject, FAntHandle AgentHandle, float &FaceAngle);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentRotation(const UObject *WorldContextObject, FAntHandle AgentHandle, float FaceAngle);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void GetAgentVelocity(const UObject *WorldContextObject, FAntHandle AgentHandle, FVector3f &Velocity);

	/** Set agent preferred velocity. */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentPreferredVelocity(const UObject *WorldContextObject, FAntHandle Handle, const FVector3f &Velocity);

	/** Set agent overlap force. */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentOverlapForce(const UObject *WorldContextObject, FAntHandle Handle, float OverlapForce);

	/**
	 * Set agent flag.
	 * @param AgentHandle Agent handle.
	 * @param Flags Agent new flag.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentFlag(const UObject *WorldContextObject, FAntHandle AgentHandle, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	/**
	 * Set agents flag.
	 * @param AgentsHandle Agents handles.
	 * @param Flags Agent new flag.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentsFlag(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	/**
	 * Set agent ignore flag.
	 * @param AgentHandle Agent handle.
	 * @param Flags Agent new IgnoreFlag.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentIgnoreFlag(const UObject *WorldContextObject, FAntHandle AgentHandle, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	/**
	 * Set agents ignore flag.
	 * @param AgentsHandle Agents handles.
	 * @param Flags Agent new IgnoreFlag.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentsIgnoreFlag(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	/**
	 * Set agent ignore-but-wakeup flag.
	 * @param AgentHandle Agent handle.
	 * @param Flags Agent new Ignore-but-wakeup Flag.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentIgnoreWakeupFlag(const UObject *WorldContextObject, FAntHandle AgentHandle, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	/**
	 * Set agents ignore-but-wakeup flag.
	 * @param AgentsHandle Agents handles.
	 * @param Flags Agent new Ignore-but-wakeup Flag.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentsIgnoreWakeupFlag(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetAgentCustomInstancedStruct(const UObject *WorldContextObject, FAntHandle AgentHandle, const FInstancedStruct &CustomStruct);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void GetAgentCustomInstancedStruct(const UObject *WorldContextObject, FAntHandle AgentHandle, FInstancedStruct &OutStruct);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", CustomThunk, meta = (WorldContext = "WorldContextObject", CustomStructureParam = "OutStruct", BlueprintInternalUseOnly = false, ExpandBoolAsExecs = "ExecResult"))
	static void GetAgentCustomStruct(const UObject *WorldContextObject, FAntHandle AgentHandle, bool &ExecResult, int32 &OutStruct);
	DECLARE_FUNCTION(execGetAgentCustomStruct);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", CustomThunk, meta = (WorldContext = "WorldContextObject", CustomStructureParam = "InStruct", BlueprintInternalUseOnly = false))
	static void SetAgentCustomStruct(const UObject *WorldContextObject, FAntHandle AgentHandle, const int32 &InStruct);
	DECLARE_FUNCTION(execSetAgentCustomStruct);

	/**
	 * Check whether an agnet handle is valid or not.
	 * @param Handle Agent handle
	 * @return True if handle is valid.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static bool IsValidAgent(const UObject *WorldContextObject, FAntHandle Handle);

	/**
	 * Check whether a list of handles is valid or not.
	 * @param Handles Handle list.
	 * @return True if all handles inside the list is valid.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static bool IsValidAgents(const UObject *WorldContextObject, const TArray<FAntHandle> Handles);

	/**
	 * Find agents with the given flag
	 * @return True if any agent found.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static bool FindAgentsByFlag(const UObject *WorldContextObject, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags, TArray<FAntHandle> &Result);

	/* Create a permanent shared path by given locations.
	* Returned path is permanent until it get removed by calling Ant->RemovePath().
	* @return return a valid path handle if it found a valid path.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static FAntHandle CreateSharedPath(const UObject *WorldContextObject, const FVector &Start, const FVector &End, float PathWidth, bool PathReplan = false, float ReplanCostThreshold = -1.f, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/* Create a permanent shared path by Spline.
	* Returned path is permanent until it get removed by calling Ant->RemovePath().
	* @return return a valid path handle if it found a valid path.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static FAntHandle CreateSharedPathBySpline(const UObject *WorldContextObject, const USplineComponent *Spline, float PathWidth, bool PathReplan = false, float ReplanCostThreshold = -1.f, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void SetPathCustomInstancedStruct(const UObject *WorldContextObject, FAntHandle PathHandle, const FInstancedStruct &CustomStruct);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void GetPathCustomInstancedStruct(const UObject *WorldContextObject, FAntHandle PathHandle, FInstancedStruct &OutStruct);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", CustomThunk, meta = (WorldContext = "WorldContextObject", CustomStructureParam = "OutStruct", BlueprintInternalUseOnly = false, ExpandBoolAsExecs = "ExecResult"))
	static void GetPathCustomStruct(const UObject *WorldContextObject, FAntHandle PathHandle, bool &ExecResult, int32 &OutStruct);
	DECLARE_FUNCTION(execGetPathCustomStruct);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", CustomThunk, meta = (WorldContext = "WorldContextObject", CustomStructureParam = "InStruct", BlueprintInternalUseOnly = false))
	static void SetPathCustomStruct(const UObject *WorldContextObject, FAntHandle PathHandle, const int32 &InStruct);
	DECLARE_FUNCTION(execSetPathCustomStruct);

	/**
	 * Query broadphase grid with a cylinder.
	 * If you have a large number of queries you've better to go with async version (C++).
	 * @param Base Base (bottom) of the cylinder.
	 * @param Radius Radius of the cylinder.
	 * @param Height Height of the cylinder.
	 * @param Flags Query flag.
	 * @param QueryResult Result of the query. may contain duplication in case of multiple instance from same owner.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void QueryCylinder(const UObject *WorldContextObject, const FVector3f &Base, float Radius, float Height,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags, bool MustIncludeCenter, TArray<FAntHandle> &QueryResult);

	static void QueryConvexVolume(const UObject *WorldContextObject, const TStaticArray<FVector, 4> &NearPlane, const TStaticArray<FVector, 4> &FarPlane,
		int32 Flags, TArray<FAntHandle>& AgentsHandle);

	/**
	 * Async query broadphase grid with a cylinder attached on the given agent current location.
	 * You can have a large number of queries in case of async queries.
	 * Bind to OnQueryFinished for the result (Game thread).
	 * @param AgentHandle Agent handle.
	 * @param Radius Radius of the cylinder.
	 * @param Height Height of the cylinder.
	 * @param Flags Query flag.
	 * @param Interval Query execution interval. -1 means one-time (single frame) query. 0 Means each-frame query.
	 * @return Query handle.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static FAntHandle QueryCylinderAttachedAsync(const UObject *WorldContextObject, FAntHandle AgentHandle, float Radius, float Height,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags, bool MustIncludeCenter, float Interval = -1.0f);

	/**
	 * Query broadphase grid with a ray.
	 * If you have a large number of queries you've better to go with async version.
	 * This function is thread-safe and lock-free.
	 * @param Start Ray start location.
	 * @param End Ray end location.
	 * @param Flags Query flag
	 * @param QueryResult Result of the query. may contain duplication in case of multiple instance from same owner.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void QueryRay(const UObject *WorldContextObject, const FVector3f &Start, const FVector3f &End, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags, TArray<FAntHandle> &QueryResult);

	/**
	 * Select agents by the given area on the screen.
	 * @param PC Current player controller
	 * @param FarClipDistance Distance to the far clip plane
	 * @param FloorChannel Collsion channel of the landscape.
	 * @param Flags Query flag
	 * @param ResultUnits Result of the query
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement")
	static void QueryOnScreenAgents(APlayerController *PC, const FBox2f &ScreenArea, float FarClipDistance, ECollisionChannel FloorChannel, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags, TArray<FAntHandle> &QueryResult);

	/** Get result of an Async query by its handle. */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void GetAsyncQueryResult(const UObject *WorldContextObject, FAntHandle QueryHandle, TArray<FAntHandle> &QueryResult);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", CustomThunk, meta = (WorldContext = "WorldContextObject", CustomStructureParam = "OutStruct", BlueprintInternalUseOnly = false, ExpandBoolAsExecs = "ExecResult"))
	static void GetAsyncQueryCustomStruct(const UObject *WorldContextObject, FAntHandle QueryHandle, bool &ExecResult, int32 &OutStruct);
	DECLARE_FUNCTION(execGetAsyncQueryCustomStruct);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", CustomThunk, meta = (WorldContext = "WorldContextObject", CustomStructureParam = "InStruct", BlueprintInternalUseOnly = false))
	static void SetAsyncQueryCustomStruct(const UObject *WorldContextObject, FAntHandle QueryHandle, const int32 &InStruct);
	DECLARE_FUNCTION(execSetAsyncQueryCustomStruct);

	/**
	 * Check whether an async query handle is valid or not.
	 * @param Handle Async query handle.
	 * @return True if the given handle is valid.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static bool IsValidAsyncQuery(const UObject *WorldContextObject, FAntHandle Handle);

	/**
	 * Move an agent through the given path.
	 * MissingVelocity = (Agent.PreferredVelocity - Agent.CurrentVelocity)
	 * if (MissingVelocity.SumOfXY >= MissingVelocityTimeout) Notify(Agent)
	 * Bind to OnAgentReached and OnAgentBlocked for events.
	 * @param Handle Agent handle.
	 * @param Path Path
	 * @param MaxSpeed Maximum speed of the agent.
	 * @param Acceleration Speed acceleration. (set it equal to MaxSpeed in case of linear speed)
	 * @param Deceleration Speed Deceleration. (set it equal to MaxSpeed or 0 in case of linear speed)
	 * @param TargetAcceptanceRadius Acceptance radius of the target (goal) location.
	 * @param PathAcceptanceRadius Acceptance radius of path nodes inside the path array. it will be checked against center of the agent.
	 * @param MissingVelocityTimeout Velocity time out threshold. -1 means no timeout.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void MoveAgentByPath(const UObject *WorldContextObject, FAntHandle AgentHandle, FAntHandle PathHandle, float MaxSpeed, float Acceleration, float Deceleration,
		float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float StartDelay = 0.0f, float MissingVelocityTimeout = -1.0f);

	/* Move an agent to the location with normal pathfinding */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static bool MoveAgentToLocation(const UObject *WorldContextObject, FAntHandle AgentHandle, const FVector &Destination, float MaxSpeed, float Acceleration, float Deceleration,
		float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float PathWidth,
		float MissingVelocityTimeout = -1.0f, bool PathReplan = false, float ReplanCostThreshold = -1.f, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/* Move an agent to the location with normal pathfinding */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static bool MoveAgentsToLocationByFlag(const UObject *WorldContextObject, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags, const FVector &Destination,
		float MaxSpeed, float Acceleration, float Deceleration,
		float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float PathWidth,
		float MissingVelocityTimeout = -1.0f, bool PathReplan = false, float ReplanCostThreshold = -1.f, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/* Move given agents to the location through corridor with normal pathfinding and square formation. */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static bool MoveAgentsToLocations(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsToMove, const TArray<FVector> &Locations, 
		const TArray<float> &MaxSpeeds, const TArray<float> &Accelerations, const TArray<float> &Decelerations,
		float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float PathWidth,
		float MissingVelocityTimeout = -1.0f, bool PathReplan = false, float ReplanCostThreshold = -1.f, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/**
	 * Follow an agent by another agents.
	 * MissingVelocity = (Agent.PreferredVelocity - Agent.CurrentVelocity)
	 * if (MissingVelocity.SumOfXY >= MissingVelocityTimeout) Notify(Agent)
	 * Bind to OnAgentReached and OnAgentBlocked for events.
	 * @param FollowerHandle Follower agent handle.
	 * @param FolloweeHandle Followee agent handle.
	 * @param MaxSpeed Maximum follow speed.
	 * @param FolloweeAcceptanceRadius Acceptance radius of the followee.
	 * @param MissingVelocityTimeout Velocity time out threshold. -1 means no timeout.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void FollowAgent(const UObject *WorldContextObject, FAntHandle FollowerHandle, FAntHandle FolloweeHandle, float MaxSpeed, float Acceleration, float Deceleration,
		float FolloweeAcceptanceRadius, float StartDelay = 0.0f, float MissingVelocityTimeout = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static FVector GetAgentMovementLocation(const UObject *WorldContextObject, FAntHandle AgentHandle);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static float GetAgentMovementSpeed(const UObject *WorldContextObject, FAntHandle AgentHandle);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void RemoveAgentsMovement(const UObject *WorldContextObject, const TArray<FAntHandle> &AgentsHandle);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static bool IsValidMovement(const UObject *WorldContextObject, FAntHandle AgentHandle);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void UpdateAgentNavModifier(const UObject *WorldContextObject, FAntHandle Agent, TSubclassOf<UNavAreaBase> AreaClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static void RemoveAgentNavModifier(const UObject *WorldContextObject, FAntHandle Agent);

	UFUNCTION(BlueprintCallable, Category = "Ant Movement")
	static void CopyAgentsTransformToISMC(UInstancedStaticMeshComponent *InstancedMeshComp, UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	/** Get AntGroup enum value by an index. */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement")
	static int GetAntGroupByIndex(int32 Index);

	/** Get AntGroup enum value by an index. */
	UFUNCTION(BlueprintCallable, Category = "Ant Movement")
	static int GetAllAntGroupsExceptThese(UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/Ant.EAntGroups")) int32 Flags);

	/**
	 * Get type of the given handle.
	 * @param Handle Handle.
	 * @return Handle type.
	*/
	UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	static EAntHandleTypes GetHandleType(const UObject *WorldContextObject, FAntHandle Handle);

	//UFUNCTION(BlueprintCallable, Category = "Ant Movement", meta = (WorldContext = "WorldContextObject"))
	//static bool GetHeightAtLocation(const UObject *WorldContextObject, const FVector2f &Location, bool CalcNormal = true);
};