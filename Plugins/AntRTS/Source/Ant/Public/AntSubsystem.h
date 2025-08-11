// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "AntModule.h"
#include "AntHandle.h"
#include "AntGrid.h"
#include "NavCorridor.h"
#include "Containers/Map.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/WorldSettings.h"
#include "StructUtils/InstancedStruct.h"
#include "AntSubsystem.generated.h"

using ArrayRange = TArray<TPair<int32, int32>>;

/** Ant collision groups. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "false"))
enum class EAntGroups : uint8
{
	Group1 = 0,
	Group2,
	Group3,
	Group4,
	Group5,
	Group6,
	Group7,
	Group8,
	Group9,
	Group10,
	Group11,
	Group12,
	Group13,
	Group14,
	Group15,
	Group16,
	Group17,
	Group18,
	Group19,
	Group20,
	Group21,
	Group22,
	Group23,
	Group24,
	Group25,
	Group26,
	Group27,
	Group28,
	Group29,
	Group30,
	Group31,
	Group32
};
ENUM_CLASS_FLAGS(EAntGroups);

/** Handle types. */
UENUM(BlueprintType)
enum class EAntHandleTypes : uint8
{
	Invalid = 0,
	Agent,
	AsyncQuery,
	Path
};

/** Path replan status. obly eefective for ReplanEnabled paths. */
UENUM(BlueprintType)
enum class EAntPathStatus : uint8
{
	/** Path is ready. */
	Ready = 0,

	/** Path is canceled due to high cost of replanning. (distance cost threshold not met). */
	HighCost,

	/** Path is blocked by an obstacle. */
	Blocked
};

/** Agent collision avoidance types. */
UENUM(BlueprintType)
enum class EAntAvoidanceTypes : uint8
{
	/** Default Ant collison avoidance, faster than RVO. */
	AntDefault = 0,

	/** RVO/ORCA collision avoidance. */
	RVO
};

/** Async query types. */
enum class EAntQueryType : uint8
{
	Cylinder = 0,
	CylinderAttached,
	Ray,
};

/** Internal movement state. */
enum class EAntMoveResult : uint8
{
	NeedUpdate = 0,
	GoalReached,
	VelocityTimedout,
	Canceled
};

/** Path follower types. */
UENUM(BlueprintType)
enum class EAntPathFollowerType : uint8
{
	/** Follow by flow field. */
	FlowField = 0,

	/** Follow by path waypoints. */
	Waypoint
};

/**
 * An agent defines a single mobile unit that can interact with other agents and obstacles in form of an axis aligned vertical cylinder.
*/
struct ANT_API FAntAgentData
{
	friend class UAntSubsystem;
	friend class UAntUtil;
	friend class AAntRecastNavMesh;

public:
	FAntAgentData() :
		bTurnByPreferred(false), bKnocked(false),
		bCanPierce(false), bUseNavigation(true),
		bDisabled(false), bUpdateGrid(true), 
		bCollided(false), bSleep(false),
		bIsOnNavLink(false), bNavLinkUpdated(false)
	{}

	/** Get current agent location in the current px step. */
	FORCEINLINE const FVector3f &GetLocation() const { return Location; }

	/** Get lerped agent location. use this for visual purpose of the agent. if you need Z (height) component of the location you have to set bLandscapeHeight to true. */
	FORCEINLINE const FVector3f &GetLocationLerped() const { return LocationLerped; }

	/** Get current agent velocity. */
	FORCEINLINE const FVector3f &GetVelocity() const { return Velocity; }

	/** Is agent sleep. */
	FORCEINLINE bool IsSleep() const { return bSleep; }

	/** Get data handle. */
	FORCEINLINE FAntHandle GetHandle() const { return Handle; }

	/** Get agent flag. */
	FORCEINLINE int32 GetFlag() const { return Flag; }

	/** Get agent radius. */
	FORCEINLINE float GetRadius() const { return Radius; }

	/** Get agent height. */
	FORCEINLINE float GetHeight() const { return Height; }

	/** Agent is stacked on top of other agents. */
	//FORCEINLINE bool IsStacked() const { return bIsStacked; }

	/** Collision avoidance type. */
	EAntAvoidanceTypes AvoidanceType = EAntAvoidanceTypes::AntDefault;

	/** Ignoring other agents by this flag. */
	int32 IgnoreFlag = 0;

