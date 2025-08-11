// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AntSubsystem.h"
#include "SkelotComponent.h"
#include "AntTest.generated.h"

USTRUCT(BlueprintType)
struct FUnitData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxHealth = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Health = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AttackDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackCooldown = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackRange = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LastAttackTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SkelotInstanceID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TeamID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAntHandle CurrentTarget;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimSequenceBase *WalkAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimSequenceBase *IdleAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimSequenceBase *AttackAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimSequenceBase *CheerAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsAttack = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsDeath = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsPlayCheerAnim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ID = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Speed = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USkelotComponent* SkelotComponent = nullptr;
};

/** This calss is a higher-level and safe version of Ant for use in bluperint. */
UCLASS()
class UAntHelperFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Ant Helper", meta = (WorldContext = "WorldContextObject"))
	static void CopyEnemyTranformsToSkelot(const UObject* WorldContextObject, USkelotComponent *SkelotCom, UPARAM(meta = (Bitmask, BitmaskEnum = EAntGroups)) int32 Flags);

	UFUNCTION(BlueprintCallable, Category = "Ant Helper", meta = (WorldContext = "WorldContextObject"))
	static void CopyHeroTransformToSkelot(const UObject* WorldContextObject, USkelotComponent *SkelotCom, FAntHandle HeroHandle);

	UFUNCTION(BlueprintCallable, Category = "Ant Helper", meta = (WorldContext = "WorldContextObject"))
	static void CopyEnemyTranformsToSkelot_New(const UObject* WorldContextObject, USkelotComponent *SkelotCom, TArray<FAntHandle> Handle);
};