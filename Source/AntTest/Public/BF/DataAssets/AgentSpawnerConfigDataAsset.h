#pragma once

#include "Traits/ActorSpawnConfig.h"
#include "AntTypes/AntStructs.h"
#include "Engine/DataAsset.h"
#include "AgentSpawnerConfigDataAsset.generated.h"


class UAgentConfigDataAsset;

UCLASS(BlueprintType)
class UAgentSpawnerConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ToolTip = "AgentConfigDataAsset"))
	TSoftObjectPtr<UAgentConfigDataAsset> GeneralAgentConfigDataAsset;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ToolTip = "AgentConfigDataAsset"))
	TSoftObjectPtr<UAgentConfigDataAsset> SoldierAgentConfigDataAsset;
	
	UPROPERTY(EditDefaultsOnly, meta = (ToolTip = "SoldierConfig"))
	TMap<ESoldierType, FAgentSpawnerData> SoldierConfig;
	
	UPROPERTY(EditDefaultsOnly, meta = (ToolTip = "Value"))
	FAgentTraitValue NearAgentTraitValue;

	UPROPERTY(EditDefaultsOnly, meta = (ToolTip = "Value"))
	FAgentTraitValue RangedAgentTraitValue;

	UPROPERTY(EditAnywhere, meta = (ToolTip = "Projectile"))
	TArray<TSoftObjectPtr<UProjectileConfigDataAsset>> ProjectileConfig;

	UPROPERTY(EditDefaultsOnly, meta = (ToolTip = "Spawn"))
	TArray<FActorSpawnConfig_Attack> SpawnActor;
};