// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "AntSubsystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavigationElement.h"
#include "NavAreas/NavArea.h"
#include "AntRecastNavMesh.generated.h"

UCLASS()
class ANT_API UAntNavRelevant : public UObject, public INavRelevantInterface
{
	GENERATED_BODY()

public:
	void GetNavigationData(FNavigationRelevantData &Data) const override;

	FBox GetNavigationBounds() const override;

	TSubclassOf<UNavAreaBase> AreaClass;
	FAntHandle AntAgent;
	float Radius = 0.0f;
	float Height = 0.0f;
	FVector3f Location;
	FNavigationElementHandle ElementHandle;
};

UCLASS()
class ANT_API AAntRecastNavMesh : public ARecastNavMesh
{
	GENERATED_BODY()

public:
	/** Track tile changes. */
	virtual void OnNavMeshTilesUpdated(const TArray<FNavTileRef>& ChangedTiles) override;

	/** Add a navigation modifier volume according to the givn agent location and size.
	 * If called multiple time on the same agent, it will update current modifier volume.
	 */
	void AddAgentNavModifier(FAntHandle AgentHandle, TSubclassOf<UNavAreaBase> AreaClass = nullptr);

	/** Remove navigation modifier related to the given agent. */
	void RemoveAgentNavModifier(FAntHandle AgentHandle);

	/** Add a path to replan list. */
	void AddPathToReplanList(FAntHandle PathHandle, const FNavigationPath *Path);

	/** Get kandscape height at the given location. */
	bool GetHeightAt(const FVector &NavLocation, const FNavigationQueryFilter &QueryFilter, const FVector &Extent, FVector &Result) const;

	/** Thread-safe find path */
	ENavigationQueryResult::Type AntFindPath(const FVector& StartLoc, const FVector& EndLoc, const FVector::FReal CostLimit, bool bRequireNavigableEndLocation, FNavMeshPath& Path,
		const FNavigationQueryFilter& InQueryFilter) const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginPlay() override;

private:
	struct FTileData
	{
		FAntHandle Path;
		uint32 TileIDX = MAX_uint32;
		int32 NextIdx = INDEX_NONE;
		int32 PrevIdx = INDEX_NONE;
		int32 NextPathNodeIdx = INDEX_NONE;
	};

	struct FHeightData
	{
		float Height = 0.0f;
		int32 NextIdx = INDEX_NONE;
	};

	void UpdateAntPaths(const TArray<uint32> &TileIds);

	void OnAgentRemoved(FAntHandle Agent);

	void OnPathRemoved(FAntHandle Path);

	void OnAntPxPostUpdate();

	/** data inside each tile */
	TSparseArray<FTileData> TileData;

	/** tile ids */
	TArray<int32> TileGrid;

	/** updated tiles */
	TArray<uint32> UpdatedTiles;

	/** Nav relevant list */
	UPROPERTY()
	TArray<UAntNavRelevant *> NavModifiers;
};
