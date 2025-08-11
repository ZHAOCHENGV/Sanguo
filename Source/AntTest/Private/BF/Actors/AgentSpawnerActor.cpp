// Fill out your copyright notice in the Description page of Project Settings.


#include "BF/Actors/AgentSpawnerActor.h"

#include "ApparatusFunctionLibrary.h"
#include "BattleFrameFunctionLibraryRT.h"
#include "BFAntFunctionLibrary.h"
#include "DebugHelper.h"
#include "UObject/Class.h"
#include "NiagaraSubjectRenderer.h"
#include "BF/DataAssets/AgentSpawnerConfigDataAsset.h"
#include "Components/ArrowComponent.h"
#include "BF/Traits/Group.h"



AAgentSpawnerActor::AAgentSpawnerActor()
{
	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(DefaultRoot);

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(DefaultRoot);
	
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(DefaultRoot);
	Box->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	
	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(DefaultRoot);
}

void AAgentSpawnerActor::BeginPlay()
{
	Super::BeginPlay();

	/*if (UAgentSpawnerConfigDataAsset* LoadedAsset = AgentSpawnerConfigDataAsset.LoadSynchronous())
	{
		SpawnAgents(LoadedAsset);
	}*/
}

#if WITH_EDITOR
void AAgentSpawnerActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Box->SetBoxExtent(FVector((SquareSize.X * SquareSpacing.X) / 2.f, (SquareSize.Y * SquareSpacing.Y) / 2.f, 200.f));
}
#endif

