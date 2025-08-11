#include "BattleManager.h"

#include "AntTest/AntTest.h"
#include "Customizations/MathStructProxyCustomizations.h"

AQuadTreeManager::AQuadTreeManager()
{
	PrimaryActorTick.bCanEverTick = true;
	// 默认区域设为原点附近，可在关卡中修改
	RegionCenter = FVector2D(0.0f, 0.0f);
	RegionHalfSize = FVector2D(1000.0f, 1000.0f);
	
}

void AQuadTreeManager::BeginPlay()
{
	Super::BeginPlay();
	if (!AntSubsystem)
	{
		AntSubsystem = GetWorld()->GetSubsystem<UAntSubsystem>();
	}
	InitializeQuadTree();
}

void AQuadTreeManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	UpdateQuadTree();

	// 示例：每帧对所有单位查找最近敌人
	for (auto& Pair : UnitTeams)
	{
		FAntHandle* UnitActor = Pair.Key;
		if (UAntFunctionLibrary::IsValidAgent(GetWorld(), *UnitActor)) continue;
		//UE_LOG(LogTemp, Warning, TEXT("通过"));
		if (FAntHandle* Enemy = FindNearestEnemy(UnitActor))
		{
			FVector3f Location;
			UAntFunctionLibrary::GetAgentLocation(GetWorld(), *Enemy, Location);
			
			FUnitData& EnemyUnitData = AntSubsystem->GetAgentUserData(*Enemy).GetMutable<FUnitData>();
			FUnitData& UnitData = AntSubsystem->GetAgentUserData(*UnitActor).GetMutable<FUnitData>();

			UE_LOG(LogTemp, Warning, TEXT("通过"));
			if (!UnitData.bIsAttack)
			{
				bool bIsMove = UAntFunctionLibrary::MoveAgentToLocation(GetWorld(), *UnitActor, FVector(Location),
				200.f, 1.f, 0.6f, 300.f, 20.f, EAntPathFollowerType::FlowField, 1200.f);
				if (bIsMove) UnitData.bIsAttack = true;
			}

			if (!EnemyUnitData.bIsAttack)
			{
				bool bIsMove = UAntFunctionLibrary::MoveAgentToLocation(GetWorld(),* Enemy, FVector(Location),
				200.f, 1.f, 0.6f, 300.f, 20.f, EAntPathFollowerType::FlowField, 1200.f);
				if (bIsMove) EnemyUnitData.bIsAttack = true;
			}
		}
	}
}

void AQuadTreeManager::InitializeQuadTree()
{
	RootNode = MakeShareable(new QuadTreeNode(RegionCenter, RegionHalfSize, NodeCapacity));
}

void AQuadTreeManager::BP_AddUnit(FAntHandle Unit, int32 Team)
{
	AddUnit(Unit, Team);
}

void AQuadTreeManager::AddUnit(FAntHandle& Unit, int32 Team)
{
	
	if (!UnitTeams.Contains(&Unit))
	{
		UnitTeams.Add(&Unit, Team);
		if (RootNode.IsValid())
		{
			RootNode->Insert(&Unit, Team, GetWorld());
		}
	}
}

void AQuadTreeManager::RemoveUnit(FAntHandle* Unit)
{
	if (Unit && UnitTeams.Contains(Unit))
	{
		UnitTeams.Remove(Unit);
		if (RootNode.IsValid())
		{
			RootNode->Remove(Unit, GetWorld());
		}
	}
}

FAntHandle* AQuadTreeManager::FindNearestEnemy(FAntHandle* Unit)
{
	if (!RootNode.IsValid()) return nullptr;
	FVector3f Location;
	UAntFunctionLibrary::GetAgentLocation(GetWorld(), *Unit, Location);
	FVector2D QueryPoint = FVector2D(Location.X, Location.Y);
	int32 MyTeam = UnitTeams.Contains(Unit) ? UnitTeams[Unit] : -1;
	float BestDistSq = TNumericLimits<float>::Max();
	return RootNode->FindNearest(QueryPoint, MyTeam, BestDistSq, GetWorld());
}

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
