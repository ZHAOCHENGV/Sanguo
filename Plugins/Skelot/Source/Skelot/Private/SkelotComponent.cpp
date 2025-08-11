// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotComponent.h"
#include "SkelotPrivate.h"
#include "SkelotRender.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkelotComponent.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Misc/MemStack.h"
#include "SkelotAnimCollection.h"
#include "AnimationRuntime.h"
#include "Math/GenericOctree.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include "Chaos/Sphere.h"
#include "Chaos/Box.h"
#include "Chaos/Capsule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "SkelotPrivateUtils.h"
#include "ContentStreaming.h"
#include "SkelotObjectVersion.h"
#include "UObject/UObjectIterator.h"
#include "UnrealEngine.h"
#include "SceneInterface.h"
#include "Engine/World.h"
#include "RHICommandList.h"
#include "Animation/Skeleton.h"
#include "SkelotAnimNotify.h"



float GSkelot_LocalBoundUpdateInterval = 1 / 25.0f;
FAutoConsoleVariableRef CVar_LocalBoundUpdateInterval(TEXT("Skelot.LocalBoundUpdateInterval"), GSkelot_LocalBoundUpdateInterval, TEXT(""), ECVF_Default);

bool GSkelot_CallAnimNotifies = true;
FAutoConsoleVariableRef CVar_CallAnimNotifies(TEXT("Skelot.CallAnimNotifies"), GSkelot_CallAnimNotifies, TEXT(""), ECVF_Default);

SKELOT_AUTO_CVAR_DEBUG(bool, DebugAnimations, false, "", ECVF_Default);
SKELOT_AUTO_CVAR_DEBUG(bool, DebugTransitions, false, "", ECVF_Default);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
FAutoConsoleCommand ConsoleCmd_PrintAllTransitions(TEXT("Skelot_DebugPrintTransitions"), TEXT(""), FConsoleCommandDelegate::CreateLambda([]() {
	
	for(USkelotAnimCollection* AC : TObjectRange<USkelotAnimCollection>())
	{
		FString A, B;
		AC->TransitionPoseAllocator.DebugPrint(A, B);
		UE_LOG(LogSkelot, Log, TEXT("%s"), *A);
		UE_LOG(LogSkelot, Log, TEXT("%s"), *B);
	}

}), ECVF_Default);
#endif



//same as FAnimationRuntime::AdvanceTime but we dont support negative play scale
inline ETypeAdvanceAnim AnimAdvanceTime(bool bAllowLooping, float MoveDelta, float& InOutTime, float EndTime)
{
	InOutTime += MoveDelta;

	if (InOutTime > EndTime)
	{
		if (bAllowLooping)
		{
			InOutTime = FMath::Fmod(InOutTime, EndTime);
			return ETAA_Looped;
		}
		else
		{
			// If not, snap time to end of sequence and stop playing.
			InOutTime = EndTime;
			return ETAA_Finished;
		}
	}

	return ETAA_Default;
}

void FSkelotInstanceAnimState::Tick(USkelotComponent* Owner, int32 InstanceIndex, float Delta)
{
	USkelotAnimCollection* AnimCollection = Owner->AnimCollection;
	ESkelotInstanceFlags& Flags = Owner->InstancesData.Flags[InstanceIndex];

	if (EnumHasAnyFlags(Flags, ESkelotInstanceFlags::EIF_Destroyed | ESkelotInstanceFlags::EIF_AnimPaused | ESkelotInstanceFlags::EIF_AnimNoSequence | ESkelotInstanceFlags::EIF_DynamicPose))
		return;	

	if (EnumHasAnyFlags(Flags, ESkelotInstanceFlags::EIF_AnimSkipTick))
	{
		EnumRemoveFlags(Flags, ESkelotInstanceFlags::EIF_AnimSkipTick);
		return;
	}

	const FSkelotSequenceDef& ActiveSequenceStruct = AnimCollection->Sequences[CurrentSequence];

	if (EnumHasAnyFlags(Flags, ESkelotInstanceFlags::EIF_AnimFinished))
	{
		EnumRemoveFlags(Flags, ESkelotInstanceFlags::EIF_AnimFinished);
		EnumAddFlags(Flags, ESkelotInstanceFlags::EIF_AnimNoSequence);
		auto& EvArray = IsRandomSequencePlayMode() ? Owner->AnimationFinishEvents_RandomMode : Owner->AnimationFinishEvents;
		EvArray.Add(FSkelotAnimFinishEvent{ InstanceIndex, ActiveSequenceStruct.Sequence });
		return;
	}

	float OldTime = Time;
	float NewDelta = PlayScale * Delta;
	//if(newDelta <= 0)
	//	return;

	const bool bShouldLoop = EnumHasAnyFlags(Flags, ESkelotInstanceFlags::EIF_AnimLoop);

	ETypeAdvanceAnim result = AnimAdvanceTime(bShouldLoop, NewDelta, Time, ActiveSequenceStruct.GetSequenceLength());

	//get notifications
	if (ActiveSequenceStruct.Notifies.Num() != 0)
	{
		if (result == ETAA_Looped)
		{
			for (int i = ActiveSequenceStruct.Notifies.Num() - 1; i >= 0; i--)
			{
				const FSkelotAnimNotifyDesc& Notify = ActiveSequenceStruct.Notifies[i];
				if ((OldTime < Notify.Time) || (Time >= Notify.Time))
				{
					if(Notify.Notify)
						Owner->AnimationNotifyObjectEvents.Add(FSkelotAnimNotifyObjectEvent { InstanceIndex, ActiveSequenceStruct.Sequence, Notify.NotifyInterface });
					else
						Owner->AnimationNotifyEvents.Add(FSkelotAnimNotifyEvent{ InstanceIndex, ActiveSequenceStruct.Sequence, Notify.Name });
				}
			}
		}
		else
		{
			for (int i = 0; i < ActiveSequenceStruct.Notifies.Num(); i++)
			{
				const FSkelotAnimNotifyDesc& Notify = ActiveSequenceStruct.Notifies[i];
				if (Time >= Notify.Time && OldTime < Notify.Time)
				{
					if (Notify.Notify)
						Owner->AnimationNotifyObjectEvents.Add(FSkelotAnimNotifyObjectEvent{ InstanceIndex, ActiveSequenceStruct.Sequence, Notify.NotifyInterface });
					else
						Owner->AnimationNotifyEvents.Add(FSkelotAnimNotifyEvent{ InstanceIndex, ActiveSequenceStruct.Sequence, Notify.Name });
				}
			}
		}
	}

	int LocalFrameIndex = static_cast<int>(Time * ActiveSequenceStruct.SampleFrequencyFloat);
	check(LocalFrameIndex < ActiveSequenceStruct.AnimationFrameCount);
	int GlobalFrameIndex = ActiveSequenceStruct.AnimationFrameIndex + LocalFrameIndex;
	check(GlobalFrameIndex < AnimCollection->FrameCountSequences);

	if (EnumHasAnyFlags(Flags, ESkelotInstanceFlags::EIF_AnimPlayingTransition))
	{
		const USkelotAnimCollection::FTransition& Transition = AnimCollection->Transitions[this->TransitionIndex];
		check((Transition.ToFI + Transition.FrameCount) <= ActiveSequenceStruct.AnimationFrameCount);
		if((LocalFrameIndex >= (Transition.ToFI + Transition.FrameCount)) || result != ETAA_Default)
		{
			EnumRemoveFlags(Flags, ESkelotInstanceFlags::EIF_AnimPlayingTransition);
			AnimCollection->DecTransitionRef(this->TransitionIndex);
		}
		else
		{
			int TransitionLFI = LocalFrameIndex - Transition.ToFI;
			check(TransitionLFI < Transition.FrameCount);
			Owner->InstancesData.FrameIndices[InstanceIndex] = Transition.FrameIndex + TransitionLFI;
			return;
		}
	}
	
	Owner->InstancesData.FrameIndices[InstanceIndex] = GlobalFrameIndex;

	if (result == ETAA_Finished)
	{
		check(LocalFrameIndex == ActiveSequenceStruct.AnimationFrameCount - 1);
		//we don't finish right away, because current frame must be rendered
		EnumAddFlags(Flags, ESkelotInstanceFlags::EIF_AnimFinished);
	}
}

namespace Utils
{
	template<bool bGlobal> int TransitionFrameRangeToSeuqnceFrameRange(const FSkelotInstanceAnimState& AS, int FrameIndex /*frame index in transition range*/, const USkelotAnimCollection* AnimCollection)
	{
		checkSlow(AnimCollection->IsTransitionFrameIndex(FrameIndex));
		const USkelotAnimCollection::FTransition& Transition = AnimCollection->Transitions[AS.TransitionIndex];
		const int TransitionFrameIndex = FrameIndex - Transition.FrameIndex;
		check(TransitionFrameIndex < Transition.FrameCount);
		const FSkelotSequenceDef& SeqDef = AnimCollection->Sequences[AS.CurrentSequence];
		const int SeqFrameIndex = Transition.ToFI + TransitionFrameIndex;
		check(SeqFrameIndex < SeqDef.AnimationFrameCount);
		return bGlobal ? SeqDef.AnimationFrameIndex + SeqFrameIndex : SeqFrameIndex;
	}
};





USkelotComponent::USkelotComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;
	InstanceMaxDrawDistance = 0;
	InstanceMinDrawDistance = 0;

	bCanEverAffectNavigation = false;
	
	Mobility = EComponentMobility::Movable;
	bCastStaticShadow = false;
	bCastDynamicShadow = true;
	bWantsInitializeComponent = true;
	bComputeFastLocalBounds = true;

	//SortMode = ESkelotInstanceSortMode::SortTranslucentOnly;

	LODDistances[0] = 1000;
	LODDistances[1] = 3000;
	LODDistances[2] = 8000;
	LODDistances[3] = 14000;
	LODDistances[4] = 20000;
	LODDistances[5] = 40000;
	LODDistances[6] = 80000;
	
	LODDistanceScale = 1;

	//MinLODIndex = 0;
	//MaxLODIndex = 0xFF;

	AnimationPlayRate = 1;
	MaxMeshPerInstance = 4;
	MaxAttachmentPerInstance = 0;

	bCallAnimNotifies = true;
	bCallAnimationFinish = true;
}

void USkelotComponent::PostApplyToComponent()
{
	Super::PostApplyToComponent();
}

void USkelotComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
}

void USkelotComponent::FixInstanceData()
{
	InstancesData.Matrices.SetNum(InstancesData.Locations.Num());
	//InstancesData.RenderMatrices.SetNum(InstancesData.Locations.Num());
	InstancesData.FrameIndices.SetNum(InstancesData.Locations.Num());
	InstancesData.RenderCustomData.SetNumZeroed(InstancesData.Locations.Num() * NumCustomDataFloats);
	InstancesData.MeshSlots.SetNumZeroed(InstancesData.Locations.Num() * (MaxMeshPerInstance + 1));

	for (int InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)
	{
		if (IsInstanceAlive(InstanceIndex))
			OnInstanceTransformChange(InstanceIndex);
	}

	for (int InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)
	{
		if (IsInstanceAlive(InstanceIndex))
		{
			FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
			bool bValidSeq = (AS.IsValid() && AnimCollection->Sequences.IsValidIndex(AS.CurrentSequence) && AnimCollection->Sequences[AS.CurrentSequence].Sequence);
			if (!bValidSeq)
			{
				AS = FSkelotInstanceAnimState();
			}
		}
	}

	CalcAnimationFrameIndices();

	if (ShouldUseFixedInstanceBound())
	{
		InstancesData.LocalBounds.Empty();
	}
	else
	{
		InstancesData.LocalBounds.SetNum(InstancesData.Locations.Num());
		UpdateLocalBounds();
	}

	//for (FArrayProperty* Arr : GetBPInstanceDataArrays())
	//{
	//	FScriptArrayHelper Helper(Arr, Arr->GetPropertyValuePtr_InContainer(this));
	//	Helper.Resize(InstancesData.Locations.Num());
	//}

}

#if WITH_EDITOR

void USkelotComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PrpName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : FName();

	if (PrpName == GET_MEMBER_NAME_CHECKED(USkelotComponent, AnimCollection))
	{
		USkelotAnimCollection* CurAC = AnimCollection;
		AnimCollection = nullptr;
		SetAnimCollection(CurAC);
	}

	if (PrpName == GET_MEMBER_NAME_CHECKED(USkelotComponent, Submeshes))
	{
		
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	MarkRenderStateDirty();
}

void USkelotComponent::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(USkelotComponent, AnimCollection))
	{
		SetAnimCollection(nullptr);
	}

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(USkelotComponent, Submeshes))
	{
		ResetMeshSlots();
	}
}

bool USkelotComponent::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
		return false;
	
	// Specific logic associated with "MyProperty"
	const FName PropertyName = InProperty->GetFName();

	return true;
}

#endif


void USkelotComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	for (int InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)
	{
		if(IsInstanceAlive(InstanceIndex))
			AddInstanceLocation(InstanceIndex, FVector3f(InOffset));
	}
}

void USkelotComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);


	if(GetAliveInstanceCount() > 0 && AnimCollection)
	{
		if(!bIgnoreAnimationsTick)
			this->TickAnimations(DeltaTime);

		UpdateAttachedComponents();

		if(IsVisible() && IsRenderStateCreated() && this->SceneProxy)
		{
			MarkRenderTransformDirty();
		}
	}

	{
		if(bCallAnimNotifies && GSkelot_CallAnimNotifies)
			CallOnAnimationNotify();
		
		if(bCallAnimationFinish)
			CallOnAnimationFinished();
		

		AnimationFinishEvents.Reset();
		AnimationFinishEvents_RandomMode.Reset();
		AnimationNotifyEvents.Reset();
		AnimationNotifyObjectEvents.Reset();

		FillDynamicPoseFromComponents_Concurrent();
	}


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GSkelot_DebugAnimations || GSkelot_DebugTransitions)
	{
		for (int InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)
		{
			if (!IsInstanceAlive(InstanceIndex))
				continue;

			if(GSkelot_DebugAnimations)
			{
				FSkelotInstanceAnimState& state = InstancesData.AnimationStates[InstanceIndex];
				FString Str = FString::Printf(TEXT("Time:%f, FrameIndex:%d"), state.Time, (int)InstancesData.FrameIndices[InstanceIndex]);
				DrawDebugString(GetWorld(), FVector(GetInstanceLocation(InstanceIndex)), Str, nullptr, FColor::Green, 0, false, 2);
			}
					
		}
	}
	INC_DWORD_STAT_BY(STAT_SKELOT_TotalInstances, GetAliveInstanceCount());
#endif

}

void USkelotComponent::SendRenderTransform_Concurrent()
{
	//DoDeferredRenderUpdates_Concurrent is not override-able so we are doing all of our end of frame computes here. its not just transform. 

	SKELOT_SCOPE_CYCLE_COUNTER(SendRenderTransform_Concurrent);

	FBoxSphereBounds OriginalBounds = this->Bounds;

	FSkelotProxy* SKProxy = static_cast<FSkelotProxy*>(this->SceneProxy);

	// If the primitive isn't hidden update its transform.
	const bool bDetailModeAllowsRendering = DetailMode <= GetCachedScalabilityCVars().DetailMode;
	if (SKProxy && bDetailModeAllowsRendering && (ShouldRender() || bCastHiddenShadow || bAffectIndirectLightingWhileHidden || bRayTracingFarField))
	{
		
		FSkelotDynamicData* DynamicData = GenerateDynamicData_Internal();
		//no need to call UpdateBounds because we have a calculated bound now
		this->Bounds = FBoxSphereBounds(DynamicData->CompBound.ToBoxSphereBounds());
		check(!this->Bounds.ContainsNaN());

		// Update the scene info's transform for this primitive.
		GetWorld()->Scene->UpdatePrimitiveTransform(this);
		
		//send dynamic data to render thread proxy
		ENQUEUE_RENDER_COMMAND(Skelot_SendRenderDynamicData)([=](FRHICommandListImmediate& RHICmdList) {
			SKProxy->SetDynamicDataRT(DynamicData);
		});
	}
	else
	{
		UpdateBounds();
	}

	UActorComponent::SendRenderTransform_Concurrent();

#if WITH_EDITOR //copied from UPrimitiveComponent::UpdateBounds()
	if (IsRegistered() && (GetWorld() != nullptr) && !GetWorld()->IsGameWorld() && (!OriginalBounds.Origin.Equals(Bounds.Origin) || !OriginalBounds.BoxExtent.Equals(Bounds.BoxExtent)))
	{
		if (!bIgnoreStreamingManagerUpdate && !bHandledByStreamingManagerAsDynamic && bAttachedToStreamingManagerAsStatic)
		{
			FStreamingManagerCollection* Collection = IStreamingManager::Get_Concurrent();
			if (Collection)
			{
				Collection->NotifyPrimitiveUpdated_Concurrent(this);
			}
		}
	}
#endif

}

void USkelotComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);
	SendRenderTransform_Concurrent();
	
}

void USkelotComponent::PostLoad()
{
	Super::PostLoad();

	if(SkeletalMesh_DEPRECATED && GetClass() /*!= USkelotComponent::StaticClass() && !this->HasAnyFlags(RF_ClassDefaultObject)*/)
	{
		if(Submeshes.Num() == 0)
			Submeshes.AddDefaulted();

		if(Submeshes[0].SkeletalMesh == nullptr)
			Submeshes[0].SkeletalMesh = SkeletalMesh_DEPRECATED;

		SkeletalMesh_DEPRECATED = nullptr;
	}
}

void USkelotComponent::BeginDestroy()
{
	ClearInstances(true);
	Super::BeginDestroy();
}

void USkelotComponent::PostInitProperties()
{
	Super::PostInitProperties();


}

void USkelotComponent::PostCDOContruct()
{
	Super::PostLoad();

	//for (FProperty* PropertyIter : TFieldRange<FProperty>(this->GetClass(), EFieldIteratorFlags::IncludeSuper))
	//{
	//	if (FArrayProperty* ArrayPrp = CastField<FArrayProperty>(PropertyIter))
	//	{
	//		if (!ArrayPrp->IsNative())
	//		{
	//			if (ArrayPrp->GetName().StartsWith(TEXT("InstanceData_")))
	//			{
	//				InstanceDataArrays.AddUnique(ArrayPrp);
	//			}
	//		}
	//	}
	//}
}

void USkelotComponent::OnVisibilityChanged()
{
	Super::OnVisibilityChanged();
	MarkRenderTransformDirty();
}

void USkelotComponent::OnHiddenInGameChanged()
{
	Super::OnHiddenInGameChanged();
	MarkRenderTransformDirty();
}

void USkelotComponent::BeginPlay()
{
	Super::BeginPlay();
}


void USkelotComponent::EndPlay(EEndPlayReason::Type Reason)
{
	ClearInstances(true);
	Super::EndPlay(Reason);
}

int32 USkelotComponent::GetNumMaterials() const
{
	int Counter = 0;
	for (const FSkelotSubmeshSlot& MeshSlot : Submeshes)
	{
		if (MeshSlot.SkeletalMesh)
		{
			Counter += MeshSlot.SkeletalMesh->GetMaterials().Num();
		}
	}
	return Counter;
}

UMaterialInterface* USkelotComponent::GetMaterial(int32 MaterialIndex) const
{
	if (OverrideMaterials.IsValidIndex(MaterialIndex) && OverrideMaterials[MaterialIndex])
		return OverrideMaterials[MaterialIndex];

	for (const FSkelotSubmeshSlot& MeshSlot : Submeshes)
	{
		if (MeshSlot.SkeletalMesh)
		{
			const TArray<FSkeletalMaterial>& Mats = MeshSlot.SkeletalMesh->GetMaterials();
			if (MaterialIndex >= Mats.Num())
			{
				MaterialIndex -= Mats.Num();
			}
			else
			{
				return Mats[MaterialIndex].MaterialInterface;
			}
		}
	}

	return nullptr;
}

int32 USkelotComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	int Counter = 0;
	for (const FSkelotSubmeshSlot& MeshSlot : Submeshes)
	{
		if (MeshSlot.SkeletalMesh)
		{
			const TArray<FSkeletalMaterial>& SkeletalMeshMaterials = MeshSlot.SkeletalMesh->GetMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMeshMaterials.Num(); ++MaterialIndex)
			{
				const FSkeletalMaterial& SkeletalMaterial = SkeletalMeshMaterials[MaterialIndex];
				if (SkeletalMaterial.MaterialSlotName == MaterialSlotName)
					return Counter + MaterialIndex;
			}
			Counter += SkeletalMeshMaterials.Num();
		}
	}
	return INDEX_NONE;
}

TArray<FName> USkelotComponent::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	for (const FSkelotSubmeshSlot& MeshSlot : Submeshes)
	{
		if (MeshSlot.SkeletalMesh)
		{
			const TArray<FSkeletalMaterial>& SkeletalMeshMaterials = MeshSlot.SkeletalMesh->GetMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMeshMaterials.Num(); ++MaterialIndex)
			{
				const FSkeletalMaterial& SkeletalMaterial = SkeletalMeshMaterials[MaterialIndex];
				MaterialNames.Add(SkeletalMaterial.MaterialSlotName);
			}
		}
	}

	return MaterialNames;
}

bool USkelotComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) >= 0;
}




void USkelotComponent::TickAnimations(float DeltaTime)
{
	SKELOT_SCOPE_CYCLE_COUNTER(AnimationsTick);

	//GAnimFinishEvents.Reset();
	//GAnimNotifyEvents.Reset();

	DeltaTime *= AnimationPlayRate;
	if (DeltaTime <= 0)
		return;

	TimeSinceLastLocalBoundUpdate += DeltaTime;

	for (int InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)
	{
		FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
		AS.Tick(this, InstanceIndex, DeltaTime);
	}
	
}




void USkelotComponent::CalcAnimationFrameIndices()
{
	FMemory::Memzero(InstancesData.FrameIndices.GetData(), InstancesData.FrameIndices.Num() * InstancesData.FrameIndices.GetTypeSize());

	if(!AnimCollection)
		return;

	for (int InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)
	{
		if (IsInstanceAlive(InstanceIndex))
		{
			const bool bShouldLoop = InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_AnimLoop);
			const FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];

			if(AS.IsValid() && !InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_DynamicPose | ESkelotInstanceFlags::EIF_AnimPlayingTransition))
			{
				const FSkelotSequenceDef& ActiveSequenceStruct = AnimCollection->Sequences[AS.CurrentSequence];
				float AnimTime = AS.Time;
				AnimAdvanceTime(bShouldLoop, 0, AnimTime, ActiveSequenceStruct.GetSequenceLength());
				InstancesData.FrameIndices[InstanceIndex] = AnimCollection->CalcFrameIndex(ActiveSequenceStruct, AnimTime);
			}
		}
	}
}



void USkelotComponent::SetLODDistanceScale(float NewLODDistanceScale)
{
	LODDistanceScale = FMath::Max(0.000001f, NewLODDistanceScale);
	FSkelotProxy* SKProxy = static_cast<FSkelotProxy*>(this->SceneProxy);
	if(SKProxy)
	{
		ENQUEUE_RENDER_COMMAND(Skelot_SetLODDistanceScale)([this](FRHICommandListImmediate& RHICmdList) {
			FSkelotProxy* LocalSKProxy = static_cast<FSkelotProxy*>(this->SceneProxy);
			LocalSKProxy->DistanceScale = this->LODDistanceScale;
		});
	}
}

void USkelotComponent::SetAnimCollection(USkelotAnimCollection* asset)
{
	if (AnimCollection != asset)
	{
		ResetAnimationStates();
		ResetMeshSlots();
		
		AnimCollection = asset;
		CheckAssets_Internal();
	}
}


void USkelotComponent::ResetInstanceAnimationState(int InstanceIndex)
{
	check(IsInstanceValid(InstanceIndex));

	if(this->InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_DynamicPose))
	{
		if (this->InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_BoundToSMC))
		{
			int DPI = AnimCollection->FrameIndexToDynamicPoseIndex(this->InstancesData.FrameIndices[InstanceIndex]);
			DynamicPoseInstancesTiedToSMC.FindAndRemoveChecked(DPI);
		}

		ReleaseDynamicPose_Internal(InstanceIndex);
	}
	else if(this->InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_AnimPlayingTransition))
	{
		AnimCollection->DecTransitionRef(InstancesData.AnimationStates[InstanceIndex].TransitionIndex);
	}

	InstanceRemoveFlags(InstanceIndex, ESkelotInstanceFlags::EIF_AllAnimationFlags | ESkelotInstanceFlags::EIF_DynamicPose | ESkelotInstanceFlags::EIF_BoundToSMC);
	InstanceAddFlags(InstanceIndex, ESkelotInstanceFlags::EIF_AnimNoSequence | ESkelotInstanceFlags::EIF_New | ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate);

	InstancesData.AnimationStates[InstanceIndex] = FSkelotInstanceAnimState();
	InstancesData.FrameIndices[InstanceIndex] = 0;
}

bool USkelotComponent::IsInstanceValid(int32 InstanceIndex) const
{
	return InstancesData.Flags.IsValidIndex(InstanceIndex) && IsInstanceAlive(InstanceIndex);
}


void USkelotComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();
}


int USkelotComponent::AddInstance(const FTransform3f& worldTransform)
{
	//if(GetAliveInstanceCount() == 0)
	//{
	//	if (IsRenderStateCreated())
	//		MarkRenderStateDirty();
	//	else if (ShouldCreateRenderState())
	//		RecreateRenderState_Concurrent();
	//}

	check(worldTransform.IsValid());

	NumAliveInstance++;
	int InstanceIndex = IndexAllocator.Allocate();

	if (IndexAllocator.GetMaxSize() > InstancesData.Flags.Num())
	{
		const int Begin = InstancesData.Flags.Num();
		const int Count = FSkelotInstancesData::LENGTH_ALIGN;

		InstancesData.Flags.AddUninitialized(Count);
		for (int i = 0; i < Count; i++)
			InstancesData.Flags[Begin + i] = ESkelotInstanceFlags::EIF_Destroyed;

		InstancesData.FrameIndices.AddUninitialized(Count);
		InstancesData.AnimationStates.AddUninitialized(Count);

		InstancesData.Locations.AddUninitialized(Count);
		InstancesData.Rotations.AddUninitialized(Count);
		InstancesData.Scales.AddUninitialized(Count);

		InstancesData.Matrices.AddUninitialized(Count);

		if (NumCustomDataFloats > 0)
			InstancesData.RenderCustomData.AddUninitialized(Count * NumCustomDataFloats);

		InstancesData.MeshSlots.AddUninitialized(Count * (this->MaxMeshPerInstance + 1));

		if (!ShouldUseFixedInstanceBound())
		{
			InstancesData.LocalBounds.AddUninitialized(Count);
		}
		if(MaxAttachmentPerInstance > 0)
		{
			InstancesData.Attachments.AddDefaulted(Count * MaxAttachmentPerInstance);
		}

		CallCustomInstanceData_SetNum(Begin + Count);

		//for (FArrayProperty* Arr : GetBPInstanceDataArrays())
		//{
		//	FScriptArrayHelper Helper(Arr, Arr->GetPropertyValuePtr_InContainer(this));
		//	Helper.AddValues(Count);
		//	check(Helper.Num() == InstancesData.Flags.Num());
		//}

		if (PerInstanceScriptStruct)
		{
			const int StructSize = PerInstanceScriptStruct->GetStructureSize();
			check(StructSize > 0);
			InstancesData.CustomPerInstanceStruct.AddUninitialized(Count * StructSize);
		}
	}


	InstancesData.FrameIndices[InstanceIndex] = 0;
	InstancesData.AnimationStates[InstanceIndex] = FSkelotInstanceAnimState();
	InstancesData.Flags[InstanceIndex] = ESkelotInstanceFlags::EIF_Default;
	
	check(MaxMeshPerInstance > 0);
	{
		uint8* MeshSlots = this->GetInstanceMeshSlots(InstanceIndex);
		MeshSlots[0] = this->InstanceDefaultAttachIndex;
		MeshSlots[1] = 0xFF;
	}
	

	if(NumCustomDataFloats > 0)
		ZeroInstanceCustomData(InstanceIndex);

	SetInstanceTransform(InstanceIndex, worldTransform);

	if (PerInstanceScriptStruct)
	{
		const int StructSize = PerInstanceScriptStruct->GetStructureSize();
		check(StructSize > 0);
		uint8* CSD = &InstancesData.CustomPerInstanceStruct[StructSize * InstanceIndex];
		check(IsAligned(CSD, PerInstanceScriptStruct->GetMinAlignment()));
		PerInstanceScriptStruct->InitializeStruct(CSD);
	}

	CallCustomInstanceData_Initialize(InstanceIndex);

	if(!IsRenderTransformDirty())
		MarkRenderTransformDirty();

	return InstanceIndex;
}

int USkelotComponent::AddInstance_CopyFrom(const USkelotComponent* Src, int SrcInstanceIndex)
{
	int InstanceIndex = AddInstance(FTransform3f::Identity);
	this->InstanceCopyFrom(InstanceIndex, Src, SrcInstanceIndex);
	return InstanceIndex;
}

bool USkelotComponent::DestroyInstance(int InstanceIndex)
{
	if (IsInstanceValid(InstanceIndex))
	{
		return DestroyAt_Internal(InstanceIndex);
	}

	return false;
}

void USkelotComponent::DestroyInstances(const TArray<int>& InstanceIndices)
{
	for (int Index : InstanceIndices)
	{
		DestroyInstance(Index);
	}
}

void USkelotComponent::DestroyInstancesByRange(int Index, int Count)
{
	for(int i = Index; i < (Index+Count); i++)
	{
		DestroyInstance(i);
	}
}

bool USkelotComponent::DestroyAt_Internal(int InstanceIndex)
{
	CallCustomInstanceData_Destroy(InstanceIndex);
	DestroyCustomStruct_Internal(InstanceIndex);
	ResetInstanceAnimationState(InstanceIndex);
	
	//those with bAutoDestroy=true need to be destroyed now
	ForEachChildOfInstance(InstanceIndex, [this](FSkelotComponentAttachData& AD) {
		if (IsValid(AD.Object) && AD.bAutoDestroy)
		{
			AD.DestroyObject();
		}
	});


	IndexAllocator.Free(InstanceIndex);

	if (IndexAllocator.GetNumPendingFreeSpans() >= 10)
		IndexAllocator.Consolidate();

	NumAliveInstance--;
	InstancesData.Flags[InstanceIndex] = ESkelotInstanceFlags::EIF_Destroyed;
	

	//if(GetAliveInstanceCount() == 0) 
	//{
	//	ClearInstances();
	//	MarkRenderStateDirty(); //won't create proxy if there is no instance
	//}

	if (!IsRenderTransformDirty())
		MarkRenderTransformDirty();

	return true;
}

void USkelotComponent::DestroyCustomStruct_Internal(int InstanceIndex)
{
	if (PerInstanceScriptStruct)
	{
		const int StructSize = PerInstanceScriptStruct->GetStructureSize();
		uint8* CSD = &InstancesData.CustomPerInstanceStruct[StructSize * InstanceIndex];
		PerInstanceScriptStruct->DestroyStruct(CSD); //ClearScriptStruct ?
	}
}

