// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "AlphaBlend.h"

#include "SkelotInstanceComponent.generated.h"

class USkelotComponent;

/*
Helper component that represent an Skelot instance. Transform is copied from Component to Skelot Instance. #WIP

*/
UCLASS(BlueprintType, editinlinenew, meta = (BlueprintSpawnableComponent))
class SKELOT_API USkelotInstanceComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	//the Skelot instance that is bound to this component. generally you may initialize it at BeginPlay by calling BindSkelotInstance
	UPROPERTY(BlueprintReadOnly, Category="Skelot")
	USkelotComponent* SkelotComponent;
	UPROPERTY(BlueprintReadOnly, Category="Skelot")
	int32 InstanceIndex = -1;

	USkelotInstanceComponent();

	void OnHiddenInGameChanged() override;
	void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;
	void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	void OnRegister() override;
	void OnUnregister() override;
	void BeginPlay() override;
	void EndPlay(EEndPlayReason::Type Reason) override;
	FTransform GetSocketTransform(FName SocketName, ERelativeTransformSpace TransformSpace) const override;
	void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;
	bool DoesSocketExist(FName InSocketName) const;
	bool HasAnySockets() const;
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Skelot")
	void BindSkelotInstance(USkelotComponent* InComponent, int32 InInstanceIndex);
	UFUNCTION(BlueprintCallable, Category="Skelot")
	void DestroySkelotInstance();

	//
	UFUNCTION(BlueprintCallable, Category="Skelot|Animation")
	UPARAM(DisplayName = "SequenceLength") float PlayAnimation(UAnimSequenceBase* Animation, bool bLoop = true, float StartAt = 0, float PlayScale = 1, float TransitionDuration = 0.2f, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear, bool bIgnoreTransitionGeneration = false);
	//
	UFUNCTION(BlueprintCallable, Category="Skelot|Animation")
	UPARAM(DisplayName = "SequenceLength") float InstancePlayRandomLoopedSequence(FName Name, float StartAt = 0, float PlayScale = 1, float TransitionDuration = 0.2f, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear, bool bIgnoreTransitionGeneration = false);

};