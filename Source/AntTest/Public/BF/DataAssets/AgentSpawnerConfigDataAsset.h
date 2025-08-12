/**
 * @file AgentSpawnerConfigDataAsset.h
 * @brief 代理生成器配置数据资产头文件
 * @details 定义了代理生成器的配置数据资产类，存储将领和士兵的配置信息
 *          包括代理配置、士兵配置、特征值和投射物配置等
 * @author AntTest Team
 * @date 2024
 */

#pragma once

#include "Traits/ActorSpawnConfig.h"
#include "AntTypes/AntStructs.h"
#include "Engine/DataAsset.h"
#include "AgentSpawnerConfigDataAsset.generated.h"

// 前向声明
class UAgentConfigDataAsset;

/**
 * @brief 代理生成器配置数据资产类
 * @details 该类继承自UPrimaryDataAsset，用于存储代理生成器的各种配置数据
 *          包括将领和士兵的配置、特征值、投射物配置等
 */
UCLASS(BlueprintType)
class UAgentSpawnerConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** @brief 将领代理配置数据资产，存储将领相关的配置信息 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ToolTip = "AgentConfigDataAsset"))
	TSoftObjectPtr<UAgentConfigDataAsset> GeneralAgentConfigDataAsset;
	
	/** @brief 士兵代理配置数据资产，存储士兵相关的配置信息 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ToolTip = "AgentConfigDataAsset"))
	TSoftObjectPtr<UAgentConfigDataAsset> SoldierAgentConfigDataAsset;
	
	/** @brief 士兵配置映射表，根据士兵类型存储对应的生成器数据 */
	UPROPERTY(EditDefaultsOnly, meta = (ToolTip = "SoldierConfig"))
	TMap<ESoldierType, FAgentSpawnerData> SoldierConfig;
	
	/** @brief 近战代理特征值，定义近战单位的各种特征参数 */
	UPROPERTY(EditDefaultsOnly, meta = (ToolTip = "Value"))
	FAgentTraitValue NearAgentTraitValue;

	/** @brief 远程代理特征值，定义远程单位的各种特征参数 */
	UPROPERTY(EditDefaultsOnly, meta = (ToolTip = "Value"))
	FAgentTraitValue RangedAgentTraitValue;

	/** @brief 投射物配置数组，存储各种投射物的配置信息 */
	UPROPERTY(EditAnywhere, meta = (ToolTip = "Projectile"))
	TArray<TSoftObjectPtr<UProjectileConfigDataAsset>> ProjectileConfig;

	/** @brief 生成Actor配置数组，存储攻击相关的Actor生成配置 */
	UPROPERTY(EditDefaultsOnly, meta = (ToolTip = "Spawn"))
	TArray<FActorSpawnConfig_Attack> SpawnActor;
};