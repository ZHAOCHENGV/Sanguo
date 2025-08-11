// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "AntSubsystem.h"
#include "Engine/EngineTypes.h"
#include "NavFilters/NavigationQueryFilter.h"
#include <type_traits>

#define CHECK_BIT_ONLY(var,pos) ((AntInternal::Enum_Cast(var) & (1llu << (pos))) != 0)
#define CHECK_BIT_ANY(var,mask) ((AntInternal::Enum_Cast(var) & AntInternal::Enum_Cast(mask)) != 0)
#define CHECK_BIT_ALL(var,mask) ((AntInternal::Enum_Cast(var) & AntInternal::Enum_Cast(mask)) == AntInternal::Enum_Cast(mask))

class APlayerController;
class USplineComponent;
class ANavigationData;

class ANT_API UAntUtil
{
public:
	/* Move given agents to the give locations.
	* Fully parallel in case of multiple agents.
	* note: MaxSpeeds, Accelerations and Decelerations can contain only 1 element. In this case, it will be applied to all agents.
	* @return return true only if all agents inside the given array move successfully, otherwise it return false.
	*/
	static bool MoveAgentsToLocations(UWorld *World, const TArray<FAntHandle> &AgentsToMove, const TArray<FVector> &Locations, 
		const TArray<float> &MaxSpeeds, const TArray<float> &Accelerations, const TArray<float> &Decelerations,
		float TargetAcceptanceRadius, float PathAcceptanceRadius, EAntPathFollowerType PathFollowerType, float PathWidth,
		float MissingVelocityTimeout = -1.0f, bool PathReplan = false, float ReplanCostThreshold = -1.f, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/* Create a permanent shared path by given locations.
	* Returned path is permanent until it get removed by calling Ant->RemovePath().
	* @return return a valid path handle if it found a valid path.
	*/
	static FAntHandle CreateSharedPath(UWorld *World, const FVector &Start, const FVector &End, float PathWidth, bool PathReplan = false, float ReplanCostThreshold = -1.f, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/* Create a permanent shared path by given spline.
	* Returned path is permanent until it get removed by calling Ant->RemovePath().
	* @return return a valid path handle if it found a valid path.
	*/
	static FAntHandle CreateSharedPathBySpline(UWorld *World, const USplineComponent *Spline, float PathWidth, bool PathReplan = false, float ReplanCostThreshold = -1.f, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/**
	 * Select agents by the given area on the screen.
	 * @param PC Current player controller
	 * @param FarClipDistance Distance to the far clip plane
	 * @param FloorChannel Collsion channel of the landscape.
	 * @param Flags Query flag
	 * @param ResultUnits Result of the query
	*/
	static void QueryOnScreenAgents(APlayerController *PC, const FBox2f &ScreenArea, float FarClipDistance, ECollisionChannel FloorChannel, int32 Flags, TArray<FAntHandle> &ResultUnits);

	/** Convert a linear path to a curved spline path */
	static void ConvertLinearPathToCurvePath(TArray<FVector> &PathToConvert, float MaxSquareDistanceFromSpline);

	/** Convert a linear path to a curved spline path */
	static void ConvertLinearPathToCurvePath(TArray<FNavPathPoint> &Path, float MaxSquareDistanceFromSpline);

	/** Convert a linear path to a curved spline path */
	static void ConvertLinearPathToCurvePath(const TArray<FNavPathPoint> &Path, float MaxSquareDistanceFromSpline, TArray<FVector> &ResultPath);

	/**
	 * Sort and formation given agnets by a square. it works best when agents have same radius.
	 * @param World Current active world.
	 * @param DestLocation Destination location (center of the quad).
	 * @param SplitSpace Space between each agent
	*/
	static void SortAgentsBySquare(UWorld *World, const FVector &DestLocation, TArray<FAntHandle> &Agents, float SplitSpace, float &SquareDimension, TArray<FVector> &ResultLocations);

	/**
	 * Sort given agnets by their current location and according to the center of the crowd.
	 * @param World Current active world.
	 * @param DestLocation Destination location (center of the crowd).
	*/
	static void SortAgentsByCurrentLocation(UWorld *World, const FVector &DestLocation, const TArray<FAntHandle> &Agents, TArray<FVector> &ResultLocations);

	static void SortAgentsByCirclePack(UWorld *World, const FVector &DestLocation, TArray<FAntHandle> &Agents, float SplitSpace, TArray<FVector> &ResultLocations);

	/** Get lerp alpha used by the path portals */
	static void GetCorridorPortalAlpha(const TArray<FVector> &SourceLocations, const FVector &DestLocation, TArray<float> &ResultAlpha);

	/** Add or update existing agent navigation modifier. */
	static void UpdateAgentNavModifier(UWorld *World, FAntHandle Agent, TSubclassOf<UNavAreaBase> AreaClass = nullptr);

	/** Remove agent navigation modifier. */
	static void RemoveAgentNavModifier(UWorld *World, FAntHandle Agent);
};

/** internal Ant utilities */
namespace AntInternal
{
	/** cast enums to its underlying type */
	template<typename E>
	constexpr auto Enum_Cast(E e) -> typename std::underlying_type<E>::type
	{
		return static_cast<typename std::underlying_type<E>::type>(e);
	}

	constexpr auto Enum_Cast(unsigned long int e) { return e; }
	constexpr auto Enum_Cast(unsigned int e) { return e; }
	constexpr auto Enum_Cast(unsigned short e) { return e; }
	constexpr auto Enum_Cast(unsigned char e) { return e; }
	constexpr auto Enum_Cast(int e) { return e; }

	/** find idex of the given element from given offset */
	template <typename T> int IndexOfElement(const TArray<T> &Array, const T &Key, int Start = 0)
	{
		for (int i = Start; i < Array.Num(); ++i)
			if (Array[i] == Key)
				return i;

		return INDEX_NONE;
	}
}