int USkelotComponent::K2_FlushInstances(TArray<int>& InOutRemapArray)
{
	int OldNumFree = GetDestroyedInstanceCount();
	if (OldNumFree <= FSkelotInstancesData::LENGTH_ALIGN)
		return 0;

	check(IsAligned(InstancesData.Flags.Num(), FSkelotInstancesData::LENGTH_ALIGN));

	const int OldInstanceCount = GetInstanceCount();
	InOutRemapArray.SetNumUninitialized(OldInstanceCount);
	IndexAllocator.Reset();
	IndexAllocator.Allocate(NumAliveInstance);

	int WriteIndex = 0;
	for (int InstanceIndex = 0; InstanceIndex < OldInstanceCount; InstanceIndex++)
	{
		if (EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Destroyed))
		{
			InOutRemapArray[InstanceIndex] = -1;
			continue;
		}

		InOutRemapArray[InstanceIndex] = WriteIndex;

		if (WriteIndex != InstanceIndex)
		{
			InstancesData.Flags[WriteIndex] = InstancesData.Flags[InstanceIndex] | ESkelotInstanceFlags::EIF_New;
			InstancesData.FrameIndices[WriteIndex] = InstancesData.FrameIndices[InstanceIndex];
			InstancesData.AnimationStates[WriteIndex] = InstancesData.AnimationStates[InstanceIndex];

			InstancesData.Locations[WriteIndex] = InstancesData.Locations[InstanceIndex];
			InstancesData.Rotations[WriteIndex] = InstancesData.Rotations[InstanceIndex];
			InstancesData.Scales[WriteIndex] = InstancesData.Scales[InstanceIndex];

			InstancesData.Matrices[WriteIndex] = InstancesData.Matrices[InstanceIndex];
			//InstancesData.RenderMatrices[WriteIndex] = InstancesData.RenderMatrices[InstanceIndex];
			
			if (InstancesData.LocalBounds.Num())
			{
				InstancesData.LocalBounds[WriteIndex] = InstancesData.LocalBounds[InstanceIndex];
			}

			if (MaxAttachmentPerInstance > 0)
			{
				FSkelotComponentAttachData* DstAD = &InstancesData.Attachments[WriteIndex * MaxAttachmentPerInstance];
				FSkelotComponentAttachData* SrcAD = &InstancesData.Attachments[InstanceIndex * MaxAttachmentPerInstance];
				SkelotElementCopy(DstAD, SrcAD, MaxAttachmentPerInstance);
			}

			if (NumCustomDataFloats)
				SkelotElementCopy(GetInstanceCustomDataFloats(WriteIndex), GetInstanceCustomDataFloats(InstanceIndex), NumCustomDataFloats);


			SkelotElementCopy(GetInstanceMeshSlots(WriteIndex), GetInstanceMeshSlots(InstanceIndex), MaxMeshPerInstance + 1);

			CallCustomInstanceData_Move(WriteIndex, InstanceIndex);

			//for (FArrayProperty* Arr : GetBPInstanceDataArrays())
			//{
			//	FScriptArray* ScriptArray = Arr->GetPropertyValuePtr_InContainer(this);
			//	uint8* ArrayData = (uint8*)ScriptArray->GetData();
			//	Arr->Inner->DestroyValue(ArrayData + (WriteIndex * Arr->Inner->ElementSize)); //#TODO do we need to call destroy or ... ?
			//	Arr->Inner->CopySingleValue(ArrayData + (WriteIndex * Arr->Inner->ElementSize), ArrayData + (InstanceIndex * Arr->Inner->ElementSize));
			//}
			if (PerInstanceScriptStruct)
			{
				const int StructSize = PerInstanceScriptStruct->GetStructureSize();
				uint8* CSD_Write = &InstancesData.CustomPerInstanceStruct[StructSize * WriteIndex];
				uint8* CSD_Read = &InstancesData.CustomPerInstanceStruct[StructSize * InstanceIndex];
				PerInstanceScriptStruct->CopyScriptStruct(CSD_Write, CSD_Read);
				PerInstanceScriptStruct->ClearScriptStruct(CSD_Read);
			}

		}

		WriteIndex++;
	}

	
	check(WriteIndex == NumAliveInstance);
	//arrays length need to be multiple of FSkelotInstancesData::LENGTH_ALIGN (SIMD Friendly)
	const int NewArrayLen = Align(WriteIndex, FSkelotInstancesData::LENGTH_ALIGN);
	for (int i = WriteIndex; i < NewArrayLen; i++)
		InstancesData.Flags[i] |= ESkelotInstanceFlags::EIF_Destroyed;

	InstanceDataSetNum_Internal(NewArrayLen);

	if (!IsRenderTransformDirty())
		MarkRenderTransformDirty();

	for (auto& PairData : DynamicPoseInstancesTiedToSMC)
	{
		PairData.Value.InstanceIndex = InOutRemapArray[PairData.Value.InstanceIndex];
	}

	return OldNumFree;
}

int USkelotComponent::FlushInstances(TArray<int>* InOutRemapArray)
{
	TArray<int> LocalRemap;
	return K2_FlushInstances(InOutRemapArray ? *InOutRemapArray : LocalRemap);
}


void USkelotComponent::RemoveTailDataIfAny()
{
	//#Note IndexAllocator can decrease MaxSize
	int RightSlack = InstancesData.Flags.Num() - IndexAllocator.GetMaxSize(); 
	if (RightSlack > (FSkelotInstancesData::LENGTH_ALIGN * 16))
	{
		const int NewArrayLen = Align(NumAliveInstance, FSkelotInstancesData::LENGTH_ALIGN);
		InstanceDataSetNum_Internal(NewArrayLen);
	}
}

void USkelotComponent::InstanceDataSetNum_Internal(int NewArrayLen)
{
	InstancesData.Flags.SetNumUninitialized(NewArrayLen, EAllowShrinking::Yes);
	InstancesData.FrameIndices.SetNumUninitialized(NewArrayLen, EAllowShrinking::Yes);
	InstancesData.AnimationStates.SetNumUninitialized(NewArrayLen, EAllowShrinking::Yes);

	InstancesData.Locations.SetNumUninitialized(NewArrayLen, EAllowShrinking::Yes);
	InstancesData.Rotations.SetNumUninitialized(NewArrayLen, EAllowShrinking::Yes);
	InstancesData.Scales.SetNumUninitialized(NewArrayLen, EAllowShrinking::Yes);

	InstancesData.Matrices.SetNumUninitialized(NewArrayLen, EAllowShrinking::Yes);

	if (NumCustomDataFloats > 0)
		InstancesData.RenderCustomData.SetNumUninitialized(NewArrayLen * NumCustomDataFloats, EAllowShrinking::Yes);

	if (InstancesData.LocalBounds.Num())
		InstancesData.LocalBounds.SetNumUninitialized(NewArrayLen, EAllowShrinking::Yes);

	InstancesData.MeshSlots.SetNumUninitialized(NewArrayLen * (MaxMeshPerInstance + 1), EAllowShrinking::Yes);

	//for (FArrayProperty* Arr : GetBPInstanceDataArrays())
	//{
	//	FScriptArrayHelper Helper(Arr, Arr->GetPropertyValuePtr_InContainer(this));
	//	Helper.Resize(NewArrayLen);
	//}

	if (PerInstanceScriptStruct)
	{
		const int StructSize = PerInstanceScriptStruct->GetStructureSize();
		InstancesData.CustomPerInstanceStruct.SetNumUninitialized(NewArrayLen * StructSize, EAllowShrinking::Yes);
	}

	CustomInstanceData_SetNum(NewArrayLen);
}

void USkelotComponent::ClearInstances(bool bEmptyOrReset)
{
	if(GetAliveInstanceCount() > 0)
	{
		DestroyInstancesByRange(0, GetInstanceCount());
		CallCustomInstanceData_SetNum(0);
	}

	NumAliveInstance = 0;
	if (bEmptyOrReset)
	{
		IndexAllocator.Reset();
		InstancesData.Empty();
	}
	else
	{
		IndexAllocator.Reset();
		InstancesData.Reset();
	}

	MarkRenderTransformDirty();
}


void USkelotComponent::InstanceCopyFrom(int InstanceIndex, const USkelotComponent* SrcComponent, int SrcInstanceIndex)
{
	if (IsValid(SrcComponent) && SrcComponent->IsInstanceValid(SrcInstanceIndex) && IsInstanceValid(InstanceIndex))
	{
		InstancesData.Locations[InstanceIndex] = SrcComponent->InstancesData.Locations[SrcInstanceIndex];
		InstancesData.Rotations[InstanceIndex] = SrcComponent->InstancesData.Rotations[SrcInstanceIndex];
		InstancesData.Scales[InstanceIndex] = SrcComponent->InstancesData.Scales[SrcInstanceIndex];
		InstancesData.Matrices[InstanceIndex] = SrcComponent->InstancesData.Matrices[SrcInstanceIndex];
		
		//#TODO maybe copying InstancesData.Attachments ?

		const FSkelotInstanceAnimState& SrcAS = SrcComponent->InstancesData.AnimationStates[SrcInstanceIndex];
		FSkelotInstanceAnimState& DstAS = InstancesData.AnimationStates[InstanceIndex];

		if (AnimCollection == SrcComponent->AnimCollection)
		{
			if (SrcAS.IsTransitionValid())
			{
				check(EnumHasAnyFlags(SrcComponent->InstancesData.Flags[SrcInstanceIndex], ESkelotInstanceFlags::EIF_AnimPlayingTransition));
				AnimCollection->IncTransitionRef(SrcAS.TransitionIndex);
			}

			if(DstAS.IsTransitionValid())
			{
				check(EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimPlayingTransition));
				AnimCollection->DecTransitionRef(DstAS.TransitionIndex);
			}

			DstAS = SrcAS;
			InstancesData.FrameIndices[InstanceIndex] = SrcComponent->InstancesData.FrameIndices[SrcInstanceIndex];

			check(!EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_DynamicPose));
			const ESkelotInstanceFlags FlagsToKeep = ESkelotInstanceFlags::EIF_New;//instance must be kept new if it is
			const ESkelotInstanceFlags FlagsToTake = ESkelotInstanceFlags::EIF_Hidden | ESkelotInstanceFlags::EIF_New | ESkelotInstanceFlags::EIF_AllUserFlags | ESkelotInstanceFlags::EIF_AllAnimationFlags;
			InstancesData.Flags[InstanceIndex] &= FlagsToKeep;
			InstancesData.Flags[InstanceIndex] |= (SrcComponent->InstancesData.Flags[SrcInstanceIndex] & FlagsToTake) | ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate;
		}
		else //don't copy animation data if AnimCollections are not identical
		{
			const ESkelotInstanceFlags FlagsToKeep = ESkelotInstanceFlags::EIF_AllAnimationFlags | ESkelotInstanceFlags::EIF_New | ESkelotInstanceFlags::EIF_DynamicPose;//instance must be kept new if it is
			const ESkelotInstanceFlags FlagsToTake = ESkelotInstanceFlags::EIF_Hidden | ESkelotInstanceFlags::EIF_New | ESkelotInstanceFlags::EIF_AllUserFlags;
			InstancesData.Flags[InstanceIndex] &= FlagsToKeep;
			InstancesData.Flags[InstanceIndex] |= (SrcComponent->InstancesData.Flags[SrcInstanceIndex] & FlagsToTake) | ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate;
		}
		if (NumCustomDataFloats > 0 && NumCustomDataFloats <= SrcComponent->NumCustomDataFloats)
		{
			SkelotElementCopy(this->GetInstanceCustomDataFloats(InstanceIndex), SrcComponent->GetInstanceCustomDataFloats(SrcInstanceIndex), NumCustomDataFloats);
		}
		if (this->Submeshes.Num() > 0 && this->Submeshes.Num() == SrcComponent->Submeshes.Num() && this->MaxMeshPerInstance == SrcComponent->MaxMeshPerInstance)
		{
			SkelotElementCopy(this->GetInstanceMeshSlots(InstanceIndex), SrcComponent->GetInstanceMeshSlots(InstanceIndex), this->MaxMeshPerInstance + 1);
		}
	}
}


void USkelotComponent::CallCustomInstanceData_Initialize(int InstanceIndex)
{
	CustomInstanceData_Initialize(InstanceIndex);
	for (int ListenerIndex = 0; ListenerIndex < this->NumListener; ListenerIndex++)
		this->ListenersPtr[ListenerIndex]->CustomInstanceData_Initialize(this->ListenersUserData[ListenerIndex], InstanceIndex);
}

void USkelotComponent::CallCustomInstanceData_Destroy(int InstanceIndex)
{
	CustomInstanceData_Destroy(InstanceIndex);
	for (int ListenerIndex = 0; ListenerIndex < this->NumListener; ListenerIndex++)
		this->ListenersPtr[ListenerIndex]->CustomInstanceData_Destroy(this->ListenersUserData[ListenerIndex], InstanceIndex);
}

void USkelotComponent::CallCustomInstanceData_Move(int DstIndex, int SrcIndex)
{
	CustomInstanceData_Move(DstIndex, SrcIndex);
	for (int ListenerIndex = 0; ListenerIndex < this->NumListener; ListenerIndex++)
		this->ListenersPtr[ListenerIndex]->CustomInstanceData_Move(this->ListenersUserData[ListenerIndex], DstIndex, SrcIndex);
}

void USkelotComponent::CallCustomInstanceData_SetNum(int NewNum)
{
	CustomInstanceData_SetNum(NewNum);
	for (int ListenerIndex = 0; ListenerIndex < this->NumListener; ListenerIndex++)
		this->ListenersPtr[ListenerIndex]->CustomInstanceData_SetNum(this->ListenersUserData[ListenerIndex], NewNum);
}

void USkelotComponent::BatchUpdateTransforms(int StartInstanceIndex, const TArray<FTransform3f>& NewTransforms)
{
	for (int i = 0; i < NewTransforms.Num(); i++)
		if(IsInstanceValid(i + StartInstanceIndex))
			SetInstanceTransform(i + StartInstanceIndex, NewTransforms[i]);
}

void USkelotComponent::SetInstanceTransform(int InstanceIndex, const FTransform3f& NewTransform)
{
	check(IsInstanceValid(InstanceIndex));
	check(NewTransform.IsValid());
	InstancesData.Locations[InstanceIndex] = NewTransform.GetLocation();
	InstancesData.Rotations[InstanceIndex] = NewTransform.GetRotation();
	InstancesData.Scales[InstanceIndex]    = NewTransform.GetScale3D();
	OnInstanceTransformChange(InstanceIndex);
}

void USkelotComponent::SetInstanceLocation(int InstanceIndex, const FVector3f& NewLocation)
{
	check(IsInstanceValid(InstanceIndex));
	InstancesData.Locations[InstanceIndex] = NewLocation;
	OnInstanceTransformChange(InstanceIndex);
}

void USkelotComponent::SetInstanceRotator(int InstanceIndex, const FRotator3f& NewRotator)
{
	SetInstanceRotation(InstanceIndex, NewRotator.Quaternion());
}

void USkelotComponent::SetInstanceRotation(int InstanceIndex, const FQuat4f& NewRotation)
{
	check(IsInstanceValid(InstanceIndex));
	InstancesData.Rotations[InstanceIndex] = NewRotation;
	OnInstanceTransformChange(InstanceIndex);
}

void USkelotComponent::SetInstanceScale(int InstanceIndex, const FVector3f& NewScale)
{
	check(IsInstanceValid(InstanceIndex));
	InstancesData.Scales[InstanceIndex] = NewScale;
	OnInstanceTransformChange(InstanceIndex);
}

void USkelotComponent::AddInstanceLocation(int InstanceIndex, const FVector3f& Offset)
{
	check(IsInstanceValid(InstanceIndex));
	InstancesData.Locations[InstanceIndex] += Offset;
	OnInstanceTransformChange(InstanceIndex);
}

void USkelotComponent::SetInstanceLocationAndRotation(int InstanceIndex, const FVector3f& NewLocation, const FQuat4f& NewRotation)
{
	check(IsInstanceValid(InstanceIndex));
	check(!NewLocation.ContainsNaN() && !NewRotation.ContainsNaN());
	InstancesData.Locations[InstanceIndex] = NewLocation;
	InstancesData.Rotations[InstanceIndex] = NewRotation;
	OnInstanceTransformChange(InstanceIndex);
}

void USkelotComponent::MoveAllInstances(const FVector3f& Offset)
{
	for(int i = 0; i < GetInstanceCount(); i++)
		if(IsInstanceAlive(i))
			AddInstanceLocation(i, Offset);
}

void USkelotComponent::BatchAddUserFlags(const TArray<int>& InstanceIndices, int32 Flags)
{
	for (int InstanceIndex : InstanceIndices)
		if (IsInstanceValid(InstanceIndex))
			InstanceAddFlags(InstanceIndex, static_cast<ESkelotInstanceFlags>(Flags << InstaceUserFlagStart));
}

void USkelotComponent::BatchRemoveUserFlags(const TArray<int>& InstanceIndices, int32 Flags)
{
	for (int InstanceIndex : InstanceIndices)
		if (IsInstanceValid(InstanceIndex))
			InstanceRemoveFlags(InstanceIndex, static_cast<ESkelotInstanceFlags>(Flags << InstaceUserFlagStart));
}

FTransform3f USkelotComponent::GetInstanceTransform(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	
	return FTransform3f(InstancesData.Rotations[InstanceIndex], InstancesData.Locations[InstanceIndex], InstancesData.Scales[InstanceIndex]);
}

const FVector3f& USkelotComponent::GetInstanceLocation(int InstanceIndex) const
{
	return InstancesData.Locations[InstanceIndex];
}

const FQuat4f& USkelotComponent::GetInstanceRotation(int InstanceIndex) const
{
	return InstancesData.Rotations[InstanceIndex];
}

FRotator3f USkelotComponent::GetInstanceRotator(int InstanceIndex) const
{
	return GetInstanceRotation(InstanceIndex).Rotator();
}

