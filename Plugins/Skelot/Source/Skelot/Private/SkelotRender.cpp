// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotRender.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkelotComponent.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "SkelotPrivate.h"
#include "SkelotAnimCollection.h"
#include "RendererInterface.h"
#include "Async/ParallelFor.h"

#include "Materials/MaterialRenderProxy.h"
#include "ConvexVolume.h"
#include "SceneView.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "UnrealEngine.h"
#include "SceneInterface.h"
#include "MaterialShared.h"

#include "SkelotPrivateUtils.h"
#include "Engine/SkinnedAssetCommon.h"

bool GSkelot_DrawInstanceBounds = false;
FAutoConsoleVariableRef CVar_DrawInstanceBounds(TEXT("skelot.DrawInstanceBounds"), GSkelot_DrawInstanceBounds, TEXT(""), ECVF_Default);

bool GSkelot_DrawCells = false;
FAutoConsoleVariableRef CVar_DrawCells(TEXT("skelot.DrawCells"), GSkelot_DrawCells, TEXT(""), ECVF_Default);

int GSkelot_ForcedAnimFrameIndex = -1;
FAutoConsoleVariableRef CVar_ForcedAnimFrameIndex(TEXT("skelot.ForcedAnimFrameIndex"), GSkelot_ForcedAnimFrameIndex, TEXT("force all instances to use the specified animation frame index. -1 to disable"), ECVF_Default);

int GSkelot_ForceLOD = -1;
FAutoConsoleVariableRef CVar_ForceLOD(TEXT("skelot.ForceLOD"), GSkelot_ForceLOD, TEXT("similar to r.ForceLOD, -1 to disable"), ECVF_Default);

int GSkelot_ShadowForceLOD = -1;
FAutoConsoleVariableRef CVar_ShadowForceLOD(TEXT("skelot.ShadowForceLOD"), GSkelot_ShadowForceLOD, TEXT(""), ECVF_Default);

int GSkelot_MaxTrianglePerInstance = -1;
FAutoConsoleVariableRef CVar_MaxTrianglePerInstance(TEXT("skelot.MaxTrianglePerInstance"), GSkelot_MaxTrianglePerInstance, TEXT("limits the per instance triangle counts, used for debug/profile purposes. <= 0 to disable"), ECVF_Default);

int GSkelot_FroceMaxBoneInfluence = -1;
FAutoConsoleVariableRef CVar_FroceMaxBoneInfluence(TEXT("skelot.FroceMaxBoneInfluence"), GSkelot_FroceMaxBoneInfluence, TEXT("limits the MaxBoneInfluence for all instances, -1 to disable"), ECVF_Default);

float GSkelot_DistanceScale = 1;
FAutoConsoleVariableRef CVar_DistanceScale(TEXT("skelot.DistanceScale"), GSkelot_DistanceScale, TEXT("scale used for distance based LOD. higher value results in higher LOD."), ECVF_Default);


bool GSkelot_DisableFrustumCull = false;
FAutoConsoleVariableRef CVar_DisableFrustumCull(TEXT("skelot.DisableFrustumCull"), GSkelot_DisableFrustumCull, TEXT(""), ECVF_Default);

bool GSkelot_DisableGridCull = false;
FAutoConsoleVariableRef CVar_DisableGriding(TEXT("skelot.DisableGridCull"), GSkelot_DisableGridCull, TEXT(""), ECVF_Default);

int GSkelot_NumInstancePerGridCell = 256;
FAutoConsoleVariableRef CVar_NumInstancePerGridCell(TEXT("skelot.NumInstancePerGridCell"), GSkelot_NumInstancePerGridCell, TEXT(""), ECVF_Default);

bool GSkelot_DisableSectionsUnification = false;
FAutoConsoleVariableRef CVar_DisableSectionsUnification(TEXT("skelot.DisableSectionsUnification"), GSkelot_DisableSectionsUnification, TEXT(""), ECVF_Default);


SKELOT_AUTO_CVAR_DEBUG(bool, DebugForceNoPrevFrameData, false, "", ECVF_Default);






#include "SkelotBatchGenerator.h"



