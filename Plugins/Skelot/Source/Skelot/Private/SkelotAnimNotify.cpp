#include "SkelotAnimNotify.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SkelotComponent.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Animation/AnimSequenceBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

void UAnimNotify_SkelotPlaySound::OnNotify(USkelotComponent* Component, int32 InstanceIndex, UAnimSequenceBase* Animation) const
{
	if (Sound)
	{
		if (!Sound->IsOneShot())
			return;

		if (bFollow)
		{
			UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(Sound, Component, AttachName, FVector(ForceInit), EAttachLocation::SnapToTarget, false, VolumeMultiplier, PitchMultiplier);
			Component->InstanceAttachChildComponent(InstanceIndex, AC, FTransform3f::Identity, AttachName, true);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(Component->GetWorld(), Sound, (FVector)Component->GetInstanceLocation(InstanceIndex), VolumeMultiplier, PitchMultiplier);
		}
	}
}

// void UAnimNotify_SkelotPlayNiagaraEffect::FillFrom(const UAnimNotify_PlayNiagaraEffect* Src)
// {
// 	
//}
// 
// void UAnimNotify_SkelotPlaySound::FillFrom(const UAnimNotify_PlaySound* Src)
// {
// 
// 
// 	Sound = Src->Sound;
// 	VolumeMultiplier = Src->VolumeMultiplier;
// 	PitchMultiplier = Src->PitchMultiplier;
// 	#if WITH_EDITORONLY_DATA
// 	bPreviewIgnoreAttenuation = Src->bPreviewIgnoreAttenuation;
// 	#endif
// 	bFollow = Src->bFollow;
// 	AttachName = Src->AttachName;
// 
// 
// }

void UAnimNotify_SkelotPlayNiagaraEffect::OnNotify(USkelotComponent* Component, int32 InstanceIndex, UAnimSequenceBase* Animation) const
{
	if (!Template || Template->IsLooping())
		return;

	FTransform LocalTransform(RotationOffsetQuat.GetNormalized(), LocationOffset, Scale);
	const FTransform MeshTransform = LocalTransform * FTransform(Component->GetInstanceSocketTransform(InstanceIndex, SocketName));
	UNiagaraComponent* NComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Component->GetWorld(), Template, MeshTransform.GetLocation(), MeshTransform.GetRotation().Rotator(), MeshTransform.GetScale3D(), true);
	if (NComp)
	{
		if (Attached)
		{
			Component->InstanceAttachChildComponent(InstanceIndex, NComp, (FTransform3f)LocalTransform, SocketName, true);
		}

		if(LifeSpan > 0)
		{
			FTimerHandle TH;
			Component->GetWorld()->GetTimerManager().SetTimer(TH, FTimerDelegate::CreateUObject(NComp, &UAudioComponent::DestroyComponent, true), LifeSpan, false);
		}
	}
}