const FBoxCenterExtentFloat& USkelotComponent::GetInstanceLocalBound(int InstanceIndex)
{
	check(IsInstanceValid(InstanceIndex));
	
	if (ShouldUseFixedInstanceBound())
		return AnimCollection->MeshesBBox;

	if (EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate))
	{
		EnumRemoveFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate);
		UpdateInstanceLocalBound(InstanceIndex);
	}
	
	return InstancesData.LocalBounds[InstanceIndex];
}


FBoxCenterExtentFloat USkelotComponent::CalculateInstanceBound(int InstanceIndex)
{
	return GetInstanceLocalBound(InstanceIndex).TransformBy(InstancesData.Matrices[InstanceIndex]);
}

bool USkelotComponent::ShouldUseFixedInstanceBound() const
{
	return bUseFixedInstanceBound || (AnimCollection && AnimCollection->bDontGenerateBounds);
}

void USkelotComponent::OnInstanceTransformChange(int InstanceIndex)
{
	InstancesData.Matrices[InstanceIndex] = GetInstanceTransform(InstanceIndex).ToMatrixWithScale();
}

bool USkelotComponent::IsInstanceHidden(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	return EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Hidden);
}

void USkelotComponent::SetInstanceHidden(int InstanceIndex, bool bNewHidden)
{
	check(IsInstanceValid(InstanceIndex));
	const bool bIsHidden = EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Hidden);
	if (bNewHidden != bIsHidden)
	{
		if(bNewHidden)
			EnumAddFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Hidden);
		else
			EnumRemoveFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Hidden);

		EnumAddFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_New);
	}
		
}

void USkelotComponent::ToggleInstanceVisibility(int InstanceIndex)
{
	check(IsInstanceValid(InstanceIndex));
	InstancesData.Flags[InstanceIndex] ^= ESkelotInstanceFlags::EIF_Hidden;
	InstancesData.Flags[InstanceIndex] |= ESkelotInstanceFlags::EIF_New;
}

float USkelotComponent::InstancePlayAnimation(int InstanceIndex, FSkelotAnimPlayParams Params)
{
	
	if (!AnimCollection || !AnimCollection->bIsBuilt || !IsInstanceValid(InstanceIndex) || !Params.Animation)
	{
		return -1;
	}
	
	ESkelotInstanceFlags& Flags = InstancesData.Flags[InstanceIndex];
	FSkelotInstanceAnimState& AnimState = InstancesData.AnimationStates[InstanceIndex];
	int32 TargetAnimSeqIndex = AnimCollection->FindSequenceDef(Params.Animation);
	if (TargetAnimSeqIndex == -1 || EnumHasAnyFlags(Flags, ESkelotInstanceFlags::EIF_AnimPaused | ESkelotInstanceFlags::EIF_DynamicPose))
	{
		return -1;
	}

	const FSkelotSequenceDef& TargetSeq = AnimCollection->Sequences[TargetAnimSeqIndex];
	AnimAdvanceTime(Params.bLoop, float(0), Params.StartAt, TargetSeq.GetSequenceLength());	//clamp or wrap time

	const int TargetLocalFrameIndex = static_cast<int>(Params.StartAt * TargetSeq.SampleFrequencyFloat);
	const int TargetGlobalFrameIndex = TargetSeq.AnimationFrameIndex + TargetLocalFrameIndex;
	check(TargetLocalFrameIndex < TargetSeq.AnimationFrameCount);
	check(TargetGlobalFrameIndex < AnimCollection->FrameCountSequences);
	const bool bIsPlayingTransition = EnumHasAnyFlags(Flags, ESkelotInstanceFlags::EIF_AnimPlayingTransition);
	const bool bCanTransition = Params.TransitionDuration > 0 && AnimState.IsValid() && !bIsPlayingTransition;
	if (bCanTransition)	//blend should happen if a sequence is already being played without blend
	{
		//#TODO looped transition is not supported yet. 
		int TransitionFrameCount = static_cast<int>(TargetSeq.SampleFrequency * Params.TransitionDuration) /*+ 1*/;
		if (TransitionFrameCount >= 3)
		{
			int Remain = TargetSeq.AnimationFrameCount - TargetLocalFrameIndex;
			TransitionFrameCount = FMath::Min(TransitionFrameCount, Remain);

			if (TransitionFrameCount >= 3)
			{
				const FSkelotSequenceDef& CurrentSeqStruct = AnimCollection->Sequences[AnimState.CurrentSequence];

				int CurFrameIndex = InstancesData.FrameIndices[InstanceIndex];
				check(CurFrameIndex < AnimCollection->FrameCountSequences);
				int CurLocalFrameIndex = CurFrameIndex - CurrentSeqStruct.AnimationFrameIndex;
				check(CurLocalFrameIndex < CurrentSeqStruct.AnimationFrameCount);

				USkelotAnimCollection::FTransitionKey TransitionKey;
				TransitionKey.FromSI = static_cast<uint16>(AnimState.CurrentSequence);
				TransitionKey.ToSI = static_cast<uint16>(TargetAnimSeqIndex);
				TransitionKey.FromFI = CurLocalFrameIndex;
				TransitionKey.ToFI = TargetLocalFrameIndex;
				TransitionKey.FrameCount = static_cast<uint16>(TransitionFrameCount);
				TransitionKey.BlendOption = Params.BlendOption;
				TransitionKey.bFromLoops = EnumHasAnyFlags(Flags, ESkelotInstanceFlags::EIF_AnimLoop);
				TransitionKey.bToLoops = Params.bLoop;

				auto [TransitionIndex, Result] = AnimCollection->FindOrCreateTransition(TransitionKey, Params.bIgnoreTransitionGeneration);
				if (TransitionIndex != -1)
				{
					EnumRemoveFlags(Flags, ESkelotInstanceFlags::EIF_AnimNoSequence | ESkelotInstanceFlags::EIF_AnimLoop | ESkelotInstanceFlags::EIF_AnimFinished);
					EnumAddFlags(Flags, ESkelotInstanceFlags::EIF_AnimSkipTick | ESkelotInstanceFlags::EIF_AnimPlayingTransition | ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate | (Params.bLoop ? ESkelotInstanceFlags::EIF_AnimLoop : ESkelotInstanceFlags::EIF_None));

					const USkelotAnimCollection::FTransition& Transition = AnimCollection->Transitions[TransitionIndex];

					AnimState.Time = Params.StartAt;
					AnimState.PlayScale = Params.PlayScale;
					AnimState.CurrentSequence = static_cast<uint16>(TargetAnimSeqIndex);
					AnimState.TransitionIndex = static_cast<uint16>(TransitionIndex);
					AnimState.RandomSequenceIndex = 0xFFff;

					InstancesData.FrameIndices[InstanceIndex] = Transition.FrameIndex;

					if (GSkelot_DebugTransitions)
						DebugDrawInstanceBound(InstanceIndex, 0, Result == USkelotAnimCollection::ETR_Success_Found ? FColor::Green : FColor::Yellow, false, 0.3f);

					return TargetSeq.GetSequenceLength();
				}
				else
				{

				}

			}
			else
			{
				UE_LOG(LogSkelot, Warning, TEXT("Can't Transition From %d To %d. wrapping is not supported."), AnimState.CurrentSequence, TargetAnimSeqIndex);
			}
		}
		else
		{
			UE_LOG(LogSkelot, Warning, TEXT("Can't Transition From %d To %d. duration too low."), AnimState.CurrentSequence, TargetAnimSeqIndex);
		}


	}
	
//NormalPlay:

	if (bIsPlayingTransition)
	{
		this->AnimCollection->DecTransitionRef(AnimState.TransitionIndex);
	}

	EnumRemoveFlags(Flags, ESkelotInstanceFlags::EIF_AnimPlayingTransition | ESkelotInstanceFlags::EIF_AnimNoSequence | ESkelotInstanceFlags::EIF_AnimLoop | ESkelotInstanceFlags::EIF_AnimFinished);
	EnumAddFlags(Flags, ESkelotInstanceFlags::EIF_AnimSkipTick | ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate | (Params.bLoop ? ESkelotInstanceFlags::EIF_AnimLoop : ESkelotInstanceFlags::EIF_None));

	AnimState.Time = Params.StartAt;
	AnimState.PlayScale = Params.PlayScale;
	AnimState.CurrentSequence = static_cast<uint16>(TargetAnimSeqIndex);
	AnimState.RandomSequenceIndex = 0xFFff;

	InstancesData.FrameIndices[InstanceIndex] = TargetGlobalFrameIndex;

	if (GSkelot_DebugTransitions && Params.TransitionDuration > 0) //show that instance failed to play transition
		DebugDrawInstanceBound(InstanceIndex, -2, FColor::Red, false, 0.3f);
	
	return TargetSeq.GetSequenceLength();
}


float USkelotComponent::InstancePlayAnimation(int InstanceIndex, UAnimSequenceBase* Animation, bool bLoop, float StartAt, float PlayScale, float TransitionDuration, EAlphaBlendOption BlendOption, bool bIgnoreTransitionGeneration)
{
	FSkelotAnimPlayParams Params;
	Params.Animation = Animation;
	Params.bLoop = bLoop;
	Params.StartAt = StartAt;
	Params.PlayScale = PlayScale;
	Params.TransitionDuration = TransitionDuration;
	Params.BlendOption = BlendOption;
	Params.bIgnoreTransitionGeneration = bIgnoreTransitionGeneration;
	return InstancePlayAnimation(InstanceIndex, Params);
}

float USkelotComponent::InstancePlayRandomLoopedSequence(int InstanceIndex, FName Name, float StartAt /*= 0*/, float PlayScale /*= 1*/, float TransitionDuration /*= 0.2f*/, EAlphaBlendOption BlendOption /*= EAlphaBlendOption::Linear*/, bool bIgnoreTransitionGeneration /*= false*/)
{
	int32 Idx = IndexOfRandomSequenceDefinition(Name);
	if (Idx != -1)
	{
		const FSkelotRandomSequenceDef& Def = this->RandomSequenceDefinitions[Idx];
		UAnimSequenceBase* Anim = SkelotGetElementRandomWeighted(Def.Sequences, FMath::FRand());
		if (Anim)
		{
			float SequenceLen = this->InstancePlayAnimation(InstanceIndex, Anim, false, StartAt, PlayScale, TransitionDuration, BlendOption, bIgnoreTransitionGeneration);
			if(SequenceLen > 0)
			{
				InstancesData.AnimationStates[InstanceIndex].RandomSequenceIndex = static_cast<uint16>(Idx);
				return SequenceLen;
			}
		}
	}
	return -1;
}

void USkelotComponent::PlayAnimationOnAll(UAnimSequenceBase* Animation, bool bLoop, float StartAt, float PlayScale, float TransitionDuration, EAlphaBlendOption BlendOption, bool bIgnoreTransitionGeneration)
{
	for (int InstanceIndex = 0; InstanceIndex < this->GetInstanceCount(); InstanceIndex++)
	{
		if (IsInstanceAlive(InstanceIndex))
			InstancePlayAnimation(InstanceIndex, Animation, bLoop, StartAt, PlayScale, TransitionDuration, BlendOption, bIgnoreTransitionGeneration);
	}
}

// void USkelotComponent::StopInstanceAnimation(int InstanceIndex, bool bResetPose)
// {
// 	check(IsInstanceValid(InstanceIndex));
// 	
// 	if (!InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_AnimNoSequence))
// 	{
// 		FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
// 		LeaveTransitionIfAny_Internal(InstanceIndex);
// 		InstanceRemoveFlags(InstanceIndex, ESkelotInstanceFlags::EIF_AllAnimationFlags);
// 		InstanceAddFlags(InstanceIndex, ESkelotInstanceFlags::EIF_AnimNoSequence);
// 		if(bResetPose)
// 		{
// 			AS.CurrentSequence = 0xFFff;
// 			InstancesData.FrameIndices[InstanceIndex] = 0;
// 		}
// 	}
// }


void USkelotComponent::LeaveTransitionIfAny_Internal(int InstanceIndex)
{
	if (InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_AnimPlayingTransition))
	{
		InstanceRemoveFlags(InstanceIndex, ESkelotInstanceFlags::EIF_AnimPlayingTransition);

		FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
		//switch to frame index in sequence range 
		InstancesData.FrameIndices[InstanceIndex] = Utils::TransitionFrameRangeToSeuqnceFrameRange<true>(AS, InstancesData.FrameIndices[InstanceIndex], AnimCollection);
		AnimCollection->DecTransitionRef(AS.TransitionIndex);
	}
}

void USkelotComponent::PauseInstanceAnimation(int InstanceIndex, bool bPause)
{
	check(IsInstanceValid(InstanceIndex));
	if (bPause)
		EnumAddFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimPaused);
	else
		EnumRemoveFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimPaused);
}

bool USkelotComponent::IsInstanceAnimationPaused(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	return EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimPaused);
}

void USkelotComponent::ToggleInstanceAnimationPause(int InstanceIndex)
{
	check(IsInstanceValid(InstanceIndex));
	InstancesData.Flags[InstanceIndex] ^= ESkelotInstanceFlags::EIF_AnimPaused;
}

void USkelotComponent::SetInstanceAnimationLooped(int InstanceIndex, bool bLoop)
{
	check(IsInstanceValid(InstanceIndex));
	if (bLoop)
		EnumAddFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimLoop);
	else
		EnumRemoveFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimLoop);
}

bool USkelotComponent::IsInstanceAnimationLooped(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	return EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimLoop);
}

void USkelotComponent::ToggleInstanceAnimationLoop(int InstanceIndex)
{
	check(IsInstanceValid(InstanceIndex));
	InstancesData.Flags[InstanceIndex] ^= ESkelotInstanceFlags::EIF_AnimLoop;
}


bool USkelotComponent::IsInstancePlayingAnimation(int InstanceIndex, const UAnimSequenceBase* Animation) const
{
	check(IsInstanceValid(InstanceIndex));
	const FSkelotInstanceAnimState& animState = InstancesData.AnimationStates[InstanceIndex];
	const bool bPlaying = !EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimNoSequence);
	return Animation && bPlaying && animState.IsValid() && animState.CurrentSequence == AnimCollection->FindSequenceDef(Animation);
}

bool USkelotComponent::IsInstancePlayingAnyAnimation(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	//#TODO should return false if its paused ?
	return !EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_AnimNoSequence);
}

UAnimSequenceBase* USkelotComponent::GetInstanceCurrentAnimSequence(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	const FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
	if(AS.IsValid())
		return AnimCollection->Sequences[AS.CurrentSequence].Sequence;

	return nullptr;
}

void USkelotComponent::SetInstancePlayScale(int InstanceIndex, float NewPlayScale)
{
	check(IsInstanceValid(InstanceIndex));
	InstancesData.AnimationStates[InstanceIndex].PlayScale = NewPlayScale;
}


float USkelotComponent::GetInstancePlayLength(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	const FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
	return AS.IsValid() ? AnimCollection->Sequences[AS.CurrentSequence].GetSequenceLength() : 0;
}

float USkelotComponent::GetInstancePlayTime(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	const FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
	return AS.Time;
}

float USkelotComponent::GetInstancePlayTimeRemaining(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	const FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
	if (AS.IsValid())
	{
		float SL = this->AnimCollection->Sequences[AS.CurrentSequence].GetSequenceLength();
		float RT = SL - AS.Time;
		checkSlow(RT >= 0 && RT <= SL);
		return RT;
	}
	return 0;
}

float USkelotComponent::GetInstancePlayTimeFraction(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	const FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
	if (AS.IsValid())
	{
		float SL = this->AnimCollection->Sequences[AS.CurrentSequence].GetSequenceLength();
		float PT = AS.Time / SL;
		checkSlow(PT >= 0.0f && PT <= 1.0f);
		return PT;
	}
	return 0;
}

float USkelotComponent::GetInstancePlayTimeRemainingFraction(int InstanceIndex) const
{
	check(IsInstanceValid(InstanceIndex));
	const FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
	if (AS.IsValid())
	{
		float SL = this->AnimCollection->Sequences[AS.CurrentSequence].GetSequenceLength();
		float RPT = 1.0f - (AS.Time / SL);
		checkSlow(RPT >= 0.0f && RPT <= 1.0f);
		return RPT;
	}
	return 0;
}

