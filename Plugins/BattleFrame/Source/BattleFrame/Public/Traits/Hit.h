#pragma once

#include "CoreMinimal.h"
#include "SubType.h"
#include "ActorSpawnConfig.h"
#include "SoundConfig.h"
#include "FxConfig.h"
#include "Hit.generated.h"

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FHit
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "是否启用被击中时的发光效果"))
	bool bCanGlow = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "播放受击动画，只会在Agent静止且播放Idle动画时生效"))
	bool bPlayAnim = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "受击动画时长"))
	float AnimLength = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = "被击中时的挤压/拉伸强度"))
	float JiggleStr = 0.5f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = ""))
    TArray<FActorSpawnConfig> SpawnActor;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = ""))
    TArray<FFxConfig> SpawnFx;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (Tooltip = ""))
    TArray<FSoundConfig> PlaySound;

};

USTRUCT(BlueprintType)
struct BATTLEFRAME_API FBeingHit
{
	GENERATED_BODY()

public:

	float GlowTime = 0.0f;
	float JiggleTime = 0.0f;
	float AnimTime = 0.0f;

	FORCEINLINE void ResetGlow()
	{
		GlowTime = 0.0f;
	}

	FORCEINLINE void ResetJiggle()
	{
		JiggleTime = 0.0f;
	}

	FORCEINLINE void ResetAnim()
	{
		AnimTime = 0.0f;
	}
};
