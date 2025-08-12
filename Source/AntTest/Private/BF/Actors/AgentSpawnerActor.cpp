/**
 * @file AgentSpawnerActor.cpp
 * @brief 兵员生成器Actor实现文件
 * @details 实现按配置批量生成将领/士兵主体，设置其战斗、导航、渲染等特征，并以方阵方式布置。
 * @note 本类仅负责“生成与初始配置”，不包含后续AI/技能/策略逻辑。
 * @see UAgentSpawnerConfigDataAsset, FAgentSpawnerData
 */

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


/**
 * @brief 构造函数
 * @details 初始化默认场景组件、辅助箭头、包围盒与广告牌组件。
 */
AAgentSpawnerActor::AAgentSpawnerActor()
{
	// 根
	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(DefaultRoot);

	// 朝向箭头（编辑器辅助）
	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(DefaultRoot);
	
	// 可视化盒体（编辑器观察阵型范围）
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(DefaultRoot);
	Box->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	
	// 广告牌（场景标识）
	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(DefaultRoot);
}

/**
 * @brief 游戏开始回调
 * @details 若需要，可在此同步加载数据资产并自动生成单位。
 * @note 默认不自动生成，避免关卡载入时的大规模生成导致卡顿。
 */
void AAgentSpawnerActor::BeginPlay()
{
	Super::BeginPlay();

	/*if (UAgentSpawnerConfigDataAsset* LoadedAsset = AgentSpawnerConfigDataAsset.LoadSynchronous())
	{
		SpawnAgents(LoadedAsset);
	}*/
}

#if WITH_EDITOR
/**
 * @brief 构造时回调（编辑器）
 * @details 根据方阵配置更新编辑器中可视化的包围盒尺寸。
 * @param Transform 当前世界变换。
 */
void AAgentSpawnerActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 按行列与间距估算X/Y半径，Z给定固定高度
	Box->SetBoxExtent(FVector((SquareSize.X * SquareSpacing.X) / 2.f, (SquareSize.Y * SquareSpacing.Y) / 2.f, 200.f));
}
#endif

/**
 * @brief 按配置生成主体并设置特征
 * @param DataAsset 兵员生成配置数据资产（必须非空）
 * @details 根据将领/士兵类型读取配置，生成主体集合，设置导航、索敌、动画、攻击、追踪、睡眠与自定义标签等特征，
 *          最后按方阵位置落点并激活主体。
 * @note 若动画或渲染配置为空，将直接返回以避免产生无渲染/无动画的主体。
 */
