// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once



struct alignas(16) FSkelotMeshGeneratorBase
{
	static uint32 OverrideNumPrimitive(uint32 Value)
	{
		if (!UE_BUILD_SHIPPING)
			return GSkelot_MaxTrianglePerInstance > 0 ? FMath::Min((uint32)GSkelot_MaxTrianglePerInstance, Value) : Value;

		return Value;
	}
	static uint32 OverrideMaxBoneInfluence(uint32 Value)
	{
		if (!UE_BUILD_SHIPPING)
			return GSkelot_FroceMaxBoneInfluence > 0 ? FMath::Min(uint32(FSkelotMeshDataEx::MAX_INFLUENCE), (uint32)GSkelot_FroceMaxBoneInfluence) : Value;

		return Value;
	}
	static uint32 OverrideAnimFrameIndex(uint32 Value)
	{
		if (!UE_BUILD_SHIPPING && GSkelot_ForcedAnimFrameIndex >= 0)
			return (uint32)GSkelot_ForcedAnimFrameIndex;

		return Value;
	};
	static ESkelotVerteFactoryMode GetTargetVFMode(int MeshMaxBoneInf)
	{
		return (ESkelotVerteFactoryMode)(MeshMaxBoneInf - 1);
	}

	static uint32 PackFrameIndices(uint16 cur, uint16 prev) { return static_cast<uint32>(cur) | (static_cast<uint32>(prev) << 16); }

	static void SimpleFNV(uint32& Hash, uint32 Value)
	{
		Hash = (Hash ^ Value) * 0x01000193; //from FFnv1aHash::Add
	}

	static void OffsetPlane(FPlane& P, const FVector& Offset)
	{
		P.W = (P.GetOrigin() - Offset) | P.GetNormal();
	}

	//////////////////////////////////////////////////////////////////////////
	static FVector3f GetMatrixInvScaleVector(const FMatrix44f& M, float Tolerance = UE_SMALL_NUMBER)
	{
		FVector3f Scale3D(1, 1, 1);

		// For each row, find magnitude, and if its non-zero re-scale so its unit length.
		for (int32 i = 0; i < 3; i++)
		{
			const float SquareSum = (M.M[i][0] * M.M[i][0]) + (M.M[i][1] * M.M[i][1]) + (M.M[i][2] * M.M[i][2]);
			if (SquareSum > Tolerance)
			{
				Scale3D[i] = FMath::InvSqrt(SquareSum);
			}
			else
			{
				Scale3D[i] = 0.f;
			}
		}

		return Scale3D;
	}





	//////////////////////////////////////////////////////////////////////////
	struct FIndexCollector
	{
		//each cache line (64 bytes) == UINT32[16]

		static const uint32 NUM_DW_PER_LINE = PLATFORM_CACHE_LINE_SIZE / sizeof(uint32);
		static const uint32 MAX_DW_INDEX = (NUM_DW_PER_LINE * 4) - (sizeof(void*) / sizeof(uint32));
		static const uint32 MAX_WORD_INDEX = MAX_DW_INDEX * 2;
		static const uint32 PAGE_DATA_SIZE_IN_BYTES = MAX_DW_INDEX * sizeof(uint32);

		struct alignas(PLATFORM_CACHE_LINE_SIZE) FPageData
		{
			FPageData* NextPage = nullptr;
			union
			{
				uint32 ValuesUINT32[MAX_DW_INDEX];
				uint16 ValuesUINT16[MAX_WORD_INDEX];
			};
		};
		static_assert((sizeof(FPageData) % PLATFORM_CACHE_LINE_SIZE) == 0);

		FPageData* PageHead;
		FPageData* PageTail;
		//uint32 BatchHash = 0x811c9dc5; //copied from FFnv1aHash
		uint32 Counter;	//counter for number of elements in tail block

		void Init(bool bUseUint32)
		{
			PageHead = PageTail = nullptr;
			Counter = bUseUint32 ? MAX_DW_INDEX : MAX_WORD_INDEX;
		}
		//
		template<typename T> void AddValue(FSkelotMeshGeneratorBase& Gen, T InValue)
		{
			constexpr uint32 MAX_IDX = (sizeof(T) == sizeof(uint32)) ? MAX_DW_INDEX : MAX_WORD_INDEX;
			if (Counter == MAX_IDX)
			{
				Counter = 0;
				FPageData* NewPage = Gen.template MempoolNew<FPageData>();
				if (PageTail)
				{
					PageTail->NextPage = NewPage;
					PageTail = NewPage;
				}
				else
				{
					PageHead = PageTail = NewPage;
				}
			}

			if constexpr (sizeof(T) == sizeof(uint32))
				PageTail->ValuesUINT32[Counter++] = InValue;
			else
				PageTail->ValuesUINT16[Counter++] = InValue;
		}
		//
		template<typename T> void CopyToArray(T* DstMem) const
		{
			constexpr uint32 MAX_IDX = (sizeof(T) == sizeof(uint32)) ? MAX_DW_INDEX : MAX_WORD_INDEX;

			const FPageData* PageIter = PageHead;
			while (PageIter)
			{
				const uint32 Num = PageIter->NextPage ? MAX_IDX : this->Counter;
				FMemory::Memcpy(DstMem, PageIter->ValuesUINT32, Num * sizeof(T));
				DstMem += Num;
				PageIter = PageIter->NextPage;
			}
		}
	};


	struct FLODData
	{
		uint32 NumInstance;
		uint32 InstanceOffset;
		FIndexCollector IndexCollector;

		void Init(bool bUseUIN32)
		{
			NumInstance = InstanceOffset = 0;
			IndexCollector.Init(bUseUIN32);
		}
		template<typename T> void AddElem(FSkelotMeshGeneratorBase& Gen, T Value)
		{
			NumInstance++;
			IndexCollector.template AddValue<T>(Gen, Value);
		}
	};


	struct FSubMeshData
	{
		FLODData LODs[SKELOT_MAX_LOD];
		bool bHasAnyLOD;

		void Init(bool bUseUIN32)
		{
			for (FLODData& L : LODs)
				L.Init(bUseUIN32);

			bHasAnyLOD = false;
		}
	};

	struct FSubMeshInfo
	{
		uint32 MaxDrawDist = ~0u;
		uint32 OverrideDist = ~0u;
		uint8 OverrideMeshIdx = 0xFF;
		bool bIsValid = false;
		uint8 LODRemap[SKELOT_MAX_LOD] = {};
	};

// #pragma region stack allocator
// 	static constexpr uint32 STACK_CAPCITY = ((sizeof(FSubMeshInfo) + sizeof(FSubMeshData)) * SKELOT_MAX_SUBMESH) + (sizeof(uint8) * SKELOT_MAX_SUBMESH) + sizeof(FConvexVolume) + 128;
// 
// 	uint8 StackBytes[STACK_CAPCITY];
// 	uint8* StackMemSeek = nullptr;
// 	const uint8* GetStackTail() const { return &StackBytes[STACK_CAPCITY - 1]; }
// 
// 	uint8* StackAlloc(const size_t SizeInBytes, const size_t InAlign = 4)
// 	{
// 		uint8* addr = Align(StackMemSeek, InAlign);
// 		StackMemSeek = addr + SizeInBytes;
// 		check(StackMemSeek <= GetStackTail());
// 		return addr;
// 	}
// 	template<typename T> T* StackNewArray(uint32 Count)
// 	{
// 		uint8* addr = StackAlloc(sizeof(T) * Count);
// 		DefaultConstructItems<T>(addr, Count);
// 		return (T*)addr;
// 	}
// #pragma endregion


