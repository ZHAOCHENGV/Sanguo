/**
 * @file MeteorProjectile.cpp
 * @brief 陨石Actor实现文件
 */

#include "Actors/MeteorProjectile.h"

#include "BFSubjectiveAgentComponent.h"
#include "Kismet/GameplayStatics.h"

AMeteorProjectile::AMeteorProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AMeteorProjectile::Init(const FVector& InTargetLocation, int32 InInstigatorTeam)
{
    TargetLocation = InTargetLocation;
    InstigatorTeam = InInstigatorTeam;
    SetActorLocation(TargetLocation + FVector(0.f, 0.f, 1000.f));
}

void AMeteorProjectile::BeginPlay()
{
    Super::BeginPlay();
}

void AMeteorProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    FVector NewLocation = GetActorLocation();
    NewLocation.Z -= FallSpeed * DeltaSeconds;
    SetActorLocation(NewLocation);

    if (NewLocation.Z <= TargetLocation.Z)
    {
        Explode();
    }
}

void AMeteorProjectile::Explode()
{
    // 查询范围内的士兵
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(DamageRadius);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MeteorDamage), false, this);

    GetWorld()->OverlapMultiByChannel(Overlaps, TargetLocation, FQuat::Identity, ECC_Pawn, Sphere, Params);

    for (const FOverlapResult& Res : Overlaps)
    {
        AActor* HitActor = Res.GetActor();
        if (!HitActor)
        {
            continue;
        }

        if (UBFSubjectiveAgentComponent* Comp = HitActor->FindComponentByClass<UBFSubjectiveAgentComponent>())
        {
            if (Comp->TeamIndex != InstigatorTeam)
            {
                UGameplayStatics::ApplyDamage(HitActor, Damage, nullptr, this, UDamageType::StaticClass());
            }
        }
    }

    Destroy();
}

