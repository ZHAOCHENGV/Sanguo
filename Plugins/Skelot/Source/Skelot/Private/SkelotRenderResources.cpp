// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotRenderResources.h"
#include "SkelotRender.h"
#include "SkelotAnimCollection.h"
#include "MeshDrawShaderBindings.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Skelot.h"
#include "RenderUtils.h"
#include "ShaderCore.h"
#include "MeshMaterialShader.h"
#include "MaterialDomain.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalRenderResources.h"
#include "SkelotPrivateUtils.h"
#include "Misc/SpinLock.h"


FSkelotBaseVertexFactory* FSkelotBaseVertexFactory::New(int InMaxBoneInfluence)
{
	switch (InMaxBoneInfluence)
	{
	case 1: return new TSkelotVertexFactory<ESkelotVerteFactoryMode::EVF_BoneInfluence1>(GMaxRHIFeatureLevel);
	case 2: return new TSkelotVertexFactory<ESkelotVerteFactoryMode::EVF_BoneInfluence2>(GMaxRHIFeatureLevel);
	case 3: return new TSkelotVertexFactory<ESkelotVerteFactoryMode::EVF_BoneInfluence3>(GMaxRHIFeatureLevel);
	case 4: return new TSkelotVertexFactory<ESkelotVerteFactoryMode::EVF_BoneInfluence4>(GMaxRHIFeatureLevel);
	case 5: return new TSkelotVertexFactory<ESkelotVerteFactoryMode::EVF_BoneInfluence5>(GMaxRHIFeatureLevel);
	case 6: return new TSkelotVertexFactory<ESkelotVerteFactoryMode::EVF_BoneInfluence6>(GMaxRHIFeatureLevel);
	case 7: return new TSkelotVertexFactory<ESkelotVerteFactoryMode::EVF_BoneInfluence7>(GMaxRHIFeatureLevel);
	case 8: return new TSkelotVertexFactory<ESkelotVerteFactoryMode::EVF_BoneInfluence8>(GMaxRHIFeatureLevel);
	};

	return nullptr;
}

void FSkelotBaseVertexFactory::FillData(FDataType& data, const FSkelotBoneIndexVertexBuffer* BoneIndexBuffer, const FSkeletalMeshLODRenderData* LODData) const
{
	const FStaticMeshVertexBuffers& smvb = LODData->StaticVertexBuffers;

	smvb.PositionVertexBuffer.BindPositionVertexBuffer(this, data);
	smvb.StaticMeshVertexBuffer.BindTangentVertexBuffer(this, data);
	smvb.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(this, data);
	smvb.ColorVertexBuffer.BindColorVertexBuffer(this, data);


	//const uint32 stride = sizeof(FSkelotSkinWeightVertexBuffer::FBoneWeight);
	//const uint32 weightOffset = sizeof(uint8[4]);
	//data.BoneIndices = FVertexStreamComponent(weightBuffer, 0, stride, VET_UByte4);
	//data.BoneWeights = FVertexStreamComponent(weightBuffer, weightOffset, stride, VET_UByte4N);

	//see InitGPUSkinVertexFactoryComponents we can take weights from the mesh since we just have different bone indices
	
	const FSkinWeightDataVertexBuffer* LODSkinData = LODData->GetSkinWeightVertexBuffer()->GetDataVertexBuffer();
	check(LODSkinData->GetMaxBoneInfluences() == BoneIndexBuffer->MaxBoneInfluences);
	//check(LODSkinData->GetMaxBoneInfluences() <= this->MaxBoneInfluence);

	{
		EVertexElementType ElemType = LODSkinData->Use16BitBoneWeight() ? VET_UShort4N : VET_UByte4N;
		uint32 Stride = LODSkinData->GetConstantInfluencesVertexStride();
		data.BoneWeights = FVertexStreamComponent(LODSkinData, LODSkinData->GetConstantInfluencesBoneWeightsOffset(), Stride, ElemType);
		if(LODSkinData->GetMaxBoneInfluences() > 4)
		{
			data.ExtraBoneWeights = FVertexStreamComponent(LODSkinData, LODSkinData->GetConstantInfluencesBoneWeightsOffset() + (LODSkinData->GetBoneWeightByteSize() * 4), Stride, ElemType);
		}
		else
		{
			data.ExtraBoneWeights = FVertexStreamComponent(&GNullVertexBuffer, 0, 0, ElemType);
		}
	}

	{
		uint32 Stride = (BoneIndexBuffer->bIs16BitBoneIndex ? 2 : 1) * LODSkinData->GetMaxBoneInfluences();
		EVertexElementType ElemType = BoneIndexBuffer->bIs16BitBoneIndex ? VET_UShort4 : VET_UByte4;
		data.BoneIndices = FVertexStreamComponent(BoneIndexBuffer, 0, Stride, ElemType);
		if(LODSkinData->GetMaxBoneInfluences() > 4)
		{
			data.ExtraBoneIndices = FVertexStreamComponent(BoneIndexBuffer, Stride / 2, Stride, ElemType);
		}
		else
		{
			data.ExtraBoneIndices = FVertexStreamComponent(&GNullVertexBuffer, 0, 0, ElemType);
		}
	}

}