void AAgentSpawnerActor::SpawnAgents(UAgentSpawnerConfigDataAsset* DataAsset)
{
	if (!DataAsset) return;

	// 1) 选择配置源与士兵专项配置
	TSoftObjectPtr<UAgentConfigDataAsset> AgentConfigDataAsset = nullptr;
	FAgentSpawnerData AgentSpawnerData = FAgentSpawnerData();
	
	switch (AgentSoldierType)
	{
	case EAgentSoldierType::General:
		// 将领：仅使用General配置
		AgentConfigDataAsset = DataAsset->GeneralAgentConfigDataAsset;
		break;
	case EAgentSoldierType::Soldier:
		// 士兵：基础配置 + 按兵种读取独立的动画/渲染等数据
		AgentConfigDataAsset = DataAsset->SoldierAgentConfigDataAsset;
		AgentSpawnerData = DataAsset->SoldierConfig[SoldierType];
		break;
	}

	// 2) 校验基础资源
	checkf(!AgentConfigDataAsset.IsNull(), TEXT("AgentConfigDataAsset空"));
	// 动画/渲染为生成必需
	if (AgentSpawnerData.AnimToTextureDataAsset.IsNull() || AgentSpawnerData.RendererClass.IsNull()) return;
	
	// 3) 生成N个主体（此处采用矩形规则，N = SquareSize.X * SquareSize.Y）
	TArray<FSubjectHandle> SubjectHandles = SpawnAgentsByConfigRectangular(
		/*bRandom*/ false,
		AgentConfigDataAsset,
		SquareSize.X * SquareSize.Y,
		TeamID,
		GetActorLocation(),
		/*Extent*/ FVector2D(500.f, 500.f),
		/*Offset*/ FVector2D(0.f, 0.f),
		EInitialDirection::FaceForward,
		/*Noise*/ FVector2D(0.f, 0.f),
		FSpawnerMult());

	if (SubjectHandles.IsEmpty())
	{
		Debug::Print(TEXT("生成空"));
		return;
	}
	
	// 4) 构建各特征模板（仅读/写一次，循环中复用并个别字段随机化）
	// 4.1 导航：指定流场
	FNavigation NavigationTrait;
	SubjectHandles[0].GetTrait(FNavigation::StaticStruct(), &NavigationTrait);
	NavigationTrait.FlowFieldToUse = FlowFieldToUse;

	// 4.2 索敌：按远/近战选择不同的公共参数
	FTrace TraceTrait;
	SubjectHandles[0].GetTrait(FTrace::StaticStruct(), &TraceTrait);
	TraceTrait.SectorTrace.Common.TraceRadius = (AgentSpawnerData.AgentAttackType == EAgentAttackType::Near)
		? DataAsset->NearAgentTraitValue.TraceRadius
		: DataAsset->RangedAgentTraitValue.TraceRadius;
	TraceTrait.SectorTrace.Common.CoolDown = (AgentSpawnerData.AgentAttackType == EAgentAttackType::Near)
		? DataAsset->NearAgentTraitValue.TraceFrequency
		: DataAsset->RangedAgentTraitValue.TraceFrequency;
	TraceTrait.SectorTrace.Common.TraceHeight = (AgentSpawnerData.AgentAttackType == EAgentAttackType::Near)
		? DataAsset->NearAgentTraitValue.TraceHeight
		: DataAsset->RangedAgentTraitValue.TraceHeight;
	TraceTrait.Mode = ETraceMode::SectorTraceByTraits;
	// 过滤：只包含可战斗目标，排除死亡、将领、己方
	TraceTrait.Filter.IncludeTraits.Add(FHealth::StaticStruct());
	TraceTrait.Filter.IncludeTraits.Add(FCollider::StaticStruct());
	TraceTrait.Filter.IncludeTraits.Add(FLocated::StaticStruct());
	TraceTrait.Filter.ExcludeTraits.Add(FDying::StaticStruct());
	TraceTrait.Filter.ExcludeTraits.Add(FGeneral::StaticStruct());
	TraceTrait.Filter.ExcludeTraits.Add(UBFAntFunctionLibrary::GetTeamTrait(TeamID));

	// 4.3 渲染子类型：根据士兵种类做索引
	FSubType SubTypeTrait;
	SubTypeTrait.Index = static_cast<uint32>(SoldierType);

	// 4.4 动画渲染：指定AnimToTexture与Niagara渲染器，以及各动画槽位
	FAnimation AnimationTrait;
	AnimationTrait.AnimToTextureDataAsset = AgentSpawnerData.AnimToTextureDataAsset;
	AnimationTrait.RendererClass = AgentSpawnerData.RendererClass;
	AnimationTrait.IndexOfIdleAnim = 0;
	AnimationTrait.IndexOfMoveAnim = 1;
	AnimationTrait.IndexOfAppearAnim = -1;
	AnimationTrait.IndexOfAttackAnim = 2;
	AnimationTrait.IndexOfDeathAnim = 3;

	// 4.5 进行中的动画：Team用于决定材质着色通道等
	FAnimating AnimatingTrait;
	AnimatingTrait.Team = bIsBlueTeam ? 1.f : 0.f;

	// 4.6 攻击：范围/命中容差根据远/近战分流
	FAttack AttackTrait;
	SubjectHandles[0].GetTrait(FAttack::StaticStruct(), &AttackTrait);
	AttackTrait.Range = (AgentSpawnerData.AgentAttackType == EAgentAttackType::Near)
		? DataAsset->NearAgentTraitValue.AttackRange
		: DataAsset->RangedAgentTraitValue.AttackRange;
	AttackTrait.RangeToleranceHit = (AgentSpawnerData.AgentAttackType == EAgentAttackType::Near)
		? DataAsset->NearAgentTraitValue.HitRange
		: DataAsset->RangedAgentTraitValue.HitRange;
	// 远程：改为抛射物/生成体方式命中
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
			AttackTrait.SpawnActor = DataAsset->SpawnActor; // 备用：用Actor替代抛射物
		}
	}

	// 4.7 追踪：进入战斗追踪状态时的移动容差与速度倍率
	FChase ChaseTrait;
	SubjectHandles[0].GetTrait(FChase::StaticStruct(), &ChaseTrait);
	ChaseTrait.AcceptanceRadius = (AgentSpawnerData.AgentAttackType == EAgentAttackType::Near)
		? DataAsset->NearAgentTraitValue.ChaseRadius
		: DataAsset->RangedAgentTraitValue.ChaseRadius;
	ChaseTrait.MoveSpeedMult = (AgentSpawnerData.AgentAttackType == EAgentAttackType::Near)
		? DataAsset->NearAgentTraitValue.ChaseMoveSpeedMult
		: DataAsset->RangedAgentTraitValue.ChaseMoveSpeedMult;

	// 4.8 睡眠开关
	FSleep SleepTrait;
	SleepTrait.bEnable = bIsEnableSleep;

	// 4.9 额外标签：分组/单位类型/开火标签（仅远程）
	FGroup GroupTrait;
	FUnit UnitTrait;
	FFireBullet FireBulletTrait;

	// 4.10 唯一ID：用于后续系统（如HUD、统计、同步）
	FUniqueID UniqueIDTrait;
	UniqueIDTrait.TeamID = TeamID;
	UniqueIDTrait.GroupID = GroupID;
	UniqueIDTrait.UnitID = UnitID;

	// 5) 计算方阵落点（若为空直接返回）
	TArray<FTransform> Transforms = SquareFormation();
	if (Transforms.IsEmpty()) return;
	
	// 6) 下发特征到每个主体并激活
	for (int32 i = 0; i < SubjectHandles.Num(); i++)
	{
		auto& SubjectHandle = SubjectHandles[i];
		if (!UApparatusFunctionLibrary::IsSubjectHandleValid(SubjectHandle)) continue;

		// 移除默认渲染，由动画渲染器控制
		SubjectHandle.RemoveTrait(FRendering::StaticStruct());
		
		// 冷却做微随机，避免整队同刻刷新造成“同步感”过强
		TraceTrait.SectorTrace.Common.CoolDown = FMath::FRandRange(2.f, 3.f);

		// 下发核心特征
		SubjectHandle.SetTrait(FSubType::StaticStruct(), &SubTypeTrait);
		SubjectHandle.SetTrait(FNavigation::StaticStruct(), &NavigationTrait);
		SubjectHandle.SetTrait(FTrace::StaticStruct(), &TraceTrait);
		SubjectHandle.SetTrait(FAnimating::StaticStruct(), &AnimatingTrait);
		SubjectHandle.SetTrait(FAnimation::StaticStruct(), &AnimationTrait);
		SubjectHandle.SetTrait(FAttack::StaticStruct(), &AttackTrait);
		SubjectHandle.SetTrait(FSleep::StaticStruct(), &SleepTrait);
		
		// 标签类特征
		SubjectHandle.SetTrait(UBFAntFunctionLibrary::GetGroupTrait(GroupID), &GroupTrait);
		SubjectHandle.SetTrait(UBFAntFunctionLibrary::GetUnitTrait(UnitID), &UnitTrait);
		if (AgentSpawnerData.AgentAttackType == EAgentAttackType::Range)
		{
			SubjectHandle.SetTrait(FFireBullet::StaticStruct(), &FireBulletTrait);
		}

		// 初始位置（忽略Z）
		FLocated LocatedTrait;
		LocatedTrait.Location = Transforms[i].GetLocation();
		LocatedTrait.Location.Z = 0.f;
		SubjectHandle.SetTrait(FLocated::StaticStruct(), &LocatedTrait);

		// 唯一ID
		SubjectHandle.SetTrait(FUniqueID::StaticStruct(), &UniqueIDTrait);
		
		// 激活主体（进入系统调度）
		ActivateAgent(SubjectHandle);
	}
}

