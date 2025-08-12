/**
 * @file AntTest.h
 * @brief AntTest模块主头文件
 * @details 该文件定义了AntTest模块的主要结构体和类，包括单位数据结构和Ant辅助函数库
 *          提供将Ant代理变换复制到Skelot组件的功能
 * @author AntTest Team
 * @date 2024
 */

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AntSubsystem.h"
#include "SkelotComponent.h"
#include "AntTest.generated.h"

/**
 * @brief 单位数据结构体
 * @details 存储单位的基本属性数据，包括血量、攻击力、动画等
 */
USTRUCT(BlueprintType)
struct FUnitData
{
	GENERATED_BODY()

	/** @brief 最大生命值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxHealth = 0;
	
	/** @brief 当前生命值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Health = 0;
	
	/** @brief 攻击伤害值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AttackDamage = 0;

	/** @brief 攻击冷却时间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackCooldown = 0.f;

	/** @brief 攻击范围 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackRange = 200.f;

	/** @brief 上次攻击时间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LastAttackTime = 0.f;

	/** @brief Skelot实例ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SkelotInstanceID = 0;

        /** @brief 队伍ID */
        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 TeamID = 0;

        /** @brief 组ID */
        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 GroupID = 0;

	/** @brief 当前目标代理句柄 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAntHandle CurrentTarget;
	
	/** @brief 行走动画序列 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimSequenceBase *WalkAnim = nullptr;

	/** @brief 待机动画序列 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimSequenceBase *IdleAnim = nullptr;

	/** @brief 攻击动画序列 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimSequenceBase *AttackAnim = nullptr;

	/** @brief 欢呼动画序列 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimSequenceBase *CheerAnim = nullptr;

	/** @brief 是否正在攻击的标志 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsAttack = false;

	/** @brief 是否已死亡的标志 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsDeath = false;

	/** @brief 是否播放欢呼动画的标志 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsPlayCheerAnim = false;

	/** @brief 单位ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ID = -1;

	/** @brief 移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Speed = 0.f;
	
	/** @brief Skelot组件引用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USkelotComponent* SkelotComponent = nullptr;
};

/**
 * @brief Ant辅助函数库类
 * @details 该类是Ant的高级安全版本，用于在蓝图中使用
 *          提供将Ant代理变换复制到Skelot组件的功能
 */
UCLASS()
class UAntHelperFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * @brief 将敌人变换复制到Skelot组件
	 * @param WorldContextObject 世界上下文对象
	 * @param SkelotCom Skelot组件
	 * @param Flags 标志位，用于过滤代理
	 * @details 将指定标志位的Ant代理的变换信息复制到Skelot组件中
	 */
	UFUNCTION(BlueprintCallable, Category = "Ant Helper", meta = (WorldContext = "WorldContextObject"))
	static void CopyEnemyTranformsToSkelot(const UObject* WorldContextObject, USkelotComponent *SkelotCom, UPARAM(meta = (Bitmask, BitmaskEnum = EAntGroups)) int32 Flags);

	/**
	 * @brief 将英雄变换复制到Skelot组件
	 * @param WorldContextObject 世界上下文对象
	 * @param SkelotCom Skelot组件
	 * @param HeroHandle 英雄代理句柄
	 * @details 将指定英雄代理的变换信息复制到Skelot组件中
	 */
	UFUNCTION(BlueprintCallable, Category = "Ant Helper", meta = (WorldContext = "WorldContextObject"))
	static void CopyHeroTransformToSkelot(const UObject* WorldContextObject, USkelotComponent *SkelotCom, FAntHandle HeroHandle);

	/**
	 * @brief 将敌人变换复制到Skelot组件（新版本）
	 * @param WorldContextObject 世界上下文对象
	 * @param SkelotCom Skelot组件
	 * @param Handle 代理句柄数组
	 * @details 将指定代理句柄数组中的代理变换信息复制到Skelot组件中
	 */
	UFUNCTION(BlueprintCallable, Category = "Ant Helper", meta = (WorldContext = "WorldContextObject"))
	static void CopyEnemyTranformsToSkelot_New(const UObject* WorldContextObject, USkelotComponent *SkelotCom, TArray<FAntHandle> Handle);
};