void FSkelotBaseVertexFactory::SetData(const FDataType& data)
{
	FVertexDeclarationElementList OutElements;
	//position
	OutElements.Add(AccessStreamComponent(data.PositionComponent, 0));
	// tangent basis vector decls
	OutElements.Add(AccessStreamComponent(data.TangentBasisComponents[0], 1));
	OutElements.Add(AccessStreamComponent(data.TangentBasisComponents[1], 2));

	// Texture coordinates
	if (data.TextureCoordinates.Num())
	{
		const uint8 BaseTexCoordAttribute = 5;
		for (int32 CoordinateIndex = 0; CoordinateIndex < data.TextureCoordinates.Num(); ++CoordinateIndex)
		{
			OutElements.Add(AccessStreamComponent(
				data.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}

		for (int32 CoordinateIndex = data.TextureCoordinates.Num(); CoordinateIndex < MAX_TEXCOORDS; ++CoordinateIndex)
		{
			OutElements.Add(AccessStreamComponent(
				data.TextureCoordinates[data.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
			));
		}
	}

	OutElements.Add(AccessStreamComponent(data.ColorComponent, 13));

	// bone indices decls
	OutElements.Add(AccessStreamComponent(data.BoneIndices, 3));
	// bone weights decls
	OutElements.Add(AccessStreamComponent(data.BoneWeights, 4));

	if (MaxBoneInfluence > 4)
	{
		OutElements.Add(AccessStreamComponent(data.ExtraBoneIndices, 14));
		OutElements.Add(AccessStreamComponent(data.ExtraBoneWeights, 15));
	}

	InitDeclaration(OutElements);
	check(GetDeclaration());


}

template<ESkelotVerteFactoryMode FactoryMode> bool TSkelotVertexFactory<FactoryMode>::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return Parameters.MaterialParameters.MaterialDomain == MD_Surface && (Parameters.MaterialParameters.bIsUsedWithSkeletalMesh || Parameters.MaterialParameters.bIsSpecialEngineMaterial);

}

template<ESkelotVerteFactoryMode FactoryMode> void TSkelotVertexFactory<FactoryMode>::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	const FStaticFeatureLevel MaxSupportedFeatureLevel = GetMaxSupportedFeatureLevel(Parameters.Platform);
	const bool bUseGPUScene = UseGPUScene(Parameters.Platform, MaxSupportedFeatureLevel) && (MaxSupportedFeatureLevel > ERHIFeatureLevel::ES3_1);
	const bool bSupportsPrimitiveIdStream = Parameters.VertexFactoryType->SupportsPrimitiveIdStream();


	OutEnvironment.SetDefine(TEXT("USE_INSTANCING"), 1);	
	OutEnvironment.SetDefine(TEXT("MAX_BONE_INFLUENCE"), StaticMaxBoneInfluence());
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 0 /*bSupportsPrimitiveIdStream && bUseGPUScene*/);
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_SPEEDTREE_WIND"), 0);
	OutEnvironment.SetDefine(TEXT("USE_DITHERED_LOD_TRANSITION_FOR_INSTANCED"), 0);
	OutEnvironment.SetDefine(TEXT("VF_SKELOT"), 1);
	//OutEnvironment.SetDefine(TEXT("SKELOT_VF_MULTIMESH"), IsMultiMesh);
	OutEnvironment.SetDefine(TEXT("USE_DITHERED_LOD_TRANSITION"), 0);
}

template<ESkelotVerteFactoryMode FactoryMode> void TSkelotVertexFactory<FactoryMode>::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
}



class FSkelotShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FSkelotShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
	}
	void GetElementShaderBindings(const class FSceneInterface* Scene, const FSceneView* View, const FMeshMaterialShader* Shader, const EVertexInputStreamType InputStreamType, ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory, const FMeshBatchElement& BatchElement, class FMeshDrawSingleShaderBindings& ShaderBindings, FVertexInputStreamArray& VertexStreams) const
	{
		const FSkelotBaseVertexFactory* vf = static_cast<const FSkelotBaseVertexFactory*>(VertexFactory);
		const FSkelotBatchElementOFR* userData = (const FSkelotBatchElementOFR*)BatchElement.UserData;

		EShaderFrequency SF = Shader->GetFrequency();
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FSkelotVertexFactoryParameters>(), userData->UniformBuffer);
	}

};

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FSkelotVertexFactoryParameters, "SkelotVF");


IMPLEMENT_TYPE_LAYOUT(FSkelotShaderParameters);

constexpr EVertexFactoryFlags VFFlags = EVertexFactoryFlags::UsedWithMaterials | EVertexFactoryFlags::SupportsDynamicLighting | EVertexFactoryFlags::SupportsPrecisePrevWorldPos;

