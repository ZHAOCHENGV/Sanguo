// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BFAnimInstanceBase.generated.h"

class UBFSubjectiveAgentComponent;
/**
 * 
 */
UCLASS()
class ANTTEST_API UBFAnimInstanceBase : public UAnimInstance
{
	GENERATED_BODY()

protected:

	virtual void NativeInitializeAnimation() override;
	
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

private:

	TObjectPtr<UBFSubjectiveAgentComponent> OwningSubjectiveAgent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	float Speed = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	bool bIsCheer = false;
};
