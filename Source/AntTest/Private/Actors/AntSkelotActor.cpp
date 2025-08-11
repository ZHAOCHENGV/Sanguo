

#include "Actors/AntSkelotActor.h"

#include "AntFunctionLibrary.h"
#include "NavigationSystem.h"
#include "SkelotComponent.h"
#include "AntTest/AntTest.h"
#include "Components/BillboardComponent.h"

AAntSkelotActor::AAntSkelotActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	
	SoldierSkelotComponent = CreateDefaultSubobject<USkelotComponent>(TEXT("SoldierSkelot"));
	SoldierSkelotComponent->SetupAttachment(GetRootComponent());
	SoldierSkelotComponent->bReceivesDecals = false;

	SkelotBillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("SkelotBillboard"));
	SkelotBillboardComponent->SetupAttachment(GetRootComponent());
}

void AAntSkelotActor::BeginPlay()
{
	Super::BeginPlay();

	SoldierAntHandles.Empty();
	
	InitSkelotData();

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(StartSpawnTimerHandle, this, &ThisClass::SpawnAntHandle, 1.f, false);
	}
}

void AAntSkelotActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (SoldierSkelotComponent && SoldierAntHandles.Num() > 0 && !bIsPlayCheerAnim)
	{
		UAntHelperFunctionLibrary::CopyEnemyTranformsToSkelot_New(this, SoldierSkelotComponent, SoldierAntHandles);
	}
}

FAntSkelotData* AAntSkelotActor::GetAntSkelotData(const ESoldierType& SoldierDataType)
{
	checkf(AntSkelotDataTable, TEXT("AntSkelotDataTable无效"));
	
	const UEnum* EnumPtr = StaticEnum<ESoldierType>();
	
	FString SoldierName = EnumPtr->GetNameStringByIndex((int32)SoldierDataType);
		
	FAntSkelotData* FoundData = AntSkelotDataTable->FindRow<FAntSkelotData>(FName(SoldierName), FString());

	if (FoundData)
	{
		return FoundData;
	}
	
	return nullptr;
}

void AAntSkelotActor::InitSkelotData()
{
	const FAntSkelotData* Data = GetAntSkelotData(SoldierType);

	if (SoldierSkelotComponent && Data)
	{
		SoldierSkelotComponent->SetAnimCollection(Data->AnimCollection);
		SoldierSkelotComponent->SetSubmeshAsset(0, Data->SkeletalMesh);
		SoldierSkelotComponent->SetMaterial(0, bIsUesBlue ? Data->BlueMaterial : Data->RedMaterial);
	}
}

void AAntSkelotActor::SpawnAntHandle()
{
	if (!GetWorld()) return;

	UAntSubsystem* AntSubsystem = GetWorld()->GetSubsystem<UAntSubsystem>();

	if (!AntSubsystem) return;
	
	if (SpawnCount == 0) return;

	FAntSkelotData* Data = GetAntSkelotData(SoldierType);

	if (!Data) return;
	
	FTransform SpawnTransform;
	
	for (int32 i = 0; i < SpawnCount; i++)
	{
		FVector SpawnLocation;
		if (UNavigationSystemV1::K2_GetRandomReachablePointInRadius(this, GetActorLocation(), SpawnLocation, SpawnCount * 10))
		{
			SpawnTransform.SetLocation(SpawnLocation);
			
			int32 InstanceIndex = SoldierSkelotComponent->AddInstance(FTransform3f(SpawnTransform));

			SoldierSkelotComponent->InstancePlayAnimation(InstanceIndex, Data->IdleAnim);

			FAntHandle AntHandle = UAntFunctionLibrary::AddAgentAdvanced(this, SpawnLocation, 70.f, 0.f, 0.f, 1.f,
				1.f, 7.f, true, true, true, 1.f, Flags, Flags);

			if (AntSubsystem->IsValidAgent(AntHandle))
			{
				FInstancedStruct InstancedStruct;
				InstancedStruct.InitializeAs<FUnitData>();
				FUnitData& UnitData = InstancedStruct.GetMutable<FUnitData>();
				UnitData.SkelotInstanceID = InstanceIndex;
				UnitData.IdleAnim = Data->IdleAnim;
				UnitData.WalkAnim = Data->RunAnim;
				UnitData.CheerAnim = Data->CheerAnim;
				UnitData.Speed = AntSpeed;
				UnitData.SkelotComponent = SoldierSkelotComponent;
				UAntFunctionLibrary::SetAgentCustomInstancedStruct(this, AntHandle, InstancedStruct);
				SoldierAntHandles.Add(AntHandle);
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("%d"), SoldierAntHandles.Num());
}