//#TODO maybe just supporting 1 and 4 Bone influence is enough ?
using SkelotVertexFactory1 = TSkelotVertexFactory < ESkelotVerteFactoryMode::EVF_BoneInfluence1>;
using SkelotVertexFactory2 = TSkelotVertexFactory < ESkelotVerteFactoryMode::EVF_BoneInfluence2>;
using SkelotVertexFactory3 = TSkelotVertexFactory < ESkelotVerteFactoryMode::EVF_BoneInfluence3>;
using SkelotVertexFactory4 = TSkelotVertexFactory < ESkelotVerteFactoryMode::EVF_BoneInfluence4>;
using SkelotVertexFactory5 = TSkelotVertexFactory < ESkelotVerteFactoryMode::EVF_BoneInfluence5>;
using SkelotVertexFactory6 = TSkelotVertexFactory < ESkelotVerteFactoryMode::EVF_BoneInfluence6>;
using SkelotVertexFactory7 = TSkelotVertexFactory < ESkelotVerteFactoryMode::EVF_BoneInfluence7>;
using SkelotVertexFactory8 = TSkelotVertexFactory < ESkelotVerteFactoryMode::EVF_BoneInfluence8>;

IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, SkelotVertexFactory1, "/Plugin/Skelot/Private/SkelotVertexFactory.ush", VFFlags);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, SkelotVertexFactory2, "/Plugin/Skelot/Private/SkelotVertexFactory.ush", VFFlags);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, SkelotVertexFactory3, "/Plugin/Skelot/Private/SkelotVertexFactory.ush", VFFlags);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, SkelotVertexFactory4, "/Plugin/Skelot/Private/SkelotVertexFactory.ush", VFFlags);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, SkelotVertexFactory5, "/Plugin/Skelot/Private/SkelotVertexFactory.ush", VFFlags);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, SkelotVertexFactory6, "/Plugin/Skelot/Private/SkelotVertexFactory.ush", VFFlags);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, SkelotVertexFactory7, "/Plugin/Skelot/Private/SkelotVertexFactory.ush", VFFlags);
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, SkelotVertexFactory8, "/Plugin/Skelot/Private/SkelotVertexFactory.ush", VFFlags);


IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory1, SF_Vertex, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory2, SF_Vertex, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory3, SF_Vertex, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory4, SF_Vertex, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory5, SF_Vertex, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory6, SF_Vertex, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory7, SF_Vertex, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory8, SF_Vertex, FSkelotShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory1, SF_Pixel, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory2, SF_Pixel, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory3, SF_Pixel, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory4, SF_Pixel, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory5, SF_Pixel, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory6, SF_Pixel, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory7, SF_Pixel, FSkelotShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(SkelotVertexFactory8, SF_Pixel, FSkelotShaderParameters);

#if 0
void FSkelotSkinWeightVertexBuffer::Serialize(FArchive& Ar)
{
	bool bAnyData = WeightData != nullptr;
	Ar << bAnyData;
	if (Ar.IsSaving())
	{
		if (WeightData)
			WeightData->Serialize(Ar);
	}
	else
	{
		if (bAnyData)
		{
			AllocBuffer();
			WeightData->Serialize(Ar);
		}
	}
}

void FSkelotSkinWeightVertexBuffer::InitRHI()
{
	uint32 size = WeightData->Num() * sizeof(FBoneWeight);
	FRHIResourceCreateInfo info(TEXT("FSkelotSkinWeightVertexBuffer"), WeightData->GetResourceArray());
	VertexBufferRHI = RHICreateVertexBuffer(size, BUF_Static, info);
	delete WeightData;
	WeightData = nullptr;
	//VertexBufferRHI = CreateRHIBuffer<true>(WeightData, 1, EBufferUsageFlags::Static | EBufferUsageFlags::VertexBuffer, TEXT("FSkelotSkinWeightVertexBuffer"));
}

void FSkelotSkinWeightVertexBuffer::ReleaseRHI()
{
	VertexBufferRHI.SafeRelease();
}

void FSkelotSkinWeightVertexBuffer::AllocBuffer()
{
	if (WeightData)
		delete WeightData;

	WeightData = new TStaticMeshVertexData<FBoneWeight>();
}

void FSkelotSkinWeightVertexBuffer::InitBuffer(const TArray<FSkinWeightInfo>& weightsArray)
{
	AllocBuffer();
	WeightData->ResizeBuffer(weightsArray.Num());

	FBoneWeight* dst = (FBoneWeight*)WeightData->GetDataPointer();
	for (int VertexIndex = 0; VertexIndex < weightsArray.Num(); VertexIndex++)
	{
		for (int InfluenceIndex = 0; InfluenceIndex < MAX_INFLUENCE; InfluenceIndex++)
		{
			check(weightsArray[VertexIndex].InfluenceBones[InfluenceIndex] <= 255);
			dst[VertexIndex].BoneIndex[InfluenceIndex] = static_cast<uint8>(weightsArray[VertexIndex].InfluenceBones[InfluenceIndex]);
			uint8 newWeight = static_cast<uint8>(weightsArray[VertexIndex].InfluenceWeights[InfluenceIndex] >> 8); //convert to 8bit skin weight
			dst[VertexIndex].BoneWeight[InfluenceIndex] = newWeight;
		}
	}
}

