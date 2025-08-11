// Fill out your copyright notice in the Description page of Project Settings.


#include "BF/AnimInstances/BFAnimInstanceBase.h"

#include "BFSubjectiveAgentComponent.h"
#include "DebugHelper.h"

void UBFAnimInstanceBase::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (!OwningSubjectiveAgent)
	{
		if (UBFSubjectiveAgentComponent* OwningComponent = GetOwningActor()->GetComponentByClass<UBFSubjectiveAgentComponent>())
		{
			OwningSubjectiveAgent = OwningComponent;
		}
	}
}

void UBFAnimInstanceBase::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!OwningSubjectiveAgent)
	{
		if (UBFSubjectiveAgentComponent* OwningComponent = GetOwningActor()->GetComponentByClass<UBFSubjectiveAgentComponent>())
		{
			OwningSubjectiveAgent = OwningComponent;
		}
	}

	if (!OwningSubjectiveAgent) return;
	
	if (OwningSubjectiveAgent->HasTrait(FMoving::StaticStruct()))
	{
		FMoving MovingTrait;
		OwningSubjectiveAgent->GetTrait(MovingTrait);
		Speed = MovingTrait.CurrentVelocity.Size();
	}

	if (OwningSubjectiveAgent->HasTrait(FAnimating::StaticStruct()))
	{
		FAnimating AnimatingTrait;
		OwningSubjectiveAgent->GetTrait(AnimatingTrait);
		bIsCheer = AnimatingTrait.CurrentMontageSlot == 4 ? true : false;
	}
}