void USkelotComponent::CallOnAnimationFinished()
{
	for (const FSkelotAnimFinishEvent& Ev : AnimationFinishEvents_RandomMode)
	{
		FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[Ev.InstanceIndex];
		const auto RSIndex = AS.RandomSequenceIndex;
		AS.RandomSequenceIndex = 0xFFff;
		const FSkelotRandomSequenceDef& RandSeq = this->RandomSequenceDefinitions[RSIndex];
		if (UAnimSequenceBase* Anim = SkelotGetElementRandomWeighted(RandSeq.Sequences, FMath::FRand()))
		{
			float SeqLen = InstancePlayAnimation(Ev.InstanceIndex, Anim, false, 0, AS.PlayScale, -1);
			if(SeqLen > 0)
				AS.RandomSequenceIndex = RSIndex;
		}
	}

	if(AnimationFinishEvents.Num())
	{
		OnAnimationFinished(AnimationFinishEvents);
		OnAnimationFinishedDelegate.Broadcast(this, AnimationFinishEvents);
		for (int ListenerIndex = 0; ListenerIndex < this->NumListener; ListenerIndex++)
			this->ListenersPtr[ListenerIndex]->OnAnimationFinished(this->ListenersUserData[ListenerIndex], AnimationFinishEvents);
	}
}

void USkelotComponent::CallOnAnimationNotify()
{
	//name only notifications
	if(AnimationNotifyEvents.Num())
	{
		OnAnimationNotify(AnimationNotifyEvents);
		OnAnimationNotifyDelegate.Broadcast(this, AnimationNotifyEvents);
		for (int ListenerIndex = 0; ListenerIndex < this->NumListener; ListenerIndex++)
			this->ListenersPtr[ListenerIndex]->OnAnimationNotify(this->ListenersUserData[ListenerIndex], AnimationNotifyEvents);
	}

	for(const FSkelotAnimNotifyObjectEvent& Ev : AnimationNotifyObjectEvents)
	{
		Ev.Notify->OnNotify(this, Ev.InstanceIndex, Ev.AnimSequence);
	}
}

FTransform3f USkelotComponent::GetInstanceBoneTransformCS(int instanceIndex, int boneIndex) const
{
	if (IsInstanceValid(instanceIndex) && AnimCollection && AnimCollection->IsBoneTransformCached(boneIndex))
	{
		//#TODO should we block here ?
		AnimCollection->ConditionalFlushDeferredTransitions(InstancesData.FrameIndices[instanceIndex]);
		return AnimCollection->GetBoneTransformFast(boneIndex, InstancesData.FrameIndices[instanceIndex]);
	}

	return FTransform3f::Identity;
}



FTransform3f USkelotComponent::GetInstanceBoneTransformWS(int instanceIndex, int boneIndex) const
{
	const FTransform3f& boneTransform = GetInstanceBoneTransformCS(instanceIndex, boneIndex);
	return boneTransform * GetInstanceTransform(instanceIndex);
}

int32 USkelotComponent::GetBoneIndex(FName BoneName) const
{
	int32 BoneIndex = INDEX_NONE;
	if (BoneName != NAME_None && AnimCollection && AnimCollection->Skeleton)
	{
		BoneIndex = AnimCollection->Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
	}
	return BoneIndex;
}



FName USkelotComponent::GetBoneName(int32 BoneIndex) const
{
	if (AnimCollection && AnimCollection->Skeleton)
	{
		const FReferenceSkeleton& Ref = AnimCollection->Skeleton->GetReferenceSkeleton();
		if(Ref.IsValidIndex(BoneIndex))
			return Ref.GetBoneName(BoneIndex);
	}

	return NAME_None;
}

USkeletalMeshSocket* USkelotComponent::FindSocket(FName SocketName, const USkeletalMesh* InMesh) const
{
	if (InMesh) //#TODO check if skeletons are identical ?
		return InMesh->FindSocket(SocketName);

	if(Submeshes.Num() == 1 && Submeshes[0].SkeletalMesh)
	{
		return Submeshes[0].SkeletalMesh->FindSocket(SocketName);
	}

	if (AnimCollection && AnimCollection->Skeleton)
		return AnimCollection->Skeleton->FindSocket(SocketName);

	return nullptr;
}

bool USkelotComponent::IsBoneTransformCached(int BoneIndex) const
{
	return (AnimCollection && AnimCollection->IsBoneTransformCached(BoneIndex));
}

bool USkelotComponent::IsSocketAvailable(FName SocketName, USkeletalMesh* InMesh) const
{
	const USkeletalMeshSocket* Socket = FindSocket(SocketName, InMesh);
	int32 boneIndex = GetBoneIndex(SocketName);
	if (Socket)
	{
		boneIndex = GetBoneIndex(Socket->BoneName);
	}

	return boneIndex != -1 ? IsBoneTransformCached(boneIndex) : false;
}

FTransform3f USkelotComponent::GetInstanceSocketTransform(int instanceIndex, FName SocketName, USkeletalMesh* InMesh, bool bWorldSpace) const
{
	const USkeletalMeshSocket* Socket = FindSocket(SocketName, InMesh);
	if (Socket)
	{
		int32 boneIndex = GetBoneIndex(Socket->BoneName);
		if(boneIndex != INDEX_NONE)
		{
			const FTransform3f transform = FTransform3f(Socket->GetSocketLocalTransform()) * GetInstanceBoneTransformCS(instanceIndex, boneIndex);
			if(bWorldSpace)
				return transform * GetInstanceTransform(instanceIndex);
			else
				return transform;
		}
	}
	else
	{
		int32 boneIndex = GetBoneIndex(SocketName);
		if (boneIndex != INDEX_NONE)
		{
			return GetInstanceBoneTransform(instanceIndex, boneIndex, bWorldSpace);
		}
	}

	return FTransform3f::Identity;
	
}


void USkelotComponent::GetInstancesSocketTransform(TArray<FTransform3f>& OutTransforms, FName SocketName, USkeletalMesh* InMesh, bool bWorldSpace) const
{
	if (AnimCollection)
	{
		const USkeletalMeshSocket* Socket = FindSocket(SocketName, InMesh);
		if (Socket)
		{
			int32 boneIndex = GetBoneIndex(Socket->BoneName);
			if (AnimCollection->IsBoneTransformCached(boneIndex))
			{
				FTransform3f SocketLocalTransform = FTransform3f(Socket->GetSocketLocalTransform());
				OutTransforms.SetNumUninitialized(GetInstanceCount());
				for (int i = 0; i < GetInstanceCount(); i++)
				{
					if (this->IsInstanceAlive(i))
					{
						int FrameIndex = InstancesData.FrameIndices[i];
						AnimCollection->ConditionalFlushDeferredTransitions(FrameIndex);
						OutTransforms[i] = SocketLocalTransform * AnimCollection->GetBoneTransformFast(boneIndex, FrameIndex);
						if (bWorldSpace)
							OutTransforms[i] = OutTransforms[i] * GetInstanceTransform(i);
					}
					else
					{
						OutTransforms[i] = FTransform3f::Identity;
					}
				}
			}
		}
		else
		{
			int32 boneIndex = GetBoneIndex(SocketName);
			if (AnimCollection->IsBoneTransformCached(boneIndex))
			{
				OutTransforms.SetNumUninitialized(GetInstanceCount());
				for (int i = 0; i < GetInstanceCount(); i++)
				{
					if (this->IsInstanceAlive(i))
					{
						int FrameIndex = InstancesData.FrameIndices[i];
						AnimCollection->ConditionalFlushDeferredTransitions(FrameIndex);
						OutTransforms[i] = AnimCollection->GetBoneTransformFast(boneIndex, FrameIndex);
						if (bWorldSpace)
							OutTransforms[i] = OutTransforms[i] * GetInstanceTransform(i);
					}
					else
					{
						OutTransforms[i] = FTransform3f::Identity;
					}
				}
			}

		}
	}
}

void USkelotComponent::GetInstancesSocketTransform(const FName SocketName, USkeletalMesh* InMesh, const bool bWorldSpace, TFunctionRef<void(int InstanceIndex, const FTransform3f& T)> Proc) const
{
	if (AnimCollection)
	{
		const USkeletalMeshSocket* Socket = FindSocket(SocketName, InMesh);
		if (Socket)
		{
			int32 boneIndex = GetBoneIndex(Socket->BoneName);
			if (AnimCollection->IsBoneTransformCached(boneIndex))
			{
				FTransform3f SocketLocalTransform = FTransform3f(Socket->GetSocketLocalTransform());
				for (int i = 0; i < GetInstanceCount(); i++)
				{
					if (this->IsInstanceAlive(i))
					{
						FTransform3f ST = SocketLocalTransform * GetInstanceBoneTransformCS(i, boneIndex);
						if (bWorldSpace)
							ST = ST * GetInstanceTransform(i);

						Proc(i, ST);
					}
				}
			}
		}
		else
		{
			int32 boneIndex = GetBoneIndex(SocketName);
			if (AnimCollection->IsBoneTransformCached(boneIndex))
			{
				for (int i = 0; i < GetInstanceCount(); i++)
				{
					if (this->IsInstanceValid(i))
					{
						FTransform3f ST = GetInstanceBoneTransformCS(i, boneIndex);
						if (bWorldSpace)
							ST = ST * GetInstanceTransform(i);

						Proc(i, ST);
					}
				}
			}

		}
	}
}

USkelotComponent::FSocketMinimalInfo USkelotComponent::GetSocketMinimalInfo(FName InSocketName, USkeletalMesh* InMesh) const
{
	FSocketMinimalInfo Ret;

	const USkeletalMeshSocket* Socket = FindSocket(InSocketName, InMesh);
	if (Socket)
	{
		Ret.BoneIndex = GetBoneIndex(Socket->BoneName);
		if (Ret.BoneIndex != INDEX_NONE)
			Ret.LocalTransform = FTransform3f(Socket->GetSocketLocalTransform());
	}
	else
	{
		Ret.BoneIndex = GetBoneIndex(InSocketName);
	}

	return Ret;
}



FTransform3f USkelotComponent::GetInstanceSocketTransform_Fast(int InstanceIndex, const FSocketMinimalInfo& SocketInfo, bool bWorldSpace) const
{
	check(IsInstanceValid(InstanceIndex));
	const FTransform3f transform = SocketInfo.LocalTransform * GetInstanceBoneTransformCS(InstanceIndex, SocketInfo.BoneIndex);
	if (bWorldSpace)
		return transform * GetInstanceTransform(InstanceIndex);
	else
		return transform;
}




// const FBoxCenterExtentFloat* USkelotSingleMeshComponent::GetMeshLocalBounds() const
// {
// 	const FSkelotMeshDef& MeshDef = AnimCollection->Meshes[MeshIndex];
// 
// 	if (AnimCollection->Meshes.IsValidIndex(MeshDef.OwningBoundMeshIndex))
// 		return AnimCollection->Meshes[MeshDef.OwningBoundMeshIndex].Bounds.GetData();
// 
// 	return MeshDef.Bounds.GetData();
// }




void USkelotComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport /*= ETeleportType::None*/)
{
	Super::OnUpdateTransform(UpdateTransformFlags, ETeleportType::None);
}

void USkelotComponent::DetachFromComponent(const FDetachmentTransformRules& DetachmentRules)
{
	Super::DetachFromComponent(DetachmentRules);
}

void USkelotComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void USkelotComponent::UninitializeComponent()
{
	Super::UninitializeComponent();
}

void USkelotComponent::ResetAnimationStates()
{
	if(AnimCollection)
	{
		for (int i = 0; i < GetInstanceCount(); i++)
		{
			if (IsInstanceAlive(i))
				ResetInstanceAnimationState(i);
		}
	}
}



// void USkelotComponent::RegisterPerInstanceData(FName ArrayPropertyName)
// {
// 	FProperty* P = this->GetClass()->FindPropertyByName(ArrayPropertyName);
// 	if (!P)
// 	{
// 		UE_LOG(LogSkelot, Warning, TEXT("RegisterPerInstanceData failed. property %s not found"), *ArrayPropertyName.ToString());
// 		return;
// 	}
// 
// 	FArrayProperty* AP = CastField<FArrayProperty>(P);
// 	if (!AP)
// 	{
// 		UE_LOG(LogSkelot, Warning, TEXT("RegisterPerInstanceData failed. property %s is not an array"), *ArrayPropertyName.ToString());
// 		return;
// 	}
// 
// 	InstanceDataProperties.AddUnique(AP);
// 
// }

void USkelotComponent::QueryLocationOverlappingSphere(const FVector3f& Center, float Radius, TArray<int>& OutIndices) const
{
	for (int i = 0; i < GetInstanceCount(); i++)
		if (IsInstanceAlive(i))
			if (FVector3f::DistSquared(InstancesData.Locations[i], Center) < (Radius * Radius))
				OutIndices.Add(i);
}

void USkelotComponent::QueryLocationOverlappingBox(const FBox3f& Box, TArray<int>& OutIndices) const
{
	for (int i = 0; i < GetInstanceCount(); i++)
		if (IsInstanceAlive(i))
			if (Box.IsInside(InstancesData.Locations[i]))
				OutIndices.Add(i);
}




int USkelotComponent::GetInstanceBoundIndex(int InstanceIndex) const
{
	const FSkelotInstanceAnimState& AS = InstancesData.AnimationStates[InstanceIndex];
	int FrameIndex = InstancesData.FrameIndices[InstanceIndex];
	if (FrameIndex < AnimCollection->FrameCountSequences) //playing sequence ?
	{
		return FrameIndex;
	}
	else if(AS.IsTransitionValid()) //playing transition ?
	{
		//we use bounding box generated for sequences
		return Utils::TransitionFrameRangeToSeuqnceFrameRange<true>(AS, FrameIndex, AnimCollection);
	}
	//#TODO what happens to dynamic pose instances ?
	return 0;
}



void USkelotComponent::DebugDrawInstanceBound(int InstanceIndex, float BoundExtentOffset, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (IsInstanceValid(InstanceIndex) && AnimCollection && Submeshes.Num() && GetWorld())
	{
		FBoxCenterExtentFloat B = this->CalculateInstanceBound(InstanceIndex);
		DrawDebugBox(GetWorld(), FVector(B.Center), FVector(B.Extent) + BoundExtentOffset, FQuat(GetInstanceRotation(InstanceIndex)), Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
	}
#endif
}

FSkelotDynamicData* USkelotComponent::GenerateDynamicData_Internal()
{
	FSkelotDynamicData* DynamicData = FSkelotDynamicData::Allocate(this);
	const uint32 InstanceCount = GetInstanceCount();
	this->CalcInstancesBound(DynamicData->CompBound, DynamicData->Bounds);

	InstancesData.RemoveFlags(ESkelotInstanceFlags::EIF_New | ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate);

	//happens if all instances are hidden or destroyed. rare case !
	if (DynamicData->CompBound.IsForceInitValue())
	{
		DynamicData->CompBound = FBoxMinMaxFloat(FVector3f::ZeroVector, FVector3f::ZeroVector);
		DynamicData->InstanceCount = 0;
		DynamicData->AliveInstanceCount = 0;
		DynamicData->NumCells = 0;
		
	}
	else
	{
		DynamicData->CompBound.Expand(this->ComponentBoundExtent + 1.0f);

		if (DynamicData->NumCells != 0)
		{
			DynamicData->InitGrid();

			for (uint32 InstanceIndex = 0; InstanceIndex < InstanceCount; InstanceIndex++)	//for each instance
			{
				if (EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Destroyed | ESkelotInstanceFlags::EIF_Hidden))
					continue;

				const FBoxCenterExtentFloat& IB = DynamicData->Bounds[InstanceIndex];
				int CellIdx = DynamicData->LocationToCellIndex(IB.Center);
				DynamicData->Cells[CellIdx].Bound.Add(IB);
				DynamicData->Cells[CellIdx].AddValue(*DynamicData, InstanceIndex);
			}
		}
	}



	return DynamicData;

	//for (uint32 i = PrevDynamicDataInstanceCount; i < InstanceCount; i++)
	//{
	//	DynamicData->Flags[i] |= ESkelotInstanceFlags::EIF_New;
	//}
	//PrevDynamicDataInstanceCount = DynamicData->InstanceCount;
	//
	//ENQUEUE_RENDER_COMMAND(Skelot_SendRenderDynamicData)([=](FRHICommandListImmediate& RHICmdList) {
	//	SkelotProxy->SetDynamicDataRT(DynamicData);
	//});
}