FSkelotSkinWeightVertexBuffer::~FSkelotSkinWeightVertexBuffer()
{
	if (WeightData)
	{
		delete WeightData;
		WeightData = nullptr;
	}
}
#endif


FSkelotAnimationBuffer::~FSkelotAnimationBuffer()
{
	DestroyBuffer();
}

void FSkelotAnimationBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	const uint32 Stride = bHighPrecision ? sizeof(float[4]) : sizeof(uint16[4]);
	FRHIResourceCreateInfo info(TEXT("FSkelotAnimationBuffer"), Transforms->GetResourceArray());
	ERHIAccess AM = ERHIAccess::Unknown;// ERHIAccess::SRVGraphics | ERHIAccess::UAVCompute | ERHIAccess::CopyDest;
	Buffer = RHICmdList.CreateVertexBuffer(Transforms->Num() * Transforms->GetStride(), BUF_UnorderedAccess | BUF_ShaderResource, AM, info);
	ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(Buffer, Stride, bHighPrecision ? PF_A32B32G32R32F : PF_FloatRGBA);
	UAV = RHICmdList.CreateUnorderedAccessView(Buffer, bHighPrecision ? PF_A32B32G32R32F : PF_FloatRGBA);

	delete Transforms;
	Transforms = nullptr;
}


void FSkelotAnimationBuffer::ReleaseRHI()
{
	ShaderResourceViewRHI.SafeRelease();
	Buffer.SafeRelease();
}

void FSkelotAnimationBuffer::Serialize(FArchive& Ar)
{
	Ar << bHighPrecision;
	bool bAnyData = Transforms != nullptr;
	Ar << bAnyData;
	if (Ar.IsSaving())
	{
		if (Transforms)
			Transforms->Serialize(Ar);
	}
	else
	{
		if (bAnyData)
		{
			AllocateBuffer();
			Transforms->Serialize(Ar);
		}
	}
}

void FSkelotAnimationBuffer::AllocateBuffer()
{
	if (Transforms)
		delete Transforms;

	if (bHighPrecision)
		Transforms = new TStaticMeshVertexData<FMatrix3x4>();
	else
		Transforms = new TStaticMeshVertexData<FMatrix3x4Half>();
}

void FSkelotAnimationBuffer::InitBuffer(const TArrayView<FTransform> InTransforms, bool InHightPrecision)
{
	InitBuffer(InTransforms.Num(), InHightPrecision, false);

	if (bHighPrecision)
	{
		FMatrix3x4* Dst = (FMatrix3x4*)Transforms->GetDataPointer();
		for (int i = 0; i < InTransforms.Num(); i++)
			Dst[i].SetMatrixTranspose(InTransforms[i].ToMatrixWithScale());

	}
	else
	{
		FMatrix3x4Half* Dst = (FMatrix3x4Half*)Transforms->GetDataPointer();
		for (int i = 0; i < InTransforms.Num(); i++)
			Dst[i].SetMatrixTranspose(FMatrix44f(InTransforms[i].ToMatrixWithScale()));

	}
}

void FSkelotAnimationBuffer::InitBuffer(uint32 NumMatrix, bool InHightPrecision, bool bFillIdentity)
{
	this->bHighPrecision = InHightPrecision;
	this->AllocateBuffer();
	this->Transforms->ResizeBuffer(NumMatrix);

	if(bFillIdentity)
	{
		if (InHightPrecision)
		{
			FMatrix3x4 IdentityMatrix;
			IdentityMatrix.SetMatrixTranspose(FMatrix::Identity);

			FMatrix3x4* Dst = (FMatrix3x4*)Transforms->GetDataPointer();
			for (uint32 i = 0; i < NumMatrix; i++)
				Dst[i] = IdentityMatrix;
		}
		else
		{
			FMatrix3x4Half IdentityMatrix;
			IdentityMatrix.SetMatrixTranspose(FMatrix44f::Identity);

			FMatrix3x4Half* Dst = (FMatrix3x4Half*)Transforms->GetDataPointer();
			for (uint32 i = 0; i < NumMatrix; i++)
				Dst[i] = IdentityMatrix;
		}
	}
}

void FSkelotAnimationBuffer::DestroyBuffer()
{
	if (Transforms)
	{
		delete Transforms;
		Transforms = nullptr;
	}
}




FMatrix3x4Half* FSkelotAnimationBuffer::GetDataPointerLP() const
{
	check(!this->bHighPrecision);
	return (FMatrix3x4Half*)this->Transforms->GetDataPointer();
}

FMatrix3x4* FSkelotAnimationBuffer::GetDataPointerHP() const
{
	check(this->bHighPrecision);
	return (FMatrix3x4*)this->Transforms->GetDataPointer();
}

