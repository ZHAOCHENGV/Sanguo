/**
 * @file BattleManager.cpp
 * @brief 战斗管理器实现文件
 * @details 该文件实现了基于四叉树的战斗管理系统，用于高效地管理RTS游戏中的单位位置和战斗逻辑
 * @author AntTest Team
 * @date 2024
 */

#include "BattleManager.h"

#include "AntTest/AntTest.h"
#include "Customizations/MathStructProxyCustomizations.h"

/**
 * @brief 构造函数
 * @details 初始化四叉树管理器的基本参数和区域设置
 */
AQuadTreeManager::AQuadTreeManager()
{
	// 启用Actor的Tick功能
	PrimaryActorTick.bCanEverTick = true;
	// 默认区域设为原点附近，可在关卡中修改
	RegionCenter = FVector2D(0.0f, 0.0f);
	RegionHalfSize = FVector2D(1000.0f, 1000.0f);
}

/**
 * @brief 游戏开始时调用
 * @details 获取Ant子系统并初始化四叉树
 */
void AQuadTreeManager::BeginPlay()
{
	Super::BeginPlay();
	// 获取Ant子系统
	if (!AntSubsystem)
	{
		AntSubsystem = GetWorld()->GetSubsystem<UAntSubsystem>();
	}
	// 初始化四叉树
	InitializeQuadTree();
}

/**
 * @brief 每帧更新函数
 * @param DeltaTime 帧间隔时间
 * @details 更新四叉树并处理单位间的战斗逻辑
 */
void AQuadTreeManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// 更新四叉树结构
	UpdateQuadTree();

	// 示例：每帧对所有单位查找最近敌人
	for (auto& Pair : UnitTeams)
	{
		FAntHandle* UnitActor = Pair.Key;
		// 检查单位是否有效
		if (UAntFunctionLibrary::IsValidAgent(GetWorld(), *UnitActor)) continue;
		
		// 查找最近的敌人
		if (FAntHandle* Enemy = FindNearestEnemy(UnitActor))
		{
			// 获取敌人位置
			FVector3f Location;
			UAntFunctionLibrary::GetAgentLocation(GetWorld(), *Enemy, Location);
			
			// 获取敌人和当前单位的单位数据
			FUnitData& EnemyUnitData = AntSubsystem->GetAgentUserData(*Enemy).GetMutable<FUnitData>();
			FUnitData& UnitData = AntSubsystem->GetAgentUserData(*UnitActor).GetMutable<FUnitData>();

			UE_LOG(LogTemp, Warning, TEXT("通过"));
			
			// 如果当前单位未在攻击状态，则移动到敌人位置
			if (!UnitData.bIsAttack)
			{
				bool bIsMove = UAntFunctionLibrary::MoveAgentToLocation(GetWorld(), *UnitActor, FVector(Location),
				200.f, 1.f, 0.6f, 300.f, 20.f, EAntPathFollowerType::FlowField, 1200.f);
				if (bIsMove) UnitData.bIsAttack = true;
			}

			// 如果敌人未在攻击状态，则移动到当前单位位置
			if (!EnemyUnitData.bIsAttack)
			{
				bool bIsMove = UAntFunctionLibrary::MoveAgentToLocation(GetWorld(),* Enemy, FVector(Location),
				200.f, 1.f, 0.6f, 300.f, 20.f, EAntPathFollowerType::FlowField, 1200.f);
				if (bIsMove) EnemyUnitData.bIsAttack = true;
			}
		}
	}
}

/**
 * @brief 初始化四叉树
 * @details 创建四叉树的根节点，设置区域范围和节点容量
 */
void AQuadTreeManager::InitializeQuadTree()
{
	RootNode = MakeShareable(new QuadTreeNode(RegionCenter, RegionHalfSize, NodeCapacity));
}

/**
 * @brief 蓝图可调用的添加单位函数
 * @param Unit 要添加的单位句柄
 * @param Team 单位所属的队伍ID
 * @details 为蓝图提供添加单位的接口
 */
void AQuadTreeManager::BP_AddUnit(FAntHandle Unit, int32 Team)
{
	AddUnit(Unit, Team);
}

/**
 * @brief 添加单位到管理器和四叉树
 * @param Unit 要添加的单位句柄引用
 * @param Team 单位所属的队伍ID
 * @details 将单位添加到单位队伍映射表和四叉树中
 */
void AQuadTreeManager::AddUnit(FAntHandle& Unit, int32 Team)
{
	// 检查单位是否已存在
	if (!UnitTeams.Contains(&Unit))
	{
		// 添加到单位队伍映射表
		UnitTeams.Add(&Unit, Team);
		// 如果根节点有效，则插入到四叉树中
		if (RootNode.IsValid())
		{
			RootNode->Insert(&Unit, Team, GetWorld());
		}
	}
}

/**
 * @brief 从管理器和四叉树移除单位
 * @param Unit 要移除的单位句柄指针
 * @details 从单位队伍映射表和四叉树中移除指定单位
 */
void AQuadTreeManager::RemoveUnit(FAntHandle* Unit)
{
	if (Unit && UnitTeams.Contains(Unit))
	{
		// 从单位队伍映射表中移除
		UnitTeams.Remove(Unit);
		// 如果根节点有效，则从四叉树中移除
		if (RootNode.IsValid())
		{
			RootNode->Remove(Unit, GetWorld());
		}
	}
}

/**
 * @brief 查找给定单位的最近敌人
 * @param Unit 要查找敌人的单位句柄指针
 * @return 返回最近敌人的句柄指针，如果未找到则返回nullptr
 * @details 使用四叉树高效查找最近的敌对单位，忽略同队单位
 */
FAntHandle* AQuadTreeManager::FindNearestEnemy(FAntHandle* Unit)
{
	if (!RootNode.IsValid()) return nullptr;
	
	// 获取单位位置
	FVector3f Location;
	UAntFunctionLibrary::GetAgentLocation(GetWorld(), *Unit, Location);
	FVector2D QueryPoint = FVector2D(Location.X, Location.Y);
	
	// 获取单位所属队伍
	int32 MyTeam = UnitTeams.Contains(Unit) ? UnitTeams[Unit] : -1;
	float BestDistSq = TNumericLimits<float>::Max();
	
	// 在四叉树中查找最近敌人
	return RootNode->FindNearest(QueryPoint, MyTeam, BestDistSq, GetWorld());
}

/**
 * @brief 更新四叉树
 * @details 重建四叉树结构，重新插入所有单位以反映位置变化
 */
void AQuadTreeManager::UpdateQuadTree()
{
	// 清空旧的四叉树，重建根节点
	RootNode = MakeShareable(new QuadTreeNode(RegionCenter, RegionHalfSize, NodeCapacity));
	
	// 重新插入所有单位
	for (auto& Pair : UnitTeams)
	{
		FAntHandle* UnitActor = Pair.Key;
		int32 Team = Pair.Value;
		if (UnitActor)
		{
			RootNode->Insert(UnitActor, Team, GetWorld());
		}
	}
}