FSkelotProxy::FSkelotProxy(const USkelotComponent* Component, FName ResourceName)
	: FPrimitiveSceneProxy(Component, ResourceName)
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, AminCollection(Component->AnimCollection)
	, InstanceMaxDrawDistance(Component->InstanceMaxDrawDistance)
	, InstanceMinDrawDistance(Component->InstanceMinDrawDistance)
	, DistanceScale(Component->LODDistanceScale)
	, NumCustomDataFloats(Component->NumCustomDataFloats)
	//, MinLODIndex(Component->MinLODIndex)
	//, MaxLODIndex(Component->MaxLODIndex)
	, ShadowLODBias(Component->ShadowLODBias)
	, StartShadowLODBias(Component->StartShadowLODBias)
	//, SortMode(Component->SortMode)
	, bNeedCustomDataForShadowPass(Component->bNeedCustomDataForShadowPass)
	, bHasAnyTranslucentMaterial(false)
	, bCheckForNegativeDeterminant(Component->bSupportNegativeScale)
	, MaxMeshPerInstance(Component->MaxMeshPerInstance)
	, MaxBatchCountPossible(0)
	, DynamicData(nullptr)
	, OldDynamicData(nullptr)

{
	//{
	//	//if (MinLODIndex > MaxLODIndex)
	//	//	Swap(MinLODIndex, MaxLODIndex);
	//
	//	//uint8 LODCount = MaxLODIndex - MinLODIndex;
	//	this->ShadowLODBias = Component->ShadowLODBias;
	//	this->StartShadowLODBias = Component->StartShadowLODBias;
	//}
	
	TArray<UMaterialInterface*> CompMaterials = Component->GetMaterials();
	check(CompMaterials.Num() <= 0xFFff);
	MaterialsProxy.SetNum(CompMaterials.Num());
	
	for (int i = 0; i < CompMaterials.Num(); i++)
	{
		UMaterialInterface* Material = CompMaterials[i];
		//if(GForceDefaultMaterial) GForceDefaultMaterial is not exported
		//	Material = UMaterial::GetDefaultMaterial(MD_Surface);

		if (Material)
		{
			if (!Material->CheckMaterialUsage_Concurrent(MATUSAGE_SkeletalMesh))
			{
				UE_LOG(LogSkelot, Error, TEXT("Material %s with missing usage flag was applied to %s"), *Material->GetName(), *Component->GetName());
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
		}
		else
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		MaterialsProxy[i] = Material->GetRenderProxy();
	}


	this->bAlwaysHasVelocity = true;
	this->bHasDeformableMesh = true;
	this->bGoodCandidateForCachedShadowmap = false;
	this->bVFRequiresPrimitiveUniformBuffer = true;

	for (int i = 0; i < SKELOT_MAX_LOD - 1; i++)
		this->LODDistances[i] = Component->LODDistances[i];

	//#TODO should we detect and set them ?
	//this->bEvaluateWorldPositionOffset = true;
	//this->bAnyMaterialHasWorldPositionOffset = true;

	check(Component->Submeshes.Num() > 0 && Component->Submeshes.Num() < 0xFF && this->MaxMeshPerInstance > 0);
	this->SubMeshes.SetNum(Component->Submeshes.Num());

	int MaterialCounter = 0;
	int SectionCounter = 0;
	//initialize sub meshes. the rest are initialized in CreateRenderThreadResources.
	for (int MeshIdx = 0; MeshIdx < Component->Submeshes.Num(); MeshIdx++)
	{
		const FSkelotSubmeshSlot& CompMeshSlot = Component->Submeshes[MeshIdx];

		if (!CompMeshSlot.SkeletalMesh)
			continue;

		int MeshDefIdx = AminCollection->FindMeshDef(CompMeshSlot.SkeletalMesh);
		if (MeshDefIdx == -1)
			continue;

		FProxyMeshData& MD = this->SubMeshes[MeshIdx];
		MD.SkeletalMesh = CompMeshSlot.SkeletalMesh;
		MD.SkeletalRenderData = CompMeshSlot.SkeletalMesh->GetResourceForRendering();
		check(MD.SkeletalRenderData);
		MD.MeshDefIndex = MeshDefIdx;
		MD.MeshDataEx = AminCollection->Meshes[MeshDefIdx].MeshData;
		check(MD.MeshDataEx);
		MD.MeshDefBaseLOD = AminCollection->Meshes[MeshDefIdx].BaseLOD;
		MD.BaseMaterialIndex = MaterialCounter;
		
		MD.MaxDrawDistance = CompMeshSlot.MaxDrawDistance;

		if (CompMeshSlot.OverrideDistance > 0)
		{
			int OverrideIdx = Component->FindSubmeshIndex(CompMeshSlot.OverrideSubmeshName);
			if (OverrideIdx != -1)
			{
				MD.OverrideDistance = CompMeshSlot.OverrideDistance;
				MD.OverrideMeshIndex = (uint8)OverrideIdx;
			}
		}

		check(MD.SkeletalRenderData->LODRenderData.Num() > 0);
		MD.MinLODIndex = FMath::Clamp(CompMeshSlot.MinLODIndex, MD.MeshDefBaseLOD, static_cast<uint8>(MD.SkeletalRenderData->LODRenderData.Num() - 1));

		const int LODCount = MD.SkeletalRenderData->LODRenderData.Num() - (int)MD.MeshDefBaseLOD;
		MaxBatchCountPossible += LODCount;
		MaterialCounter += MD.SkeletalMesh->GetNumMaterials();

		for (int LODIndex = 0; LODIndex < MD.SkeletalRenderData->LODRenderData.Num(); LODIndex++)
		{
			const FSkeletalMeshLODRenderData& SKMeshLOD = MD.SkeletalRenderData->LODRenderData[LODIndex];
			SectionCounter += SKMeshLOD.RenderSections.Num();
		}
	}
	MaxBatchCountPossible *= 2; //those with negative determinant we draw them as separate batch, FMeshBatch.ReverseCulling 
	this->MaterialIndicesArray.SetNumZeroed(SectionCounter);

	

}

FSkelotProxy::~FSkelotProxy()
{
	if (DynamicData)
	{
		delete DynamicData;
		DynamicData = nullptr;
	}

	if (OldDynamicData)
	{
		delete OldDynamicData;
		OldDynamicData = nullptr;
	}
}

bool FSkelotProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest && !ShouldRenderCustomDepth();
}