// void FSkelotAnimationBuffer::SetPoseTransform(uint32 PoseIndex, uint32 BoneCount, const FTransform* BoneTransforms)
// {
// 	uint32 BaseIdx = PoseIndex * BoneCount;
// 	if (this->bHighPrecision)
// 	{
// 		FMatrix3x4* Dst = reinterpret_cast<FMatrix3x4*>(Transforms->GetDataPointer()) + BaseIdx;
// 		for (uint32 i = 0; i < BoneCount; i++)
// 			Dst[i].SetMatrixTranspose(BoneTransforms[i].ToMatrixWithScale());
// 	}
// 	else
// 	{
// 		FMatrix3x4Half* Dst = reinterpret_cast<FMatrix3x4Half*>(Transforms->GetDataPointer()) + BaseIdx;
// 		for (uint32 i = 0; i < BoneCount; i++)
// 			Dst[i].SetMatrixTranspose(FMatrix44f(BoneTransforms[i].ToMatrixWithScale()));
// 	}
// }


// void FSkelotAnimationBuffer::SetPoseTransform(uint32 PoseIndex, uint32 BoneCount, const FMatrix3x4* BoneTransforms)
// {
// 	uint32 BaseIdx = PoseIndex * BoneCount;
// 	if (this->bHighPrecision)
// 	{
// 		FMatrix3x4* Dst = reinterpret_cast<FMatrix3x4*>(Transforms->GetDataPointer()) + BaseIdx;
// 		for (uint32 i = 0; i < BoneCount; i++)
// 			SkelotSetMatrix3x4Transpose(Dst[i], BoneTransforms[i]);
// 	}
// 	else
// 	{
// 		FMatrix3x4Half* Dst = reinterpret_cast<FMatrix3x4Half*>(Transforms->GetDataPointer()) + BaseIdx;
// 		for (uint32 i = 0; i < BoneCount; i++)
// 			Dst[i].SetMatrixTranspose(BoneTransforms[i]);
// 	}
// }

#if WITH_EDITOR

void FSkelotMeshDataEx::InitFromMesh(int InBaseLOD, USkeletalMesh* SKMesh, const USkelotAnimCollection* AnimSet)
{
	const FSkeletalMeshRenderData* SKMRenderData = SKMesh->GetResourceForRendering();

	this->LODs.Reserve(SKMRenderData->LODRenderData.Num() - InBaseLOD);

	for (int LODIndex = InBaseLOD; LODIndex < SKMRenderData->LODRenderData.Num(); LODIndex++)	//for each LOD
	{
		const FSkeletalMeshLODRenderData& SKMLODData = SKMRenderData->LODRenderData[LODIndex];
		const FSkinWeightVertexBuffer* SkinVB = SKMLODData.GetSkinWeightVertexBuffer();
		check(SKMLODData.GetVertexBufferMaxBoneInfluences() <= MAX_INFLUENCE);
		check(SkinVB->GetBoneInfluenceType() == DefaultBoneInfluence);
		check(SkinVB->GetVariableBonesPerVertex() == false);
		check(SkinVB->GetMaxBoneInfluences() == 4 || SkinVB->GetMaxBoneInfluences() == 8);

		FSkelotMeshDataEx::FLODData& SkelotLODData = this->LODs.Emplace_GetRef(GMaxRHIFeatureLevel);
		const bool bNeeds16BitBoneIndex = SkinVB->Use16BitBoneIndex() || SKMesh->GetSkeleton()->GetReferenceSkeleton().GetNum() > 255;
		SkelotLODData.BoneData.bIs16BitBoneIndex = bNeeds16BitBoneIndex;
		SkelotLODData.BoneData.MaxBoneInfluences = SkinVB->GetMaxBoneInfluences();
		SkelotLODData.BoneData.NumVertices = SkinVB->GetNumVertices();
		SkelotLODData.BoneData.ResizeBuffer();
		

		for (uint32 VertexIndex = 0; VertexIndex < SkinVB->GetNumVertices(); VertexIndex++)	//for each vertex
		{
			int SectionIndex, SectionVertexIndex;
			SKMLODData.GetSectionFromVertexIndex(VertexIndex, SectionIndex, SectionVertexIndex);

			const FSkelMeshRenderSection& SectionInfo = SKMLODData.RenderSections[SectionIndex];
			for (uint32 InfluenceIndex = 0; InfluenceIndex < (uint32)SectionInfo.MaxBoneInfluences; InfluenceIndex++)
			{
				uint32 BoneIndex = SkinVB->GetBoneIndex(VertexIndex, InfluenceIndex);
				uint16 BoneWeight = SkinVB->GetBoneWeight(VertexIndex, InfluenceIndex);
				FBoneIndexType MeshBoneIndex = SectionInfo.BoneMap[BoneIndex];
				int SkeletonBoneIndex = AnimSet->Skeleton->GetSkeletonBoneIndexFromMeshBoneIndex(SKMesh, MeshBoneIndex);
				check(SkeletonBoneIndex != INDEX_NONE);
				int RenderBoneIndex = AnimSet->SkeletonBoneToRenderBone[SkeletonBoneIndex];
				check(RenderBoneIndex != INDEX_NONE);

				SkelotLODData.BoneData.SetBoneIndex(VertexIndex, InfluenceIndex, RenderBoneIndex);
			}
		}
	}
}
#endif

