// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "Containers/SparseArray.h"
#include "Containers/StaticArray.h"
#include "AntHandle.generated.h"

/** TSparseArray Serializer in its original form. */
template<typename ElementType, typename Allocator>
FArchive &SparseSerialize(FArchive &Ar, TSparseArray<ElementType, Allocator> &Array)
{
	Array.CountBytes(Ar);
	if (Ar.IsLoading())
	{
		// Load array.
		int32 maxIndex = 0;
		Ar << maxIndex;
		Array.Empty(maxIndex);
		for (int32 idx = 0; idx < maxIndex; ++idx)
		{
			bool isValidIdx = false;
			Ar << isValidIdx;
			if (isValidIdx)
				Ar << *::new(Array.InsertUninitialized(idx)) ElementType;
		}
	}
	else
	{
		// Save array.
		int32 maxIndex = Array.GetMaxIndex();
		Ar << maxIndex;
		for (int32 idx = 0; idx < maxIndex; ++idx)
		{
			auto isValidIdx = Array.IsValidIndex(idx);
			Ar << isValidIdx;
			if (isValidIdx)
				Ar << Array[idx];
		}
	}
	return Ar;
}

USTRUCT(BlueprintType)
struct ANT_API FAntHandle
{
	GENERATED_BODY()

	/** Ctor (invalid handle). */
	FAntHandle() :
		Idx(INDEX_NONE), Ver(0), Type(0)
	{}

	/** Ctor. */
	FAntHandle(int32 Index, uint8 Version, uint8 Type) :
		Idx(Index), Ver(Version), Type(Type)
	{}

	/** Copy Ctor. */
	FAntHandle(const FAntHandle &CopyRef) :
		Idx(CopyRef.Idx), Ver(CopyRef.Ver), Type(CopyRef.Type)
	{}

	/** Actual underlyieng index. */
	const int32 Idx;

	/** Version. */
	const uint16 Ver;

	/** Type. */
	const uint8 Type;

	FAntHandle& operator=(const FAntHandle& RHS)
	{
		const_cast<int32 &>(Idx) = RHS.Idx;
		const_cast<uint16 &>(Ver) = RHS.Ver;
		const_cast<uint8 &>(Type) = RHS.Type;
		return *this;
	}

	bool operator==(const FAntHandle& RHS) const
	{
		return (RHS.Idx == Idx && RHS.Ver == Ver);
	}

	bool operator!=(const FAntHandle& RHS) const
	{
		return (RHS.Idx != Idx || RHS.Ver != Ver);
	}

	friend FArchive& operator<<(FArchive& Ar, FAntHandle& Obj)
	{
		Ar << const_cast<int32 &>(Obj.Idx);
		Ar << const_cast<uint16 &>(Obj.Ver);
		Ar << const_cast<uint8 &>(Obj.Type);
		return Ar;
	}

	static uint16 GetUniqueVersion()
	{
		static uint16 ver;
		return ver++;
	}
};

template <uint8 NumSlots, uint8 TYPE>
struct ANT_API FAntIndexer 
{
	static_assert(NumSlots > 0, "NumSlots must be greater than 0");
	
	/** Add a fresh new index storage. */
	FORCEINLINE FAntHandle Add()
	{
		// 
		FAntHandle index(IndexList.Add({}), FAntHandle::GetUniqueVersion(), TYPE);
		
		// initialize with invalid index
		for (auto &it : IndexList[index.Idx])
			it = INDEX_NONE;

		// set version at the last room
		IndexList[index.Idx][NumSlots] = index.Ver;

		// 
		return index;
	}

	/** Add a fresh new index storage at the given offset. */
	FORCEINLINE FAntHandle AddAt(int32 Offset)
	{
		check(!IndexList.IsValidIndex(Offset) && "The given offset is already in use!");

		// insert at the given index
		IndexList.Insert(Offset, {});

		// initialize with invalid index
		for (auto &it : IndexList[Offset])
			it = INDEX_NONE;

		// set version at the last room
		IndexList[Offset][NumSlots] = FAntHandle::GetUniqueVersion();

		//
		return FAntHandle(Offset, IndexList[Offset][NumSlots], TYPE);
	}

	FORCEINLINE void Remove(FAntHandle Handle) 
	{
		check(IsValid(Handle) && "Given Handle is invalid!");

		IndexList.RemoveAt(Handle.Idx);
	}

	FORCEINLINE FAntHandle GetHandleAt(int32 Offset) const
	{
		check(IndexList.IsValidIndex(Offset) && "The given offset is invalid!");

		//
		return FAntHandle(Offset, IndexList[Offset][NumSlots], TYPE);
	}

	/** Set a value at the given slot. */
	FORCEINLINE void Set(FAntHandle Handle, uint8 SlotIndex, int32 SlotValue) 
	{
		check(IsValid(Handle) && "Given Handle is invalid!");
		check(SlotIndex < NumSlots && "Given SlotIndex is out of bound");

		IndexList[Handle.Idx][SlotIndex] = SlotValue;
	}

	/** Set a value at the given free slot. */
	FORCEINLINE void SetChecked(FAntHandle Handle, uint8 SlotIndex, int32 SlotValue) 
	{
		check(IsValid(Handle) && "Given Handle is invalid!");
		check(SlotIndex < NumSlots && "Given SlotIndex is out of bound");
		check(IndexList[Handle.Idx][SlotIndex] == INDEX_NONE && "Given SlotIndex is already in use!");

		IndexList[Handle.Idx][SlotIndex] = SlotValue;
	}

	/** Get slot value at the given slot index. */
	FORCEINLINE int32 Get(FAntHandle Handle, uint8 SlotIndex) const
	{
		check(IsValid(Handle) && "Given Handle is invalid!");
		check(SlotIndex < NumSlots && "Given SlotIndex is out of bound");

		return IndexList[Handle.Idx][SlotIndex];
	}

	FORCEINLINE bool IsValid(FAntHandle Handle) const
	{
		return (IndexList.IsValidIndex(Handle.Idx) && IndexList[Handle.Idx][NumSlots] == Handle.Ver && Handle.Type == TYPE);
	}

	friend FArchive &operator<<(FArchive &Ar, FAntIndexer<NumSlots, TYPE> &Obj)
	{
		SparseSerialize(Ar, Obj.IndexList);
		return Ar;
	}

private:
	/** Last slot is reserved for version index. */
	TSparseArray<TStaticArray<int32, NumSlots + 1>> IndexList;
};