	FMemStackBase MemStack { FMemStackBase::EPageSize::Large };

	template <typename T> T* MempoolAlloc(const uint32 Count, size_t InAlign = alignof(T))
	{	
		return (T*)MemStack.Alloc(sizeof(T) * Count, InAlign);
	};
	template<typename T> T* MempoolNew(const uint32 Count = 1, size_t InAlign = alignof(T))
	{
		T* Ptr = (T*)MemStack.Alloc(sizeof(T) * Count, InAlign);
		DefaultConstructItems<T>(Ptr, Count);
		return Ptr;
	}

	const FConvexVolume* EditedViewFrustum = nullptr;
	uint32* VisibleInstances = nullptr;	//instance index of visible instances
	uint32* Distances = nullptr; //distance of instances (float is treated as uint32 for faster comparison)
	//FVector3f* VisibleInstances_InvScale = nullptr;
	//bool* VisibleInstances_DeterminantSign = nullptr;

	uint32 NumVisibleInstance = 0;
	bool Use32BitElementIndex() const { return NumVisibleInstance >= 0xFFFF; }
	auto GetElementIndexSize() const { return Use32BitElementIndex() ? 4u : 2u; }


	uint32 TotalElementCount = 0;	//
	uint32 TotalBatch = 0;	//number of FMeshBatch we draw
	uint32 NumSubMesh = 0;
	
	TArrayView<FSubMeshData> SubMeshes_Data[2]; //0 for those with positive determinant, 1 for negative determinant
	TArrayView<FSubMeshInfo> SubMeshes_Info;

	const FSkelotProxy* Proxy = nullptr;
	const FSceneViewFamily* ViewFamily = nullptr;
	const FSceneView* View = nullptr;
	FMeshElementCollector* Collector = nullptr;
	int ViewIndex = 0;
	FVector3f ResolvedViewLocation;
	uint32 LODDrawDistances[SKELOT_MAX_LOD] = {};	//sorted from low to high. value is binary form of float. in other words we use: reinterpret_cast<uint32>(floatA) < reinterpret_cast<uint32>(floatB)
	uint32 MinDrawDist = 0;
	uint32 MaxDrawDist = ~0u;
	uint8 MaxMeshPerInstance = 0;
	bool bWireframe = false;
	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	FSkelotInstanceBufferPtr InstanceBuffer;
	FSkelotCIDBufferPtr CIDBuffer;
	FSkelotElementIndexBufferPtr ElementIndexBuffer;


	static const uint32 DISTANCING_NUM_FLOAT_PER_REG = 4;


};