void FSkelotMeshDataEx::InitResources(FRHICommandListBase& RHICmdList)
{
	check(IsInRenderingThread());
	for (FLODData& LODData : LODs)
	{
		LODData.InitResources(RHICmdList);
	}
}

void FSkelotMeshDataEx::ReleaseResouces()
{
	check(IsInRenderingThread());
	for (FLODData& LODData : LODs)
	{
		LODData.ReleaseResources();
	}
}

void FSkelotMeshDataEx::Serialize(FArchive& Ar)
{
	int NumLOD = this->LODs.Num();
	Ar << NumLOD;

	if (Ar.IsSaving())
	{
		for (int i = 0; i < NumLOD; i++)
			this->LODs[i].Serialize(Ar);
	}
	else
	{
		this->LODs.Reserve(NumLOD);
		for (int i = 0; i < NumLOD; i++)
		{
			this->LODs.Emplace(GMaxRHIFeatureLevel);
			this->LODs.Last().Serialize(Ar);
		}
	}
}

void FSkelotMeshDataEx::InitMeshData(const FSkeletalMeshRenderData* SKRenderData, int InBaseLOD)
{
	check(this->LODs.Num() <= SKRenderData->LODRenderData.Num());
	
	for (int LODIndex = 0; LODIndex < this->LODs.Num(); LODIndex++)
	{
		this->LODs[LODIndex].SkelLODData = &SKRenderData->LODRenderData[LODIndex + InBaseLOD];
	}
}

uint32 FSkelotMeshDataEx::GetTotalBufferSize() const
{
	uint32 Size = 0;
	for (const FLODData& L : LODs)
		Size += L.BoneData.GetBufferSizeInBytes();

	return Size;
}

FSkelotMeshDataEx::FLODData::FLODData(ERHIFeatureLevel::Type InFeatureLevel) 
{

}

FSkelotBaseVertexFactory* FSkelotMeshDataEx::FLODData::GetOrCreateVertexFactory(FRHICommandListBase& RHICmdList, int MaxBoneInfluence)
{
	check(MaxBoneInfluence > 0 && MaxBoneInfluence <= FSkelotMeshDataEx::MAX_INFLUENCE);
	
	static UE::FSpinLock AccessLock;
	UE::TScopeLock Lock(AccessLock);

	if (!VertexFactories[MaxBoneInfluence])
	{
		check(this->SkelLODData);
		FSkelotBaseVertexFactory* VF = FSkelotBaseVertexFactory::New(MaxBoneInfluence);
		FSkelotBaseVertexFactory::FDataType VFData;
		VF->FillData(VFData, &BoneData, SkelLODData);
		VF->SetData(VFData);
		VF->InitResource(RHICmdList);

		VertexFactories[MaxBoneInfluence] = TUniquePtr<FSkelotBaseVertexFactory>(VF);
	}

	return VertexFactories[MaxBoneInfluence].Get();
}

void FSkelotMeshDataEx::FLODData::InitResources(FRHICommandListBase& RHICmdList)
{
	check(IsInRenderingThread());
	BoneData.InitResource(RHICmdList);

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	//only create if needed by render section
	for (const FSkelMeshRenderSection& RenderSection : this->SkelLODData->RenderSections)
	{
		GetOrCreateVertexFactory(RHICmdList, RenderSection.MaxBoneInfluences);
	}
#else
	//editor build may use Skelot.FroceMaxBoneInfluence so make all vertex factories
	for(int MBI = 1; MBI < MAX_INFLUENCE + 1; MBI++)
		GetOrCreateVertexFactory(RHICmdList, MBI);
#endif
}

void FSkelotMeshDataEx::FLODData::ReleaseResources()
{
	check(IsInRenderingThread());
	
	//for (int i = 1; i <= FSkelotMeshDataEx::MAX_INFLUENCE; i++)
	//	GetVertexFactory(i)->ReleaseResource();

	for (TUniquePtr<FSkelotBaseVertexFactory>& VF : VertexFactories)
	{
		if (VF)
		{
			VF->ReleaseResource();
			VF = nullptr;
		}
	}

	BoneData.ReleaseResource();
}

// void FSkelotMeshDataEx::FLODData::FillVertexFactories(const FSkeletalMeshLODRenderData* LODData)
// {
// 	for (int i = 1; i <= FSkelotMeshDataEx::MAX_INFLUENCE; i++)
// 	{
// 		//#TODO we only need VF for influences used by the mesh and  ?
// 		FSkelotBaseVertexFactory* vertexFactory = GetVertexFactory(i);
// 		FSkelotBaseVertexFactory::FDataType data;
// 		vertexFactory->FillData(data, &BoneData, LODData);
// 		vertexFactory->SetData(data);
// 
// 	}
// }

