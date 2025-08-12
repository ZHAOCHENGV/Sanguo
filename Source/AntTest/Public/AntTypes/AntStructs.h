/**
 * @file AntStructs.h
 * @brief Ant系统结构体定义头文件
 * @details 该文件定义了AntTest项目中使用的各种数据结构，包括骨骼数据、代理数据、单位数据等
 * @author AntTest Team
 * @date 2024
 */

#pragma once
#include "CoreMinimal.h"
#include "AntEnums.h"
#include "BattleFrameStructs.h"
#include "AntStructs.generated.h"

// 前向声明
class ANiagaraSubjectRenderer;
class UAnimToTextureDataAsset;
class USkelotAnimCollection;

/**
 * @brief Ant骨骼数据结构体
 * @details 存储骨骼动画相关的数据，包括动画集合、网格、材质和动画序列
 */
USTRUCT(BlueprintType)
struct FAntSkelotData : public FTableRowBase
{
	GENERATED_BODY()

	/** @brief 动画集合，包含所有相关的动画数据 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkelotAnimCollection* AnimCollection;

	/** @brief 骨骼网格，定义单位的3D模型 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkeletalMesh* SkeletalMesh;

	/** @brief 红色阵营材质，用于区分不同阵营 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UMaterialInterface* RedMaterial;

	/** @brief 蓝色阵营材质，用于区分不同阵营 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UMaterialInterface* BlueMaterial;

	/** @brief 待机动画序列 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequenceBase* IdleAnim;

	/** @brief 跑步动画序列 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequenceBase* RunAnim;

	/** @brief 欢呼动画序列 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequenceBase* CheerAnim;
};

/**
 * @brief 代理生成器数据结构体
 * @details 存储代理生成相关的配置数据
 */
USTRUCT(BlueprintType)
struct FAgentSpawnerData
{
	GENERATED_BODY()

	/** @brief 动画到纹理数据资产，用于动画纹理渲染 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UAnimToTextureDataAsset> AnimToTextureDataAsset = nullptr;

	/** @brief 渲染器类，用于渲染代理单位 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<ANiagaraSubjectRenderer> RendererClass = nullptr;

	/** @brief 代理攻击类型，决定攻击方式 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EAgentAttackType AgentAttackType = EAgentAttackType::Near;
};

/**
 * @brief 代理特征值结构体
 * @details 存储代理单位的各种特征参数，如攻击范围、追踪半径等
 */
USTRUCT(BlueprintType)
struct FAgentTraitValue
{
	GENERATED_BODY()

	/** @brief 攻击范围，单位可以攻击的最大距离 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AttackRange = 0.f;
	
	/** @brief 追踪半径，单位开始追踪敌人的距离 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ChaseRadius = 0.f;

	/** @brief 追踪移动速度倍数，追踪时的速度倍率 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ChaseMoveSpeedMult = 0.f;

	/** @brief 追踪半径，用于检测敌人的范围 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float TraceRadius = 0.f;

	/** @brief 命中范围，实际攻击的有效范围 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float HitRange = 0.f;

	/** @brief 追踪频率，追踪检测的时间间隔 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float TraceFrequency = 0.5f;

	/** @brief 追踪高度，追踪检测的高度范围 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float TraceHeight = 300.f;
};

/**
 * @brief 将领数据结构体
 * @details 存储将领单位的特殊属性和动画数据
 */
USTRUCT(BlueprintType)
struct FGeneralData
{
	GENERATED_BODY()

	/** @brief 生命值，将领的最大生命值 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Health = 500.f;

	/** @brief 伤害值，将领的攻击伤害 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Damage = 100.f;

	/** @brief 攻击动画蒙太奇 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* AttackAnimMontage = nullptr;

	/** @brief 技能动画蒙太奇 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* SkillAnimMontage = nullptr;

	/** @brief 上马动画蒙太奇 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* MountAnimMontage = nullptr;
};

/**
 * @brief 将领特征结构体
 * @details 用于标识将领类型的特征标记
 */
USTRUCT(BlueprintType)
struct FGeneral
{
	GENERATED_BODY()
	
};

/**
 * @brief 已检测特征结构体
 * @details 用于标识已被检测到的单位特征标记
 */
USTRUCT(BlueprintType)
struct FDetected
{
	GENERATED_BODY()
	
};

/**
 * @brief 唯一ID结构体
 * @details 用于标识单位的唯一ID，包含队伍ID、组ID和单位ID
 */
USTRUCT(BlueprintType)
struct FUniqueID
{
	GENERATED_BODY()

	/** @brief 队伍ID，标识单位所属的队伍 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TeamID = INDEX_NONE;

	/** @brief 组ID，标识单位所属的组 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 GroupID = INDEX_NONE;

	/** @brief 单位ID，标识具体的单位 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 UnitID = INDEX_NONE;

	/**
	 * @brief 相等比较运算符
	 * @param Other 要比较的另一个FUniqueID
	 * @return 如果所有ID都相等返回true，否则返回false
	 */
	bool operator==(const FUniqueID& Other) const
	{
		return TeamID == Other.TeamID && GroupID == Other.GroupID && UnitID == Other.UnitID;
	}

	/**
	 * @brief 获取哈希值的友元函数
	 * @param Key 要计算哈希值的FUniqueID
	 * @return 返回计算出的哈希值
	 * @details 用于在容器中作为键值使用
	 */
	friend uint32 GetTypeHash(const FUniqueID& Key)
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(Key.TeamID));
		Hash = HashCombine(Hash, GetTypeHash(Key.GroupID));
		Hash = HashCombine(Hash, GetTypeHash(Key.UnitID));
		return Hash;
	}
};

/**
 * @brief 发射子弹结构体
 * @details 存储发射子弹的相关数据
 */
USTRUCT(BlueprintType)
struct FFireBullet
{
	GENERATED_BODY()

	/** @brief 发射位置，子弹的起始位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;
};

/**
 * @brief 目标追踪结果结构体
 * @details 存储目标追踪的相关数据和结果
 */
USTRUCT(BlueprintType)
struct FGoalTraceResult
{
	GENERATED_BODY()

	/** @brief 目标位置，追踪的目标位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	/** @brief 追踪结果数组，存储所有追踪检测的结果 */
	TArray<FTraceResult> TraceResult;
};
