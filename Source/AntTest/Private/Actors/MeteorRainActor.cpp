/**
 * @file MeteorRainActor.cpp
 * @brief 陨石雨生成Actor实现文件
 */

#include "Actors/MeteorRainActor.h"
#include "Actors/MeteorProjectile.h"

#include "BFSubjectiveAgentComponent.h"
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

    GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &AMeteorRainActor::SpawnSingleMeteor, MeteorInterval, true);
}

void AMeteorRainActor::SpawnSingleMeteor()
{
    if (!MeteorClass)
    {
        return;
    }

    FVector TargetLocation = GetActorLocation();
    AMeteorProjectile* Meteor = GetWorld()->SpawnActor<AMeteorProjectile>(MeteorClass, TargetLocation + FVector(0.f,0.f,1000.f), FRotator::ZeroRotator);
    if (Meteor)
    {
        Meteor->Init(TargetLocation, OwnerTeam);
    }
}