void FSkelotMeshDataEx::FLODData::Serialize(FArchive& Ar)
{
	//SkinWeight.Serialize(Ar);
	BoneData.Serialize(Ar);
}

void FSkelotInstanceBuffer::LockBuffers(FRHICommandListBase& RHICmdList)
{
	check(MappedTransforms == nullptr && MappedFrameIndices == nullptr);
	MappedTransforms = (SkelotShaderMatrixT*) RHICmdList.LockBuffer(TransformVB, 0, InstanceCount * sizeof(SkelotShaderMatrixT), RLM_WriteOnly);
	MappedFrameIndices = (uint32*)RHICmdList.LockBuffer(FrameIndexVB, 0, InstanceCount * sizeof(uint32), RLM_WriteOnly);
	//MappedInvNonUniformScaleAndDeterminantSign = (NonUniformScaleT*)RHICmdList.LockBuffer(InvNonUniformScaleAndDeterminantSign, 0, InstanceCount * sizeof(NonUniformScaleT), RLM_WriteOnly);
}

void FSkelotInstanceBuffer::UnlockBuffers(FRHICommandListBase& RHICmdList)
{
	check(IsLocked());
	RHICmdList.UnlockBuffer(FrameIndexVB);
	RHICmdList.UnlockBuffer(TransformVB);
	//RHICmdList.UnlockBuffer(InvNonUniformScaleAndDeterminantSign);
	MappedTransforms = nullptr;
	MappedFrameIndices = nullptr;
	//MappedInvNonUniformScaleAndDeterminantSign = nullptr;
}

TSharedPtr<FSkelotInstanceBuffer> FSkelotInstanceBuffer::Create(FRHICommandListBase& RHICmdList, uint32 InstanceCount)
{
	FSkelotInstanceBufferPtr Resource = MakeShared<FSkelotInstanceBuffer>();
	Resource->InstanceCount = InstanceCount;
	Resource->CreationFrameNumber = GFrameNumberRenderThread;

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("InstanceTransform"));
		Resource->TransformVB = RHICmdList.CreateVertexBuffer(InstanceCount * sizeof(SkelotShaderMatrixT), (BUF_Dynamic | BUF_ShaderResource), CreateInfo);
		Resource->TransformSRV = RHICmdList.CreateShaderResourceView(Resource->TransformVB, sizeof(float[4]), PF_A32B32G32R32F);
	}
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("InstanceAnimationFrameIndex"));
		Resource->FrameIndexVB = RHICmdList.CreateVertexBuffer(InstanceCount * sizeof(uint32), (BUF_Dynamic | BUF_ShaderResource), CreateInfo);
		Resource->FrameIndexSRV = RHICmdList.CreateShaderResourceView(Resource->FrameIndexVB, sizeof(uint32), PF_R32_UINT);
	}
// 	{
// 		FRHIResourceCreateInfo CreateInfo(TEXT("InstanceInvNonUniformScaleAndDeterminantSign"));
// 		Resource->InvNonUniformScaleAndDeterminantSign = RHICmdList.CreateVertexBuffer(InstanceCount * sizeof(NonUniformScaleT), (BUF_Dynamic | BUF_ShaderResource), CreateInfo);
// 		Resource->InvNonUniformScaleAndDeterminantSignSRV = RHICmdList.CreateShaderResourceView(Resource->InvNonUniformScaleAndDeterminantSign, sizeof(NonUniformScaleT), PF_A32B32G32R32F /*PF_FloatRGBA*/);
// 	}

	return Resource;
}




void Skelot_PreRenderFrame(class FRDGBuilder&)
{
	//#Note r.DoInitViewsLightingAfterPrepass removed from UE5
	//static IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DoInitViewsLightingAfterPrepass"));
	//bool bDoInitViewAftersPrepass = !!CV->GetInt();
	//checkf(!bDoInitViewAftersPrepass, TEXT("r.DoInitViewsLightingAfterPrepass must be zero because engine has not exposed required delegates."));
	
	//GSkelotInstanceBufferPool.Commit();
	//GSkelotCIDBufferAllocatorForInitViews.Commit();
}

void Skelot_PostRenderFrame(class FRDGBuilder&)
{
	GSkelotInstanceBufferPool.EndOfFrame();
	GSkelotCIDBufferPool.EndOfFrame();
	GSkelotElementIndexBufferPool.EndOfFrame();
}

void FSkelotCIDBuffer::LockBuffers(FRHICommandListBase& RHICmdList)
{
	check(MappedData == nullptr);
	MappedData = (float*)RHICmdList.LockBuffer(CustomDataBuffer, 0, NumberOfFloat * sizeof(float), RLM_WriteOnly);
}

void FSkelotCIDBuffer::UnlockBuffers(FRHICommandListBase& RHICmdList)
{
	check(IsLocked());
	RHICmdList.UnlockBuffer(CustomDataBuffer);
	MappedData = nullptr;
}