void FSkelotProxy::ApplyWorldOffset(FRHICommandListBase& RHICmdList, FVector InOffset)
{
	Super::ApplyWorldOffset(RHICmdList, InOffset);

	FVector3f InOffset3f(InOffset); //#TODO precision loss ?
	if (DynamicData)
	{
		for (uint32 InstanceIndex = 0; InstanceIndex < DynamicData->InstanceCount; InstanceIndex++)
		{
			auto& M = DynamicData->Transforms[InstanceIndex];
			M.SetOrigin(M.GetOrigin() + InOffset3f);
		}
	}
	if (OldDynamicData)
	{
		for (uint32 InstanceIndex = 0; InstanceIndex < OldDynamicData->InstanceCount; InstanceIndex++)
		{
			auto& M = OldDynamicData->Transforms[InstanceIndex];
			M.SetOrigin(M.GetOrigin() + InOffset3f);
		}
	}
}

void FSkelotProxy::GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const
{
	bDynamic = true;
	bRelevant = true;
	bLightMapped = false;
	bShadowMapped = true;
}

FPrimitiveViewRelevance FSkelotProxy::GetViewRelevance(const FSceneView* View) const
{
	bool bHasData = DynamicData && DynamicData->AliveInstanceCount > 0;
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = bHasData && IsShown(View) && View->Family->EngineShowFlags.SkeletalMeshes != 0;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bRenderInDepthPass = ShouldRenderInDepthPass();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque == 1 && Result.bRenderInMainPass;

	return Result;
}

void FSkelotProxy::SetDynamicDataRT(FSkelotDynamicData* pData)
{
	check(IsInRenderingThread());
	if (OldDynamicData)
	{
		delete OldDynamicData;
	}

	OldDynamicData = DynamicData;
	DynamicData = pData;

	pData->CreationNumber = GFrameNumberRenderThread;

	//if(OldDynamicData)
	//{
	//	for (uint32 i = OldDynamicData->InstanceCount; i < DynamicData->InstanceCount; i++)
	//	{
	//		DynamicData->Flags[i] |= ESkelotInstanceFlags::EIF_New;
	//	}
	//}
}

SIZE_T FSkelotProxy::GetTypeHash() const
{
	static SIZE_T UniquePointer;
	return reinterpret_cast<SIZE_T>(&UniquePointer);
}

void FSkelotProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	uint16* MatIndexIter = this->MaterialIndicesArray.GetData();
	this->bHasAnyTranslucentMaterial = false;

	for (int MeshIdx = 0; MeshIdx < SubMeshes.Num(); MeshIdx++)
	{
		FProxyMeshData& MD = SubMeshes[MeshIdx];

		if (MD.SkeletalRenderData)
		{
			check(MD.SkeletalRenderData->LODRenderData.Num() <= SKELOT_MAX_LOD);
			//initialize Proxy LODData
			for (int LODIndex = 0; LODIndex < MD.SkeletalRenderData->LODRenderData.Num(); LODIndex++)
			{
				const FSkeletalMeshLODRenderData& SkelLODData = MD.SkeletalRenderData->LODRenderData[LODIndex];
				check(SkelLODData.RenderSections.Num());

				FProxyLODData& ProxyLODData = MD.LODs[LODIndex];

				ProxyLODData.bAnySectionCastShadow = false;
				ProxyLODData.bAllSectionsCastShadow = true;
				ProxyLODData.bSameCastShadow = true;
				ProxyLODData.bMeshUnificationApplicable = true;
				ProxyLODData.bSameMaterials = true;
				ProxyLODData.bHasAnyTranslucentMaterial = false;
				ProxyLODData.bSameMaxBoneInfluence = true;
				ProxyLODData.SectionsMaxBoneInfluence = 0;
				ProxyLODData.SectionsNumTriangle = 0;
				ProxyLODData.SectionsNumVertices = 0;


				const FSkeletalMeshLODInfo* SKMeshLODInfo = MD.SkeletalMesh->GetLODInfo(LODIndex);

				ProxyLODData.SectionsMaterialIndices = MatIndexIter;
				MatIndexIter += SkelLODData.RenderSections.Num();
				check(MatIndexIter <= (this->MaterialIndicesArray.GetData() + this->MaterialIndicesArray.Num()));

				for (int SectionIndex = 0; SectionIndex < SkelLODData.RenderSections.Num(); SectionIndex++)
				{
					const FSkelMeshRenderSection& SectionInfo = SkelLODData.RenderSections[SectionIndex];

					int SolvedMI = MD.BaseMaterialIndex + SectionInfo.MaterialIndex;
					if (SKMeshLODInfo && SKMeshLODInfo->LODMaterialMap.IsValidIndex(SectionIndex) && SKMeshLODInfo->LODMaterialMap[SectionIndex] != INDEX_NONE)
					{
						SolvedMI = MD.BaseMaterialIndex + SKMeshLODInfo->LODMaterialMap[SectionIndex];
					}
					ProxyLODData.SectionsMaterialIndices[SectionIndex] = SolvedMI;

					const FMaterialRenderProxy* SectionMaterialProxy = this->MaterialsProxy[SolvedMI];

					checkf(IsInRenderingThread(), TEXT("GetIncompleteMaterialWithFallback must be called from render thread"));
					const FMaterial& Material = SectionMaterialProxy->GetIncompleteMaterialWithFallback(this->GetScene().GetFeatureLevel());

					check(SectionMaterialProxy);
					ProxyLODData.bAnySectionCastShadow |= SectionInfo.bCastShadow;
					ProxyLODData.bAllSectionsCastShadow &= SectionInfo.bCastShadow;
					ProxyLODData.bSameCastShadow &= SectionInfo.bCastShadow == SkelLODData.RenderSections[0].bCastShadow;
					ProxyLODData.bHasAnyTranslucentMaterial |= IsTranslucentBlendMode(Material.GetBlendMode());
					MD.bHasAnyTranslucentMaterial |= ProxyLODData.bHasAnyTranslucentMaterial;

					const FMaterialRenderProxy* Section0Material = this->MaterialsProxy[ProxyLODData.SectionsMaterialIndices[0]];
					const bool bSameMaterial = SectionMaterialProxy == Section0Material;
					ProxyLODData.bSameMaterials &= bSameMaterial;
					const bool bUseUnifiedMesh = Material.WritesEveryPixel() && !Material.IsTwoSided() && !IsTranslucentBlendMode(Material.GetBlendMode()) && !Material.MaterialModifiesMeshPosition_RenderThread();
					ProxyLODData.bMeshUnificationApplicable &= bUseUnifiedMesh;
					ProxyLODData.bSameMaxBoneInfluence &= (SectionInfo.MaxBoneInfluences == SkelLODData.RenderSections[0].MaxBoneInfluences);

					ProxyLODData.SectionsMaxBoneInfluence = FMath::Max(ProxyLODData.SectionsMaxBoneInfluence, SectionInfo.MaxBoneInfluences);
					ProxyLODData.SectionsNumTriangle += SectionInfo.NumTriangles;
					ProxyLODData.SectionsNumVertices += SectionInfo.NumVertices;
				}
			}
		}

		this->bHasAnyTranslucentMaterial |= MD.bHasAnyTranslucentMaterial;
	}
}

