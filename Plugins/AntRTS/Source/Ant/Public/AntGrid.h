// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "AntHandle.h"
#include "AntMath.h"
#include "Math/Box2D.h"

/** Contact data gathered by queries */
struct ANT_API FAntContactInfo
{
	int32 Flag = 0;
	FAntHandle Handle;
	uint32 InstanceID = 0;
	float SqDist = 0.0f;

	// ignore this contact on all FContactInfo::* functions
	bool bIgnored = false;

	struct { FVector3f Base; float Radius = 0.0f; float Height = 0.0f; } Cylinder;

	bool operator<(const FAntContactInfo& r) const
	{
		// in case of multiple instance of the same owner, we have to take id into account
		return Handle.Idx < r.Handle.Idx;
	}

	bool operator==(const FAntContactInfo& r) const
	{
		// in case of multiple instance of the same owner, we have to take id into account
		return (Handle.Idx == r.Handle.Idx && InstanceID == r.InstanceID);
	}

	/** find closest intersection */
	static bool FindClosestCollision(const FAntRay &Ray, float MaxLength, float Radius, const TArray<FAntContactInfo>& Contacts, FVector2f &OutNoCollidPos);
};

/**
 * Spatial grid with lock-free query functions.
*/
class ANT_API FAntGrid
{
public:
	FAntGrid() = default;
	~FAntGrid();

	/** Reset grid size */
	void Reset(uint32 CellNumber, float CellSize, float ShiftSize);

	/** Add a Cylinder on the grid.
	 * @return Return Grid handle.
	*/
	int32 AddCylinder(const FAntHandle &Handle, const FVector3f &Base, float Radius, float Height, int32 Flags);

	/** Update flag */
	void UpdateFlag(int32 GridHandle, int32 NewFlags);

	/** Update height */
	void UpdateHeight(int32 GridHandle, float NewHeight);

	/** Remove an object from the grid */
	void Remove(int32 GridHandle);

	/** Thread safe and lock-free */
	void QueryCylinder(const FVector3f &Base, float Radius, float Height, int32 Flags, bool MustIncludeCenter, TArray<FAntContactInfo> &OutCollided, const TArray<FAntHandle> *IgnoreList = nullptr) const;

	/** Thread safe and lock-free */
	void QueryRay(const FVector3f &Start, const FVector3f &End, int32 Flags, TArray<FAntContactInfo> &OutCollided, const TArray<FAntHandle> *IgnoreList = nullptr) const;

	/** Thread safe and lock-free */
	void QueryConvexVolume(const TStaticArray<FVector, 4> &NearPlane, const TStaticArray<FVector, 4> &FarPlane, int32 Flags, TArray<FAntContactInfo> &OutCollided, const TArray<FAntHandle> *IgnoreList = nullptr) const;

	/** Get last version. each non-const operation will increase the version. */
	FORCEINLINE unsigned int GetVersion() const { return Version; }

	/** Get number of the objects inside the grid. */
	FORCEINLINE unsigned int GetCount() const { return Count; }

private:
	/** Internal grid node */
	struct FGridNode
	{
		int32 PrevIdx = INDEX_NONE;
		int32 NextIdx = INDEX_NONE;
		int32 DataIdx = INDEX_NONE;
		int32 NextListIdx = INDEX_NONE;
		int32 CellIdx = INDEX_NONE;
	};

	/** Internal grid node date */
	struct FNodeData
	{
		int32 Flag = 0;
		uint32 InstanceID = 0;
		FAntHandle Handle;
		uint8 bMultiCell : 1;
		struct { FVector3f Base; float Radius; float height; } Cylinder;
	};

	/** Highest level of the grid. */
	TArray<int32> Grid;

	/** Grid sub-level nodes. */
	TSparseArray<FGridNode> GridNodes; 

	/** Shared node data. */
	TSparseArray<FNodeData> GridData;

	/** Number of the cells inside the grid. */
	int32 CellNumber = 0;

	/** Size of the cells inside the grid. */
	float CellSize = 0.0f;

	/** Shift size to adjust objects inside the 2d array. */
	float ShiftSize = 0.0f;

	/** current version of the grid. */
	uint32 Version = 0;

	/** Number of the objects inside the grid. */
	uint32 Count = 0;
};