TSharedPtr<FSkelotCIDBuffer> FSkelotCIDBuffer::Create(FRHICommandListBase& RHICmdList, uint32 InNumberOfFloat)
{
	FSkelotCIDBufferPtr Resource = MakeShared<FSkelotCIDBuffer>();
	Resource->NumberOfFloat = InNumberOfFloat;
	Resource->CreationFrameNumber = GFrameNumberRenderThread;

	FRHIResourceCreateInfo CreateInfo(TEXT("CustomData"));
	Resource->CustomDataBuffer = RHICmdList.CreateVertexBuffer(InNumberOfFloat * sizeof(float), (BUF_Dynamic | BUF_ShaderResource), CreateInfo);
	Resource->CustomDataSRV = RHICmdList.CreateShaderResourceView(Resource->CustomDataBuffer, sizeof(float), PF_R32_FLOAT);

	return Resource;
}

FSkelotInstanceBufferAllocator GSkelotInstanceBufferPool;
FSkelotCIDBufferAllocator GSkelotCIDBufferAllocatorForInitViews;
FSkelotCIDBufferAllocator GSkelotCIDBufferPool;


FSkelotProxyOFR::~FSkelotProxyOFR()
{
	//if(this->CDIShadowBuffer)
	//	GSkelotCIDBufferPool.Release(this->CDIShadowBuffer);
	//
	//if(this->InstanceShadowBuffer)
	//	GSkelotShadowInstanceBufferPool.Release(this->InstanceShadowBuffer);
	//
	check(true);
}

void FSkelotElementIndexBuffer::LockBuffers(FRHICommandListBase& RHICmdList)
{
	check(MappedData == nullptr);
	MappedData = RHICmdList.LockBuffer(ElementIndexBuffer, 0, SizeInBytes, RLM_WriteOnly);
}

void FSkelotElementIndexBuffer::UnlockBuffers(FRHICommandListBase& RHICmdList)
{
	check(IsLocked());
	RHICmdList.UnlockBuffer(ElementIndexBuffer);
	MappedData = nullptr;
}

TSharedPtr<FSkelotElementIndexBuffer> FSkelotElementIndexBuffer::Create(FRHICommandListBase& RHICmdList, uint32 InSizeInBytes)
{
	FSkelotElementIndexBufferPtr Resource = MakeShared<FSkelotElementIndexBuffer>();
	Resource->SizeInBytes = InSizeInBytes;
	Resource->CreationFrameNumber = GFrameNumberRenderThread;

	FRHIResourceCreateInfo CreateInfo(TEXT("ElementIndices"));
	Resource->ElementIndexBuffer = RHICmdList.CreateVertexBuffer(InSizeInBytes, (BUF_Dynamic | BUF_ShaderResource), CreateInfo);
	//create two SRV, we use uint16 when possible
	Resource->ElementIndexUIN16SRV = RHICmdList.CreateShaderResourceView(Resource->ElementIndexBuffer, sizeof(uint16), PF_R16_UINT);
	Resource->ElementIndexUIN32SRV = RHICmdList.CreateShaderResourceView(Resource->ElementIndexBuffer, sizeof(uint32), PF_R32_UINT);

	return Resource;
}

FSkelotElementIndexBufferAllocator GSkelotElementIndexBufferPool;

void FSkelotBoneIndexVertexBuffer::Serialize(FArchive& Ar)
{
	Ar << bIs16BitBoneIndex << MaxBoneInfluences << NumVertices;
	if (Ar.IsLoading())
	{
		ResizeBuffer();
	}
	
	BoneData->Serialize(Ar);
}

void FSkelotBoneIndexVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	check(BoneData);
	uint32 size = BoneData->Num();
	FRHIResourceCreateInfo info(TEXT("FSkelotBoneIndexVertexBuffer"), BoneData->GetResourceArray());
	VertexBufferRHI = RHICmdList.CreateVertexBuffer(size, BUF_Static, info);
	delete BoneData;
	BoneData = nullptr;
}

void FSkelotBoneIndexVertexBuffer::ReleaseRHI()
{
	VertexBufferRHI.SafeRelease();
}

void FSkelotCurveBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	const uint32 Stride = 2;
	FRHIResourceCreateInfo info(TEXT("FSkelotCurveBuffer"), &Values);
	ERHIAccess AM = ERHIAccess::Unknown;// ERHIAccess::SRVGraphics | ERHIAccess::UAVCompute | ERHIAccess::CopyDest;
	Buffer = RHICmdList.CreateVertexBuffer(Values.Num() * Stride, BUF_UnorderedAccess | BUF_ShaderResource, AM, info);
	//const EPixelFormat LUT[] = {PF_Unknown, PF_R16F,  PF_G16R16F, PF_Unknown, PF_FloatRGBA};
	const EPixelFormat PXFormat = PF_R16F;
	ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(Buffer, Stride, PXFormat);
	UAV = RHICmdList.CreateUnorderedAccessView(Buffer, PXFormat);

	Values.Empty();
}

void FSkelotCurveBuffer::ReleaseRHI()
{
	ShaderResourceViewRHI.SafeRelease();
	Buffer.SafeRelease();
}