void FSkelotProxy::DestroyRenderThreadResources()
{

}

uint32 FSkelotProxy::GetMemoryFootprint(void) const
{
	return (sizeof(*this) + GetAllocatedSize());
}

uint32 FSkelotProxy::GetAllocatedSize(void) const
{
	return FPrimitiveSceneProxy::GetAllocatedSize() + this->SubMeshes.GetAllocatedSize() + this->MaterialsProxy.GetAllocatedSize() + this->MaterialIndicesArray.GetAllocatedSize();
}

void FSkelotProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SKELOT_SCOPE_CYCLE_COUNTER(GetDynamicMeshElements);
	

	const bool bHasData = SubMeshes.Num() > 0 &&  DynamicData && DynamicData->AliveInstanceCount > 0;
	if (!bHasData)
		return;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			if (View->GetDynamicMeshElementsShadowCullFrustum())
			{
				check(Views.Num() == 1);
				FSkelotMultiMeshGenerator<true> generator(this, &ViewFamily, View, &Collector, ViewIndex);
			}
			else
			{
				bool bIgnoreView = (View->bIsInstancedStereoEnabled && View->StereoPass == EStereoscopicPass::eSSP_SECONDARY);
				if (!bIgnoreView)
				{
					FSkelotMultiMeshGenerator<false> generator(this, &ViewFamily, View, &Collector, ViewIndex);
				}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				// Render bounds
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
			}
		}
	}
}

void FSkelotDynamicData::InitGrid()
{
	FVector3f InCompBoundSize;
	CompBound.GetSize(InCompBoundSize);

	SkelotCalcGridSize(InCompBoundSize.X, InCompBoundSize.Y, this->NumCells, GridSize.X, GridSize.Y);

	GridCellSize.X = (InCompBoundSize.X / GridSize.X);
	GridCellSize.Y = (InCompBoundSize.Y / GridSize.Y);
	GridInvCellSize = FVector2f(1.0f) / GridCellSize;

	BoundMin = FVector2f(CompBound.GetMin());
}

