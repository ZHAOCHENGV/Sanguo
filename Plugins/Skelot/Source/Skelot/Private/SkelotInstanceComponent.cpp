// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotInstanceComponent.h"
#include "SkelotComponent.h"
#include "SkelotAnimCollection.h"


USkelotInstanceComponent::USkelotInstanceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USkelotInstanceComponent::OnHiddenInGameChanged()
{
	Super::OnHiddenInGameChanged();
	if (SkelotComponent && SkelotComponent->IsInstanceValid(InstanceIndex))
	{
		SkelotComponent->SetInstanceHidden(InstanceIndex, this->bHiddenInGame);
	}
}

void USkelotInstanceComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport /*= ETeleportType::None*/)
{
	if (SkelotComponent && SkelotComponent->IsInstanceValid(InstanceIndex))
	{
		SkelotComponent->SetInstanceTransform(InstanceIndex, (FTransform3f)this->GetComponentTransform());
	}
}

void USkelotInstanceComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	DestroySkelotInstance();
}

void USkelotInstanceComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);
	if (SkelotComponent && SkelotComponent->IsInstanceValid(InstanceIndex))
	{
		SkelotComponent->AddInstanceLocation(InstanceIndex, FVector3f(InOffset));
	}
}

void USkelotInstanceComponent::OnRegister()
{
	Super::OnRegister();
}

void USkelotInstanceComponent::OnUnregister()
{
	Super::OnUnregister();
	DestroySkelotInstance();
}

void USkelotInstanceComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USkelotInstanceComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	DestroySkelotInstance();
}

FTransform USkelotInstanceComponent::GetSocketTransform(FName SocketName, ERelativeTransformSpace TransformSpace) const
{
	if (!SocketName.IsNone() && SkelotComponent && SkelotComponent->AnimCollection && SkelotComponent->IsInstanceValid(InstanceIndex))
	{
		FTransform ST = (FTransform)SkelotComponent->GetInstanceSocketTransform(InstanceIndex, SocketName, nullptr, false);
		return ST * Super::GetSocketTransform(SocketName, TransformSpace);
	}

	return Super::GetSocketTransform(SocketName, TransformSpace);
}

bool USkelotInstanceComponent::DoesSocketExist(FName InSocketName) const
{
	if (SkelotComponent && SkelotComponent->AnimCollection)
	{
		return SkelotComponent->IsSocketAvailable(InSocketName);
	}
	return false;
}

bool USkelotInstanceComponent::HasAnySockets() const
{
	if (SkelotComponent && SkelotComponent->AnimCollection)
		return SkelotComponent->AnimCollection->BonesToCache.Num() > 0;

	return false;
}

void USkelotInstanceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateChildTransforms(EUpdateTransformFlags::OnlyUpdateIfUsingSocket);
}

void USkelotInstanceComponent::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	Super::QuerySupportedSockets(OutSockets);
	// ?
}
void USkelotInstanceComponent::BindSkelotInstance(USkelotComponent* InComponent, int32 InInstanceIndex)
{
	DestroySkelotInstance();
	if (IsValid(InComponent) && InInstanceIndex != -1)
	{	
		bWantsOnUpdateTransform = true;
		SkelotComponent = InComponent;
		InstanceIndex = InInstanceIndex;
	}
}


void USkelotInstanceComponent::DestroySkelotInstance()
{
	if (SkelotComponent)
	{
		bWantsOnUpdateTransform = false;
		SkelotComponent->DestroyInstance(InstanceIndex);
		InstanceIndex = -1;
		SkelotComponent = nullptr;
	}
}

float USkelotInstanceComponent::PlayAnimation(UAnimSequenceBase* Animation, bool bLoop /*= true*/, float StartAt /*= 0*/, float PlayScale /*= 1*/, float TransitionDuration /*= 0.2f*/, EAlphaBlendOption BlendOption /*= EAlphaBlendOption::Linear*/, bool bIgnoreTransitionGeneration /*= false*/)
{
	if(SkelotComponent)
		return SkelotComponent->InstancePlayAnimation(InstanceIndex, Animation, bLoop, StartAt, PlayScale, TransitionDuration,  BlendOption, bIgnoreTransitionGeneration);

	return -1;
}

float USkelotInstanceComponent::InstancePlayRandomLoopedSequence(FName Name, float StartAt /*= 0*/, float PlayScale /*= 1*/, float TransitionDuration /*= 0.2f*/, EAlphaBlendOption BlendOption /*= EAlphaBlendOption::Linear*/, bool bIgnoreTransitionGeneration /*= false*/)
{
	if(SkelotComponent)
		SkelotComponent->InstancePlayRandomLoopedSequence(InstanceIndex, Name, StartAt, PlayScale, TransitionDuration, BlendOption, bIgnoreTransitionGeneration);
	
	return -1;
}

