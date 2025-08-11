// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AntTypes/AntEnums.h"
#include "AntTypes/AntStructs.h"
#include "AntHandle.h"
#include "GameFramework/Actor.h"
#include "AntSkelotActor.generated.h"

class USkelotComponent;
class USkelotAnimCollection;

UCLASS()
class ANTTEST_API AAntSkelotActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AAntSkelotActor();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:

	FAntSkelotData* GetAntSkelotData(const ESoldierType& SoldierDataType);

	void InitSkelotData();

	void SpawnAntHandle();
	
	TArray<FAntHandle> SoldierAntHandles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skelot", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skelot", meta = (AllowPrivateAccess = "true"))
	USkelotComponent* SoldierSkelotComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skelot", meta = (AllowPrivateAccess = "true"))
	UBillboardComponent* SkelotBillboardComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data", meta = (AllowPrivateAccess = "true"))
	UDataTable* AntSkelotDataTable;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	ESoldierType SoldierType = ESoldierType::DaoBing;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	int32 SpawnCount = 100;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	int32 Flags = 1;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	bool bIsUesBlue;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AntConfig", meta = (AllowPrivateAccess = "true"))
	float AntSpeed = 15.f;

	FTimerHandle StartSpawnTimerHandle;

	bool bIsPlayCheerAnim = false;
};