FSkelotDynamicData* FSkelotDynamicData::Allocate(USkelotComponent* Comp)
{
	SKELOT_SCOPE_CYCLE_COUNTER(FSkelotDynamicData_Allocate);

	uint32 InstanceCount = Comp->GetInstanceCount();
	uint32 MaxNumCell = Comp->GetAliveInstanceCount() / GSkelot_NumInstancePerGridCell;
	MaxNumCell = MaxNumCell >= 6 ? MaxNumCell : 0;	//zero if we don't need grid
	if(GSkelot_DisableGridCull)
		MaxNumCell = 0;

	const size_t MemSizeTransforms = sizeof(*Transforms) * InstanceCount;
	const size_t MemSizeBounds = sizeof(*Bounds) * InstanceCount;
	const size_t MemSizeFrameIndices = sizeof(*FrameIndices) * InstanceCount;
	const size_t MemSizeFlags = sizeof(ESkelotInstanceFlags) * InstanceCount;
	const size_t MemSizeCustomData = Comp->NumCustomDataFloats > 0 ? (Comp->NumCustomDataFloats * sizeof(float) * InstanceCount) : 0;
	const size_t MemSizeMeshSlots = (Comp->MaxMeshPerInstance + 1) * sizeof(uint8) * InstanceCount;

	size_t MemSizeCells = 0;
	uint32 MaxCellPageNeeded = 0;
	if (MaxNumCell > 0)
	{
		MemSizeCells = MaxNumCell * sizeof(FCell);
		MaxCellPageNeeded = FMath::DivideAndRoundUp((uint32)Comp->GetAliveInstanceCount(), FCell::MAX_INSTANCE_PER_CELL) + MaxNumCell;
	}

	const size_t MemSizeCellPages = MaxCellPageNeeded * sizeof(FCell::FCellPage);

	const size_t OverallSize = sizeof(FSkelotDynamicData) + MemSizeTransforms + MemSizeBounds + MemSizeFrameIndices + MemSizeFlags + MemSizeCustomData + MemSizeMeshSlots + MemSizeCells + MemSizeCellPages + 256;

	uint8* MemBlock = (uint8*)FMemory::Malloc(OverallSize);
	FSkelotDynamicData* DynData = new (MemBlock) FSkelotDynamicData();
	uint8* DataIter = (uint8*)(DynData + 1);

	auto TakeMem = [&](size_t SizeInBytes, size_t InAlign = 4) {
		uint8* cur = Align(DataIter, InAlign);
		DataIter = cur + SizeInBytes;
		check(DataIter <= (MemBlock + OverallSize));
		return cur;
	};

	DynData->InstanceCount = InstanceCount;
	DynData->AliveInstanceCount = Comp->GetAliveInstanceCount();

	DynData->Flags = (ESkelotInstanceFlags*)TakeMem(MemSizeFlags);
	DynData->Bounds = (FBoxCenterExtentFloat*)TakeMem(MemSizeBounds);
	DynData->Transforms = (FMatrix44f*)TakeMem(MemSizeTransforms, 16);
	DynData->FrameIndices = (uint32*)TakeMem(MemSizeFrameIndices);
	DynData->CustomData = MemSizeCustomData ? (float*)TakeMem(MemSizeCustomData) : nullptr;
	DynData->MeshSlots = MemSizeMeshSlots ? (uint8*)TakeMem(MemSizeMeshSlots) : nullptr;

	if (MaxNumCell > 0)	//use grid culling ?
	{
		DynData->NumCells = MaxNumCell;
		DynData->MaxCellPage = MaxCellPageNeeded;
		DynData->Cells = (FCell*)TakeMem(MemSizeCells, alignof(FCell));
		DynData->CellPagePool = (FCell::FCellPage*)TakeMem(MemSizeCellPages, PLATFORM_CACHE_LINE_SIZE);

		DefaultConstructItems<FCell>(DynData->Cells, MaxNumCell);
	}

	//copy data
	FMemory::Memcpy(DynData->Flags, Comp->InstancesData.Flags.GetData(), MemSizeFlags);
	FMemory::Memcpy(DynData->FrameIndices, Comp->InstancesData.FrameIndices.GetData(), MemSizeFrameIndices);
	FMemory::Memcpy(DynData->Transforms, Comp->InstancesData.Matrices.GetData(), MemSizeTransforms);
	if (MemSizeCustomData )
		FMemory::Memcpy(DynData->CustomData, Comp->InstancesData.RenderCustomData.GetData(), MemSizeCustomData);
	
	FMemory::Memcpy(DynData->MeshSlots, Comp->InstancesData.MeshSlots.GetData(), MemSizeMeshSlots);

	

	return DynData;
}

void FSkelotDynamicData::FCell::AddValue(FSkelotDynamicData& Owner, uint32 InValue)
{
	if (Counter == MAX_INSTANCE_PER_CELL)
	{
		Counter = 1;
		FCellPage* CellPage = new (Owner.CellPagePool + Owner.CellPageCounter) FCellPage();
		CellPage->NextPage = -1;
		CellPage->Indices[0] = InValue;

		if (PageTail != -1)
		{
			Owner.CellPagePool[PageTail].NextPage = Owner.CellPageCounter;
			PageTail = Owner.CellPageCounter;
		}
		else
		{
			PageHead = PageTail = Owner.CellPageCounter;
		}

		Owner.CellPageCounter++;
		check(Owner.CellPageCounter <= Owner.MaxCellPage);
	}
	else
	{
		Owner.CellPagePool[PageTail].Indices[this->Counter++] = InValue;
	}
}

uint8* FSkelotDynamicData::FCell::CopyToArray(const FSkelotDynamicData& Owner, uint8* DstMem) const
{
	int IterIdx = PageHead;
	while (IterIdx != -1)
	{
		const FCellPage* CellData = Owner.CellPagePool + IterIdx;
		const uint32 DataSize = (CellData->NextPage == -1 ? this->Counter : MAX_INSTANCE_PER_CELL) * sizeof(uint32);
		//#TODO can't be optimized by just copying it all ? 
		FMemory::Memcpy(DstMem, CellData->Indices, DataSize);
		DstMem += DataSize;
		IterIdx = CellData->NextPage;
	}

	return DstMem;
}