void USkelotComponent::CalcInstancesBound(FBoxMinMaxFloat& CompBound, FBoxCenterExtentFloat* InstancesBounds)
{
	SKELOT_SCOPE_CYCLE_COUNTER(USkelotComponent_CalcInstancesBound);

	if (ShouldUseFixedInstanceBound())
	{
		const FBoxCenterExtentFloat FixedBound = AnimCollection->MeshesBBox;
		for (int32 InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)	//for each instance
		{
			if (EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Destroyed | ESkelotInstanceFlags::EIF_Hidden))
				continue;

			//#TODO why FixedBound.Center += Locations[InstanceIndex] is slower
			FBoxCenterExtentFloat IB = FixedBound.TransformBy(InstancesData.Matrices[InstanceIndex]);
			CompBound.Add(IB);
			if(InstancesBounds)
				InstancesBounds[InstanceIndex] = IB;
		}
	}
	else
	{
		if (InstancesData.LocalBounds.Num() != InstancesData.Flags.Num())
		{
			InstancesData.LocalBounds.SetNum(InstancesData.Flags.Num());
			this->TimeSinceLastLocalBoundUpdate = GSkelot_LocalBoundUpdateInterval;	//fore to update local bounds
		}

		const bool bTimeForLBUpdate = this->TimeSinceLastLocalBoundUpdate >= GSkelot_LocalBoundUpdateInterval;
		if (bTimeForLBUpdate)
		{
			this->TimeSinceLastLocalBoundUpdate = FMath::Fmod(this->TimeSinceLastLocalBoundUpdate, GSkelot_LocalBoundUpdateInterval);
		}
		
		const ESkelotInstanceFlags FlagToCheck = bTimeForLBUpdate ? ESkelotInstanceFlags::EIF_None : ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate;

		for (int32 InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)	//for each instance
		{
			if (EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Destroyed | ESkelotInstanceFlags::EIF_Hidden))
				continue;

			if (EnumHasAllFlags(InstancesData.Flags[InstanceIndex], FlagToCheck))
			{
				UpdateInstanceLocalBound(InstanceIndex);
			}
			
			FBoxCenterExtentFloat IB = InstancesData.LocalBounds[InstanceIndex].TransformBy(InstancesData.Matrices[InstanceIndex]);
			CompBound.Add(IB);
			if(InstancesBounds)
				InstancesBounds[InstanceIndex] = IB;
		}
	}
}

/*
void USkelotComponent::CalcInstancesBound_Fixed(FSkelotDynamicData* DynamicData)
{
	const FBoxCenterExtentFloat FixedBound(this->FixedInstanceBound_Center, this->FixedInstanceBound_Extent);

	for (int32 InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)	//for each instance
	{
		if (EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Destroyed | ESkelotInstanceFlags::EIF_Hidden))
			continue;

		//#TODO why the fuck FixedBound.Center += Locations[InstanceIndex] is slower
		FBoxCenterExtentFloat IB = FixedBound.TransformBy(InstancesData.Matrices[InstanceIndex]);
		DynamicData->CompBound.Add(IB);
		DynamicData->Bounds[InstanceIndex] = IB;

	}
}

void USkelotComponent::CalcInstancesBound(FSkelotDynamicData* DynamicData)
{
	const bool bTimeForLBUpdate = this->TimeSinceLastLocalBoundUpdate >= GSkelot_LocalBoundUpdateInterval;
	if (bTimeForLBUpdate)
	{
		this->TimeSinceLastLocalBoundUpdate -= GSkelot_LocalBoundUpdateInterval;
	}

	for (int32 InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)	//for each instance
	{
		if (EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Destroyed | ESkelotInstanceFlags::EIF_Hidden))
			continue;

		if (bTimeForLBUpdate || EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate))
		{
			UpdateInstanceLocalBound(InstanceIndex);
		}

		FBoxCenterExtentFloat IB = InstancesData.LocalBounds[InstanceIndex].TransformBy(InstancesData.Matrices[InstanceIndex]);
		DynamicData->CompBound.Add(IB);
		DynamicData->Bounds[InstanceIndex] = IB;
	}
}
*/

// void USkelotComponent::Internal_UpdateInstanceLocalBoundMM(int InstanceIndex)
// {
// #if 0
// 	const uint8* MeshSlotIter = GetInstanceMeshSlots(InstanceIndex);
// 	if (*MeshSlotIter == 0xFF) //has no mesh ?
// 	{
// 		this->InstancesData.LocalBounds[InstanceIndex] = FBoxCenterExtentFloat(ForceInit);
// 		return;
// 	}
// 
// 	FBoxMinMaxFloat LocalBound(ForceInit);
// 
// 	do
// 	{
// 		const FSkelotSubmeshSlot& Slot = this->Submeshes[*MeshSlotIter];
// 		const FBoxMinMaxFloat MaxBox = this->AnimCollection->Meshes[Slot.MeshDefIndex].MaxBBox;
// 		LocalBound.Add(MaxBox);
// 		MeshSlotIter++;
// 
// 	} while (*MeshSlotIter != 0xFF);
// 
// 	LocalBound.ToCenterExtentBox(this->InstancesData.LocalBounds[InstanceIndex]);
// 
// #else
// 	const auto BoundIndex = GetInstanceBoundIndex(InstanceIndex);
// 	const uint8* MeshSlotIter = GetInstanceMeshSlots(InstanceIndex);
// 	if (*MeshSlotIter == 0xFF) //has no mesh ?
// 	{
// 		this->InstancesData.LocalBounds[InstanceIndex] = FBoxCenterExtentFloat(ForceInit);
// 		return;
// 	}
// 
// 	FBoxMinMaxFloat LocalBound(ForceInit);
// 
// 	do
// 	{
// 		const FSkelotSubmeshSlot& Slot = this->Submeshes[*MeshSlotIter];
// 		const TConstArrayView<FBoxCenterExtentFloat> MeshBounds = this->AnimCollection->GetMeshBounds(Slot.MeshDefIndex);
// 		LocalBound.Add(MeshBounds[BoundIndex]);
// 		MeshSlotIter++;
// 
// 	} while (*MeshSlotIter != 0xFF);
// 
// 	LocalBound.ToCenterExtentBox(this->InstancesData.LocalBounds[InstanceIndex]);
// #endif // 
// }

// void USkelotComponent::Internal_UpdateInstanceLocalBoundSM(int InstanceIndex)
// {
// 	const auto BoundIndex = GetInstanceBoundIndex(InstanceIndex);
// 	const TConstArrayView<FBoxCenterExtentFloat> MeshBounds = this->AnimCollection->GetMeshBounds(this->Submeshes[0].MeshDefIndex);
// 	InstancesData.LocalBounds[InstanceIndex] = MeshBounds[BoundIndex];
// }

void USkelotComponent::UpdateInstanceLocalBound(int InstanceIndex)
{
	check(IsInstanceValid(InstanceIndex) && !ShouldUseFixedInstanceBound() && this->InstancesData.LocalBounds.IsValidIndex(InstanceIndex));

	const auto BoundIndex = GetInstanceBoundIndex(InstanceIndex);
	const uint8* MeshSlotIter = GetInstanceMeshSlots(InstanceIndex);
	if (*MeshSlotIter == 0xFF) //has no mesh ?
	{
		this->InstancesData.LocalBounds[InstanceIndex] = FBoxCenterExtentFloat(ForceInit);
		return;
	}

	FBoxMinMaxFloat LocalBound(ForceInit);

	do
	{
		const FSkelotSubmeshSlot& Slot = this->Submeshes[*MeshSlotIter];
		const TConstArrayView<FBoxCenterExtentFloat> MeshBounds = this->AnimCollection->GetMeshBounds(Slot.MeshDefIndex);
		LocalBound.Add(MeshBounds[BoundIndex]);
		check(!LocalBound.IsForceInitValue());
		MeshSlotIter++;

	} while (*MeshSlotIter != 0xFF);

	LocalBound.ToCenterExtentBox(this->InstancesData.LocalBounds[InstanceIndex]);
}

void USkelotComponent::UpdateLocalBounds()
{
	if(!ShouldUseFixedInstanceBound())
	{
		for (int32 InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)
		{
			if (EnumHasAnyFlags(InstancesData.Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Destroyed | ESkelotInstanceFlags::EIF_Hidden))
				continue;

			UpdateInstanceLocalBound(InstanceIndex);
		}
	}
}



void USkelotComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
}

void USkelotComponent::PreDuplicate(FObjectDuplicationParameters& DupParams)
{
	Super::PreDuplicate(DupParams);
}

void USkelotComponent::OnRegister()
{
	Super::OnRegister();
	CheckAssets_Internal();
}

void USkelotComponent::OnUnregister()
{
	Super::OnUnregister();
}

FBoxSphereBounds USkelotComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if(!AnimCollection || !bAnyValidSubmesh || Submeshes.Num() == 0 || GetAliveInstanceCount() == 0)
		return FBoxSphereBounds(ForceInit);

	USkelotComponent* NonConst = const_cast<USkelotComponent*>(this);
	FBoxMinMaxFloat CompBound(ForceInit);
	NonConst->CalcInstancesBound(CompBound, nullptr);

	//happens if all instances are hidden or destroyed. rare case !
	if (CompBound.IsForceInitValue())
	{
		return FBoxSphereBounds(ForceInit);
	}
	
	CompBound.Expand(NonConst->ComponentBoundExtent);
	return FBoxSphereBounds(CompBound.ToBoxSphereBounds());
}

FBoxSphereBounds USkelotComponent::CalcLocalBounds() const
{
	return FBoxSphereBounds();
}

TStructOnScope<FActorComponentInstanceData> USkelotComponent::GetComponentInstanceData() const
{
	//return Super::GetComponentInstanceData();
	return MakeStructOnScope<FActorComponentInstanceData, FSkelotComponentInstanceData>(this);
}


void USkelotComponent::ApplyInstanceData(FSkelotComponentInstanceData& ID)
{
	InstancesData = ID.InstanceData;
	IndexAllocator = ID.IndexAllocator;
	FixInstanceData();
	MarkRenderStateDirty();
}


FSkelotComponentInstanceData::FSkelotComponentInstanceData(const USkelotComponent* InComponent) : FPrimitiveComponentInstanceData(InComponent)
{
	InstanceData = InComponent->InstancesData;
	IndexAllocator = InComponent->IndexAllocator;
}

void FSkelotComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	Super::ApplyToComponent(Component, CacheApplyPhase);

	USkelotComponent* Skelot = CastChecked<USkelotComponent>(Component);
	Skelot->ApplyInstanceData(*this);
}

void FSkelotComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}





void FSkelotInstancesData::Reset()
{
	Flags.Reset();
	FrameIndices.Reset();
	AnimationStates.Reset();
	Locations.Reset();
	Rotations.Reset();
	Scales.Reset();
	Matrices.Reset();
	RenderCustomData.Reset();
	MeshSlots.Reset();
	LocalBounds.Reset();
	CustomPerInstanceStruct.Reset();
}

void FSkelotInstancesData::Empty()
{
	Flags.Empty();
	FrameIndices.Empty();
	AnimationStates.Empty();
	Locations.Empty();
	Rotations.Empty();
	Scales.Empty();
	Matrices.Empty();
	RenderCustomData.Empty();
	MeshSlots.Empty();
	LocalBounds.Empty();
	CustomPerInstanceStruct.Empty();
}

// FArchive& operator<<(FArchive& Ar, FSkelotInstancesData& R)
// {
// 	R.Flags.BulkSerialize(Ar);
// 	R.Locations.BulkSerialize(Ar);
// 	R.Rotations.BulkSerialize(Ar);
// 	R.Scales.BulkSerialize(Ar);
// 	R.RenderCustomData.BulkSerialize(Ar);
// 	//R.AnimationStates.BulkSerialize(Ar); //#TODO
// 	R.CustomPerInstanceStruct.BulkSerialize(Ar);
// 	return Ar;
// }


void FSkelotInstancesData::RemoveFlags(ESkelotInstanceFlags FlagsToRemove)
{
	constexpr int NumEnumPerPack = sizeof(VectorRegister4Int) / sizeof(ESkelotInstanceFlags);
	constexpr int EnumNumBits = sizeof(ESkelotInstanceFlags) * 8;
	check(Flags.Num() % NumEnumPerPack == 0);
	const int MaskDW = ~(static_cast<int>(FlagsToRemove) | (static_cast<int>(FlagsToRemove) << EnumNumBits));
	SkelotArrayAndSSE(Flags.GetData(), Flags.Num() / NumEnumPerPack, MaskDW);
}





//bool FSkelotInstancesData::operator==(const FSkelotInstancesData& Other) const
//{
//	return this->Flags.Num() == Other.Flags.Num()
//		&& this->FrameIndices.Num() == Other.FrameIndices.Num()
//		&& this->AnimationStates.Num() == Other.AnimationStates.Num()
//		&& this->Locations.Num() == Other.Locations.Num()
//		&& this->Rotations.Num() == Other.Rotations.Num()
//		&& this->Scales.Num() == Other.Scales.Num()
//		&& this->Matrices.Num() == Other.Matrices.Num()
//		&& this->RenderMatrices.Num() == Other.RenderMatrices.Num()
//		&& this->RenderCustomData.Num() == Other.RenderCustomData.Num()
//		&& this->MeshSlots.Num() == Other.MeshSlots.Num()
//		&& this->LocalBounds.Num() == Other.LocalBounds.Num();
//}

















//FPrimitiveSceneProxy* USkelotSingleMeshComponent::CreateSceneProxy()
//{
//	if (GetAliveInstanceCount() > 0 && SkeletalMesh && AnimCollection && AnimCollection->Skeleton == SkeletalMesh->GetSkeleton() && MeshIndex != -1 && AnimCollection->Meshes[MeshIndex].MeshData)
//	{
//		return new FSkelotProxy(this, GetFName());
//	}
//
//	return nullptr;
//}

//FBoxSphereBounds USkelotSingleMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
//{
//	//bRenderDynamicDataDirty must be true because we mark both transform and dynamic data dirty at the same time, see UActorComponent::DoDeferredRenderUpdates_Concurrent
//	if(MeshIndex == -1)
//		return FBoxSphereBounds(ForceInit);
//
//	return FBoxSphereBounds(CalcInstancesBound(GetMeshLocalBounds()).ToBoxSphereBounds());
//}
//
//FBoxSphereBounds USkelotSingleMeshComponent::CalcLocalBounds() const
//{
//	if(!SkeletalMesh)
//		return FBoxSphereBounds(ForceInit);
//	
//	return SkeletalMesh->GetBounds();
//}


/*
void USkelotSingleMeshComponent::Internal_CheckAssets()
{
	check(IsInGameThread());
	MeshIndex = -1;
	PrevDynamicDataInstanceCount = 0;
	MarkRenderStateDirty();

	if (SkeletalMesh && AnimCollection)
	{
		if (AnimCollection->Skeleton == nullptr || SkeletalMesh->GetSkeleton() != AnimCollection->Skeleton)
		{
			//#TODO how to log bone mismatch ? logging error during cook will interrupt it
			//UE_LOG(LogSkelot, Error, TEXT("SkeletalMesh and AnimSet must use the same Skeleton. Component:%s AnimSet:%s SkeletalMesh:%s"), *GetName(), *AnimCollection->GetName(), *SkeletalMesh->GetName());
			//if new data is invalid (null AnimSet, mismatch skeleton, ...) we keep transforms but reset animation related data
			ResetAnimationStates();
			return;
		}

		if (WITH_EDITOR && FApp::CanEverRender())
		{
			AnimCollection->TryBuildAll();
		}

		int MeshDefIdx = AnimCollection->FindMeshDef(SkeletalMesh);
		if (MeshDefIdx == -1 || !AnimCollection->Meshes[MeshDefIdx].MeshData)
		{
			//UE_LOG(LogSkelot, Error, TEXT("SkeletalMesh [%s] is not registered in AnimSet [%s]"), *SkeletalMesh->GetName(), *AnimCollection->GetName());
			//if new data is invalid (null AnimSet, mismatch skeleton, ...) we keep transforms but reset animation related data
			ResetAnimationStates();
			return;
		}

		this->MeshIndex = MeshDefIdx;
	}
}*/


int USkelotComponent::LineTraceInstanceAny(int InstanceIndex, const FVector& Start, const FVector& End) const
{
	check(IsInstanceValid(InstanceIndex) && AnimCollection);

	const FTransform T = FTransform(GetInstanceTransform(InstanceIndex));
	FVector LocalStart = T.InverseTransformPosition(Start);
	FVector LocalEnd = T.InverseTransformPosition(End);
	FVector LocalDir = LocalEnd - LocalStart;

	if (!FMath::LineBoxIntersection(FBox(AnimCollection->MeshesBBox.GetFBox()), LocalStart, LocalEnd, LocalDir))
	{
		return -1;
	}

	auto Len = LocalDir.Size();
	LocalDir /= Len;

	AnimCollection->ConditionalFlushDeferredTransitions(InstancesData.FrameIndices[InstanceIndex]);

	const uint8* MeshSlotIter = GetInstanceMeshSlots(InstanceIndex);
	while (*MeshSlotIter != 0xFF) //for each sub mesh
	{
		const FSkelotSubmeshSlot& MeshSlot = this->Submeshes[*MeshSlotIter];
		const FSkelotMeshDef& MeshDef = this->AnimCollection->Meshes[MeshSlot.MeshDefIndex];
		int32 HitBoneIndex = MeshDef.CompactPhysicsAsset.RayCastAny(AnimCollection, InstancesData.FrameIndices[InstanceIndex], LocalStart, LocalDir, Len);
		if(HitBoneIndex != -1)
			return HitBoneIndex;

		MeshSlotIter++;
	}

	return -1;
}



