// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

/*

*/

#pragma once

#include "SkelotComponent.h"

#include "GameFramework/Character.h"

#include "SkelotCharacterIntegration.generated.h"

class USkelotCharacterSyncComponent;

/*
ACharacter without USkeletalMeshComponent, uses USkelotCharacterSyncComponent for rendering instead
*/
UCLASS()
class ACharacter_SkelotBound : public ACharacter
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="Skelot|Character Integration")
	int InstanceIndex = -1;
	UPROPERTY(BlueprintReadOnly, Category="Skelot|Character Integration")
	TWeakObjectPtr<USkelotCharacterSyncComponent> SkelotComponent;

	ACharacter_SkelotBound(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Skelot|Character Integration")
	void BindToSkelot(USkelotCharacterSyncComponent* InSkelotComponent);

	UFUNCTION(BlueprintCallable, Category = "Skelot|Character Integration")
	void UnbindFromSkelot();

	void Destroyed() override;
};

/*
copies transform from bound actors to the the instances
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class USkelotCharacterSyncComponent : public USkelotComponent
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Character Integration")
	FTransform3f MeshTransform;

	TArray<TWeakObjectPtr<ACharacter_SkelotBound>> CharacterActors;

	USkelotCharacterSyncComponent();

	void CustomInstanceData_Initialize(int InstanceIndex) override;
	void CustomInstanceData_Destroy(int InstanceIndex) override;
	void CustomInstanceData_Move(int DstIndex, int SrcIndex) override;
	void CustomInstanceData_SetNum(int NewNum) override;

	void OnAnimationFinished(const TArray<FSkelotAnimFinishEvent>& Events) override;
	void OnAnimationNotify(const TArray<FSkelotAnimNotifyEvent>& Events) override;

	int FlushInstances(TArray<int>* OutRemapArray) override
	{
		int Ret = Super::FlushInstances(OutRemapArray);
		
		for (int i = 0; i < CharacterActors.Num(); i++)
		{
			ACharacter_SkelotBound* Chr = CharacterActors[i].Get();
			if (Chr)
			{
				Chr->InstanceIndex = i;
			}
		}

		return Ret;
	}
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Skelot|Character Integration")
	ACharacter_SkelotBound* GetCharacterActor(int InstanceIndex) const { return CharacterActors[InstanceIndex].Get(); }

	void CopyTransforms();
};