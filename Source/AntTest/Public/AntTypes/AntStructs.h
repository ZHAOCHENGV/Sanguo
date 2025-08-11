#pragma once
#include "CoreMinimal.h"
#include "AntEnums.h"
#include "BattleFrameStructs.h"
#include "AntStructs.generated.h"

class ANiagaraSubjectRenderer;
class UAnimToTextureDataAsset;
class USkelotAnimCollection;

USTRUCT(BlueprintType)
struct FAntSkelotData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkelotAnimCollection* AnimCollection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkeletalMesh* SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UMaterialInterface* RedMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UMaterialInterface* BlueMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequenceBase* IdleAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequenceBase* RunAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequenceBase* CheerAnim;
};

USTRUCT(BlueprintType)
struct FAgentSpawnerData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UAnimToTextureDataAsset> AnimToTextureDataAsset = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<ANiagaraSubjectRenderer> RendererClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EAgentAttackType AgentAttackType = EAgentAttackType::Near;
};

USTRUCT(BlueprintType)
struct FAgentTraitValue
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AttackRange = 0.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ChaseRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ChaseMoveSpeedMult = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float TraceRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float HitRange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float TraceFrequency = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float TraceHeight = 300.f;
};

USTRUCT(BlueprintType)
struct FGeneralData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Health = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Damage = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* AttackAnimMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* SkillAnimMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* MountAnimMontage = nullptr;
};

USTRUCT(BlueprintType)
struct FGeneral
{
	GENERATED_BODY()
	
};

USTRUCT(BlueprintType)
struct FDetected
{
	GENERATED_BODY()
	
};

USTRUCT(BlueprintType)
struct FUniqueID
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TeamID = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 GroupID = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 UnitID = INDEX_NONE;

	bool operator==(const FUniqueID& Other) const
	{
		return TeamID == Other.TeamID && GroupID == Other.GroupID && UnitID == Other.UnitID;
	}

	friend uint32 GetTypeHash(const FUniqueID& Key)
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(Key.TeamID));
		Hash = HashCombine(Hash, GetTypeHash(Key.GroupID));
		Hash = HashCombine(Hash, GetTypeHash(Key.UnitID));
		return Hash;
	}
};

USTRUCT(BlueprintType)
struct FFireBullet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FGoalTraceResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	TArray<FTraceResult> TraceResult;
};