int USkelotComponent::LineTraceInstanceSingle(int InstanceIndex, const FVector& Start, const FVector& End, float Thickness, double& OutTime, FVector& OutPosition, FVector& OutNormal, bool bCheckBoundingBoxFirst) const
{
	check(IsInstanceValid(InstanceIndex) && AnimCollection);
	//invert the ray, we don't need to transform shapes
	const FTransform InstanceTransform = FTransform(GetInstanceTransform(InstanceIndex));
	FVector LocalStart = InstanceTransform.InverseTransformPosition(Start);
	FVector LocalEnd = InstanceTransform.InverseTransformPosition(End);
	FVector LocalDir = LocalEnd - LocalStart;
	auto Len = LocalDir.Size();
	LocalDir /= Len;

	if(bCheckBoundingBoxFirst) //check for Max Bounding Box vs Ray first 
	{
		const FBox LocalBound = FBox(AnimCollection->MeshesBBox.GetFBox());
		Chaos::FReal unusedTime;
		Chaos::FVec3 unusedPos, unusedNormal;
		int unusedFaceIndex;
		if (!Chaos::FAABB3(LocalBound.Min, LocalBound.Max).Raycast(LocalStart, LocalDir, Len, Thickness, unusedTime, unusedPos, unusedNormal, unusedFaceIndex))
		{
			return -1;
		}
	}
	

	AnimCollection->ConditionalFlushDeferredTransitions(InstancesData.FrameIndices[InstanceIndex]);
	
	int32 LeastHitBone = -1;
	OutTime = DBL_MAX;

	const uint8* MeshSlotIter = GetInstanceMeshSlots(InstanceIndex);
	while (*MeshSlotIter != 0xFF) //for each sub mesh
	{
		const FSkelotSubmeshSlot& MeshSlot = this->Submeshes[*MeshSlotIter];
		const FSkelotMeshDef& MeshDef = this->AnimCollection->Meshes[MeshSlot.MeshDefIndex];
		double HitTime;
		FVector HitPos, HitNorm;
		int32 HitBone = MeshDef.CompactPhysicsAsset.Raycast(AnimCollection, InstancesData.FrameIndices[InstanceIndex], LocalStart, LocalDir, Len, Thickness, HitTime, HitPos, HitNorm);
		if (HitBone != -1 && HitTime < OutTime)
		{
			LeastHitBone = HitBone;
			OutTime = HitTime;
			OutPosition = InstanceTransform.TransformPosition(HitPos);
			OutNormal = InstanceTransform.TransformVector(HitNorm);
		}
		MeshSlotIter++;
	}

	return LeastHitBone;
}














bool USkelotComponent::EnableInstanceDynamicPose(int InstanceIndex, bool bEnable)
{
	if(!IsInstanceValid(InstanceIndex))
		return false;

	const bool bIsDynamic = InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_DynamicPose);
	if (bIsDynamic == bEnable)
		return true;

	
	if (bEnable)
	{
		int DynamicPoseIndex = AnimCollection->AllocDynamicPose();
		if (DynamicPoseIndex == -1)
		{
			UE_LOG(LogSkelot, Warning, TEXT("EnableInstanceDynamicPose Failed. Buffer Full! InstanceIndex:%d"), InstanceIndex);
			return false;
		}

		//release transition if any
		if (InstanceHasAnyFlag(InstanceIndex, ESkelotInstanceFlags::EIF_AnimPlayingTransition))
			AnimCollection->DecTransitionRef(InstancesData.AnimationStates[InstanceIndex].TransitionIndex);

		InstanceRemoveFlags(InstanceIndex, ESkelotInstanceFlags::EIF_AllAnimationFlags);
		InstanceAddFlags(InstanceIndex, ESkelotInstanceFlags::EIF_DynamicPose);
		InstancesData.FrameIndices[InstanceIndex] = AnimCollection->DynamicPoseIndexToFrameIndex(DynamicPoseIndex);
		InstancesData.AnimationStates[InstanceIndex] = FSkelotInstanceAnimState{};
	}
	else
	{
		ReleaseDynamicPose_Internal(InstanceIndex);
	}

	return true;
}

void USkelotComponent::ReleaseDynamicPose_Internal(int InstanceIndex)
{
	InstanceRemoveFlags(InstanceIndex, ESkelotInstanceFlags::EIF_AllAnimationFlags | ESkelotInstanceFlags::EIF_DynamicPose);
	InstanceAddFlags(InstanceIndex, ESkelotInstanceFlags::EIF_AnimNoSequence);
	int& FrameIndex = InstancesData.FrameIndices[InstanceIndex];
	check(AnimCollection->IsDynamicPoseFrameIndex(FrameIndex));
	int DynamicPoseIndex = AnimCollection->FrameIndexToDynamicPoseIndex(FrameIndex);
	AnimCollection->FlipDynamicPoseSign(DynamicPoseIndex);
	AnimCollection->FreeDynamicPose(DynamicPoseIndex);
	
	FrameIndex = 0;
	InstancesData.AnimationStates[InstanceIndex] = FSkelotInstanceAnimState{};
}

/*
returned pointer must be filled before any new call.
*/
FMatrix3x4* USkelotComponent::InstanceRequestDynamicPoseUpload(int InstanceIndex)
{
	check(AnimCollection && IsInstanceValid(InstanceIndex) && IsInstanceDynamicPose(InstanceIndex));
	int& FrameIndex = InstancesData.FrameIndices[InstanceIndex];
	check(AnimCollection->IsDynamicPoseFrameIndex(FrameIndex));
	int DynamicPoseIndex = AnimCollection->FrameIndexToDynamicPoseIndex(FrameIndex);
	FrameIndex = AnimCollection->FlipDynamicPoseSign(DynamicPoseIndex);
	return AnimCollection->RequestPoseUpload(FrameIndex);
}



namespace Utils
{
	void FillDynamicPoseFromComponent_Concurrent(USkelotComponent * Self, USkeletalMeshComponent * MeshComp, int ScatterIdx, int InstanceIndex, TArrayView<FTransform> FullPose)
	{
		Self->SetInstanceTransform(InstanceIndex, FTransform3f(MeshComp->GetComponentTransform()));

		const TArray<FTransform>& Transforms = MeshComp->GetComponentSpaceTransforms();
		USkeleton* Skeleton = Self->AnimCollection->Skeleton;

		const FSkeletonToMeshLinkup& LinkupCache = Skeleton->FindOrAddMeshLinkupData(MeshComp->GetSkeletalMeshAsset());
		for (FBoneIndexType BoneIndex : MeshComp->RequiredBones)
		{
			int SKBoneIndex = LinkupCache.MeshToSkeletonTable[BoneIndex];
			FullPose[SKBoneIndex] = Transforms[BoneIndex];
		}

		Self->AnimCollection->CurrentUpload.ScatterData[ScatterIdx] = Self->InstancesData.FrameIndices[InstanceIndex];
		FMatrix3x4* RenderMatrices = &Self->AnimCollection->CurrentUpload.PoseData[ScatterIdx * Self->AnimCollection->RenderBoneCount];

		Self->AnimCollection->CalcRenderMatrices(FullPose, RenderMatrices);
	}
};


void USkelotComponent::FillDynamicPoseFromComponents()
{
	if (DynamicPoseInstancesTiedToSMC.Num() == 0)
		return;

	SKELOT_SCOPE_CYCLE_COUNTER(FillDynamicPoseFromComponents);
	
	int ScatterBaseIndex = AnimCollection->ReserveUploadData(DynamicPoseInstancesTiedToSMC.Num());
	int NumProcessed = 0;

	TArray<FTransform, TInlineAllocator<255>> FullPose;
	FullPose.Init(FTransform::Identity, AnimCollection->AnimationBoneCount);

	for (const auto& Pair : DynamicPoseInstancesTiedToSMC)
	{
		if (Pair.Value.MeshComponent)
		{
			int& FrameIndex = InstancesData.FrameIndices[Pair.Value.InstanceIndex];
			int DynamicPoseIndex = AnimCollection->FrameIndexToDynamicPoseIndex(FrameIndex);
			FrameIndex = AnimCollection->FlipDynamicPoseSign(DynamicPoseIndex); //#TODO is safe for concurrent ?

			FullPose.Init(FTransform::Identity, AnimCollection->AnimationBoneCount);
			Utils::FillDynamicPoseFromComponent_Concurrent(this, Pair.Value.MeshComponent, ScatterBaseIndex + NumProcessed, Pair.Value.InstanceIndex, FullPose);
			NumProcessed++;
		}
		else
		{
		}
	}

	if (NumProcessed != DynamicPoseInstancesTiedToSMC.Num())
	{
		AnimCollection->UploadDataSetNumUninitialized(ScatterBaseIndex + NumProcessed);
	}
}

void USkelotComponent::FillDynamicPoseFromComponents_Concurrent()
{
	FillDynamicPoseFromComponents();
}

USkeletalMeshComponent* USkelotComponent::GetInstanceTiedSkeletalMeshComponent(int InstanceIndex) const
{
	int DPI = AnimCollection->FrameIndexToDynamicPoseIndex(this->InstancesData.FrameIndices[InstanceIndex]);
	return DynamicPoseInstancesTiedToSMC.FindRef(DPI).MeshComponent;
}

void USkelotComponent::TieDynamicPoseToComponent(int InstanceIndex, USkeletalMeshComponent* SrcComponent, int UserData)
{
	if(IsInstanceValid(InstanceIndex) && IsInstanceDynamicPose(InstanceIndex) && IsValid(SrcComponent))
	{
		InstanceAddFlags(InstanceIndex, ESkelotInstanceFlags::EIF_BoundToSMC);
		int DPI = AnimCollection->FrameIndexToDynamicPoseIndex(this->InstancesData.FrameIndices[InstanceIndex]);
		DynamicPoseInstancesTiedToSMC.FindOrAdd(DPI, FSkelotDynInsTieData { SrcComponent, InstanceIndex, UserData });
	}
}

void USkelotComponent::UntieDynamicPoseFromComponent(int InstanceIndex)
{
	if(IsInstanceValid(InstanceIndex) && InstanceHasAllFlag(InstanceIndex, ESkelotInstanceFlags::EIF_DynamicPose | ESkelotInstanceFlags::EIF_BoundToSMC))
	{
		InstanceRemoveFlags(InstanceIndex, ESkelotInstanceFlags::EIF_BoundToSMC);
		int DPI = AnimCollection->FrameIndexToDynamicPoseIndex(this->InstancesData.FrameIndices[InstanceIndex]);
		DynamicPoseInstancesTiedToSMC.FindAndRemoveChecked(DPI);
	}
}

int USkelotComponent::FindListener(ISkelotListenerInterface* InterfacePtr) const
{
	for (int i = 0; i < NumListener; i++)
		if (ListenersPtr[i] == InterfacePtr)
			return i;

	return -1;
}

int USkelotComponent::FindListenerUserData(ISkelotListenerInterface* InterfacePtr) const
{
	for (int i = 0; i < NumListener; i++)
		if (ListenersPtr[i] == InterfacePtr)
			return ListenersUserData[i];

	return -1;
}

void USkelotComponent::RemoveListener(ISkelotListenerInterface* InterfacePtr)
{
	int Index = FindListener(InterfacePtr);
	if (Index != -1)
	{
		//perform remove at swap
		NumListener--;
		ListenersPtr[Index] = ListenersPtr[NumListener];
		ListenersUserData[Index] = ListenersUserData[NumListener];
	}
}

int USkelotComponent::AddListener(ISkelotListenerInterface* InterfacePtr, int UserData)
{
	if (InterfacePtr && CanAddListener())
	{
		ListenersPtr[NumListener] = InterfacePtr;
		ListenersUserData[NumListener] = UserData;
		return NumListener++;
	}
	return -1;
}





template<class TObject> bool InstanceAttachChildInternal(USkelotComponent* Self, int InstanceIndex, const TObject* ObjectToAttach, int SkelotInstanceIndex, const FTransform3f& RelativeTransform, FName SocketName, bool bAutoDestroy)
{
	if (!Self->IsInstanceValid(InstanceIndex) || Self->MaxAttachmentPerInstance == 0 || !IsValid(ObjectToAttach))
		return false;

	for (int ChildIdx = 0; ChildIdx < Self->MaxAttachmentPerInstance; ChildIdx++)
	{
		FSkelotComponentAttachData& AD = Self->InstancesData.Attachments[InstanceIndex * Self->MaxAttachmentPerInstance + ChildIdx];
		if (!IsValid(AD.Object))
		{
			const USkelotComponent::FSocketMinimalInfo SMI = Self->GetSocketMinimalInfo(SocketName);

			AD.Object = (UObject*)(ObjectToAttach);
			AD.RelativeTransform = RelativeTransform;
			AD.SocketTransform = SMI.LocalTransform;
			AD.SocketBoneIndex = static_cast<int16>(SMI.BoneIndex);
			AD.bAutoDestroy = bAutoDestroy;
			AD.SkelotInstanceIndex = SkelotInstanceIndex;

			return true;
		}
	}

	UE_LOGFMT(LogSkelot, Warning, "USkelotComponent.InstanceAttachChild failed. AnimCollection:{0} InstanceIndex:{1} ObjectToAttach:{2}", Self->AnimCollection->GetName(), InstanceIndex, ObjectToAttach->GetName());
	return false;
}
bool USkelotComponent::InstanceAttachChildComponent(int InstanceIndex, const USceneComponent* ComponentToAttach, const FTransform3f& RelativeTransform, FName SocketName, bool bAutoDestroy)
{
	return InstanceAttachChildInternal<USceneComponent>(this, InstanceIndex, ComponentToAttach, -1, RelativeTransform, SocketName, bAutoDestroy);
}

bool USkelotComponent::InstanceAttachChildActor(int InstanceIndex, const AActor* ActorToAttach, const FTransform3f& RelativeTransform, FName SocketName, bool bAutoDestroy)
{
	return InstanceAttachChildInternal<AActor>(this, InstanceIndex, ActorToAttach, -1, RelativeTransform, SocketName, bAutoDestroy);
}

bool USkelotComponent::InstanceAttachChildSkelot(int InstanceIndex, const USkelotComponent* ChildComponent, int ChildInstanceIndex, const FTransform3f& RelativeTransform, FName SocketName, bool bAutoDestroy)
{
	if(!IsValid(ChildComponent) || !ChildComponent->IsInstanceValid(ChildInstanceIndex))
		return false;

	return InstanceAttachChildInternal<USkelotComponent>(this, InstanceIndex, ChildComponent, ChildInstanceIndex, RelativeTransform, SocketName, bAutoDestroy);
}

void USkelotComponent::InstanceGetAttachedComponents(int InstanceIndex, TArray<USceneComponent*>& AttachedComponents)
{
	if (IsInstanceValid(InstanceIndex))
	{
		for (int ChildIdx = 0; ChildIdx < MaxAttachmentPerInstance; ChildIdx++)
		{
			const FSkelotComponentAttachData& AD = InstancesData.Attachments[InstanceIndex * MaxAttachmentPerInstance + ChildIdx];
			if (USkelotComponent* AsSC = Cast<USkelotComponent>(AD.Object))
				AttachedComponents.Add(AsSC);
		}
	}
}

bool USkelotComponent::InstanceDetachComponent(int InstanceIndex, const USceneComponent* ComponentToDetach)
{
	if (IsInstanceValid(InstanceIndex) && IsValid(ComponentToDetach))
	{
		for (int ChildIdx = 0; ChildIdx < MaxAttachmentPerInstance; ChildIdx++)
		{
			FSkelotComponentAttachData& AD = InstancesData.Attachments[InstanceIndex * MaxAttachmentPerInstance + ChildIdx];
			if (AD.Object == ComponentToDetach)
			{
				AD.Object = nullptr;
				return true;
			}
		}
	}
	return false;
}