void AAgentSpawnerActor::SpawnAgents(UAgentSpawnerConfigDataAsset* DataAsset)
{
	if (!DataAsset) return;
	TSoftObjectPtr<UAgentConfigDataAsset> AgentConfigDataAsset = nullptr;
	FAgentSpawnerData AgentSpawnerData = FAgentSpawnerData();
	
	switch (AgentSoldierType)
	{
	case EAgentSoldierType::General:
		AgentConfigDataAsset = DataAsset->GeneralAgentConfigDataAsset;
		break;
	case EAgentSoldierType::Soldier:
		AgentConfigDataAsset = DataAsset->SoldierAgentConfigDataAsset;
		AgentSpawnerData = DataAsset->SoldierConfig[SoldierType];
		break;
	}

	checkf(!AgentConfigDataAsset.IsNull(), TEXT("AgentConfigDataAsset空"));
	
	if (AgentSpawnerData.AnimToTextureDataAsset.IsNull() || AgentSpawnerData.RendererClass.IsNull()) return;
	
	 TArray<FSubjectHandle> SubjectHandles = SpawnAgentsByConfigRectangular(false,
		AgentConfigDataAsset, SquareSize.X * SquareSize.Y, TeamID, GetActorLocation(),
		FVector2D(500.f, 500.f), FVector2D(0.f, 0.f), EInitialDirection::FaceForward,
		FVector2D(0.f, 0.f), FSpawnerMult());


	if (SubjectHandles.IsEmpty())
	{
		Debug::Print(TEXT("生成空"));
		return;
	}
	
	//导航
	FNavigation NavigationTrait;
	SubjectHandles[0].GetTrait(FNavigation::StaticStruct(), &NavigationTrait);
	NavigationTrait.FlowFieldToUse = FlowFieldToUse;

	//索敌
	FTrace TraceTrait;
	SubjectHandles[0].GetTrait(FTrace::StaticStruct(), &TraceTrait);
	
	TraceTrait.SectorTrace.Common.TraceRadius = AgentSpawnerData.AgentAttackType == EAgentAttackType::Near ?
				DataAsset->NearAgentTraitValue.TraceRadius : DataAsset->RangedAgentTraitValue.TraceRadius;
	
	TraceTrait.SectorTrace.Common.CoolDown = AgentSpawnerData.AgentAttackType == EAgentAttackType::Near ?
				DataAsset->NearAgentTraitValue.TraceFrequency : DataAsset->RangedAgentTraitValue.TraceFrequency;

	TraceTrait.SectorTrace.Common.TraceHeight = AgentSpawnerData.AgentAttackType == EAgentAttackType::Near ?
				DataAsset->NearAgentTraitValue.TraceHeight : DataAsset->RangedAgentTraitValue.TraceHeight;
		
	TraceTrait.Mode = ETraceMode::SectorTraceByTraits;
	
	TraceTrait.Filter.IncludeTraits.Add(FHealth::StaticStruct());
	TraceTrait.Filter.IncludeTraits.Add(FCollider::StaticStruct());
	TraceTrait.Filter.IncludeTraits.Add(FLocated::StaticStruct());
	
	TraceTrait.Filter.ExcludeTraits.Add(FDying::StaticStruct());
	TraceTrait.Filter.ExcludeTraits.Add(FGeneral::StaticStruct());
	TraceTrait.Filter.ExcludeTraits.Add(UBFAntFunctionLibrary::GetTeamTrait(TeamID));

	//渲染子类型
	FSubType SubTypeTrait;
	SubTypeTrait.Index = static_cast<uint32>(SoldierType);

	//动画
	FAnimation AnimationTrait;
	AnimationTrait.AnimToTextureDataAsset = AgentSpawnerData.AnimToTextureDataAsset;
	AnimationTrait.RendererClass = AgentSpawnerData.RendererClass;
	AnimationTrait.IndexOfIdleAnim = 0;
	AnimationTrait.IndexOfMoveAnim = 1;
	AnimationTrait.IndexOfAppearAnim = -1;
	AnimationTrait.IndexOfAttackAnim = 2;
	AnimationTrait.IndexOfDeathAnim = 3;

	//进行中的动画
	FAnimating AnimatingTrait;
	AnimatingTrait.Team = bIsBlueTeam == false ? 0.f : 1.f;

	//攻击
	FAttack AttackTrait;
	SubjectHandles[0].GetTrait(FAttack::StaticStruct(), &AttackTrait);
	AttackTrait.Range = AgentSpawnerData.AgentAttackType == EAgentAttackType::Near ?
			DataAsset->NearAgentTraitValue.AttackRange : DataAsset->RangedAgentTraitValue.AttackRange;
	
	AttackTrait.RangeToleranceHit = AgentSpawnerData.AgentAttackType == EAgentAttackType::Near ?
			DataAsset->NearAgentTraitValue.HitRange : DataAsset->RangedAgentTraitValue.HitRange;
	
	if (AgentSpawnerData.AgentAttackType == EAgentAttackType::Range)
	{
		AttackTrait.TimeOfHitAction = EAttackMode::None;
		
		if (!DataAsset->ProjectileConfig.IsEmpty())
		{
			FProjectileParamsRT_DA ProjectileParamsRT_DA;
			ProjectileParamsRT_DA.ProjectileConfig = DataAsset->ProjectileConfig[TeamID];
			AttackTrait.SpawnProjectile.Add(ProjectileParamsRT_DA);
		}
		else
		{
			AttackTrait.SpawnActor = DataAsset->SpawnActor;
		}
	}

	//追踪
	FChase ChaseTrait;
	SubjectHandles[0].GetTrait(FChase::StaticStruct(), &ChaseTrait);
	ChaseTrait.AcceptanceRadius = AgentSpawnerData.AgentAttackType == EAgentAttackType::Near ?
			DataAsset->NearAgentTraitValue.ChaseRadius : DataAsset->RangedAgentTraitValue.ChaseRadius;
	
	ChaseTrait.MoveSpeedMult = AgentSpawnerData.AgentAttackType == EAgentAttackType::Near ?
			DataAsset->NearAgentTraitValue.ChaseMoveSpeedMult : DataAsset->RangedAgentTraitValue.ChaseMoveSpeedMult;

	//睡眠
	FSleep SleepTrait;
	SleepTrait.bEnable = bIsEnableSleep;

	//额外的
	FGroup GroupTrait;
	FUnit UnitTrait;
	FFireBullet FireBulletTrait;

	FUniqueID UniqueIDTrait;
	UniqueIDTrait.TeamID = TeamID;
	UniqueIDTrait.GroupID = GroupID;
	UniqueIDTrait.UnitID = UnitID;

	//移动中
	/*FMoving MovingTrait;
	SubjectHandles[0].GetTrait(MovingTrait);
	MovingTrait.bPushedBack = false;*/
	
	TArray<FTransform> Transforms = SquareFormation();

	if (Transforms.IsEmpty()) return;
	
	for (int32 i = 0; i < SubjectHandles.Num(); i++)
	{
		auto& SubjectHandle = SubjectHandles[i];
		if (!UApparatusFunctionLibrary::IsSubjectHandleValid(SubjectHandle)) continue;

		SubjectHandle.RemoveTrait(FRendering::StaticStruct());
		
		TraceTrait.SectorTrace.Common.CoolDown = FMath::FRandRange(3.5f, 4.5f);
		SubjectHandle.SetTrait(FSubType::StaticStruct(), &SubTypeTrait);
		SubjectHandle.SetTrait(FNavigation::StaticStruct(), &NavigationTrait);
		SubjectHandle.SetTrait(FTrace::StaticStruct(), &TraceTrait);
		SubjectHandle.SetTrait(FAnimating::StaticStruct(), &AnimatingTrait);
		SubjectHandle.SetTrait(FAnimation::StaticStruct(), &AnimationTrait);
		SubjectHandle.SetTrait(FAttack::StaticStruct(), &AttackTrait);
		SubjectHandle.SetTrait(FSleep::StaticStruct(), &SleepTrait);
		
		//额外的
		SubjectHandle.SetTrait(UBFAntFunctionLibrary::GetGroupTrait(GroupID), &GroupTrait);
		SubjectHandle.SetTrait(UBFAntFunctionLibrary::GetUnitTrait(UnitID), &UnitTrait);
		if (AgentSpawnerData.AgentAttackType == EAgentAttackType::Range)
		{
			SubjectHandle.SetTrait(FFireBullet::StaticStruct(), &FireBulletTrait);
		}
		//SubjectHandle.SetTrait(FChase::StaticStruct(), &ChaseTrait);

		FLocated LocatedTrait;
		LocatedTrait.Location = Transforms[i].GetLocation();
		LocatedTrait.Location.Z = 0.f;
		SubjectHandle.SetTrait(FLocated::StaticStruct(), &LocatedTrait);

		SubjectHandle.SetTrait(FUniqueID::StaticStruct(), &UniqueIDTrait);
		
		ActivateAgent(SubjectHandle);
	}
}

