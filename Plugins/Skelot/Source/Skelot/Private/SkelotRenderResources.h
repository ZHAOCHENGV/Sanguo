// Copyright 2024 Lazy Marmot Games. All Rights Reserved.


#pragma once


#include "Engine/SkeletalMesh.h"
#include "VertexFactory.h"
#include "SkelotBase.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "RenderResource.h"
#include "SceneManagement.h"
#include "Skelot.h"
#include "SkelotResourcePool.h"


class FSkelotSkinWeightVertexBuffer;
class USkelotAnimCollection;
class FSkeletalMeshLODRenderData;
class FSkelotBoneIndexVertexBuffer;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSkelotVertexFactoryParameters, )
SHADER_PARAMETER(uint32, BoneCount)
SHADER_PARAMETER(uint32, InstanceOffset)
SHADER_PARAMETER(uint32, InstanceEndOffset)
SHADER_PARAMETER(uint32, NumCustomDataFloats)
SHADER_PARAMETER(uint32, CurveCount)
SHADER_PARAMETER(float, DeterminantSign)
SHADER_PARAMETER_SRV(Buffer<float4>, AnimationBuffer)
SHADER_PARAMETER_SRV(Buffer<float>, CurveBuffer)
SHADER_PARAMETER_SRV(Buffer<float4>, Instance_Transforms)
//SHADER_PARAMETER_SRV(Buffer<float4>, Instance_InvScaleAndDeterminantSign)
SHADER_PARAMETER_SRV(Buffer<uint>, Instance_AnimationFrameIndices)
SHADER_PARAMETER_SRV(Buffer<float>, Instance_CustomData)
SHADER_PARAMETER_SRV(Buffer<uint>, ElementIndices)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FSkelotVertexFactoryParameters> FSkelotVertexFactoryBufferRef;


/*
*/
class FSkelotBaseVertexFactory : public FVertexFactory
{
public:
	typedef FVertexFactory Super;

	//DECLARE_VERTEX_FACTORY_TYPE(FSkelotBaseVertexFactory);

	struct FDataType : FStaticMeshDataType
	{
		/** The stream to read the bone indices from */
		FVertexStreamComponent BoneIndices;
		/** The stream to read the bone weights from */
		FVertexStreamComponent BoneWeights;

		FVertexStreamComponent ExtraBoneIndices;
		FVertexStreamComponent ExtraBoneWeights;
	};

	static FSkelotBaseVertexFactory* New(int InMaxBoneInfluence);

	FSkelotBaseVertexFactory(ERHIFeatureLevel::Type InFeatureLevel) : Super(InFeatureLevel)
	{
	}
	~FSkelotBaseVertexFactory()
	{
	}
	void FillData(FDataType& data, const FSkelotBoneIndexVertexBuffer* BoneIndexBuffer, const FSkeletalMeshLODRenderData* LODData) const;
	void SetData(const FDataType& data);

	FString GetFriendlyName() const override { return TEXT("FSkelotBaseVertexFactory"); }

	uint32 MaxBoneInfluence;
};

enum ESkelotVerteFactoryMode
{
	EVF_BoneInfluence1,
	EVF_BoneInfluence2,
	EVF_BoneInfluence3,
	EVF_BoneInfluence4,

	EVF_BoneInfluence5,
	EVF_BoneInfluence6,
	EVF_BoneInfluence7,
	EVF_BoneInfluence8,

	EVF_Max,
};


template<ESkelotVerteFactoryMode FactoryMode> class TSkelotVertexFactory : public FSkelotBaseVertexFactory
{
	
	DECLARE_VERTEX_FACTORY_TYPE(TSkelotVertexFactory);
	
	typedef FSkelotBaseVertexFactory Super;

	TSkelotVertexFactory(ERHIFeatureLevel::Type InFeatureLevel) : Super(InFeatureLevel)
	{
		this->MaxBoneInfluence = StaticMaxBoneInfluence();
	}
	static uint32 StaticMaxBoneInfluence()
	{
		return ((int)FactoryMode) + 1;
	}

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static bool SupportsTessellationShaders() { return false; }
	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);
};


class FSkelotBoneIndexVertexBuffer : public FVertexBuffer
{
public:
	FStaticMeshVertexDataInterface* BoneData = nullptr;
	bool bIs16BitBoneIndex = false;
	int MaxBoneInfluences = 0;
	int NumVertices = 0;

	~FSkelotBoneIndexVertexBuffer()
	{
		if (BoneData)
		{
			delete BoneData;
			BoneData = nullptr;
		}
	}

	void Serialize(FArchive& Ar);

	void InitRHI(FRHICommandListBase& RHICmdList) override;
	void ReleaseRHI() override;
	