template<bool bShaddowCollector> struct FSkelotMultiMeshGenerator : FSkelotMeshGeneratorBase
{
	

	FSkelotMultiMeshGenerator(const FSkelotProxy* InProxy, const FSceneViewFamily* InViewFamily, const FSceneView* InView, FMeshElementCollector* InCollector, int InViewIndex) 
	{
		Proxy = InProxy;
		ViewFamily = InViewFamily;
		View = InView;
		Collector = InCollector;
		ViewIndex = InViewIndex;

		//StackMemSeek = StackBytes;
		check(InProxy->SubMeshes.Num() > 0 && InProxy->MaxMeshPerInstance > 0);
		this->NumSubMesh = InProxy->SubMeshes.Num();
		this->MaxMeshPerInstance = InProxy->MaxMeshPerInstance;
		
		this->SubMeshes_Data[0] = MakeArrayView(MempoolNew<FSubMeshData>(NumSubMesh), NumSubMesh);
		this->SubMeshes_Data[1] = MakeArrayView(MempoolNew<FSubMeshData>(NumSubMesh), NumSubMesh);
		this->SubMeshes_Info = MakeArrayView(MempoolNew<FSubMeshInfo>(NumSubMesh), NumSubMesh);

		GenerateData();

		if (this->TotalElementCount == 0)
			return;

		GenerateBatches();


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (GSkelot_DrawInstanceBounds && !bShaddowCollector)
		{
			for (uint32 VisIndex = 0; VisIndex < NumVisibleInstance; VisIndex++)
			{
				RenderInstanceBound(this->VisibleInstances[VisIndex]);
			}


		}
		if (GSkelot_DrawCells && !bShaddowCollector)
		{
			DrawCellBounds();
		}
#endif

	}
	~FSkelotMultiMeshGenerator()
	{
	}
	
	//////////////////////////////////////////////////////////////////////////
	void RenderInstanceBound(uint32 InstanceIndex) const
	{
		check(InstanceIndex < static_cast<int>(Proxy->DynamicData->InstanceCount));
		const FMatrix InstanceMatrix = FMatrix(Proxy->DynamicData->Transforms[InstanceIndex]);
		FPrimitiveDrawInterface* PDI = Collector->GetPDI(ViewIndex);
		const ESceneDepthPriorityGroup DrawBoundsDPG = SDPG_World;
		uint16 InstanceAnimationFrameIndex = Proxy->DynamicData->FrameIndices[InstanceIndex];
		const FBoxCenterExtentFloat& InstanceBound = Proxy->DynamicData->Bounds[InstanceIndex];
		DrawWireBox(PDI, FBox(InstanceBound.GetFBox()), FLinearColor::Green, DrawBoundsDPG);
		//draw axis
		{
			FVector AxisLoc = InstanceMatrix.GetOrigin();
			FVector X, Y, Z;
			InstanceMatrix.GetScaledAxes(X, Y, Z);
			const float Scale = 50;
			PDI->DrawLine(AxisLoc, AxisLoc + X * Scale, FColor::Red, DrawBoundsDPG, 1);
			PDI->DrawLine(AxisLoc, AxisLoc + Y * Scale, FColor::Green, DrawBoundsDPG, 1);
			PDI->DrawLine(AxisLoc, AxisLoc + Z * Scale, FColor::Blue, DrawBoundsDPG, 1);
		}
	}
	//////////////////////////////////////////////////////////////////////////
	void RenderCircleBound(const FVector& InBoundOrigin, float InBoundSphereRadius, FColor DrawColor = FColor::Yellow) const
	{
		FPrimitiveDrawInterface* PDI = Collector->GetPDI(ViewIndex);
		const ESceneDepthPriorityGroup DrawBoundsDPG = SDPG_World;
		DrawCircle(PDI, InBoundOrigin, FVector(1, 0, 0), FVector(0, 1, 0), DrawColor, InBoundSphereRadius, 32, DrawBoundsDPG);
		DrawCircle(PDI, InBoundOrigin, FVector(1, 0, 0), FVector(0, 0, 1), DrawColor, InBoundSphereRadius, 32, DrawBoundsDPG);
		DrawCircle(PDI, InBoundOrigin, FVector(0, 1, 0), FVector(0, 0, 1), DrawColor, InBoundSphereRadius, 32, DrawBoundsDPG);
	}
	//////////////////////////////////////////////////////////////////////////
	void RenderBox(const FBox& Box, FColor DrawColor = FColor::Green) const
	{
		FPrimitiveDrawInterface* PDI = Collector->GetPDI(ViewIndex);
		const ESceneDepthPriorityGroup DrawBoundsDPG = SDPG_World;
		DrawWireBox(PDI, Box, DrawColor, DrawBoundsDPG);
	}
	//////////////////////////////////////////////////////////////////////////
	void DrawCellBounds()
	{
		for (uint32 CellIndex = 0; CellIndex < this->Proxy->DynamicData->NumCells; CellIndex++)
		{
			const FSkelotDynamicData::FCell& Cell = this->Proxy->DynamicData->Cells[CellIndex];
			if (Cell.IsEmpty())
				continue;

			FPrimitiveDrawInterface* PDI = Collector->GetPDI(ViewIndex);
			FColor Color = FColor::MakeRandomSeededColor(CellIndex);
			DrawWireBox(PDI, FBox(Cell.Bound.ToBox()), Color, SDPG_World, 3);
			if(0)
			{
				Cell.ForEachInstance(*this->Proxy->DynamicData, [&](uint32 InstanceIndex) {
					FVector3f Center = this->Proxy->DynamicData->Bounds[InstanceIndex].Center;
					PDI->DrawPoint(FVector(Center), Color, 8, SDPG_World);
				});
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	TUniformBufferRef<FSkelotVertexFactoryParameters> CreateUniformBuffer(const FLODData& LODData, bool bDeterminantNegative)
	{
		const USkelotAnimCollection* AC = this->Proxy->AminCollection;
		FSkelotVertexFactoryParameters UniformParams;
		UniformParams.BoneCount = AC->RenderBoneCount;
		UniformParams.InstanceOffset = LODData.InstanceOffset;
		UniformParams.InstanceEndOffset = LODData.InstanceOffset + LODData.NumInstance - 1;
		UniformParams.NumCustomDataFloats = 0;
		UniformParams.CurveCount = 0;
		UniformParams.DeterminantSign = bDeterminantNegative ? -1 : 1;
		UniformParams.AnimationBuffer = AC->AnimationBuffer->ShaderResourceViewRHI;
		UniformParams.CurveBuffer = GNullVertexBuffer.VertexBufferSRV;
		UniformParams.Instance_CustomData = GNullVertexBuffer.VertexBufferSRV;//#TODO proper SRV ?
		
		if (AC->CurveBuffer)
		{
			UniformParams.CurveCount = AC->CurveBuffer->NumCurve;
			UniformParams.CurveBuffer = AC->CurveBuffer->ShaderResourceViewRHI;
		}
		if (this->CIDBuffer) //do we have any per instance custom data 
		{
			UniformParams.Instance_CustomData = this->CIDBuffer->CustomDataSRV;
			UniformParams.NumCustomDataFloats = this->Proxy->NumCustomDataFloats;
		}

		UniformParams.Instance_Transforms = this->InstanceBuffer->TransformSRV;
		//UniformParams.Instance_InvScaleAndDeterminantSign = this->InstanceBuffer->InvNonUniformScaleAndDeterminantSignSRV;
		UniformParams.Instance_AnimationFrameIndices = this->InstanceBuffer->FrameIndexSRV;

		UniformParams.ElementIndices = this->Use32BitElementIndex() ? this->ElementIndexBuffer->ElementIndexUIN32SRV : this->ElementIndexBuffer->ElementIndexUIN16SRV;


		return FSkelotVertexFactoryBufferRef::CreateUniformBufferImmediate(UniformParams, UniformBuffer_SingleFrame);//will take from pool , no worry

	}
	//////////////////////////////////////////////////////////////////////////
	FMeshBatch& AllocateMeshBatch(const FSkeletalMeshLODRenderData& SkelLODData, uint32 SubMeshIndex, uint32 LODIndex, uint32 SectionIndex, FSkelotBatchElementOFR* BatchUserData, bool bDeterminantNegative)
	{
		FMeshBatch& Mesh = this->Collector->AllocateMesh();
		Mesh.ReverseCulling = bDeterminantNegative;//IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = this->Proxy->GetDepthPriorityGroup(View);
		Mesh.bCanApplyViewModeOverrides = true;
		Mesh.bSelectable = false;
		Mesh.bUseForMaterial = true;
		Mesh.bUseSelectionOutline = false;
		Mesh.LODIndex = static_cast<int8>(LODIndex);	//?
		Mesh.SegmentIndex = static_cast<uint8>(SectionIndex);
		//its useless, MeshIdInPrimitive is set by Collector->AddMesh()
		//Mesh.MeshIdInPrimitive = static_cast<uint16>(LODIndex); //static_cast<uint16>((LODIndex << 8) | SectionIndex);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		Mesh.VisualizeLODIndex = static_cast<int8>(LODIndex);
#endif
		Mesh.VertexFactory = BatchUserData->VertexFactory;

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.UserData = BatchUserData;

		BatchElement.PrimitiveIdMode = PrimID_ForceZero;
		BatchElement.IndexBuffer = SkelLODData.MultiSizeIndexContainer.GetIndexBuffer();
		BatchElement.UserIndex = 0;
		//BatchElement.PrimitiveUniformBufferResource = &PrimitiveUniformBuffer->UniformBuffer; //&GIdentityPrimitiveUniformBuffer; 
		BatchElement.PrimitiveUniformBuffer = this->Proxy->GetUniformBuffer();

		BatchElement.NumInstances = this->SubMeshes_Data[bDeterminantNegative][SubMeshIndex].LODs[LODIndex].NumInstance;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		BatchElement.VisualizeElementIndex = static_cast<int32>(SectionIndex);
#endif
		return Mesh;
	}


	//////////////////////////////////////////////////////////////////////////
	void Cull()
	{
		{
			SKELOT_SCOPE_CYCLE_COUNTER(FirstCull);

			const FSkelotDynamicData* DynData = this->Proxy->DynamicData;
			uint32* VisibleInstancesIter = this->VisibleInstances;

			if(DynData->NumCells > 0 && !GSkelot_DisableFrustumCull)
			{
				//cells intersection with frustum
				for (uint32 CellIndex = 0; CellIndex < DynData->NumCells; CellIndex++)
				{
					const FSkelotDynamicData::FCell& Cell = DynData->Cells[CellIndex];
					if (Cell.IsEmpty())
						continue;

					bool bFullyInside;
					FBoxCenterExtentFloat CellBound;
					Cell.Bound.ToCenterExtentBox(CellBound);
					if (!EditedViewFrustum->IntersectBox(FVector(CellBound.Center), FVector(CellBound.Extent), bFullyInside))
						continue;

					if (bFullyInside)
					{
						VisibleInstancesIter = (uint32*)Cell.CopyToArray(*DynData, (uint8*)VisibleInstancesIter);
					}
					else
					{
						Cell.ForEachInstance(*DynData, [&](uint32 InstanceIndex) {
							const FBoxCenterExtentFloat& IB = DynData->Bounds[InstanceIndex];
							if (EditedViewFrustum->IntersectBox(FVector(IB.Center), FVector(IB.Extent)))
							{
								*VisibleInstancesIter++ = InstanceIndex;
							}
						});
					}
				}
			}
			else
			{
				//per instance frustum cull 
				for (uint32 InstanceIndex = 0; InstanceIndex < DynData->InstanceCount; InstanceIndex++)
				{
					if (EnumHasAnyFlags(DynData->Flags[InstanceIndex], ESkelotInstanceFlags::EIF_Destroyed | ESkelotInstanceFlags::EIF_Hidden))
						continue;

					if(!GSkelot_DisableFrustumCull)
					{
						const FBoxCenterExtentFloat& Bound = DynData->Bounds[InstanceIndex];
						if (!EditedViewFrustum->IntersectBox(FVector(Bound.Center), FVector(Bound.Extent)))	//frustum cull
							continue;
					}

					*VisibleInstancesIter++ = InstanceIndex;
				}
			}

			this->NumVisibleInstance = VisibleInstancesIter - this->VisibleInstances;

			if (this->NumVisibleInstance == 0)
				return;
			
		}

		{
			//fix the remaining data because we go SIMD only
			uint32 AVI = Align(this->NumVisibleInstance, DISTANCING_NUM_FLOAT_PER_REG);
			for (uint32 i = this->NumVisibleInstance; i < AVI; i++)
				this->VisibleInstances[i] = this->VisibleInstances[0];

			//MempoolAlloc<uint32>(AVI * sizeof(uint32)); //tricky! pass over VisibleInstances
			this->Distances = MempoolAlloc<uint32>(AVI, 16);
			//this->VisibleInstances_InvScale = MempoolAlloc<FVector3f>(AVI);
			//this->VisibleInstances_DeterminantSign = MempoolAlloc<bool>(AVI);

			CollectDistances();
		}

		//#TODO maybe sort with Algo::Sort when NumVisibleInstance is small
		//sort the instances from front to rear
		if(NumVisibleInstance > 1)
		{
			SKELOT_SCOPE_CYCLE_COUNTER(SortInstances);

			uint32* Distances_Sorted = MempoolAlloc<uint32>(NumVisibleInstance);
			uint32* VisibleInstances_Sorted = MempoolAlloc<uint32>(NumVisibleInstance);

			if (NumVisibleInstance > 0xFFff)
				SkelotRadixSort32(VisibleInstances_Sorted, this->VisibleInstances, Distances_Sorted, this->Distances, static_cast<uint32>(NumVisibleInstance));
			else
				SkelotRadixSort32(VisibleInstances_Sorted, this->VisibleInstances, Distances_Sorted, this->Distances, static_cast<uint16>(NumVisibleInstance));

			this->VisibleInstances = VisibleInstances_Sorted;
			this->Distances = Distances_Sorted;
		}

		if(MaxDrawDist != ~0u)
		{
// 			int i = this->NumVisibleInstance - 1;
// 			for (; i >= 0; i--)
// 			{
// 				if (this->Distances[i] <= this->MaxDrawDist)
// 					break;
// 			}
// 			this->NumVisibleInstance = i + 1;

			uint32* DistIter = this->Distances + this->NumVisibleInstance;
			do
			{
				DistIter--;
				if (*DistIter <= this->MaxDrawDist)
				{
					DistIter++;
					break;
				}
			} while (DistIter != this->Distances);

			
			this->NumVisibleInstance = DistIter - this->Distances;

			if (this->NumVisibleInstance == 0)
				return;
		}

		if(MinDrawDist > 0)
		{
			uint32 i = 0;
			for (; i < this->NumVisibleInstance; i++)
			{
				if(this->Distances[i] >= this->MinDrawDist)
					break;
			}
			this->VisibleInstances += i;
			this->Distances += i;
			this->NumVisibleInstance -= i;

			if (this->NumVisibleInstance == 0)
				return;
		}



		{
			if (NumVisibleInstance > 0xFFff)
				SecondCull<uint32>();
			else
				SecondCull<uint16>();
		}

	}
	//////////////////////////////////////////////////////////////////////////
	void CollectDistances()
	{
		SKELOT_SCOPE_CYCLE_COUNTER(CollectDistances);

		auto CamPos = VectorLoad(&ResolvedViewLocation.X);
		auto CamX = VectorReplicate(CamPos, 0);
		auto CamY = VectorReplicate(CamPos, 1);
		auto CamZ = VectorReplicate(CamPos, 2);

		const uint32 AlignedNumVis = Align(this->NumVisibleInstance, DISTANCING_NUM_FLOAT_PER_REG);
		const FBoxCenterExtentFloat* Bounds = this->Proxy->DynamicData->Bounds;

		//collect distances of visible indices
		for (uint32 VisIndex = 0; VisIndex < AlignedNumVis; VisIndex += DISTANCING_NUM_FLOAT_PER_REG)
		{
			auto CentersA = VectorLoad(&Bounds[this->VisibleInstances[VisIndex + 0]].Center.X);
			auto CentersB = VectorLoad(&Bounds[this->VisibleInstances[VisIndex + 1]].Center.X);
			auto CentersC = VectorLoad(&Bounds[this->VisibleInstances[VisIndex + 2]].Center.X);
			auto CentersD = VectorLoad(&Bounds[this->VisibleInstances[VisIndex + 3]].Center.X);

			SkelotVectorTranspose4x4(CentersA, CentersB, CentersC, CentersD);

			auto XD = VectorSubtract(CamX, CentersA);
			auto YD = VectorSubtract(CamY, CentersB);
			auto ZD = VectorSubtract(CamZ, CentersC);

			auto LenSQ = VectorMultiplyAdd(XD, XD, VectorMultiplyAdd(YD, YD, VectorMultiply(ZD, ZD)));

			VectorStoreAligned(LenSQ, reinterpret_cast<float*>(this->Distances + VisIndex));
		}
	}
	//////////////////////////////////////////////////////////////////////////
	template<typename TVisIndex> void SecondCull()
	{
		SKELOT_SCOPE_CYCLE_COUNTER(SecondCull);

		const bool bCheckForNegativeDeterminant = this->Proxy->bCheckForNegativeDeterminant;

		//set initial values of batches
		for (uint32 MeshIdx = 0; MeshIdx < this->NumSubMesh; MeshIdx++)
		{
			this->SubMeshes_Data[0][MeshIdx].Init(sizeof(TVisIndex) == sizeof(uint32));
			this->SubMeshes_Data[1][MeshIdx].Init(sizeof(TVisIndex) == sizeof(uint32));
		}

		const uint8* InstancesMeshSlots = this->Proxy->DynamicData->MeshSlots;

		uint32 CurLODIndex = 0;
		uint32 NextLODDist = LODDrawDistances[CurLODIndex];

		uint8 SubMeshCurLOD[SKELOT_MAX_SUBMESH];
		for (uint32 MeshIdx = 0; MeshIdx < this->NumSubMesh; MeshIdx++)
			SubMeshCurLOD[MeshIdx] = this->SubMeshes_Info[MeshIdx].LODRemap[CurLODIndex];

		

		//second culling + batching 
		for (uint32 VisIndex = 0; VisIndex < this->NumVisibleInstance; VisIndex++)
		{
			const uint32 InstanceIndex = this->VisibleInstances[VisIndex];
			check(InstanceIndex < this->Proxy->DynamicData->InstanceCount);
			const uint32 DistanceSQ = this->Distances[VisIndex];
			const FMatrix44f& InstanceTransform = this->Proxy->DynamicData->Transforms[InstanceIndex];
			
			const bool bInstanceDeterminantNegative = bCheckForNegativeDeterminant && InstanceTransform.RotDeterminant() < 0;
			TArrayView<FSubMeshData>& TargetSubMeshData = SubMeshes_Data[bInstanceDeterminantNegative];

			while (DistanceSQ > NextLODDist)
			{
				CurLODIndex++;
				NextLODDist = this->LODDrawDistances[CurLODIndex];

				for (uint32 MeshIdx = 0; MeshIdx < this->NumSubMesh; MeshIdx++)
					SubMeshCurLOD[MeshIdx] = this->SubMeshes_Info[MeshIdx].LODRemap[CurLODIndex];

			}

			const uint8* MeshSlotIter = InstancesMeshSlots + InstanceIndex * (this->MaxMeshPerInstance + 1);
			while (true)
			{
				uint8 SubMeshIdx = *MeshSlotIter++;

				if (SubMeshIdx == 0xFF) //data should be terminated with 0xFF
					break;

				check(SubMeshIdx < this->NumSubMesh);

				if (DistanceSQ > SubMeshes_Info[SubMeshIdx].MaxDrawDist)
					continue;

				if (DistanceSQ > SubMeshes_Info[SubMeshIdx].OverrideDist)
				{
					SubMeshIdx = SubMeshes_Info[SubMeshIdx].OverrideMeshIdx;

					if (DistanceSQ > SubMeshes_Info[SubMeshIdx].MaxDrawDist)
						continue;
				}

				if (bShaddowCollector)
				{
					//##TODO optimize. in shadow pass sub meshes with same skeletal mesh can be merge if materials have same VS (default vertex shader)
					//SubMeshIdx = this->ShadowSubmeshRemap[SubMeshIdx];
				}
				
				
				FLODData& LODData = TargetSubMeshData[SubMeshIdx].LODs[SubMeshCurLOD[SubMeshIdx]];
				LODData.template AddElem<TVisIndex>(*this, static_cast<TVisIndex>(VisIndex));
			}
		}
		

		//assign offsets and count total
		{
			this->TotalElementCount = 0;
			this->TotalBatch = 0;

			for(int GroupIndex = 0; GroupIndex < 2; GroupIndex++)
			{
				for (uint32 MeshIdx = 0; MeshIdx < this->NumSubMesh; MeshIdx++)
				{
					if (!this->SubMeshes_Info[MeshIdx].bIsValid)
						continue;

					FSubMeshData& SubMesh = this->SubMeshes_Data[GroupIndex][MeshIdx];
					for (uint32 LODIndex = 0; LODIndex < SKELOT_MAX_LOD; LODIndex++)
					{
						FLODData& LODData = SubMesh.LODs[LODIndex];
						if (LODData.NumInstance == 0)
							continue;

						SubMesh.bHasAnyLOD = true;

						LODData.InstanceOffset = this->TotalElementCount;
						this->TotalElementCount += LODData.NumInstance;
						this->TotalBatch++;
					}
				}
			};

			check(this->TotalBatch <= this->Proxy->MaxBatchCountPossible);
		}
	}
	//////////////////////////////////////////////////////////////////////////
	void FillBuffers()
	{
		const FSkelotDynamicData* DynamicData = this->Proxy->DynamicData;
		const FSkelotDynamicData* OldDynamicData = this->Proxy->OldDynamicData;

		if (OldDynamicData && !GSkelot_DebugForceNoPrevFrameData)
		{
			const int CFN = GFrameNumberRenderThread;
			if ((CFN - OldDynamicData->CreationNumber) == 1)	//must belong to the previous frame exactly
			{

			}
			else
			{
				OldDynamicData = DynamicData;
			}
		}
		else
		{
			OldDynamicData = DynamicData;
		}

		//EIF_New means instance just added this frame, so it doesn't have previous frame data
		check((int)ESkelotInstanceFlags::EIF_New == 2);
		const FSkelotDynamicData* PrevDynamicDataLUT[3] = { OldDynamicData, OldDynamicData, DynamicData };

		//fill instance buffers
		{
			SkelotShaderMatrixT* RESTRICT DstInstanceTransform = this->InstanceBuffer->MappedTransforms;
			uint32* RESTRICT DstPackedFrameIndex = this->InstanceBuffer->MappedFrameIndices;
			check(IsAligned(DstInstanceTransform, 16));
			//FVector4f* RESTRICT DstInvNonUniformScale = this->InstanceBuffer->MappedInvNonUniformScaleAndDeterminantSign;
			//check(IsAligned(DstInvNonUniformScale, 16));

			float* RESTRICT DstCustomDatas = nullptr;
			if (this->CIDBuffer)
			{
				DstCustomDatas = this->CIDBuffer->MappedData;
			}
			const uint32 NumCustomDataFloats = DstCustomDatas ? Proxy->NumCustomDataFloats : 0;

			//#Note stores instance data from front to rear
			//for each visible instance
			for (uint32 VisIdx = 0; VisIdx < NumVisibleInstance; VisIdx++)
			{
				uint32 InstanceIndex = this->VisibleInstances[VisIdx];
				check(InstanceIndex < DynamicData->InstanceCount);

				const FSkelotDynamicData* PrevFrameDynamicData = PrevDynamicDataLUT[static_cast<uint16>(DynamicData->Flags[InstanceIndex] & ESkelotInstanceFlags::EIF_New)];
				check(InstanceIndex < PrevFrameDynamicData->InstanceCount);

				const FMatrix44f& InstanceTransform = DynamicData->Transforms[InstanceIndex];
				//converts from Matrix4x4f
				DstInstanceTransform[VisIdx * 2 + 0] = InstanceTransform;
				DstInstanceTransform[VisIdx * 2 + 1] = PrevFrameDynamicData->Transforms[InstanceIndex];

				DstPackedFrameIndex[VisIdx * 2 + 0] = OverrideAnimFrameIndex(DynamicData->FrameIndices[InstanceIndex]);
				DstPackedFrameIndex[VisIdx * 2 + 1] = OverrideAnimFrameIndex(PrevFrameDynamicData->FrameIndices[InstanceIndex]);

				//#TODO can we cache InvScale as InstanceData when SetInstanceScale happens ? most of the times Scale are set once and not changed every frame
				//FVector4f InvScaleAndDeterminant = FVector4f(GetMatrixInvScaleVector(InstanceTransform), InstanceTransform.RotDeterminant() < 0.0f ? -1 : 1);
				//FFloat16Color PackedInvScale(*reinterpret_cast<FLinearColor*>(&InvScaleAndDeterminant));
				//check(FMath::IsNearlyEqual(PackedInvScale.GetFloats().A, InvScaleAndDeterminant.W, 0.01f));
				//DstInvNonUniformScaleAndDeterminantSign[VisIdx] = InvScaleAndDeterminant;
				//new (DstInvNonUniformScaleAndDeterminantSign + VisIdx) FFloat16Color(*reinterpret_cast<FLinearColor*>(&InvScaleAndDeterminant));
				
				//DstInvNonUniformScale[VisIdx] = VisibleInstances_InvScale[VisIdx];

				//#TODO optimize
				for (uint32 FloatIndex = 0; FloatIndex < NumCustomDataFloats; FloatIndex++)
				{
					DstCustomDatas[VisIdx * NumCustomDataFloats + FloatIndex] = DynamicData->CustomData[InstanceIndex * NumCustomDataFloats + FloatIndex];
				}

			}

			InstanceBuffer->UnlockBuffers(Collector->GetRHICommandList());
			if (CIDBuffer)
				CIDBuffer->UnlockBuffers(Collector->GetRHICommandList());
		}
	}
	//////////////////////////////////////////////////////////////////////////
	void FillShadowBuffers()
	{
		const FSkelotDynamicData* DynamicData = this->Proxy->DynamicData;

		SkelotShaderMatrixT* RESTRICT DstInstanceTransform = this->InstanceBuffer->MappedTransforms;
		uint32* RESTRICT DstFrameIndex = this->InstanceBuffer->MappedFrameIndices;
		check(IsAligned(DstInstanceTransform, 16));

		float* RESTRICT DstCustomDatas = nullptr;
		if (this->CIDBuffer) //could be null for shadow pass
		{
			DstCustomDatas = this->CIDBuffer->MappedData;
		}
		const uint32 NumCustomDataFloats = DstCustomDatas ? Proxy->NumCustomDataFloats : 0;

		//for each visible instance, copy their data to the buffer
		for (uint32 VisIdx = 0; VisIdx < NumVisibleInstance; VisIdx++)
		{
			uint32 InstanceIndex = this->VisibleInstances[VisIdx];
			check(InstanceIndex < DynamicData->InstanceCount);

			DstInstanceTransform[VisIdx] = DynamicData->Transforms[InstanceIndex];
			DstFrameIndex[VisIdx] = OverrideAnimFrameIndex(DynamicData->FrameIndices[InstanceIndex]);

			for (uint32 FloatIndex = 0; FloatIndex < NumCustomDataFloats; FloatIndex++)
			{
				DstCustomDatas[VisIdx * NumCustomDataFloats + FloatIndex] = DynamicData->CustomData[InstanceIndex * NumCustomDataFloats + FloatIndex];
			}
		}

		this->InstanceBuffer->UnlockBuffers(Collector->GetRHICommandList());
		if (this->CIDBuffer)
			this->CIDBuffer->UnlockBuffers(Collector->GetRHICommandList());
	}
	//////////////////////////////////////////////////////////////////////////
	template<typename TVisIndex> void FillElementsBuffer()
	{
		void* MappedElementVB = this->ElementIndexBuffer->MappedData;

		for(int GroupIndex = 0; GroupIndex < 2; GroupIndex++)
		{
			for (uint32 MeshIdx = 0; MeshIdx < this->NumSubMesh; MeshIdx++)
			{
				if (!this->SubMeshes_Data[GroupIndex][MeshIdx].bHasAnyLOD)
					continue;

				for (uint32 LODIndex = 0; LODIndex < SKELOT_MAX_LOD; LODIndex++)
				{
					const FLODData& LODData = this->SubMeshes_Data[GroupIndex][MeshIdx].LODs[LODIndex];
					TVisIndex* DstMem = reinterpret_cast<TVisIndex*>(MappedElementVB) + LODData.InstanceOffset;
					LODData.IndexCollector.template CopyToArray<TVisIndex>(DstMem);
				}
			}
		};

		this->ElementIndexBuffer->UnlockBuffers(Collector->GetRHICommandList());
	}
	//////////////////////////////////////////////////////////////////////////
	void InitLODData()
	{
		const float DistanceFactor = (1.0f / (GSkelot_DistanceScale * Proxy->DistanceScale));


		//init instance data
		{
			if (Proxy->InstanceMinDrawDistance > 0)
			{
				float MinDD = FMath::Square(Proxy->InstanceMinDrawDistance * DistanceFactor);
				this->MinDrawDist = *reinterpret_cast<uint32*>(&MinDD);
			}
			if(Proxy->InstanceMaxDrawDistance > 0)
			{
				float MaxDD = FMath::Square(Proxy->InstanceMaxDrawDistance * DistanceFactor);
				this->MaxDrawDist = *reinterpret_cast<uint32*>(&MaxDD);
			}

			for (int LODIndex = 0; LODIndex < SKELOT_MAX_LOD - 1; LODIndex++)
			{
				//we use our own global variables instead :|
				//const FCachedSystemScalabilityCVars& CVars = GetCachedScalabilityCVars();
				//CVars.ViewDistanceScale
				//float LDF = View->LODDistanceFactor;
				//float VS = CVars.ViewDistanceScale;

				float LODDrawDist = FMath::Square(Proxy->LODDistances[LODIndex] * DistanceFactor);
				LODDrawDistances[LODIndex] = *reinterpret_cast<uint32*>(&LODDrawDist);
			};
		}


		LODDrawDistances[SKELOT_MAX_LOD - 1] = ~0u;

		//init sub mesh data
		for (uint32 SubMeshIdx = 0; SubMeshIdx < NumSubMesh; SubMeshIdx++)
		{
			FSubMeshInfo& GenSubMesh = this->SubMeshes_Info[SubMeshIdx];
			const FProxyMeshData& ProxySubMesh = this->Proxy->SubMeshes[SubMeshIdx];

			GenSubMesh.bIsValid = ProxySubMesh.SkeletalRenderData != nullptr;
			if(!GenSubMesh.bIsValid)
				continue;

			if (ProxySubMesh.MaxDrawDistance > 0)
			{
				float flt = FMath::Square(ProxySubMesh.MaxDrawDistance * DistanceFactor);
				GenSubMesh.MaxDrawDist = *reinterpret_cast<uint32*>(&flt);
			}

			if (ProxySubMesh.OverrideDistance > 0)
			{
				float flt = FMath::Square(ProxySubMesh.OverrideDistance * DistanceFactor);
				GenSubMesh.OverrideDist = *reinterpret_cast<uint32*>(&flt);
				GenSubMesh.OverrideMeshIdx = ProxySubMesh.OverrideMeshIndex;
				check(GenSubMesh.OverrideMeshIdx != 0xFF);
			}

			
			const uint8 MinPossibleLOD = FMath::Max(ProxySubMesh.SkeletalRenderData->CurrentFirstLODIdx, ProxySubMesh.MinLODIndex);
			const uint8 MaxPossibleLOD = static_cast<uint8>(ProxySubMesh.SkeletalRenderData->LODRenderData.Num()) - 1;
			check(MaxPossibleLOD <= MaxPossibleLOD);

			uint8 MinLOD = MinPossibleLOD;
			uint8 MaxLOD = MaxPossibleLOD;

			if (GSkelot_ForceLOD >= 0)
			{
				MinLOD = MaxLOD = FMath::Clamp((uint8)GSkelot_ForceLOD, MinPossibleLOD, MaxPossibleLOD);
			}

			if (bShaddowCollector && GSkelot_ShadowForceLOD >= 0)	//GSkelot_ShadowForceLOD overrides GSkelot_ForceLOD if its collecting for shadow
			{
				MinLOD = MaxLOD = FMath::Clamp((uint8)GSkelot_ShadowForceLOD, MinPossibleLOD, MaxPossibleLOD);
			}
			
			for (uint8 LODIndex = 0; LODIndex < static_cast<uint8>(SKELOT_MAX_LOD); LODIndex++)
			{
				const uint8 MeshLODBias = bShaddowCollector ? (LODIndex >= this->Proxy->StartShadowLODBias ? this->Proxy->ShadowLODBias : 0) : 0;
				GenSubMesh.LODRemap[LODIndex] = FMath::Clamp(LODIndex + MeshLODBias, MinLOD, MaxLOD);
			}

		}


		ResolvedViewLocation = (FVector3f)View->CullingOrigin.GridSnap(16);	//OverrideLODViewOrigin ?
	}
	//////////////////////////////////////////////////////////////////////////
	void GenerateData()
	{
		InitLODData();

		const uint32 TotalInstances = Proxy->DynamicData->AliveInstanceCount;
		check(TotalInstances > 0);

		{
			const uint32 ATI = Align(TotalInstances, DISTANCING_NUM_FLOAT_PER_REG);
			this->VisibleInstances = MempoolAlloc<uint32>(ATI);
		}

		if (bShaddowCollector)	//is it collecting for shadow ?
		{
			//SCOPE_CYCLE_COUNTER(STAT_SKELOT_ShadowCullTime);
			this->EditedViewFrustum = View->GetDynamicMeshElementsShadowCullFrustum();
			const bool bShadowNeedTranslation = !View->GetPreShadowTranslation().IsZero();
			if (bShadowNeedTranslation)
			{
				auto NewPlanes = View->GetDynamicMeshElementsShadowCullFrustum()->Planes;
				//apply offset to the planes
				for (FPlane& P : NewPlanes)
					P.W = (P.GetOrigin() - View->GetPreShadowTranslation()) | P.GetNormal();

				//regenerate Permuted planes
				this->EditedViewFrustum = new (MempoolAlloc<FConvexVolume>(1)) FConvexVolume(NewPlanes);
			}

			Cull();

			INC_DWORD_STAT_BY(STAT_SKELOT_ShadowNumCulled, TotalInstances - this->NumVisibleInstance);
		}
		else
		{
			//SCOPE_CYCLE_COUNTER(STAT_SKELOT_CullTime);
			EditedViewFrustum = &View->CullingFrustum;
			Cull();
			INC_DWORD_STAT_BY(STAT_SKELOT_ViewNumCulled, TotalInstances - this->NumVisibleInstance);
		}

		if (this->TotalElementCount == 0)
			return;


		if (bShaddowCollector)
		{
			INC_DWORD_STAT_BY(STAT_SKELOT_ShadowNumVisible, this->NumVisibleInstance);
		}
		else
		{
			INC_DWORD_STAT_BY(STAT_SKELOT_ViewNumVisible, this->NumVisibleInstance);
		}

		//allocate vertex buffers
		//#Note because PreRenderDelegateEx is called before InitShadow we can't share a global vertex buffer for all proxies :(
		{
			FRHICommandListBase& RHICmdList = this->Collector->GetRHICommandList();

			this->ElementIndexBuffer = GSkelotElementIndexBufferPool.Alloc(RHICmdList, this->TotalElementCount * this->GetElementIndexSize());
			this->ElementIndexBuffer->LockBuffers(RHICmdList);

			//need custom per instance float ?
			if (Proxy->NumCustomDataFloats > 0 && (!bShaddowCollector || Proxy->bNeedCustomDataForShadowPass))
			{
				this->CIDBuffer = GSkelotCIDBufferPool.Alloc(RHICmdList , NumVisibleInstance * Proxy->NumCustomDataFloats);
				this->CIDBuffer->LockBuffers(RHICmdList);
			}

			this->InstanceBuffer = GSkelotInstanceBufferPool.Alloc(RHICmdList , this->NumVisibleInstance * (bShaddowCollector ? 1 : 2));	//#Note shadow pass doesn't need prev frame data
			this->InstanceBuffer->LockBuffers(RHICmdList);
		}

		//fill mapped buffers
		{
			SKELOT_SCOPE_CYCLE_COUNTER(BufferFilling);

			if (bShaddowCollector)
				FillShadowBuffers();
			else
				FillBuffers();

			if (Use32BitElementIndex())
				FillElementsBuffer<uint32>();
			else
				FillElementsBuffer<uint16>();
		}

	}
	//////////////////////////////////////////////////////////////////////////
	void GenerateBatches()
	{

		SKELOT_SCOPE_CYCLE_COUNTER(GenerateBatches);

		for(int32 GroupIndex = 0; GroupIndex < 2; GroupIndex++)
		{
			const bool bDeterminantNegative = GroupIndex == 1;

			for (uint32 SubMeshIdx = 0; SubMeshIdx < this->NumSubMesh; SubMeshIdx++)
			{
				const FSubMeshData& SubMesh = this->SubMeshes_Data[GroupIndex][SubMeshIdx];
				if (!SubMesh.bHasAnyLOD)
					continue;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				this->bWireframe = AllowDebugViewmodes() && this->ViewFamily->EngineShowFlags.Wireframe;
				if (this->bWireframe)
				{
					FLinearColor MaterialColor = FLinearColor(SubMeshIdx / static_cast<float>(this->NumSubMesh), bDeterminantNegative ? 0.1f : 0.5f, 1.f);
					this->WireframeMaterialInstance = new FColoredMaterialRenderProxy(GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL, MaterialColor);
					this->Collector->RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
				}
#endif
				//better to add opaque meshes from near to far ? :thinking:
				for (uint32 LODIndex = 0; LODIndex < SKELOT_MAX_LOD; LODIndex++)
				{
					if (SubMesh.LODs[LODIndex].NumInstance == 0)
						continue;

					if (!this->Proxy->SubMeshes[SubMeshIdx].LODs[LODIndex].bHasAnyTranslucentMaterial)
						GenerateLODBatch(SubMeshIdx, LODIndex, false, bDeterminantNegative);
				}
				//translucent need to be added from far to near
				for (uint32 LODIndex = SKELOT_MAX_LOD; LODIndex--; )
				{
					if (SubMesh.LODs[LODIndex].NumInstance == 0)
						continue;

					if (this->Proxy->SubMeshes[SubMeshIdx].LODs[LODIndex].bHasAnyTranslucentMaterial)
						GenerateLODBatch(SubMeshIdx, LODIndex, true, bDeterminantNegative);
				}
			}
		};
	};
	//////////////////////////////////////////////////////////////////////////
	void GenerateLODBatch(uint32 SubMeshIdx, uint32 LODIndex, bool bAnyTranslucentMaterial, bool bDeterminantNegative)
	{
		const FProxyMeshData& ProxyMD = this->Proxy->SubMeshes[SubMeshIdx];
		const FSubMeshData& SubMeshData = this->SubMeshes_Data[bDeterminantNegative][SubMeshIdx];
		const FSkeletalMeshLODRenderData& SkelLODData = ProxyMD.SkeletalRenderData->LODRenderData[LODIndex];
		const FProxyLODData& ProxyLODData = ProxyMD.LODs[LODIndex];
		FSkelotVertexFactoryBufferRef UniformBuffer = this->CreateUniformBuffer(SubMeshData.LODs[LODIndex], bDeterminantNegative);

		FSkelotBatchElementOFR* LastOFRS[(int)ESkelotVerteFactoryMode::EVF_Max] = {};

		//when all materials are the same, one FMeshBatch can be used for all passes, we draw them with MaxBoneInfluence of the LOD (will result the same for depth/material pass)
		//same MaxBoneInfleunce must be used for depth and material pass, different value causes depth mismatch, because of floating point precision, denorm-flush, ...
		
		//unify as whole if all materials are same, or just try merge depth batches if possible.

		const bool bTryUnifySections = GSkelot_DisableSectionsUnification == false;
		const bool bIdenticalMaterials = bTryUnifySections && ProxyLODData.bSameMaterials && ProxyLODData.bSameCastShadow && SkelLODData.RenderSections.Num() > 1 /*&& ProxyLODData.bSameMaxBoneInfluence*/;
		const int NumSection = bIdenticalMaterials ? 1 : SkelLODData.RenderSections.Num();
		const bool bUseUnifiedMeshForDepth = bTryUnifySections && NumSection > 1 && ProxyLODData.bMeshUnificationApplicable && ProxyLODData.bSameCastShadow /*&& ProxyLODData.bSameMaxBoneInfluence*/;
		for (int32 SectionIndex = 0; SectionIndex < NumSection; SectionIndex++) //for each section
		{
			const FSkelMeshRenderSection& SectionInfo = SkelLODData.RenderSections[SectionIndex];
			
			//int SolvedMaterialSection = SectionInfo.MaterialIndex;
			//const FSkeletalMeshLODInfo* SKMLODInfo = ProxyMD.SkeletalMesh->GetLODInfo(LODIndex);
			//if (SKMLODInfo && SKMLODInfo->LODMaterialMap.IsValidIndex(SectionInfo.MaterialIndex))
			//{
			//	SolvedMaterialSection = SKMLODInfo->LODMaterialMap[SectionInfo.MaterialIndex];
			//}
			const uint16 MaterialIndex = ProxyLODData.SectionsMaterialIndices[SectionIndex];
			FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : Proxy->MaterialsProxy[MaterialIndex];

			const uint32 MaxBoneInfluence = OverrideMaxBoneInfluence((bIdenticalMaterials || bUseUnifiedMeshForDepth) ? ProxyLODData.SectionsMaxBoneInfluence : SectionInfo.MaxBoneInfluences);
			check(MaxBoneInfluence > 0 && MaxBoneInfluence <= FSkelotMeshDataEx::MAX_INFLUENCE);

			const ESkelotVerteFactoryMode VFMode = GetTargetVFMode(MaxBoneInfluence);
			FSkelotBatchElementOFR*& BatchUserData = LastOFRS[VFMode];	//FSkelotBatchElementOFR with same MaxBoneInfluence can be shared for sections
			if (!BatchUserData)
			{
				BatchUserData = &Collector->AllocateOneFrameResource<FSkelotBatchElementOFR>();
				//initialize OFR
				BatchUserData->MaxBoneInfluences = MaxBoneInfluence;
				BatchUserData->VertexFactory = ProxyMD.MeshDataEx->LODs[LODIndex - ProxyMD.MeshDefBaseLOD].GetVertexFactory(MaxBoneInfluence);
				BatchUserData->UniformBuffer = UniformBuffer;
			}

			// Draw the mesh.
			FMeshBatch& Mesh = AllocateMeshBatch(SkelLODData, SubMeshIdx, LODIndex, SectionIndex, BatchUserData, bDeterminantNegative);
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			Mesh.bWireframe = bWireframe;
			Mesh.MaterialRenderProxy = MaterialProxy;

			if (bUseUnifiedMeshForDepth)
			{
				Mesh.bUseAsOccluder = Mesh.bUseForDepthPass = Mesh.CastShadow = false; //this batch must be material only
			}
			else
			{
				Mesh.bUseForDepthPass = Proxy->ShouldRenderInDepthPass();
				Mesh.bUseAsOccluder = Proxy->ShouldUseAsOccluder();
				Mesh.CastShadow = SectionInfo.bCastShadow;
			}

			if (bIdenticalMaterials)
			{
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = OverrideNumPrimitive(ProxyLODData.SectionsNumTriangle);
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = SkelLODData.GetNumVertices() - 1; //SectionInfo.GetVertexBufferIndex() + SectionInfo.GetNumVertices() - 1; //
			}
			else
			{
				BatchElement.FirstIndex = SectionInfo.BaseIndex;
				BatchElement.NumPrimitives = OverrideNumPrimitive(SectionInfo.NumTriangles);
				BatchElement.MinVertexIndex = SectionInfo.BaseVertexIndex;
				BatchElement.MaxVertexIndex = SkelLODData.GetNumVertices() - 1; //SectionInfo.GetVertexBufferIndex() + SectionInfo.GetNumVertices() - 1; //

			}

			Collector->AddMesh(ViewIndex, Mesh);
		}

		if (bUseUnifiedMeshForDepth)
		{
			const uint32 MaxBoneInfluence = OverrideMaxBoneInfluence(ProxyLODData.SectionsMaxBoneInfluence);
			const ESkelotVerteFactoryMode VFMode = GetTargetVFMode(MaxBoneInfluence);
			FSkelotBatchElementOFR*& BatchUserData = LastOFRS[VFMode];
			check(BatchUserData);

			FMeshBatch& Mesh = AllocateMeshBatch(SkelLODData, SubMeshIdx, LODIndex, 0, BatchUserData, bDeterminantNegative);
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			Mesh.bUseForMaterial = false;
			Mesh.bUseForDepthPass = Proxy->ShouldRenderInDepthPass();
			Mesh.bUseAsOccluder = Proxy->ShouldUseAsOccluder();
			Mesh.CastShadow = ProxyLODData.bAllSectionsCastShadow;
			Mesh.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = OverrideNumPrimitive(ProxyLODData.SectionsNumTriangle);
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = SkelLODData.GetNumVertices() - 1;

			Collector->AddMesh(ViewIndex, Mesh);
		}
	}
};



