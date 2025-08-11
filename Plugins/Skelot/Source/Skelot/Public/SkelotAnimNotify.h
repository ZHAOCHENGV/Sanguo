#pragma once

#include "UObject/Interface.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayNiagaraEffect.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"


#include "SkelotAnimNotify.generated.h"

class USkelotComponent;

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class USkelotNotifyInterface : public UInterface
{
	GENERATED_BODY()
};

class ISkelotNotifyInterface
{
public:
	GENERATED_BODY()
	virtual void OnNotify(USkelotComponent* Component, int32 InstanceIndex, UAnimSequenceBase* Animation) const {}
};


UCLASS(const, abstract, hidecategories=Object, collapsecategories, Config = Game, meta=(DisplayName="Skelot Notify"))
class SKELOT_API UAnimNotify_Skelot : public UAnimNotify, public ISkelotNotifyInterface
{
	GENERATED_BODY()
public:
};


UCLASS(abstract, Blueprintable)
class SKELOT_API UAnimNotify_SkelotBP : public UAnimNotify_Skelot
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnNotify"))
	void BP_OnNotify(USkelotComponent* Component, int32 InstanceIndex, UAnimSequenceBase* Animation) const;

	void OnNotify(USkelotComponent* Component, int32 InstanceIndex, UAnimSequenceBase* Animation) const final { BP_OnNotify(Component, InstanceIndex, Animation); }
};


UCLASS(meta = (DisplayName = "Skelot Play Sound"))
class SKELOT_API UAnimNotify_SkelotPlaySound : public UAnimNotify_PlaySound, public ISkelotNotifyInterface
{
	GENERATED_BODY()
public:
	void OnNotify(USkelotComponent* Component, int32 InstanceIndex, UAnimSequenceBase* Animation) const override;
};




UCLASS(meta = (DisplayName = "Skelot Play Niagara"))
class SKELOT_API UAnimNotify_SkelotPlayNiagaraEffect : public UAnimNotify_PlayNiagaraEffect, public ISkelotNotifyInterface
{
	GENERATED_BODY()
public:	
	//lifespan of the spawned particle, can be used instead of NotifyState since Skelot doesn't support it yet
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	float LifeSpan = -1;

	void OnNotify(USkelotComponent* Component, int32 InstanceIndex, UAnimSequenceBase* Animation) const override;
};