	void ResizeBuffer()
	{
		if(!BoneData)
			BoneData = new TStaticMeshVertexData<uint8>();

		BoneData->ResizeBuffer(NumVertices * MaxBoneInfluences * (bIs16BitBoneIndex ? 2 : 1));
		FMemory::Memzero(BoneData->GetDataPointer(), BoneData->GetResourceSize());
	}
	void SetBoneIndex(uint32 VertexIdx, uint32 InfluenceIdx, uint32 BoneIdx)
	{
		if(bIs16BitBoneIndex)
		{
			FBoneIndex16* Data = ((FBoneIndex16*)BoneData->GetDataPointer());
			Data[VertexIdx * MaxBoneInfluences + InfluenceIdx] = static_cast<FBoneIndex16>(BoneIdx);
		}
		else
		{
			FBoneIndex8* Data = ((FBoneIndex8*)BoneData->GetDataPointer());
			Data[VertexIdx * MaxBoneInfluences + InfluenceIdx] = static_cast<FBoneIndex8>(BoneIdx);
		}
	}

	uint32 GetBufferSizeInBytes() const
	{
		return NumVertices * MaxBoneInfluences * (bIs16BitBoneIndex ? 2u : 1u);
	}
	
};

//vertex buffer containing bone transforms of all baked animations
class FSkelotAnimationBuffer : public FRenderResource
{
public:
	FStaticMeshVertexDataInterface* Transforms = nullptr;

	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef ShaderResourceViewRHI;
	FUnorderedAccessViewRHIRef UAV;
	bool bHighPrecision = false;
	
	~FSkelotAnimationBuffer();
	void InitRHI(FRHICommandListBase& RHICmdList) override;
	void ReleaseRHI() override;
	void Serialize(FArchive& Ar);

	void AllocateBuffer();
	void InitBuffer(const TArrayView<FTransform> InTransforms, bool InHightPrecision);
	void InitBuffer(uint32 NumMatrix, bool InHightPrecision, bool bFillIdentity);
	void DestroyBuffer();

	FMatrix3x4* GetDataPointerHP() const;
	FMatrix3x4Half* GetDataPointerLP() const;

	//void SetPoseTransform(uint32 PoseIndex, uint32 BoneCount, const FTransform* BoneTransforms);
	//void SetPoseTransform(uint32 PoseIndex, uint32 BoneCount, const FMatrix3x4* BoneTransforms);

};

//buffer containing value of curve animations
class FSkelotCurveBuffer : public FRenderResource
{
public:
	static_assert(sizeof(FFloat16) == 2);

	TResourceArray<FFloat16> Values;
	FBufferRHIRef Buffer;
	FShaderResourceViewRHIRef ShaderResourceViewRHI;
	FUnorderedAccessViewRHIRef UAV;
	int32 NumCurve = 0;

	void InitRHI(FRHICommandListBase& RHICmdList) override;
	void ReleaseRHI() override;

	void InitBuffer(uint32 NumFrame, int32 InNumCurve)
	{
		NumCurve = InNumCurve;
		Values.SetNumZeroed(NumFrame * InNumCurve);
	}
};

/*
*/
class FSkelotMeshDataEx :  public TSharedFromThis<FSkelotMeshDataEx>
{
public:
	static const int MAX_INFLUENCE = 8;

	struct FLODData
	{
		FSkelotBoneIndexVertexBuffer BoneData;
		TUniquePtr<FSkelotBaseVertexFactory> VertexFactories[MAX_INFLUENCE + 1];
		const FSkeletalMeshLODRenderData* SkelLODData = nullptr;

		FLODData(ERHIFeatureLevel::Type InFeatureLevel);
		FSkelotBaseVertexFactory* GetVertexFactory(int MaxBoneInfluence) { return  VertexFactories[MaxBoneInfluence].Get(); }
		FSkelotBaseVertexFactory* GetOrCreateVertexFactory(FRHICommandListBase& RHICmdList, int MaxBoneInfluence);
		void InitResources(FRHICommandListBase& RHICmdList);
		void ReleaseResources();

		void Serialize(FArchive& Ar);
	};

	//accessed by [SkeletalMeshLODIndex - BaseLOD]
	TArray<FLODData, TFixedAllocator<SKELOT_MAX_LOD>> LODs;

	#if WITH_EDITOR
	void InitFromMesh(int InBaseLOD, USkeletalMesh* SKMesh, const USkelotAnimCollection* AnimSet);
	#endif
	void InitResources(FRHICommandListBase& RHICmdList);
	void ReleaseResouces();
	void Serialize(FArchive& Ar);

	void InitMeshData(const FSkeletalMeshRenderData* SKRenderData, int InBaseLOD);

	uint32 GetTotalBufferSize() const;

};