void USkelotComponent::InstanceDetachAllChildren(int InstanceIndex, bool bDestroyThemAll, bool bPromoteChildren)
{
	if (IsInstanceValid(InstanceIndex))
	{
		for (int ChildIdx = 0; ChildIdx < MaxAttachmentPerInstance; ChildIdx++)
		{
			FSkelotComponentAttachData& AD = InstancesData.Attachments[InstanceIndex * MaxAttachmentPerInstance + ChildIdx];
			if (bDestroyThemAll && IsValid(AD.Object))
			{
				AD.DestroyObject(bPromoteChildren);
			}
			AD.Object = nullptr;
		}
	}
}

void USkelotComponent::UpdateAttachedComponents()
{
	AnimCollection->FlushDeferredTransitions();
	SKELOT_SCOPE_CYCLE_COUNTER(UpdateAttachedComponents);

	if(MaxAttachmentPerInstance == 0)
		return;

	for (int InstanceIndex = 0; InstanceIndex < GetInstanceCount(); InstanceIndex++)
	{
		if (!IsInstanceAlive(InstanceIndex))
			continue;

		for(int ChildIndex = 0; ChildIndex < MaxAttachmentPerInstance; ChildIndex++)
		{
			const FSkelotComponentAttachData& AttachData = InstancesData.Attachments[InstanceIndex * MaxAttachmentPerInstance + ChildIndex];
			if (!IsValid(AttachData.Object))
				continue;

			FTransform3f FinalTransform;
			if (AttachData.SocketBoneIndex != -1 && AnimCollection->IsBoneTransformCached(AttachData.SocketBoneIndex)) //is attached to a Socket ?
			{
				FTransform3f BoneT = AnimCollection->GetBoneTransformFast(AttachData.SocketBoneIndex, InstancesData.FrameIndices[InstanceIndex]);
				FinalTransform = AttachData.RelativeTransform * AttachData.SocketTransform * BoneT * GetInstanceTransform(InstanceIndex);
			}
			else
			{
				FinalTransform = AttachData.RelativeTransform * GetInstanceTransform(InstanceIndex);
			}

			if (AttachData.Object->IsA<USkelotComponent>())
			{
				USkelotComponent* AsSkelot = static_cast<USkelotComponent*>(AttachData.Object);
				AsSkelot->SetInstanceTransform(AttachData.SkelotInstanceIndex, FinalTransform);
			}
			else if (AttachData.Object->IsA<USceneComponent>())
			{
				USceneComponent* AsSC = static_cast<USceneComponent*>(AttachData.Object);
				AsSC->SetWorldTransform(FTransform(FinalTransform));
			}
			else if (AttachData.Object->IsA<AActor>())
			{
				AActor* AsActor = static_cast<AActor*>(AttachData.Object);
				AsActor->SetActorTransform(FTransform(FinalTransform));
			}
		}
	}
}

FPrimitiveSceneProxy* USkelotComponent::CreateSceneProxy()
{
	if (AnimCollection && AnimCollection->bIsBuilt && Submeshes.Num() > 0 && bAnyValidSubmesh)
	{
		return new FSkelotProxy(this, GetFName());
	}

	//this->bRenderStateCreated will be still true but this->SceneProxy null
	return nullptr;
}

bool USkelotComponent::ShouldCreateRenderState() const
{
	return true;
	//return AnimCollection != nullptr && Submeshes.Num() > 0;
}

void USkelotComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

// FBoxSphereBounds USkelotComponent::CalcBounds(const FTransform& LocalToWorld) const
// {
// 	//bRenderDynamicDataDirty must be true because we mark both transform and dynamic data dirty at the same time, see UActorComponent::DoDeferredRenderUpdates_Concurrent
// 	if (Submeshes.Num() == 0 || !Submeshes[0].SkeletalMesh || Submeshes[0].MeshDefIndex == -1)
// 		return FBoxSphereBounds(ForceInit);
// 
// 
// 	return FBoxSphereBounds(CalcInstancesBound(GetMeshLocalBounds()).ToBoxSphereBounds());
// }
// 
// FBoxSphereBounds USkelotComponent::CalcLocalBounds() const
// {
// 	return FBoxSphereBounds(ForceInit);
// }

void USkelotComponent::CheckAssets_Internal()
{
	check(IsInGameThread());
	PrevDynamicDataInstanceCount = 0;
	for(FSkelotSubmeshSlot& MeshSlot : Submeshes)
	{
		MeshSlot.MeshDefIndex = -1;
	}
	bAnyValidSubmesh = false;

	if (AnimCollection)
	{
		if(AnimCollection->Skeleton)
		{
			for (FSkelotSubmeshSlot& MeshSlot : Submeshes)
			{
				if (!MeshSlot.SkeletalMesh)
				{
					continue;
				}

				if (MeshSlot.SkeletalMesh->GetSkeleton() != AnimCollection->Skeleton)
				{
					continue;
				}

				int MeshDefIdx = AnimCollection->FindMeshDef(MeshSlot.SkeletalMesh);
				if (MeshDefIdx == -1 /*|| !AnimCollection->Meshes[MeshDefIdx].MeshData*/)
				{
					continue;
				}

				MeshSlot.MeshDefIndex = MeshDefIdx;
				bAnyValidSubmesh = true;
			}
#if WITH_EDITOR
			if (FApp::CanEverRender() && bAnyValidSubmesh)
			{
				AnimCollection->TryBuildAll();
			}
#endif
		}
	}

	//UpdateBounds();
	MarkRenderStateDirty();
	MarkCachedMaterialParameterNameIndicesDirty();
	IStreamingManager::Get().NotifyPrimitiveUpdated(this);
}

#if WITH_EDITORONLY_DATA
void USkelotComponent::AddAllSkeletalMeshes()
{
	if (AnimCollection)
	{
		for (const FSkelotMeshDef& MD : AnimCollection->Meshes)
		{
			if (MD.Mesh && FindSubmeshIndex(MD.Mesh) == -1)
			{
				FSkelotSubmeshSlot& SM = Submeshes.AddDefaulted_GetRef();
				SM.SkeletalMesh = MD.Mesh;
			}
		}
		CheckAssets_Internal();
	}
}
#endif



int USkelotComponent::FindSubmeshIndex(const USkeletalMesh* SubMeshAsset) const
{
	return Submeshes.IndexOfByPredicate([=](const FSkelotSubmeshSlot& Slot) { return Slot.SkeletalMesh == SubMeshAsset; });
}

int USkelotComponent::FindSubmeshIndex(FName SubMeshName) const
{
	return Submeshes.IndexOfByPredicate([=](const FSkelotSubmeshSlot& Slot) { return Slot.Name == SubMeshName; });
}

int USkelotComponent::FindSubmeshIndex(FName SubmeshName, const USkeletalMesh* OrSubmeshAsset) const
{
	return SubmeshName.IsNone() ? FindSubmeshIndex(OrSubmeshAsset) : FindSubmeshIndex(SubmeshName);
}

bool USkelotComponent::InstanceAttachSubmeshByName(int InstanceIndex, FName SubMeshName, bool bAttach)
{
	if (IsInstanceValid(InstanceIndex) && !SubMeshName.IsNone())
	{
		int MeshIndex = FindSubmeshIndex(SubMeshName);
		if (MeshIndex != -1 && Submeshes[MeshIndex].MeshDefIndex != -1)
		{
			return InstanceAttachSubmeshByIndex_Internal(InstanceIndex, (uint8)MeshIndex, bAttach);
		}
	}
	return false;
}

bool USkelotComponent::InstanceAttachSubmeshByAsset(int InstanceIndex, USkeletalMesh* InMesh, bool bAttach)
{
	if (IsInstanceValid(InstanceIndex) && InMesh)
	{
		int MeshIndex = FindSubmeshIndex(InMesh);
		if (MeshIndex != -1 && Submeshes[MeshIndex].MeshDefIndex != -1)
		{
			return InstanceAttachSubmeshByIndex_Internal(InstanceIndex, (uint8)MeshIndex, bAttach);
		}
	}
	return false;
}


bool USkelotComponent::InstanceAttachSubmeshByIndex(int InstanceIndex, uint8 MeshIndex, bool bAttach)
{
	if(IsInstanceValid(InstanceIndex) && Submeshes.IsValidIndex(MeshIndex))
	{
		return InstanceAttachSubmeshByIndex_Internal(InstanceIndex, MeshIndex, bAttach);
	}
	return false;
}


bool USkelotComponent::InstanceAttachSubmeshByIndex_Internal(int InstanceIndex, uint8 MeshIndex, bool bAttach)
{
	check(this->Submeshes.IsValidIndex(MeshIndex) && this->Submeshes[MeshIndex].MeshDefIndex != -1);
	InstanceAddFlags(InstanceIndex, ESkelotInstanceFlags::EIF_NeedLocalBoundUpdate);
	uint8* SlotBegin = GetInstanceMeshSlots(InstanceIndex);
	if (bAttach)
	{
		return SkelotTerminatedArrayAddUnique(SlotBegin, MaxMeshPerInstance, MeshIndex);
	}
	else
	{
		return SkelotTerminatedArrayRemoveShift(SlotBegin, MaxMeshPerInstance, MeshIndex);
	}
}

int USkelotComponent::InstanceAttachSubmeshes(int InstanceIndex, const TArray<USkeletalMesh*>& InMeshes, bool bAttach)
{
	int NumAttach = 0;
	if (IsInstanceValid(InstanceIndex))
	{
		for (USkeletalMesh* SKMesh : InMeshes)
		{
			if(InstanceAttachSubmeshByAsset(InstanceIndex, SKMesh, bAttach))
				NumAttach++;
		}
	}
	return NumAttach;
}

void USkelotComponent::InstanceDetachAllSubmeshes(int InstanceIndex)
{
	if(IsInstanceValid(InstanceIndex))
	{
		uint8* MS = this->GetInstanceMeshSlots(InstanceIndex);
		*MS = 0xFF;
	}
}

void USkelotComponent::GetInstanceAttachedSkeletalMeshes(int InstanceIndex, TArray<USkeletalMesh*>& OutMeshes) const
{
	if(IsInstanceValid(InstanceIndex))
	{
		const uint8* SlotIter = this->GetInstanceMeshSlots(InstanceIndex);
		while(*SlotIter != 0xFF)
		{
			OutMeshes.Add(this->Submeshes[*SlotIter].SkeletalMesh);
			SlotIter++;
		}
	}
	
}

void USkelotComponent::ResetMeshSlots()
{
	if (InstancesData.MeshSlots.Num())
	{
		FMemory::Memset(InstancesData.MeshSlots.GetData(), 0xFF, InstancesData.MeshSlots.Num() * InstancesData.MeshSlots.GetTypeSize());
	}
}

void USkelotComponent::AddSubmesh(const FSkelotSubmeshSlot& InData)
{
	if (Submeshes.Num() >= SKELOT_MAX_SUBMESH)
	{
		UE_LOG(LogSkelot, Warning, TEXT("AddSubmesh failed. reached maximum."));
		return;
	}

	Submeshes.Emplace(InData);
	CheckAssets_Internal();
}



void USkelotComponent::InitSubmeshesFromAnimCollection()
{
	if (this->AnimCollection)
	{
		const int Count = FMath::Min(SKELOT_MAX_SUBMESH, this->AnimCollection->Meshes.Num());
		this->Submeshes.Reset();
		this->Submeshes.SetNum(Count);

		for (int SubMeshIndex = 0; SubMeshIndex < Count; SubMeshIndex++)
		{
			this->Submeshes[SubMeshIndex].SkeletalMesh = this->AnimCollection->Meshes[SubMeshIndex].Mesh;
		}

		ResetMeshSlots();
		CheckAssets_Internal();
	}
	
}

void USkelotComponent::SetSubmeshAsset(int SubmeshIndex, USkeletalMesh* InMesh)
{
	if (Submeshes.IsValidIndex(SubmeshIndex))
	{
		if (Submeshes[SubmeshIndex].SkeletalMesh != InMesh)
		{
			Submeshes[SubmeshIndex].SkeletalMesh = InMesh;
			CheckAssets_Internal();
		}
	}
}

int USkelotComponent::GetSubmeshBaseMaterialIndex(int SubmeshIndex) const
{
	if (Submeshes.IsValidIndex(SubmeshIndex) && Submeshes[SubmeshIndex].SkeletalMesh)
	{
		int Counter = 0;
		for (int i = 0; i < Submeshes.Num(); i++)
		{
			if (Submeshes[i].SkeletalMesh)
			{
				if (i == SubmeshIndex)
					return Counter;

				Counter += Submeshes[i].SkeletalMesh->GetMaterials().Num();
			}
		}
	}
	return -1;
}

int USkelotComponent::GetSubmeshBaseMaterialIndexByAsset(USkeletalMesh* InMesh) const
{
	return GetSubmeshBaseMaterialIndex(FindSubmeshIndex(InMesh));
}

int USkelotComponent::GetSubmeshBaseMaterialIndexByName(FName InName) const
{
	return GetSubmeshBaseMaterialIndex(FindSubmeshIndex(InName));
}

void USkelotComponent::SetSubmeshMaterial(int SubmeshIndex, int MaterialIndex, UMaterialInterface* Material)
{
	if (Submeshes.IsValidIndex(SubmeshIndex) && Submeshes[SubmeshIndex].SkeletalMesh)
	{
		const TArray<FSkeletalMaterial>& Mats = Submeshes[SubmeshIndex].SkeletalMesh->GetMaterials();
		if (Mats.IsValidIndex(MaterialIndex))
		{
			SetMaterial(GetSubmeshBaseMaterialIndex(SubmeshIndex) + MaterialIndex, Material);
			return;
		}
	}
}

void USkelotComponent::SetSubmeshMaterial(int SubmeshIndex, FName MaterialSlotName, UMaterialInterface* Material)
{
	if (Submeshes.IsValidIndex(SubmeshIndex) && Submeshes[SubmeshIndex].SkeletalMesh)
	{
		const TArray<FSkeletalMaterial>& Mats = Submeshes[SubmeshIndex].SkeletalMesh->GetMaterials();
		for (int MaterialIndex = 0; MaterialIndex < Mats.Num(); MaterialIndex++)
		{
			if (Mats[MaterialIndex].MaterialSlotName == MaterialSlotName)
			{
				SetMaterial(GetSubmeshBaseMaterialIndex(SubmeshIndex) + MaterialIndex, Material);
				return;
			}
		}
	}
}

void USkelotComponent::SetSubmeshMaterial(FName SubmeshName, USkeletalMesh* OrSubmeshAsset, FName MaterialSlotName, int OrMaterialIndex, UMaterialInterface* Material)
{
	if (!SubmeshName.IsNone())
	{
		if (!MaterialSlotName.IsNone())
		{
			SetSubmeshMaterial(FindSubmeshIndex(SubmeshName), MaterialSlotName, Material);
		}
		else
		{
			SetSubmeshMaterial(FindSubmeshIndex(SubmeshName), OrMaterialIndex, Material);
		}
	}
	else if (OrSubmeshAsset)
	{
		if (!MaterialSlotName.IsNone())
		{
			SetSubmeshMaterial(FindSubmeshIndex(OrSubmeshAsset), MaterialSlotName, Material);
		}
		else
		{
			SetSubmeshMaterial(FindSubmeshIndex(OrSubmeshAsset), OrMaterialIndex, Material);
		}
	}
}

TArray<FName> USkelotComponent::GetSubmeshNames() const
{
	TArray<FName> Names;
	for (const FSkelotSubmeshSlot& S : Submeshes)
	{
		Names.AddUnique(S.Name);
	}
	return Names;
}

void FSkelotInstanceRef::TryDestroyInstance()
{
	if (Component && InstanceIndex != -1)
	{
		Component->DestroyInstance(InstanceIndex);
		Component = nullptr;
		InstanceIndex = -1;
	}
}

FSkelotComponentAttachData::FSkelotComponentAttachData()
{
	RelativeTransform = SocketTransform = FTransform3f::Identity;
	Object = nullptr;
	SocketBoneIndex = -1;
	bAutoDestroy = false;
}

void FSkelotComponentAttachData::DestroyObject(bool bPromoteChildren/*= false*/)
{
	if (Object->IsA<USceneComponent>())
	{
		USceneComponent* AsSC = static_cast<USceneComponent*>(Object);
		AsSC->DestroyComponent(bPromoteChildren);
	}
	else if (Object->IsA<AActor>())
	{
		AActor* AsActor = static_cast<AActor*>(Object);
		AsActor->K2_DestroyActor();
	}
	else if (Object->IsA<USkelotComponent>())
	{
		USkelotComponent* AsSkelot = static_cast<USkelotComponent*>(Object);
		AsSkelot->DestroyInstance(SkelotInstanceIndex);
	}
	Object = nullptr;
}
