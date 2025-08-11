#pragma once

#include "CoreMinimal.h"
#include "SubType.h"
#include "ActorSpawnConfig.h"
#include "SoundConfig.h"
#include "FxConfig.h"
#include "Death.generated.h"

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FDeath
{
    GENERATED_BODY()

public:

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "是否启用死亡逻辑"))
    bool bEnable = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "死亡后删除的延迟时间（单位：秒）"))
    float DespawnDelay = 3.f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "是否启用淡出效果"))
    bool bCanFadeout = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "淡出效果的延迟时间（单位：秒）"))
    float FadeOutDelay = 2.f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "是否播放死亡动画"))
    bool bCanPlayAnim = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "淡出效果的延迟时间（单位：秒）"))
    float AnimLength = 2.f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "寿命，负值为无限长"))
    float LifeSpan = -1.f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "死亡后的尸体是否关闭碰撞"))
    bool bDisableCollision = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = ""))
    TArray<FActorSpawnConfig> SpawnActor;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = ""))
    TArray<FFxConfig> SpawnFx;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = ""))
    TArray<FSoundConfig> PlaySound;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FDying
{
    GENERATED_BODY()

public:

    bool bInitialized = false;

    float Duration = 0.0f;

    float Time = 0.0f;

    float DeathDissolveTime = 0.0f;

    float DeathAnimTime = 0.0f;

    FSubjectHandle Instigator = FSubjectHandle();

    FVector HitDirection = FVector::ZeroVector;

};