/**
 * @brief 生成方阵的世界变换列表
 * @return 位置仅设置的变换数组
 * @note 会考虑Actor的世界旋转，使阵列随Actor朝向对齐。
 */
TArray<FTransform> AAgentSpawnerActor::SquareFormation()
{
	TArray<FTransform> Transforms;
	FVector Center = GetActorLocation();
	FRotator Rotation = GetActorRotation();

	// 自中心对称的起始偏移
	float StartX = -(SquareSize.X - 1) * SquareSpacing.X / 2.0f;
	float StartY = -(SquareSize.Y - 1) * SquareSpacing.Y / 2.0f;

	for (int32 i = 0; i < SquareSize.X; i++)
	{
		for (int32 j = 0; j < SquareSize.Y; j++)
		{
			// 未旋转的局部偏移（X前后，Y左右）
			FVector LocalOffset(
				StartX + i * SquareSpacing.X,
				StartY + j * SquareSpacing.Y,
				0.0f
			);
			
			// 应用Actor旋转得到世界偏移
			FVector WorldOffset = Rotation.RotateVector(LocalOffset);
			
			// 构建仅带位置的变换
			FTransform Transform;
			Transform.SetLocation(Center + WorldOffset);
			Transforms.Add(Transform);
		}
	}
	return Transforms;
}