	/** Ignoring other agents but it will wake up ignored agents if a collision happens. */
	int32 IgnoreButWakeUpFlag = 0;

	/** Can step on top of other agents that have this flag. */
	//int32 StackFlag = 0;

	/** Can step on top of other agents with lower priority than this. */
	//uint8 StackPriority = 0;

	/** Maximum velocity used for forcing this agent away when overlapped with other agents. */
	float MaxOverlapForce = 1.0f;

	/** Current preferred velocity. */
	FVector3f PreferredVelocity = FVector3f::ZeroVector;

	/** Turn rate by radian. */
	float TurnRate = 0.05f;

	/** Current face angle in radian and clockwise. */
	float FaceAngle = 0.0f;

	/** Final face angle. agent will interpolate from current FaceAngle to this value by TurnRate. */
	float FinalFaceAngle = 0.0f;

	/** Agent only moves if the difference between FaceAngle and FinalFaceAngle is less than this value. 0 means fully turn-in-place (radian) */
	float MoveAngleThreshold = 0.0f;

	/** Extra query radius effective in RVO/ORCA avoidance. more radius need more proccess. */
	float ExtraQueryRadius = 48.0f;

	/** Max step height for stacking. */
	float MaxStackStepHeight = 105;

	/** Turn angle only changes by the preferred velocity direction */
	uint8 bTurnByPreferred : 1;

	/** Knocked agents will ignore all other agents (not obstacles) at a frame. */
	uint8 bKnocked : 1;

	/** Can this agent pierce trough other agents by PierceThreshold. */
	uint8 bCanPierce : 1;

	/** Use ant navigation system for any movement. 
	 * Navigation guarantees accurate height sampling and obstacle avoidance during pure velocity movement with a little extra cost. */
	uint8 bUseNavigation : 1;

	/** Disbale agent. */
	uint8 bDisabled : 1;

	/** custom user index. */
	int32 UserIndex = INDEX_NONE;

	/** Agent nav query. */
	FSharedConstNavQueryFilter QueryFilterClass;

private:
	/** Data handle. */
	FAntHandle Handle;

	/** Curretn un-stacked location. */
	FVector3f Location;

	/** Current lerped (smooth) location. this value is effective only when we have e different px tickrate than lerp tickrate. */
	FVector3f LocationLerped;

	/** Current Velocity. */
	FVector3f Velocity = FVector3f::ZeroVector;

	/** Total forces from overlapping of the last frame. */
	FVector2f OverlapForce = FVector2f::ZeroVector;

	/** Agent flag. */
	int32 Flag = 1u << 0;

	float LerpAlpha = 0.0f;

	/** Agent radius. */
	float Radius = 40.0f;

	/** Agent height */
	float Height = 1.0f;

	int32 GridHandle = INDEX_NONE;

	/** Navigation node refrenve */
	NavNodeRef NodeRef = INVALID_NAVNODEREF;

	/** Navigation modifier index */
	int32 NavModifierIdx = INDEX_NONE;

	uint8 bUpdateGrid : 1;

	uint8 bCollided : 1;

	/** Is Sleep */
	uint8 bSleep : 1;

	/** Agent is stacked on top of other agents. */
	//uint8 bIsStacked : 1;

	/** Agent is moveing throgh a navigation link and is seperated from navigation mesh. */
	uint8 bIsOnNavLink : 1;

	uint8 bNavLinkUpdated : 1;
};

/**
 * Movement path data.
*/
struct ANT_API FAntPathData
{
	friend class UAntSubsystem;
	friend class UAntUtil;
	friend class AAntRecastNavMesh;

public:
	/** Extra path data. */
	struct Portal : public FNavCorridorPortal
	{
		float Distance = 0.0f;
		enum {Point, Link} Type = Point;
	};

	/** Get data handle. */
	FAntHandle GetHandle() const { return Handle; }

	/** Get owner agent. */
	FAntHandle GetOwner() const { return Owner; }

	/** Path status after replan. */
	EAntPathStatus GetStatus() const { return Status; }

	/** Get path version. (will be changed after replan) */
	uint16 GetVersion() const { return Ver; }

	/** Get path width. */
	float GetPathWidth() const { return Width; }

	/** Get path lenght. */
	float GetPathLenght() const { return !Data.IsEmpty() ? Data[0].Distance : 0.0f; }

	/** Get underlying path data. */
	const TArray<Portal> &GetData() const { return Data; }

