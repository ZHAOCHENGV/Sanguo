

#include "BF/Actors/GeneralActorBase.h"

#include "BFSubjectiveAgentComponent.h"
#include "DebugHelper.h"

AGeneralActorBase::AGeneralActorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(Root);

	MountSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mount"));
	MountSkeletalMeshComponent->SetupAttachment(Root);

	GeneralSkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("General");
	GeneralSkeletalMeshComponent->SetupAttachment(MountSkeletalMeshComponent);
	
	SubjectiveAgentComponent = CreateDefaultSubobject<UBFSubjectiveAgentComponent>(TEXT("SubjectiveAgent"));
	SubjectiveAgentComponent->bAutoInitWithDataAsset = false;
}

void AGeneralActorBase::BeginPlay()
{
	Super::BeginPlay();
	SubjectiveAgentComponent->InitializeSubjectTraits(false, this);
	InitializeSubjectiveAgent();
	SubjectiveAgentComponent->ActivateAgent(SubjectiveAgentComponent->GetHandle());
}

void AGeneralActorBase::InitializeSubjectiveAgent()
{
	if (!SubjectiveAgentComponent) return;

	// ***** 血量 *****//
	FHealth HealthTrait;
	SubjectiveAgentComponent->GetTrait(HealthTrait);
	HealthTrait.Current = GeneralData.Health;
	HealthTrait.Maximum = GeneralData.Health;
	SubjectiveAgentComponent->SetTrait(HealthTrait);
	
	// ***** 导航 *****//
	FNavigation NavigationTrait;
	SubjectiveAgentComponent->GetTrait(NavigationTrait);
	NavigationTrait.FlowFieldToUse = FlowFieldToUse;
	SubjectiveAgentComponent->SetTrait(NavigationTrait);

	// ***** 索敌 *****//
	FTrace TraceTrait;
	SubjectiveAgentComponent->GetTrait(TraceTrait);
	TraceTrait.Mode = ETraceMode::SectorTraceByTraits;
	TraceTrait.Filter.IncludeTraits.Add(FHealth::StaticStruct());
	TraceTrait.Filter.IncludeTraits.Add(FCollider::StaticStruct());
	TraceTrait.Filter.IncludeTraits.Add(FLocated::StaticStruct());
	
	TraceTrait.Filter.ExcludeTraits.Add(FDying::StaticStruct());
	TraceTrait.Filter.ExcludeTraits.Add(TeamStructs[SubjectiveAgentComponent->TeamIndex]);
	SubjectiveAgentComponent->SetTrait(TraceTrait);

	// ***** 伤害 *****//
	FDamage DamageTrait;
	SubjectiveAgentComponent->GetTrait(DamageTrait);
	DamageTrait.Damage = GeneralData.Damage;
	SubjectiveAgentComponent->SetTrait(DamageTrait);

	// ***** 额外标签 *****//
	SubjectiveAgentComponent->SetTrait(FGeneral::StaticStruct(), FGeneral::StaticStruct());
}

