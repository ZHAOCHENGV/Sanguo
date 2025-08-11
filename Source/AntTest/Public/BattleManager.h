// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Quadtree.h"
#include "GameFramework/Actor.h"
#include "BattleManager.generated.h"

class UAntSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQueryTarget, FAntHandle, CurrentSelf, FAntHandle, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAntDeath, FAntHandle, AntHandle);

UCLASS()
class ANTTEST_API AQuadTreeManager : public AActor
{
	GENERATED_BODY()

public:
	AQuadTreeManager();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	// 四叉树根节点
	TSharedPtr<QuadTreeNode> RootNode;

	// 四叉树区域中心和半尺寸
	FVector2D RegionCenter;
	FVector2D RegionHalfSize;

	// 节点容量阈值（超过后会细分）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="QuadTree")
	int NodeCapacity = 4;

	// 存储所有单位及其队伍ID
	//UPROPERTY()
	TMap<FAntHandle*, int32> UnitTeams;

	// 初始化或清空四叉树（在BeginPlay中调用）
	void InitializeQuadTree();

	// 添加单位到管理器和四叉树
	UFUNCTION(BlueprintCallable, Category="QuadTree", DisplayName = "AddUnit")
	void BP_AddUnit(FAntHandle Unit, int32 Team);
	
	void AddUnit(FAntHandle& Unit, int32 Team);

	// 从管理器和四叉树移除单位
	//UFUNCTION(BlueprintCallable, Category="QuadTree")
	void RemoveUnit(FAntHandle* Unit);

	// 查找给定单位的最近敌人（忽略同队）
	//UFUNCTION(BlueprintCallable, Category="QuadTree")
	FAntHandle* FindNearestEnemy(FAntHandle* Unit);

private:
	// 每帧更新四叉树（此示例中简单重建）
	void UpdateQuadTree();

	UPROPERTY()
	UAntSubsystem* AntSubsystem;
};
