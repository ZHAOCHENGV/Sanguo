/**
 * @file MeteorRainActor.cpp
 * @brief 陨石雨生成Actor实现文件
 */

#include "Actors/MeteorRainActor.h"
#include "Actors/MeteorProjectile.h"

#include "BFSubjectiveAgentComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AMeteorRainActor::AMeteorRainActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AMeteorRainActor::BeginPlay()
{
    Super::BeginPlay();

    if (UBFSubjectiveAgentComponent* Comp = FindComponentByClass<UBFSubjectiveAgentComponent>())
    {
        OwnerTeam = Comp->TeamIndex;
    }

    GetWorld()->GetTimerManager().SetTimer(DetectTimer, this, &AMeteorRainActor::DetectEnemies, 0.5f, true);
}

void AMeteorRainActor::DetectEnemies()
{
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(DetectRadius);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MeteorDetect), false, this);

    GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, Sphere, Params);

    for (const FOverlapResult& Res : Overlaps)
    {
        AActor* Other = Res.GetActor();
        if (!Other || Other == this)
        {
            continue;
        }
        if (UBFSubjectiveAgentComponent* Comp = Other->FindComponentByClass<UBFSubjectiveAgentComponent>())
        {
            if (Comp->TeamIndex != OwnerTeam)
            {
                StartMeteorRain(Other);
            }
        }
    }
}

void AMeteorRainActor::StartMeteorRain(AActor* Target)
{
    if (!Target || !MeteorClass)
    {
        return;
    }

    for (int32 i = 0; i < MeteorCount; ++i)
    {
        FTimerDelegate Delegate;
        Delegate.BindUObject(this, &AMeteorRainActor::SpawnSingleMeteor, Target);
        FTimerHandle Handle;
        GetWorld()->GetTimerManager().SetTimer(Handle, Delegate, MeteorInterval * i, false);
    }
}

void AMeteorRainActor::SpawnSingleMeteor(AActor* Target)
{
    if (!Target || !MeteorClass)
    {
        return;
    }

    FVector TargetLocation = Target->GetActorLocation();
    AMeteorProjectile* Meteor = GetWorld()->SpawnActor<AMeteorProjectile>(MeteorClass, TargetLocation + FVector(0.f,0.f,1000.f), FRotator::ZeroRotator);
    if (Meteor)
    {
        Meteor->Init(TargetLocation, OwnerTeam);
    }
}