	/** Get underlying path data. */
	TArray<Portal> &GetMutableData() { return Data; }

	/** Utility function to find nearest location on the path corridor. */
	FAntCorridorLocation FindNearestLocationOnCorridor(const FVector &NavLocation, int32 StartIndex = 0) const;

	/** Rebuilding distance list for each waypoint according to its distance to the end of the path. it is mandatory for moving with acceleration. */
	void RebuildDistanceList();

	/** Nav query filter to generate this path. */
	FSharedConstNavQueryFilter FilterClass;

	/** Maximum acceptable distance cost while replanning a blocked path (-1 means no threashold). */
	float ReplanCostThreshold = -1.f;

	/** Width of Nav Link Proxies within this path. it is basically shaped like a tube and not flat. */
	float NavLinkWidth = 100;

private:
	/** Path owner. */
	FAntHandle Handle;

	/** Agent owner. only non-shared path has an owner. */
	FAntHandle Owner;

	/** index of the PathDataList. */
	int32 TileNodeIdx = INDEX_NONE;

	/** Path width. */
	float Width = 100;

	/** Path status after replan. */
	EAntPathStatus Status = EAntPathStatus::Ready;

	/** Path version. will be changed after replan. */
	uint16 Ver = 0;

	/** Path data. */
	TArray<Portal> Data;
};

/**
 * Agent's in-progress movement data.
*/
struct ANT_API FAntMovementData
{
	friend class UAntSubsystem;
	friend class UAntUtil;

public:
	/** Get path handle. */
	FAntHandle GetPath() const { return Path; }

	/** Get last path version. */
	uint16 GetLastPathVersion() const { return PathVer; }

	/** Get data handle. */
	FAntHandle GetHandle() const { return Handle; }

	/** Get current followee agent. */
	FAntHandle GetFollowee() const { return Followee; }

	/** Get current virtual accelerated speed. */
	float GetSpeed() const { return Speed; }

	/** Get destination location. */
	const FVector &GetDestinationLocation() const { return Destination; }

	/** Squared target (goal) radius. */
	float TargetRadiusSquared = 1;

	/** Squared path node radius. */
	float PathNodeRadiusSquared = 5.0f;

	/** Timeout threshold for missing velocity. -1 means no timeout. */
	float MissingVelocityTimeout = -1.0;

	/** Maximum movement speed. */
	float MaxSpeed = 0.0f;

	/** Speed acceleration. */
	float Acceleration = 0.0f;

	/** Speed deceleration. */
	float Deceleration = 0.0f;

	/** Start delay. */
	float StartDelay = 0.0f;

	/** How this agent should follow the path. */
	EAntPathFollowerType PathFollowerType = EAntPathFollowerType::Waypoint;

private:

	/** Owner agent. */
	FAntHandle Handle;

	/** Followee agent. */
	FAntHandle Followee;

	/** Current path. */
	FAntHandle Path;

	/** Path index. (can be used as portal index) */
	uint32 PathIndex = 0;

	/** Destination location. */
	FVector Destination;

	/** Sum of the missing velocity from last frame. */
	float MissingVelocitySum = 0;

	/** Current virtual accelerated speed. */
	float Speed = 0.0f;

	/** Final result of the update phase. */
	EAntMoveResult UpdateResult = EAntMoveResult::NeedUpdate;

	/** Current path version */
	uint16 PathVer = 0;
};

/**
 * Async query data.
*/
struct ANT_API FAntQueryData
{
	friend class UAntSubsystem;
	friend class UAntUtil;

public:
	/** Get data handle. */
	FORCEINLINE FAntHandle GetHandle() const { return Handle; }

	/** Get elapsed time. */
	FORCEINLINE float GetElapsedTime() const { return ElapsedTime; }

	/** Get query flag. */
	FORCEINLINE int32 GetFlag() const { return Flag; }

	/** Get query type. */
	FORCEINLINE EAntQueryType GetType() const { return Type; }

	/** Query result array. */
	TArray<FAntContactInfo> Result;

	/** Query execution interval. -1 means one-time (single frame) query. 0 Means each-frame query. */
	float Interval = -1;

private:
	/** Query owner. */
	FAntHandle Handle;

	/** Query flag. */
	int32 Flag = 1u << 0;

	/** Elapsed time. */
	float ElapsedTime = 0.0f;

