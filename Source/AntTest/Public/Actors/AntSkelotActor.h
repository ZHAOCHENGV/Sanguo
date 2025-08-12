/**
 * @file AntSkelotActor.h
 * @brief 骨骼动画角色头文件
 * @details 定义了基于Skelot系统的骨骼动画角色类，用于在RTS游戏中生成和管理士兵单位
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AntTypes/AntEnums.h"
#include "AntTypes/AntStructs.h"
#include "AntHandle.h"
#include "GameFramework/Actor.h"
#include "AntSkelotActor.generated.h"

// 前向声明
class USkelotComponent;
class USkelotAnimCollection;

/**
 * @brief 骨骼动画角色类
 * @details 该类继承自AActor，用于在RTS游戏中生成和管理基于Skelot系统的士兵单位
 *          支持多种士兵类型，可以批量生成带有骨骼动画的单位
 */
UCLASS()
class ANTTEST_API AAntSkelotActor : public AActor
{
	GENERATED_BODY()
	
public:	
	/**
	 * @brief 构造函数
	 * @details 初始化骨骼动画角色的组件和属性
	 */
	AAntSkelotActor();

protected:
	/**
	 * @brief 游戏开始时调用
	 * @details 初始化骨骼数据并设置生成计时器
	 */
	virtual void BeginPlay() override;
	
	/**
	 * @brief 每帧更新函数
	 * @param DeltaTime 帧间隔时间
	 * @details 更新骨骼动画组件的变换数据
	 */
	virtual void Tick(float DeltaTime) override;

private:
	/**
	 * @brief 获取指定士兵类型的骨骼数据
	 * @param SoldierDataType 士兵类型枚举
	 * @return 返回对应的骨骼数据指针，如果未找到则返回nullptr
	 * @details 从数据表中查找指定士兵类型的骨骼动画配置数据
	 */
	FAntSkelotData* GetAntSkelotData(const ESoldierType& SoldierDataType);

	/**
	 * @brief 初始化骨骼数据
	 * @details 根据士兵类型设置骨骼组件的动画集合、网格和材质
	 */
	void InitSkelotData();

	/**
	 * @brief 生成Ant句柄
	 * @details 在导航系统中随机位置生成指定数量的士兵单位
	 */
	void SpawnAntHandle();
	
	/** @brief 存储所有生成的士兵Ant句柄 */
	TArray<FAntHandle> SoldierAntHandles;

	/** @brief 根组件，作为所有子组件的父级 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skelot", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	/** @brief 士兵骨骼动画组件，用于渲染和管理骨骼动画实例 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skelot", meta = (AllowPrivateAccess = "true"))
	USkelotComponent* SoldierSkelotComponent;

	/** @brief 骨骼广告牌组件，用于在编辑器中显示标识 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skelot", meta = (AllowPrivateAccess = "true"))
	UBillboardComponent* SkelotBillboardComponent;

	/** @brief 骨骼数据表，包含不同士兵类型的配置信息 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	UDataTable* AntSkelotDataTable;
	
	/** @brief 当前士兵类型，决定使用哪种骨骼动画配置 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	ESoldierType SoldierType = ESoldierType::DaoBing;

	/** @brief 生成士兵的数量 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	int32 SpawnCount = 100;

	/** @brief Ant代理的标志位，用于标识和控制代理行为 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	int32 Flags = 1;

	/** @brief 是否使用蓝色阵营材质 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	bool bIsUesBlue;

	/** @brief 士兵移动速度 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	float AntSpeed = 15.f;

	/** @brief 延迟生成计时器句柄 */
	FTimerHandle StartSpawnTimerHandle;

	/** @brief 是否正在播放欢呼动画的标志 */
	bool bIsPlayCheerAnim = false;
};