typedef TSharedPtr<FSkelotMeshDataEx, ESPMode::ThreadSafe> FSkelotMeshDataExPtr;


struct FSkelotInstanceBuffer : TSharedFromThis<FSkelotInstanceBuffer>
{
	static const uint32 SizeAlign = 4096;	//must be pow2	InstanceCount is aligned to this 

	FBufferRHIRef TransformVB;
	FShaderResourceViewRHIRef TransformSRV;
	FBufferRHIRef FrameIndexVB;
	FShaderResourceViewRHIRef FrameIndexSRV;
	//FBufferRHIRef InvNonUniformScaleAndDeterminantSign;
	//FShaderResourceViewRHIRef InvNonUniformScaleAndDeterminantSignSRV;
	uint32 CreationFrameNumber = 0;
	uint32 InstanceCount = 0;
	
	SkelotShaderMatrixT* MappedTransforms = nullptr;
	uint32* MappedFrameIndices = nullptr;
	//using NonUniformScaleT = FVector4f;
	//NonUniformScaleT* MappedInvNonUniformScaleAndDeterminantSign = nullptr;
	
	

	void LockBuffers(FRHICommandListBase& RHICmdList);
	void UnlockBuffers(FRHICommandListBase& RHICmdList);
	bool IsLocked() const { return MappedTransforms != nullptr; }
	uint32 GetSize() const { return InstanceCount; }

	static TSharedPtr<FSkelotInstanceBuffer> Create(FRHICommandListBase& RHICmdList, uint32 InstanceCount);
};
typedef TSharedPtr<FSkelotInstanceBuffer> FSkelotInstanceBufferPtr;




typedef TBufferAllocatorSingle<FSkelotInstanceBufferPtr> FSkelotInstanceBufferAllocator;
extern FSkelotInstanceBufferAllocator GSkelotInstanceBufferPool;



struct FSkelotCIDBuffer : TSharedFromThis<FSkelotCIDBuffer>
{
	static const uint32 SizeAlign = 4096;	//must be pow2	InstanceCount is aligned to this 

	FBufferRHIRef CustomDataBuffer;
	FShaderResourceViewRHIRef CustomDataSRV;
	
	uint32 CreationFrameNumber = 0;
	uint32 NumberOfFloat = 0;

	float* MappedData = nullptr;

	void LockBuffers(FRHICommandListBase& RHICmdList);
	void UnlockBuffers(FRHICommandListBase& RHICmdList);
	bool IsLocked() const { return MappedData != nullptr; }
	uint32 GetSize() const { return NumberOfFloat; }

	static TSharedPtr<FSkelotCIDBuffer> Create(FRHICommandListBase& RHICmdList, uint32 InNumberOfFloat);
};
typedef TSharedPtr<FSkelotCIDBuffer> FSkelotCIDBufferPtr;

typedef TBufferAllocatorSingle<FSkelotCIDBufferPtr> FSkelotCIDBufferAllocator;
extern FSkelotCIDBufferAllocator GSkelotCIDBufferPool;

struct FSkelotBatchElementOFR : FOneFrameResource
{
	uint32 MaxBoneInfluences;
	FSkelotBaseVertexFactory* VertexFactory;
	FSkelotVertexFactoryBufferRef UniformBuffer;	//uniform buffer holding data for drawing a LOD
};


struct FSkelotProxyOFR : FOneFrameResource
{
	FSkelotCIDBufferPtr CDIShadowBuffer;

	~FSkelotProxyOFR();
};








struct FSkelotElementIndexBuffer : TSharedFromThis<FSkelotElementIndexBuffer>
{
	static const uint32 SizeAlign = 4096;

	FBufferRHIRef ElementIndexBuffer;
	FShaderResourceViewRHIRef ElementIndexUIN16SRV;
	FShaderResourceViewRHIRef ElementIndexUIN32SRV;
	uint32 CreationFrameNumber = 0;
	uint32 SizeInBytes = 0;

	void* MappedData = nullptr;

	void LockBuffers(FRHICommandListBase& RHICmdList);
	void UnlockBuffers(FRHICommandListBase& RHICmdList);
	bool IsLocked() const { return MappedData != nullptr; }
	uint32 GetSize() const { return SizeInBytes; }

	static TSharedPtr<FSkelotElementIndexBuffer> Create(FRHICommandListBase& RHICmdList, uint32 InSizeInBytes);
};

typedef TSharedPtr<FSkelotElementIndexBuffer> FSkelotElementIndexBufferPtr;
typedef TBufferAllocatorSingle<FSkelotElementIndexBufferPtr> FSkelotElementIndexBufferAllocator;

extern FSkelotElementIndexBufferAllocator GSkelotElementIndexBufferPool;