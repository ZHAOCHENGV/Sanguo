// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AgentSpawner.h"
#include "AntTypes/AntEnums.h"
#include "AntTypes/AntStructs.h"
#include "BF/Traits/Group.h"
#include "BF/Traits/Unit.h"
#include "Traits/Team.h"
#include "AgentSpawnerActor.generated.h"


USTRUCT()
struct FRanged
{
	GENERATED_BODY()
	
};

class UAgentSpawnerConfigDataAsset;
/**
 * 
 */
UCLASS()
class ANTTEST_API AAgentSpawnerActor : public AAgentSpawner
{
	GENERATED_BODY()

public:
	AAgentSpawnerActor();
	
protected:
	virtual void BeginPlay() override;
	
#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
#endif
	
private:
	
	TArray<FTransform> SquareFormation();
	
	UFUNCTION(BlueprintCallable)
	void SpawnAgents(UAgentSpawnerConfigDataAsset* DataAsset);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> DefaultRoot;

	UPROPERTY(VisibleAnywhere)
	UArrowComponent* Arrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Billboard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBillboardComponent> Billboard;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UAgentSpawnerConfigDataAsset> AgentSpawnerConfigDataAsset;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	EAgentSoldierType AgentSoldierType = EAgentSoldierType::Soldier;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true", EditConditionHides, EditCondition = "(AgentSoldierType) == EAgentSoldierType::Soldier"))
	ESoldierType SoldierType = ESoldierType::DaoBing;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	bool bIsBlueTeam = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData|ID", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "9"))
	int32 TeamID = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData|ID", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "9"))
	int32 GroupID = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData|ID", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "9"))
	int32 UnitID = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	bool bIsEnableSleep = false;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	FIntPoint SquareSize = FIntPoint(4, 8);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	FVector2D SquareSpacing = FVector2D(100.f, 100.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConfigData|Navigation", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<AFlowField> FlowFieldToUse;
};