	/** Querirer centers must be inside the query area. */
	bool MustIncludeCenter = false;

	/** Query type. */
	EAntQueryType Type = EAntQueryType::Cylinder;

	/** Internal query data. */
	union QueryData
	{
		struct { FVector3f Base; float Radius = 0.0f; float Height = 0.0f; } Cylinder;
		struct { FAntHandle Handle; FVector3f Base;  float Radius = 0.0f; float Height = 0.0f; } CylinderAttached;
		struct { FVector3f Start; FVector3f End; } Ray;

		QueryData() {}
		QueryData(const QueryData &Data) { FMemory::Memcpy(this, &Data, sizeof(QueryData)); }
	} Data;
};

UCLASS()
class AAntWorldSettings : public AWorldSettings
{
	GENERATED_BODY()
public:
	/** Size of each cell of the collision grid. */
	UPROPERTY(EditAnywhere, meta = (UIMin = 1.0f), Category = "Ant")
	float CollisionCellSize = 250;

	/** Update tick rate of the collision system. */
	UPROPERTY(EditAnywhere, Category = "Ant")
	float CollisionTickInterval = 0.016f;

	/** Update tick rate of the movements lerping system. */
	UPROPERTY(EditAnywhere, Category = "Ant")
	float LerpTickInterval = 0.016f;

	/** Enable location lerping system. it should be enabled in case of low CollisionTickInterval. */
	UPROPERTY(EditAnywhere, Category = "Ant")
	bool EnableLerp = false;

	/** Number of landscape cells per row / col */
	UPROPERTY(EditAnywhere, meta = (UIMin = 1), Category = "Ant")
	int32 NumCells = 150;

	/**
	* Pierce angle in radian.
	* Avoid stocking multiple agents while trying to move through a tight bottleneck.
	* Agents which blocked with equal or higher than this angle will pierce other agents.
	* Check bCanPierce on the agent.
	*/
	UPROPERTY(EditAnywhere, Category = "Ant")
	float PierceAngle = 2.35619f;

	/** Minimum overlap size to start stacking agents on top of each other (Squared). */
	UPROPERTY(EditAnywhere, Category = "Ant")
	float MinOverlapStackSizeSq = 16.0f;

	/** Gravity of falling stacked agents. */
	//UPROPERTY(EditAnywhere, Category = "Ant")
	//float Gravity = 16.0f;

	/** Extent size to find nearest valid navigation location by agents. */
	UPROPERTY(EditAnywhere, Category = "Ant")
	FVector NavLocationExtent = { 300, 300, 50 };

	/** Time horizon for RVO based agents. */
	UPROPERTY(EditAnywhere, Category = "Ant")
	float RVOTimeHorizon = 5.0f;

	/** Extra debug draw height. */
	UPROPERTY(EditAnywhere, Category = "Ant")
	float DebugDrawHeight = 0;
};

/**
 * Called during collision updating phase.
 * DeltaMul is not a real delta, its just a multiplicative value according to the CollisionTickInterval and elapsed time.
 * Note that its value is always equal or greater than 1.0f.
 * DeltaMul == ElapsedTime / CollisionTickInterval
*/
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPxUpdate, float);

/** General use delegate for different events. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAntEvent, FAntHandle);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAntEvents, const TArray<FAntHandle> &);

/** General use delegate for different events. (BP version) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAntEventsDynamic, const TArray<FAntHandle> &, Handles);

/** Internal use. */
DECLARE_DELEGATE(FOnAntInternalUpdateNav);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAntHandleCompleteSomething);

/**
 * Ant main class.
 * This class handle both high and low-level tasks such as multi-threaded collision and movement processing and act as a building block for other utility clasess.
 */
UCLASS()
class ANT_API UAntSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	friend class UAntFunctionLibrary;
	friend class AAntRecastNavMesh;

