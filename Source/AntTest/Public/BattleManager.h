/**
 * @file BattleManager.h
 * @brief 战斗管理器头文件
 * @details 定义了基于四叉树的战斗管理系统，用于高效地管理RTS游戏中的单位位置和战斗逻辑
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Quadtree.h"
#include "GameFramework/Actor.h"
#include "BattleManager.generated.h"

// 前向声明
class UAntSubsystem;

/**
 * @brief 查询目标委托
 * @param CurrentSelf 当前单位句柄
 * @param Target 目标单位句柄
 * @details 当单位找到目标时触发的委托
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQueryTarget, FAntHandle, CurrentSelf, FAntHandle, Target);

/**
 * @brief Ant死亡委托
 * @param AntHandle 死亡的Ant句柄
 * @details 当Ant单位死亡时触发的委托
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAntDeath, FAntHandle, AntHandle);

/**
 * @brief 四叉树管理器类
 * @details 该类继承自AActor，使用四叉树数据结构来高效管理RTS游戏中的单位位置
 *          支持快速查找最近敌人、单位分组和战斗逻辑处理
 */
UCLASS()
class ANTTEST_API AQuadTreeManager : public AActor
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details 初始化四叉树管理器的基本参数和区域设置
	 */
	AQuadTreeManager();

protected:
	/**
	 * @brief 游戏开始时调用
	 * @details 获取Ant子系统并初始化四叉树
	 */
	virtual void BeginPlay() override;

public:
	/**
	 * @brief 每帧更新函数
	 * @param DeltaTime 帧间隔时间
	 * @details 更新四叉树并处理单位间的战斗逻辑
	 */
	virtual void Tick(float DeltaTime) override;

	/** @brief 四叉树根节点，管理整个空间分割结构 */
	TSharedPtr<QuadTreeNode> RootNode;

	/** @brief 四叉树区域中心点，定义管理区域的中心位置 */
	FVector2D RegionCenter;
	
	/** @brief 四叉树区域半尺寸，定义管理区域的大小 */
	FVector2D RegionHalfSize;

	/** @brief 节点容量阈值，超过此数量后会细分节点 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QuadTree")
	int NodeCapacity = 4;

	/** @brief 存储所有单位及其队伍ID的映射表 */
	TMap<FAntHandle*, int32> UnitTeams;

	/**
	 * @brief 初始化或清空四叉树
	 * @details 在BeginPlay中调用，创建四叉树的根节点
	 */
	void InitializeQuadTree();

	/**
	 * @brief 添加单位到管理器和四叉树（蓝图可调用）
	 * @param Unit 要添加的单位句柄
	 * @param Team 单位所属的队伍ID
	 * @details 为蓝图提供添加单位的接口
	 */
	UFUNCTION(BlueprintCallable, Category="QuadTree", DisplayName = "AddUnit")
	void BP_AddUnit(FAntHandle Unit, int32 Team);
	
	/**
	 * @brief 添加单位到管理器和四叉树
	 * @param Unit 要添加的单位句柄引用
	 * @param Team 单位所属的队伍ID
	 * @details 将单位添加到单位队伍映射表和四叉树中
	 */
	void AddUnit(FAntHandle& Unit, int32 Team);

	/**
	 * @brief 从管理器和四叉树移除单位
	 * @param Unit 要移除的单位句柄指针
	 * @details 从单位队伍映射表和四叉树中移除指定单位
	 */
	void RemoveUnit(FAntHandle* Unit);

	/**
	 * @brief 查找给定单位的最近敌人
	 * @param Unit 要查找敌人的单位句柄指针
	 * @return 返回最近敌人的句柄指针，如果未找到则返回nullptr
	 * @details 使用四叉树高效查找最近的敌对单位，忽略同队单位
	 */
	FAntHandle* FindNearestEnemy(FAntHandle* Unit);

private:
	/**
	 * @brief 每帧更新四叉树
	 * @details 重建四叉树结构，重新插入所有单位以反映位置变化
	 */
	void UpdateQuadTree();

	/** @brief Ant子系统引用，用于访问Ant相关功能 */
	UPROPERTY()
	UAntSubsystem* AntSubsystem;
};
