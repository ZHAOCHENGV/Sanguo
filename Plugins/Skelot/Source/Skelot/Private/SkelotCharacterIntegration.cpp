// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotCharacterIntegration.h"


ACharacter_SkelotBound::ACharacter_SkelotBound(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.DoNotCreateDefaultSubobject(ACharacter::MeshComponentName))
{

}

void ACharacter_SkelotBound::BindToSkelot(USkelotCharacterSyncComponent* InSkelotComponent)
{
	UnbindFromSkelot();

	if (IsValid(InSkelotComponent))
	{
		this->SkelotComponent = InSkelotComponent;
		this->InstanceIndex = InSkelotComponent->AddInstance(InSkelotComponent->MeshTransform * FTransform3f(this->GetActorTransform()));
		InSkelotComponent->CharacterActors[this->InstanceIndex] = this;
		
	}
}

void ACharacter_SkelotBound::UnbindFromSkelot()
{
	if (SkelotComponent.IsValid() && InstanceIndex != -1)
	{
		SkelotComponent->DestroyInstance(InstanceIndex);
		SkelotComponent = nullptr;
		InstanceIndex = -1;
		return;
	}
}

void ACharacter_SkelotBound::Destroyed()
{
	UnbindFromSkelot();
	Super::Destroyed();
}

USkelotCharacterSyncComponent::USkelotCharacterSyncComponent()
{
	MeshTransform = FTransform3f(FRotator3f(0, -90, 0), FVector3f(0, 0, -87));
}

void USkelotCharacterSyncComponent::CustomInstanceData_Initialize(int InstanceIndex)
{
	this->CharacterActors[InstanceIndex] = nullptr;
}

void USkelotCharacterSyncComponent::CustomInstanceData_Destroy(int InstanceIndex)
{
	this->CharacterActors[InstanceIndex] = nullptr;
}

void USkelotCharacterSyncComponent::CustomInstanceData_Move(int DstIndex, int SrcIndex)
{
	this->CharacterActors[DstIndex] = MoveTemp(this->CharacterActors[SrcIndex]);
}

void USkelotCharacterSyncComponent::CustomInstanceData_SetNum(int NewNum)
{
	this->CharacterActors.SetNum(NewNum);
}

void USkelotCharacterSyncComponent::OnAnimationFinished(const TArray<FSkelotAnimFinishEvent>& Events)
{

}

void USkelotCharacterSyncComponent::OnAnimationNotify(const TArray<FSkelotAnimNotifyEvent>& Events)
{

}

void USkelotCharacterSyncComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	CopyTransforms();
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USkelotCharacterSyncComponent::CopyTransforms()
{
	for (int InstanceIndex = 0; InstanceIndex < this->GetInstanceCount(); InstanceIndex++)
	{
		if (this->IsInstanceAlive(InstanceIndex))
		{
			ACharacter* Chr = CharacterActors[InstanceIndex].Get();
			if (Chr)
			{
				this->SetInstanceTransform(InstanceIndex, MeshTransform * FTransform3f(Chr->GetActorTransform()));
			}
// 			else
// 			{
// 				this->DestroyInstance(InstanceIndex);
// 			}
		}
	}
}