public:
	UAntSubsystem();

	void Initialize(FSubsystemCollectionBase &Collection) override;

	void Deinitialize() override;

	void Tick(float Delta) override;

	bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	TStatId GetStatId() const override { return GetStatID(); }

	/**
	 * Add a new agent at the given location.
	 * @param Location Agent location.
	 * @param Radius Agent radius.
	 * @param FaceAngle Agent face angle.
	 * @param Flags Agent flag.
	 * @return Agent handle.
	*/
	FAntHandle AddAgent(const FVector &Location, float Radius, float Height, float FaceAngle, int32 Flags);

	/**
	 * @brief Destroy an agent.
	 * @param Handle Agent handle.
	*/
	void RemoveAgent(FAntHandle Handle);

	/** Clear all agents. */
	void ClearAgents();

	/**
	 * Get agent data.
	 * @param Handle Agent handle
	 * @return Agent data.
	*/
	const FAntAgentData &GetAgentData(FAntHandle Handle) const;

	/**
	 * Get agent data.
	 * @param Handle Agent handle
	 * @return Agent data.
	*/
	FAntAgentData &GetMutableAgentData(FAntHandle Handle);

	/**
	 * Get custom user data that specified to the given agent.
	 * @param Handle Agent handle.
	 * @return Refrence to the data.
	*/
	UFUNCTION(BlueprintCallable)
	FInstancedStruct &GetAgentUserData(FAntHandle Handle);
	

	/**
	 * Set agent location directly (teleport).
	 * If you need to move an agent use MoveAgent() instead.
	 * @param Handle Agent handle.
	 * @param Location Agent new location.
	*/
	void SetAgentLocation(FAntHandle Handle, const FVector &Location);

	/**
	 * Set agent radius
	 * @param Handle Agent handle.
	 * @param Radius Agent new radius.
	*/
	void SetAgentRadius(FAntHandle Handle, float Radius);

	/**
	 * Set agent height
	 * @param Handle Agent handle.
	 * @param Radius Agent new height.
	*/
	void SetAgentHeight(FAntHandle Handle, float Height);

	/**
	 * Set agent flag.
	 * @param Handle Agent handle.
	 * @param Flags Agent new flag.
	*/
	void SetAgentFlag(FAntHandle Handle, int32 Flags);

	/**
	 * Check whether an agnet handle is valid or not.
	 * @param Handle Agent handle
	 * @return True if handle is valid.
	*/
	bool IsValidAgent(FAntHandle Handle) const;

	/**
	 * Check whether a list of handles is valid or not.
	 * @param Handles Handle list.
	 * @return True if all handles inside the list is valid.
	*/
	bool IsValidAgents(const TArray<FAntHandle> Handles) const;

	/** Get total number of the agents. */
	FORCEINLINE int32 GetNumAgents() const { return Agents.Num(); }

	/** Raw Agents array accessor */
	FORCEINLINE const TSparseArray<FAntAgentData> &GetUnderlyingAgentsList() const { return Agents; }
	FORCEINLINE TSparseArray<FAntAgentData> &GetMutableUnderlyingAgentsList() { return Agents; }

	/**
	 * Move an agent through the given path.
	 * MissingVelocity = (Agent.PreferredVelocity - Agent.CurrentVelocity)
	 * if (MissingVelocity.SumOfXY >= MissingVelocityTimeout) Notify(Agent)
	 * Bind to OnAgentReached and OnAgentBlocked for events.
	 * @param AgentHandle Agent handle.
	 * @param PathHandle Path handle.
	 * @param PathFollowerType Path follower methode.
	 * @param MaxSpeed Maximum speed of the agent.
	 * @param Acceleration Speed accelerating. (set it equal to MaxSpeed in case of linear speed)
	 * @param Deceleration Speed deceleration. (set it equal to MaxSpeed or 0 in case of linear speed)
	 * @param TargetAcceptanceRadius Acceptance radius of the target (goal) location.
	 * @param PathAcceptanceRadius Acceptance radius of path nodes inside the path array. it will be checked against center of the agent.
	 * @param MissingVelocityTimeout Velocity time out threshold. -1 means no timeout.
	*/
	void MoveAgentByPath(FAntHandle AgentHandle, FAntHandle PathHandle, EAntPathFollowerType PathFollowerType, float MaxSpeed, float Acceleration, float Deceleration,
		float TargetAcceptanceRadius, float PathAcceptanceRadius, float StartDelay = 0.0f, float MissingVelocityTimeout = -1.0f);

	/**
	 * Follow an agent by another agents.
	 * MissingVelocity = (Agent.PreferredVelocity - Agent.CurrentVelocity)
	 * if (MissingVelocity.SumOfXY >= MissingVelocityTimeout) Notify(Agent)
	 * Bind to OnAgentReached and OnAgentBlocked for events.
	 * @param FollowerHandle Follower agent handle.
	 * @param FolloweeHandle Followee agent handle.
	 * @param MaxSpeed Maximum follow speed.
	 * @param Acceleration Speed accelerating.
	 * @param Deceleration Speed deceleration.
	 * @param FolloweeAcceptanceRadius Acceptance radius of the followee.
	 * @param MissingVelocityTimeout Velocity time out threshold. -1 means no timeout.
	*/
	void FollowAgent(FAntHandle FollowerHandle, FAntHandle FolloweeHandle, float MaxSpeed, float Acceleration, float Deceleration,
		int32 FolloweeAcceptanceRadius, float StartDelay = 0.0f, float MissingVelocityTimeout = -1.0f);

	/**
	 * Cancel any in-progress movements and remove its movements data.
	 * @param Handle Agent handle.
	*/
	void RemoveAgentMovement(FAntHandle Handle, bool ShouldNotify = false);

	/** Get agent movement data */
	const FAntMovementData &GetAgentMovement(FAntHandle Handle) const;

	/** Get agent movement data */
	FAntMovementData &GetMutableAgentMovement(FAntHandle Handle);

	/** Cancel all in-progress movements. */
	void ClearMovements();

	/**
	 * Check whether a movement handle is valid or not.
	 * @param Handle Agent handle.
	 * @return True if the given handle has a movement.
	*/
	bool IsValidMovement(FAntHandle Handle) const;

	/**
	 * Add a new empty path. (must be initialized before use)
	 * @param Owner Shared paths have no owner and are permanent. owned paths will be removed after the movement is is done.
	 * @return
	*/
	FAntHandle AddEmptyPath(float Width, FSharedConstNavQueryFilter QueryFilter, FAntHandle Owner = FAntHandle());

	/**
	 * @brief Destroy a path.
	 * if you remove an in-use path, all corresponding movements will be canceled.
	 * @param Handle Path handle.
	*/
	void RemovePath(FAntHandle Handle);

	/**
	 * Get path data.
	 * @param Handle Path handle
	 * @return Path data.
	*/
	const FAntPathData &GetPathData(FAntHandle Handle) const;

	/**
	 * Get custom user data that specified to the given path.
	 * @param Handle Path handle.
	 * @return Refrence to the data.
	*/
	FInstancedStruct &GetPathUserData(FAntHandle Handle);

	/**
	 * Get path data.
	 * @param Handle Path handle
	 * @return Path data.
	*/
	FAntPathData &GetMutablePathData(FAntHandle Handle);

	/** Get total number of the paths. */
	FORCEINLINE int32 GetNumPaths() const { return Paths.Num(); }

	/** Raw Path array accessor */
	FORCEINLINE const TSparseArray<FAntPathData> &GetUnderlyingPathList() const { return Paths; }

	/**
	 * Check whether a path handle is valid or not.
	 * @param Handle Path handle.
	 * @return True if the given handle is a path.
	*/
	bool IsValidPath(FAntHandle Handle) const;

	/**
	 * Query broadphase grid with a point.
	 * If you have a large number of queries you've better to go with async version.
	 * This function is thread-safe and lock-free.
	 * @param Center Center of the point.
	 * @param Radius Radius of the point.
	 * @param Flags Query flag.
	 * @param QueryResult Result of the query. may contain duplication in case of multiple instance from same owner.
	*/
	void QueryCylinder(const FVector3f &Base, float Radius, float Height, int32 Flags, bool MustIncludeCenter, TArray<FAntContactInfo> &QueryResult) const;

	/**
	 * Async query broadphase grid with a cylinder.
	 * You can have a large number of queries in case of async queries.
	 * Bind to OnQueryFinished for the result (Game thread).
	 * @param Base Base (bottom) of the cylinder.
	 * @param Radius Radius of the cylinder.
	 * @param Height Height of the cylinder.
	 * @param Flags Query flag.
	 * @param Interval Query execution interval. -1 means one-time (single frame) query. 0 Means each-frame query.
	 * @return Query handle.
	*/
	FAntHandle QueryCylinderAsync(const FVector3f &Base, float Radius, float Height, int32 Flags, bool MustIncludeCenter, float Interval = -1.0f);

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
	FAntHandle QueryCylinderAttachedAsync(FAntHandle AgentHandle, float Radius, float Height, int32 Flags, bool MustIncludeCenter, float Interval = -1.0f);

	/**
	 * Query broadphase grid with a ray.
	 * If you have a large number of queries you've better to go with async version.
	 * This function is thread-safe and lock-free.
	 * @param Start Ray start location.
	 * @param End Ray end location.
	 * @param Flags Query flag
	 * @param QueryResult Result of the query. may contain duplication in case of multiple instance from same owner.
	*/
	void QueryRay(const FVector3f &Start, const FVector3f &End, int32 Flags, TArray<FAntContactInfo> &QueryResult) const;

	/**
	 * Async query broadphase grid with a ray.
	 * You can have a large number of queries in case of async queries.
	 * Bind to OnQueryFinished for the result (Game thread).
	 * @param Start Ray start location.
	 * @param End Ray end location.
	 * @param Flags Query flag
	 * @param Interval Query execution interval. -1 means one-time (single frame) query. 0 Means each-frame query.
	 * @return Query handle.
	*/
	FAntHandle QueryRayAsync(const FVector3f &Start, const FVector3f &End, int32 Flags, float Interval = -1.0f);

	/**
	 * Query broadphase grid with a convex volume.
	 * If you have a large number of queries you've better to go with async version.
	 * This function is thread-safe and lock-free.
	 * @param NearPlane Near plane.
	 * @param FarPlane Far plane.
	 * @param Flags Query flag.
	 * @param QueryResult Result of the query. may contain duplication in case of multiple instance from same owner.
	*/
	void QueryConvexVolume(const TStaticArray<FVector, 4> &NearPlane, const TStaticArray<FVector, 4> &FarPlane, int32 Flags, TArray<FAntContactInfo> &QueryResult) const;

	/**
	 * Get async query data.
	 * @param Handle Async query handle.
	 * @return Query data.
	*/
	const FAntQueryData &GetAsyncQueryData(FAntHandle Handle) const;

	/**
	 * Get async query data.
	 * @param Handle Async query handle.
	 * @return Query data.
	*/
	FAntQueryData &GetMutableAsyncQueryData(FAntHandle Handle);

	/**
	 * Get custom user data that specified to the given async query.
	 * @param Handle Async query handle.
	 * @return Refrence to the data.
	*/
	FInstancedStruct &GetAsyncQueryUserData(FAntHandle Handle);

	/**
	 * Remove an async query.
	 * @param Handle Async query handle
	*/
	void RemoveAsyncQuery(FAntHandle Handle);

	/** Clear all async queries. */
	void ClearAsyncQueries();

	/**
	 * Check whether an async query handle is valid or not.
	 * @param Handle Async query handle.
	 * @return True if the given handle is valid.
	*/
	bool IsValidAsyncQuery(FAntHandle Handle);

	/**
	 * Get type of the given handle.
	 * @param Handle Handle.
	 * @return Handle type.
	*/
	EAntHandleTypes GetHandleType(FAntHandle Handle);

	/** Get underlying broad phase grid */
	FORCEINLINE FAntGrid *GetBroadphaseGrid() { return BroadphaseGrid; }

	/** Return true if the given Location is valid */
	bool IsValidLocation(const FVector2f &Location);

	/** Get number of available threads wich can be utilised by Ant */
	FORCEINLINE int32 GetNumAvailThreads() const { return NumAvailThreads; }

	/** Get Ant world settings. */
	FORCEINLINE const AAntWorldSettings *GetSettings() const { return Settings; }

	/** Collision pre-update delegates */
	FOnPxUpdate OnPxPreUpdate;

	/** Collision post-update delegates */
	FOnPxUpdate OnPxPostUpdate;

	/**
	* Will be called whenever an agent reached its target location.
	* it's a deferred event, which means you can do non-read-only operations (e.g. add()/remove()) inside this event without any restriction.
	*/
	FOnAntEvents OnMovementGoalReached;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnMovementGoalReached"), Category = "Ant")
	FOnAntEventsDynamic OnMovementGoalReached_BP;

	/**
	* Will be called whenever an agent reached its missing velocity threshold.
	* it's a deferred event, which means you can do non-read-only operations (e.g. add()/remove()) inside this event without any restriction.
	*/
	FOnAntEvents OnMovementMissingVelocity;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnMovementMissingVelocity"), Category = "Ant")
	FOnAntEventsDynamic OnMovementMissingVelocity_BP;

	/**
	* Will be called whenever an in-progress movement get canceled by user itself or whenever its followee agent is not valid anymore.
	* it's a deferred event, which means you can do non-read-only operations (e.g. add()/remove()) inside this event without any restriction.
	*/
	FOnAntEvents OnMovementCanceled;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnMovementCanceled"), Category = "Ant")
	FOnAntEventsDynamic OnMovementCanceled_BP;

	/**
	* Will be called whenever a query result is ready to use.
	* it's a deferred event, which means you can do non-read-only operations (e.g. add()/remove()) inside this event without any restriction.
	*/
	FOnAntEvents OnQueryFinished;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnQueryFinished"), Category = "Ant")
	FOnAntEventsDynamic OnQueryFinished_BP;

	/**
	* Will be called whenever a path status changed after replan.
	* it's a deferred event, which means you can do non-read-only operations (e.g. add()/remove()) inside this event without any restriction.
	*/
	FOnAntEvents OnPathReplaned;

	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnPathReplaned"), Category = "Ant")
	FOnAntEventsDynamic OnPathReplaned_BP;

	/** Will be called whenever an agent is removed. */
	FOnAntEvent OnAgentRemoved;

	/** Will be called whenever a path is removed. */
	FOnAntEvent OnPathRemoved;

	UPROPERTY(BlueprintCallable, BlueprintAssignable, meta = (DisplayName = "OnAntHandleCompleteSomething"), Category = "Ant")
	FOnAntHandleCompleteSomething OnAntHandleCompleteSomething;