TArray<FTransform> AAgentSpawnerActor::SquareFormation()
{
	TArray<FTransform> Transforms;
	FVector Center = GetActorLocation();
	FRotator Rotation = GetActorRotation();

	// 计算方阵的行列偏移量（从中心对称分布）
	float StartX = -(SquareSize.X - 1) * SquareSpacing.X / 2.0f;
	float StartY = -(SquareSize.Y - 1) * SquareSpacing.Y / 2.0f;

	for (int32 i = 0; i < SquareSize.X; i++)
	{
		for (int32 j = 0; j < SquareSize.Y; j++)
		{
			// 计算局部空间中的偏移（未旋转状态）
			FVector LocalOffset(
				StartX + i * SquareSpacing.X,  // X轴：前后方向
				StartY + j * SquareSpacing.Y,  // Y轴：左右方向
				0.0f
			);
            
			// 将局部偏移转换为世界空间（应用Actor旋转）
			FVector WorldOffset = Rotation.RotateVector(LocalOffset);
            
			// 创建变换并设置位置
			FTransform Transform;
			Transform.SetLocation(Center + WorldOffset);
			Transforms.Add(Transform);
		}
	}
	return Transforms;
	/*TArray<FTransform> Transforms;

	FVector Center = GetActorLocation();

	float High = SquareSize.X * SquareSpacing.X;
	float Width = SquareSize.Y * SquareSpacing.Y;
	High -= SquareSpacing.X;
	Width -= SquareSpacing.Y;
	
	for (int32 i = 0; i < SquareSize.X; i++)
	{
		for (int32 j = 0; j < SquareSize.Y; j++)
		{
			FTransform RowTransform;
			RowTransform.SetLocation(
				Center + FVector(High / 2 - i * SquareSpacing.X + GetActorRotation().Yaw,
					Width / 2 - j * SquareSpacing.Y + GetActorRotation().Yaw, 0.f));
			
			Transforms.Add(RowTransform);
		}
	}
	return Transforms;*/
}
