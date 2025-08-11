/*
 * ░▒▓ APPARATUS ▓▒░
 * 
 * File: UnrealMemory.h
 * Created: 2021-10-27 16:14:36
 * Author: Vladislav Dmitrievich Turbanov (vladislav@turbanov.ru)
 * ───────────────────────────────────────────────────────────────────
 * 
 * The Apparatus source code is for your internal usage only.
 * Redistribution of this file is strictly prohibited.
 * 
 * Community forums: https://talk.turbanov.ru
 * 
 * Copyright 2019 - 2023, SP Vladislav Dmitrievich Turbanov
 * Made in Russia, Moscow City, Chekhov City ♡
 */
/**
 * @file 
 * @brief Additional utilities for Unreal Engine's memory handling.
 */

#pragma once

#include <unordered_set>

#include "HAL/UnrealMemory.h"
#include "Misc/ScopeLock.h"


#define DEBUG_APPARATUS_MEMORY 0

/**
 * Additional utilities for Unreal Engine's memory handling.
 */
struct APPARATUSRUNTIME_API FMoreMemory
{
  private:

	FMoreMemory()
	{}

#if DEBUG_APPARATUS_MEMORY

	static FCriticalSection AllocationsCS;
	static std::unordered_set<void*> Allocations;
	static constexpr int DebugAllocationSourceOffset = 48;

	static void*
	MakeDebugAddress(void*  OrigMemory,
					 uint16 DebugAllocationSource)
	{
		return (void*)(((uint64)DebugAllocationSource << DebugAllocationSourceOffset) | (uint64)OrigMemory);
	}

#endif // DEBUG_APPARATUS_MEMORY

  public:

	/**
	 * A more secure technique to allocate memory.
	 * 
	 * @param OutMemory The resulting allocated memory receiver.
	 * Must be a @c null pointer.
	 * @param Count The number of bytes to allocate.
	 * @param Alignment The alignment of the allocation.
	 * @param DebugAllocationSource A tag used for debugging the allocation.
	 */
	static void
	Allocate(void*& OutMemory, SIZE_T Count, uint32 Alignment = 0U, uint16 DebugAllocationSource = 0U)
	{
		check(OutMemory == nullptr);
		OutMemory = FMemory::Malloc(Count, Alignment);
#if DEBUG_APPARATUS_MEMORY
		{
			FScopeLock Lock(&AllocationsCS);
			Allocations.insert(MakeDebugAddress(OutMemory, DebugAllocationSource));
		}
#endif
	}

	/**
	 * A more secure technique to reallocate memory.
	 * 
	 * @param InOutMemory The memory to re-allocate and receive the result.
	 * @param Count The number of bytes to allocate anew.
	 * @param Alignment The alignment of the allocation.
	 * @param DebugAllocationSource A tag used for debugging the allocation.
	 */
	static void
	Reallocate(void*& InOutMemory, SIZE_T Count, uint32 Alignment = 0U, uint16 DebugAllocationSource = 0U)
	{
#if DEBUG_APPARATUS_MEMORY
		const auto OrigMemory = InOutMemory;
#endif
		InOutMemory = FMemory::Realloc(InOutMemory, Count, Alignment);
#if DEBUG_APPARATUS_MEMORY
		{
			FScopeLock Lock(&AllocationsCS);
			Allocations.erase(MakeDebugAddress(OrigMemory, DebugAllocationSource));
			Allocations.insert(MakeDebugAddress(InOutMemory, DebugAllocationSource));
		}
#endif
	}

	/**
	 * Free an allocated memory in a safe way.
	 * 
	 * @tparam T The pointed type to free.
	 * @param Original The memory to free.
	 * @param DebugAllocationSource The debugging tag.
	 */
	template < typename T >
	static void
	Free(T*& Original, uint16 DebugAllocationSource = 0U)
	{
		if (LIKELY(Original != nullptr))
		{
			FMemory::Free(Original);
#if DEBUG_APPARATUS_MEMORY
			{
				FScopeLock Lock(&AllocationsCS);
				Allocations.erase(MakeDebugAddress(Original, DebugAllocationSource));
			}
#endif
			Original = nullptr;
		}
	}

	/**
	 * Reallocate an array in a safe way.
	 */
	static void
	ReallocateArray(void*& InOutMemory,
					int32  ElementsCount,
					int32  ElementSize,
					uint32 Alignment = 0U,
					uint16 DebugAllocationSource = 0U)
	{
		Reallocate(InOutMemory, (SIZE_T)ElementsCount * (SIZE_T)ElementSize, Alignment, DebugAllocationSource);
	}

	/**
	 * Reallocate an array of pointers in a safe way.
	 */
	template < typename T >
	static void
	ReallocateArray(T**&   InOutMemory,
					int32& InOutCurrentElementsCount,
					int32  ElementsCount,
					int32  ElementSize,
					uint32 Alignment = 0U,
					uint16 DebugAllocationSource = 0U)
	{
		const auto TotalSize = (SIZE_T)ElementsCount * (SIZE_T)ElementSize;
		Reallocate((void*&)InOutMemory, TotalSize, Alignment, DebugAllocationSource);
		if (ElementsCount > InOutCurrentElementsCount)
		{
			const auto ZeroOffset = (SIZE_T)ElementsCount * (SIZE_T)InOutCurrentElementsCount;
			FMemory::Memzero((void*)((uint8*)InOutMemory + ZeroOffset),
							 TotalSize - ZeroOffset);
		}
		InOutCurrentElementsCount = ElementsCount;
	}

	/**
	 * A utility function to allocate an array of elements in a safe way.
	 */
	template < typename T >
	static void
	AllocateArray(T*&    OutMemory,
				  int32  ElementsCount,
				  int32  ElementSize,
				  uint32 Alignment = 0U,
				  uint16 DebugAllocationSource = 0U)
	{
		Allocate(OutMemory, (SIZE_T)ElementsCount * (SIZE_T)ElementSize, Alignment, DebugAllocationSource);
	}

	/**
	 * Allocate an array of pointers in a safe way.
	 */
	template < typename T >
	static void
	AllocateArray(T**&   OutMemory,
				  int32  ElementsCount,
				  int32  ElementSize,
				  uint32 Alignment = 0U,
				  uint16 DebugAllocationSource = 0U)
	{
		const auto TotalSize = (SIZE_T)ElementsCount * (SIZE_T)ElementSize;
		Allocate((void*&)OutMemory, TotalSize, Alignment, DebugAllocationSource);
		FMemory::Memzero((void*)OutMemory, TotalSize);
	}

	/**
	 * Type-safe memory swapping.
	 */
	template < typename T >
	static void
	Memswap(T& RefA, T& RefB)
	{
		FMemory::Memswap(static_cast<void*>(&RefA),
						 static_cast<void*>(&RefB),
						 sizeof(T));
	}

	/**
	 * Type-safe memory comparison.
	 */
	template < typename T >
	static int32
	Memcmp(const T& RefA, const T& RefB)
	{
		return FMemory::Memcmp(static_cast<const void*>(&RefA),
							   static_cast<const void*>(&RefB),
							   sizeof(T));
	}

}; //-struct FMoreMemory
