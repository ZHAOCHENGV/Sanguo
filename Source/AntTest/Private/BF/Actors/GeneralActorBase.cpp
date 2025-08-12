/**
 * @file GeneralActorBase.cpp
 * @brief 将领Actor基类实现文件
 * @details 该文件实现了将领Actor的基类，提供将领单位的基本功能
 *          包括骨骼网格组件、主观代理组件和特征初始化等
 * @author AntTest Team
 * @date 2024
 */

#include "BF/Actors/GeneralActorBase.h"

#include "BFSubjectiveAgentComponent.h"
#include "DebugHelper.h"

/**
 * @brief 构造函数
 * @details 初始化将领Actor的组件，包括根组件、骨骼网格组件和主观代理组件
 */
AGeneralActorBase::AGeneralActorBase()
{
	// 禁用Tick功能，因为不需要每帧更新
	PrimaryActorTick.bCanEverTick = false;

	// 创建根组件
	Root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(Root);

	// 创建坐骑骨骼网格组件
	MountSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mount"));
	MountSkeletalMeshComponent->SetupAttachment(Root);

	// 创建将领骨骼网格组件，附加到坐骑组件上
	GeneralSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("General");
	GeneralSkeletalMeshComponent->SetupAttachment(MountSkeletalMeshComponent);
	
	// 创建主观代理组件
	SubjectiveAgentComponent = CreateDefaultSubobject<UBFSubjectiveAgentComponent>(TEXT("SubjectiveAgent"));
	SubjectiveAgentComponent->bAutoInitWithDataAsset = false;
}

/**
 * @brief 游戏开始时调用
 * @details 初始化主观代理组件，设置特征并激活代理
 */
void AGeneralActorBase::BeginPlay()
{
	Super::BeginPlay();
	// 初始化主观代理特征
	SubjectiveAgentComponent->InitializeSubjectTraits(false, this);
	// 初始化主观代理
	InitializeSubjectiveAgent();
	// 激活代理
	SubjectiveAgentComponent->ActivateAgent(SubjectiveAgentComponent->GetHandle());
}

/**
 * @brief 初始化主观代理
 * @details 设置将领的各种特征，包括血量、导航、索敌、伤害等
 */
void AGeneralActorBase::InitializeSubjectiveAgent()
{
	if (!SubjectiveAgentComponent) return;

	// ***** 血量特征 *****//
	FHealth HealthTrait;
	SubjectiveAgentComponent->GetTrait(HealthTrait);
	HealthTrait.Current = GeneralData.Health;
	HealthTrait.Maximum = GeneralData.Health;
	SubjectiveAgentComponent->SetTrait(HealthTrait);
	
	// ***** 导航特征 *****//
	FNavigation NavigationTrait;
	SubjectiveAgentComponent->GetTrait(NavigationTrait);
	NavigationTrait.FlowFieldToUse = FlowFieldToUse;
	SubjectiveAgentComponent->SetTrait(NavigationTrait);

	// ***** 索敌特征 *****//
	FTrace TraceTrait;
	SubjectiveAgentComponent->GetTrait(TraceTrait);
	TraceTrait.Mode = ETraceMode::SectorTraceByTraits;
	// 包含的特征：血量、碰撞体、位置
	TraceTrait.Filter.IncludeTraits.Add(FHealth::StaticStruct());
	TraceTrait.Filter.IncludeTraits.Add(FCollider::StaticStruct());
	TraceTrait.Filter.IncludeTraits.Add(FLocated::StaticStruct());
	
	// 排除的特征：死亡、同队
	TraceTrait.Filter.ExcludeTraits.Add(FDying::StaticStruct());
	TraceTrait.Filter.ExcludeTraits.Add(TeamStructs[SubjectiveAgentComponent->TeamIndex]);
	SubjectiveAgentComponent->SetTrait(TraceTrait);

	// ***** 伤害特征 *****//
	FDamage DamageTrait;
	SubjectiveAgentComponent->GetTrait(DamageTrait);
	DamageTrait.Damage = GeneralData.Damage;
	SubjectiveAgentComponent->SetTrait(DamageTrait);

	// ***** 额外标签 *****//
	// 设置将领特征标记
	SubjectiveAgentComponent->SetTrait(FGeneral::StaticStruct(), FGeneral::StaticStruct());
}