protected:
	void UpdateCollisionsAndQueries(float Delta, bool CollisionCanTick);

	void UpdateFaceDirection(FAntAgentData &CylinderAttached);

	void UpdateMovements(float Delta, float DeltaMul);

	void DispatchAsyncQueries(float Delta);

	/** Solve collisions and find best locations according to the preffered velocity. */
	FVector3f DefaultSolver(FAntAgentData &AgentData);

	/** Solve collisions and find best locations according to the preffered velocity. */
	FVector3f ORCASolver(FAntAgentData &AgentData);

	/**
	* Solves a one-dimensional linear program on a specified line subject to linear constraints defined by lines and a circular constraint.
	* @param Lines Lines defining the linear constraints.
	* @param LineNo The specified line constraint.
	* @param Radius The radius of the circular constraint.
	* @param OptVelocity The optimization velocity.
	* @param DirectionOpt True if the direction should be optimized.
	* @param Result A reference to the result of the linear program.
	* @return True if successful. */
	bool RVOProgram1(const TArray<FAntRay> &Lines, int32 LineNo, float Radius, const FVector2f &OptVelocity, bool DirectionOpt, FVector2f &Result);

	/**
	* Solves a two-dimensional linear program subject to linear
	* @return The number of the line it fails on, and the number of lines if successful. */
	int32 RVOProgram2(const TArray<FAntRay> &Lines, float Radius, const FVector2f &OptVelocity, bool DirectionOpt, FVector2f &Result);

	/**
	* Solves a two-dimensional linear program subject to linear constraints defined by lines and a circular constraint.
	* @param Result A reference to the result of the linear program.. */
	void RVOProgram3(const TArray<FAntRay> &Lines, int32 NumObstLines, int32 BeginLine, float Radius, FVector2f &Result);


	/**
	 * draw debug stuff per frame
	 * 1: points
	 * 2: points + path
	 * 3: circles
	 * 4: circles + path
	*/
	void DebugDraw();

	FAntIndexer<3, static_cast<uint8>(EAntHandleTypes::Agent)> AgentStorage;
	TSparseArray<FAntAgentData> Agents;
	TSparseArray<FAntMovementData> Movements;

	FAntIndexer<2, static_cast<uint8>(EAntHandleTypes::Path)> PathStorage;
	TSparseArray<FAntPathData> Paths;

	FAntIndexer<2, static_cast<uint8>(EAntHandleTypes::AsyncQuery)> QueryStorage;
	TSparseArray<FAntQueryData> Queries;

	TSparseArray<FInstancedStruct> UserData;

	FAntGrid *BroadphaseGrid = nullptr;

	int32 NumAvailThreads = 0;

	float ShiftSize = 0.0f;

	float PxDeltaTime = 0.0f;

	UPROPERTY()
	const AAntWorldSettings *Settings = nullptr;

	/** Move agents on navigation surface */
	FOnAntInternalUpdateNav OnUpdateNav;
};
