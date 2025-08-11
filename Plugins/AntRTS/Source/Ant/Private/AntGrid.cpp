// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "AntGrid.h"
#include "AntModule.h"
#include "AntUtil.h"
#include "ConvexVolume.h"
#include <limits>

#define IND_2D_TO_1D(x, y) ((y) * CellNumber) + (x)

bool FAntContactInfo::FindClosestCollision(const FAntRay &Ray, float MaxLength, float Radius, const TArray<FAntContactInfo> &Contacts, FVector2f &OutNoCollidPos)
{
	bool collided = false;
	float clossestTime = MaxLength;
	OutNoCollidPos = Ray.Start + Ray.Dir * MaxLength;
	for (const auto& it : Contacts)
	{
		// skip ignored contacts
		if (it.bIgnored)
			continue;

		float timeOfImpact = 0;
		if (FAntMath::CircleRay(FVector2f(it.Cylinder.Base), it.Cylinder.Radius + Radius, Ray, timeOfImpact) && timeOfImpact <= MaxLength)
		{
			collided = true;
			clossestTime = FMath::Min(timeOfImpact, clossestTime);
		}
	}

	OutNoCollidPos = collided ? Ray.Start + Ray.Dir * clossestTime : OutNoCollidPos;
	return collided;
}

FAntGrid::~FAntGrid()
{
}

void FAntGrid::Reset(uint32 cellNumber, float cellSize, float shiftSize)
{
	// calculating landscape shift size
	ShiftSize = shiftSize;

	// reset containers
	Grid.Reset();
	GridNodes.Reset();
	GridData.Reset();

	// resize grid
	Grid.Init(INDEX_NONE, cellNumber * cellNumber);

	CellNumber = cellNumber;
	CellSize = cellSize;
	Version = 0;
}

int32 FAntGrid::AddCylinder(const FAntHandle &Handle, const FVector3f &Base, float Radius, float Height, int32 Flags)
{
	check(CellNumber != 0 && CellSize != 0.0f && ShiftSize != 0.0f && "Grid is not initialized!");

	// checking boundaries
	const auto landscapeRect = FBox2f{ {0.0f, 0.0f}, {CellSize * CellNumber, CellSize * CellNumber} };
	const auto circleRect = FBox2f{ FVector2f((Base - Radius) + ShiftSize), FVector2f((Base + Radius) + ShiftSize) };
	const auto overlapRect = landscapeRect.Overlap(circleRect);
	if (!overlapRect.bIsValid)
	{
		UE_LOG(LogAnt, Warning, TEXT("[ID: %i] Circle center is out of landscape bound!"), Handle.Idx);
		return INDEX_NONE;
	}

	// find overlapped cells
	const int xstart = FMath::Min(overlapRect.Min.X / CellSize, CellNumber - 1);
	const int xend = FMath::Min(overlapRect.Max.X / CellSize, CellNumber - 1);
	const int ystart = FMath::Min(overlapRect.Min.Y / CellSize, CellNumber - 1);
	const int yend = FMath::Min(overlapRect.Max.Y / CellSize, CellNumber - 1);

	FNodeData nodeData;
	nodeData.Flag = Flags;
	nodeData.Handle = Handle;
	nodeData.InstanceID = FAntMath::GetUniqueNumber();
	nodeData.Cylinder.Base = Base;
	nodeData.Cylinder.height = Height;
	nodeData.Cylinder.Radius = Radius;
	nodeData.bMultiCell = (ystart != yend) || (xstart != xend);
	const auto nodeDataIdx = GridData.Add(nodeData);

	// iterate overlapped cells
	int32 lastNodeIdx = INDEX_NONE;
	for (auto y = ystart; y <= yend; ++y)
		for (auto x = xstart; x <= xend; ++x)
		{
			// check collison between cell and circle
			const FBox2f cellRect = { {x * CellSize, y * CellSize}, {x * CellSize + CellSize, y * CellSize + CellSize} };
			if (FAntMath::RectCircle(cellRect, FVector2f(Base) + ShiftSize, Radius))
			{
				const auto cellInd = IND_2D_TO_1D(x, y);

				// add new node
				const auto nodeIdx = GridNodes.Add({});
				// set cell index
				GridNodes[nodeIdx].CellIdx = cellInd;
				// set node data
				GridNodes[nodeIdx].DataIdx = nodeDataIdx;
				// link to next node
				GridNodes[nodeIdx].NextIdx = Grid[cellInd];
				// link prev node to this node
				if (Grid[cellInd] != INDEX_NONE)
					GridNodes[Grid[cellInd]].PrevIdx = nodeIdx;

				// link to local list
				GridNodes[nodeIdx].NextListIdx = lastNodeIdx;
				lastNodeIdx = nodeIdx;

				// add new node to the begining of the high level map
				Grid[cellInd] = nodeIdx;
				// debug draw queried cell
				/*if (DebugDraw)
					DrawDebugSolidBox(DebugDraw, FVector(cellRect.GetCenter().X - ShiftSize, cellRect.GetCenter().Y - ShiftSize, 2),
						FVector(cellRect.GetExtent().X, cellRect.GetExtent().Y, 5), FColor::Magenta, false);*/
			}
		}

	++Count;
	++Version;

	return lastNodeIdx;
}

void FAntGrid::UpdateFlag(int32 GridHandle, int32 NewFlags)
{
	check(GridNodes.IsValidIndex(GridHandle) && "Invalid handle");

	// update grid version
	Version = GridData[GridNodes[GridHandle].DataIdx].Flag == NewFlags ? Version : Version + 1;

	// update flag
	GridData[GridNodes[GridHandle].DataIdx].Flag = NewFlags;
}

void FAntGrid::UpdateHeight(int32 GridHandle, float NewHeight)
{
	check(GridNodes.IsValidIndex(GridHandle) && "Invalid handle");

	// update grid version
	Version = GridData[GridNodes[GridHandle].DataIdx].Cylinder.height == NewHeight ? Version : Version + 1;

	// update flag
	GridData[GridNodes[GridHandle].DataIdx].Cylinder.height = NewHeight;
}

void FAntGrid::Remove(int32 GridHandle)
{
	check(GridNodes.IsValidIndex(GridHandle) && "Invalid handle");

	// remove node data
	GridData.RemoveAt(GridNodes[GridHandle].DataIdx);

	// remove node itself
	auto firstIdx = GridHandle;
	while (firstIdx != INDEX_NONE)
	{
		auto &node = GridNodes[firstIdx];

		// remove prev link
		if (node.PrevIdx != INDEX_NONE)
			GridNodes[node.PrevIdx].NextIdx = node.NextIdx;

		// remove next link
		if (node.NextIdx != INDEX_NONE)
			GridNodes[node.NextIdx].PrevIdx = node.PrevIdx;

		// remove head link
		if (Grid[node.CellIdx] == firstIdx)
			Grid[node.CellIdx] = node.NextIdx;

		// 
		const auto nextIdx = node.NextListIdx;
		GridNodes.RemoveAt(firstIdx);
		firstIdx = nextIdx;
	}

	--Count;
	++Version;
}

void FAntGrid::QueryCylinder(const FVector3f &Base, float Radius, float Height, int32 Flags, bool MustIncludeCenter, TArray<FAntContactInfo> &OutCollided, const TArray<FAntHandle> *IgnoreList) const
{
	check(CellNumber != 0 && CellSize != 0.0f && ShiftSize != 0.0f && "Grid is not initialized!");

	// checking boundaries
	const FBox2f landscapeRect{ {0.0f, 0.0f}, {CellSize * CellNumber, CellSize * CellNumber} };
	const FBox2f circleRect{ FVector2f((Base - Radius) + ShiftSize), FVector2f((Base + Radius) + ShiftSize) };
	const auto overlapRect = landscapeRect.Overlap(circleRect);

	// out of bound query!
	if (!overlapRect.bIsValid)
		return;

	// find overlapped cells
	const int32 xstart = FMath::Min(overlapRect.Min.X / CellSize, CellNumber - 1);
	const int32 xend = FMath::Min(overlapRect.Max.X / CellSize, CellNumber - 1);
	const int32 ystart = FMath::Min(overlapRect.Min.Y / CellSize, CellNumber - 1);
	const int32 yend = FMath::Min(overlapRect.Max.Y / CellSize, CellNumber - 1);

	// store overlapped cells
	TArray<TPair<int32, int32>, TInlineAllocator<32>> cells;
	for (auto y = ystart; y <= yend; ++y)
		for (auto x = xstart; x <= xend; ++x)
			if (FAntMath::RectCircle({ {x * CellSize, y * CellSize}, {x * CellSize + CellSize, y * CellSize + CellSize} }, FVector2f(Base) + ShiftSize, Radius))
				cells.Add({ x, y });

	// in case of pre-filled OutCollided, we store the size
	const auto preSize = OutCollided.Num();
	// we can't use session counter in multi thread scenario so
	// we iterate over nodes 2 times, 1 for multi-cell (shared) nodes and 1 for single-cell nodes
	// this way we can use OutCollided array efficiently for checking duplication.
	bool multiCellPhase = true;
	do {
		for (const auto &it : cells)
		{
			const auto cellInd = IND_2D_TO_1D(it.Key, it.Value);
			// check nodes
			auto beginIdx = Grid[cellInd];
			while (beginIdx != INDEX_NONE)
			{
				auto &node = GridNodes[beginIdx];
				const auto &nodeData = GridData[node.DataIdx];
				beginIdx = node.NextIdx;

				// skip single-cell nodes in first phase
				if (multiCellPhase && !nodeData.bMultiCell)
					continue;

				// skip multi-cell nodes in second phase
				if (!multiCellPhase && nodeData.bMultiCell)
					continue;

				// check flag and disbality
				if (!CHECK_BIT_ANY(Flags, nodeData.Flag))
					continue;

				// check ignore list
				if (IgnoreList && AntInternal::IndexOfElement(*IgnoreList, nodeData.Handle, 0) != INDEX_NONE)
					continue;

				// init contact info
				FAntContactInfo info;
				info.Flag = nodeData.Flag;
				info.Handle = nodeData.Handle;
				info.InstanceID = nodeData.InstanceID;

				// duplication check for multi-cell nodes
				if (nodeData.bMultiCell && AntInternal::IndexOfElement(OutCollided, info, preSize) != INDEX_NONE)
					continue;

				// check collision
				if ((nodeData.Cylinder.Base.Z + nodeData.Cylinder.height < Base.Z) || (nodeData.Cylinder.Base.Z > Base.Z + Height))
					continue;

				const auto sumRadius = Radius + (MustIncludeCenter ? 0 : nodeData.Cylinder.Radius);
				const auto sqRadius = sumRadius * sumRadius;
				info.SqDist = FVector2f::DistSquared(FVector2f(Base), FVector2f(nodeData.Cylinder.Base));
				if (info.SqDist < sqRadius)
				{
					info.Cylinder.Base = nodeData.Cylinder.Base;
					info.Cylinder.Radius = nodeData.Cylinder.Radius;
					info.Cylinder.Height = nodeData.Cylinder.height;

					OutCollided.Push(info);
				}
				continue;
			}
		}

		// switch phase
		multiCellPhase = !multiCellPhase;
	} while (!multiCellPhase);
}

void FAntGrid::QueryRay(const FVector3f &Start, const FVector3f &End, int32 Flags, TArray<FAntContactInfo> &OutCollided, const TArray<FAntHandle> *IgnoreList) const
{
	check(CellNumber != 0 && CellSize != 0.0f && ShiftSize != 0.0f && "Grid is not initialized!");

	// checking boundaries
	const FBox2f landscapeRect{ {0.0f, 0.0f}, {CellSize * CellNumber, CellSize * CellNumber} };
	const FVector2f rectMin(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y));
	const FVector2f rectMax(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y));
	const FBox2f segmentRect{ rectMin + ShiftSize, rectMax + ShiftSize };
	const auto overlapRect = landscapeRect.Overlap(segmentRect);

	// out of bound query!
	if (!overlapRect.bIsValid)
		return;

	//DrawDebugLine(DebugDraw, FVector(Start.X, Start.Y, 40), FVector(End.X, End.Y, 40), FColor::Black, false);

	// in case of pre-filled OutCollided, we store the size
	const auto preSize = OutCollided.Num();
	// we can't use session counter in multi thread scenario so
	// we iterate over nodes 2 times, 1 for multi-cell (shared) nodes and 1 for single-cell nodes
	// this way we can use OutCollided array efficiently for checking duplication.
	bool multiCellPhase = true;

	// find overlapped cells
	const auto xstart = (Start.X + ShiftSize) / CellSize;
	const auto xend = (End.X + ShiftSize) / CellSize;
	const auto ystart = (Start.Y + ShiftSize) / CellSize;
	const auto yend = (End.Y + ShiftSize) / CellSize;

	const float dx = FMath::Abs(xend - xstart);
	const float dy = FMath::Abs(yend - ystart);

	int32 x = int32(FMath::Floor(xstart));
	int32 y = int32(FMath::Floor(ystart));

	int32 n = 1;
	int32 x_inc, y_inc;
	float error;

	if (dx == 0)
	{
		x_inc = 0;
		error = std::numeric_limits<float>::infinity();
	}
	else if (xend > xstart)
	{
		x_inc = 1;
		n += int32(floor(xend)) - x;
		error = (floor(xstart) + 1 - xstart) * dy;
	}
	else
	{
		x_inc = -1;
		n += x - int32(floor(xend));
		error = (xstart - FMath::Floor(xstart)) * dy;
	}

	if (dy == 0)
	{
		y_inc = 0;
		error -= std::numeric_limits<int32>::infinity();
	}
	else if (yend > ystart)
	{
		y_inc = 1;
		n += int32(FMath::Floor(yend)) - y;
		error -= (FMath::Floor(ystart) + 1 - ystart) * dx;
	}
	else
	{
		y_inc = -1;
		n += y - int32(floor(yend));
		error -= (ystart - FMath::Floor(ystart)) * dx;
	}

	for (; n > 0; --n)
	{
		// iterate over involved cells
		if (x >= 0 && y >= 0 && x < CellNumber && y < CellNumber)
		{
			do
			{
				const auto cellInd = IND_2D_TO_1D(x, y);
				auto beginIdx = Grid[cellInd];

				// itertae over objects inside each cell
				while (beginIdx != INDEX_NONE)
				{
					auto &node = GridNodes[beginIdx];
					const auto &nodeData = GridData[node.DataIdx];
					beginIdx = node.NextIdx;

					// skip single-cell nodes in first phase
					if (multiCellPhase && !nodeData.bMultiCell)
						continue;

					// skip multi-cell nodes in second phase
					if (!multiCellPhase && nodeData.bMultiCell)
						continue;

					// check flag and disbality
					if (!CHECK_BIT_ANY(Flags, nodeData.Flag))
						continue;

					// check ignore list
					if (IgnoreList && AntInternal::IndexOfElement(*IgnoreList, nodeData.Handle, 0) != INDEX_NONE)
						continue;

					// init contact info
					FAntContactInfo info;
					info.Flag = nodeData.Flag;
					info.Handle = nodeData.Handle;
					info.InstanceID = nodeData.InstanceID;

					// duplication check for multi-cell nodes
					if (nodeData.bMultiCell && AntInternal::IndexOfElement(OutCollided, info, preSize) != INDEX_NONE)
						continue;

					// check collision against cylinder
					float timeOfImpact = 0.0f;
					if (FAntMath::CylinderSegment({ nodeData.Cylinder.Base, nodeData.Cylinder.Base + nodeData.Cylinder.height }, nodeData.Cylinder.Radius, { Start, End }, timeOfImpact))
					{
						info.Cylinder.Base = nodeData.Cylinder.Base;
						info.Cylinder.Radius = nodeData.Cylinder.Radius;
						info.SqDist = 0;
						OutCollided.Push(info);
					}
					continue;
				}

				// switch phase
				multiCellPhase = !multiCellPhase;
			} while (!multiCellPhase);

			//const FBox2f cellRect = { {x * CellSize, y * CellSize}, {x * CellSize + CellSize, y * CellSize + CellSize} };
			//DrawDebugSolidBox(DebugDraw, FVector(cellRect.GetCenter().X - ShiftSize, cellRect.GetCenter().Y - ShiftSize, 25),
			//	FVector(cellRect.GetExtent().X, cellRect.GetExtent().Y, 10), FColor::Magenta, false);
		}

		if (error > 0)
		{
			y += y_inc;
			error -= dx;
		}
		else
		{
			x += x_inc;
			error += dy;
		}
	}
}

void FAntGrid::QueryConvexVolume(const TStaticArray<FVector, 4> &NearPlane, const TStaticArray<FVector, 4> &FarPlane, int32 Flags, TArray<FAntContactInfo> &OutCollided, const TArray<FAntHandle> *IgnoreList) const
{
	// create 2d list of the coordinates to compute convex hull
	TArray<FVector> pointLists;
	TArray<FVector2f> sortedPointLists;
	TArray<int32> convexHullIdx;

	// create 3d volume
	FConvexVolume convexVolume;
	convexVolume.Planes.Empty(6);
	FPlane tempPlnae;

	// left clipping plane.
	tempPlnae = FPlane(NearPlane[0] + FarPlane[0], NearPlane[0], NearPlane[3]);
	convexVolume.Planes.Add(tempPlnae);

	// right clipping plane.
	tempPlnae = FPlane(NearPlane[2], NearPlane[1], NearPlane[1] + FarPlane[1]);
	convexVolume.Planes.Add(tempPlnae);

	// top clipping plane.
	tempPlnae = FPlane(NearPlane[1], NearPlane[0], NearPlane[0] + FarPlane[0]);
	convexVolume.Planes.Add(tempPlnae);

	// bottom clipping plane.
	tempPlnae = FPlane(NearPlane[3] + FarPlane[3], NearPlane[3], NearPlane[2]);
	convexVolume.Planes.Add(tempPlnae);

	convexVolume.Init();

	// add near plane points
	for (const auto &it : NearPlane)
		pointLists.AddUnique(it);

	// add far plane points
	for (int8 idx = 0; idx < 4; ++idx)
		pointLists.AddUnique(NearPlane[idx] + FarPlane[idx]);

	// compute 2d convex hull of the area
	ConvexHull2D::ComputeConvexHull(pointLists, convexHullIdx);

	// make convex hull by sorting points
	sortedPointLists.Reserve(convexHullIdx.Num());
	for (int8 idx = 0; idx < convexHullIdx.Num(); ++idx)
		sortedPointLists.Add(FVector2f(pointLists[convexHullIdx[idx]].X, pointLists[convexHullIdx[idx]].Y));

	// query 2d area of the volume
	const FBox2f landscapeRect{ {0.0f, 0.0f}, {CellSize * CellNumber, CellSize * CellNumber} };
	const FBox2f polyAabb(sortedPointLists);
	const auto overlapRect = landscapeRect.Overlap(polyAabb.ShiftBy({ ShiftSize, ShiftSize }));

	// out of bound query!
	if (!overlapRect.bIsValid)
		return;

	// find overlapped cells
	const int32 xstart = FMath::Min(overlapRect.Min.X / CellSize, CellNumber - 1);
	const int32 xend = FMath::Min(overlapRect.Max.X / CellSize, CellNumber - 1);
	const int32 ystart = FMath::Min(overlapRect.Min.Y / CellSize, CellNumber - 1);
	const int32 yend = FMath::Min(overlapRect.Max.Y / CellSize, CellNumber - 1);

	// store overlapped cells
	TArray<TPair<int32, int32>, TInlineAllocator<48>> cells;
	for (auto y = ystart; y <= yend; ++y)
		for (auto x = xstart; x <= xend; ++x)
			if (FAntMath::RectPolygon(FBox2f({ x * CellSize, y * CellSize }, { x * CellSize + CellSize, y * CellSize + CellSize }).ShiftBy({ -ShiftSize, -ShiftSize }), sortedPointLists))
				cells.Add({ x, y });

	// in case of pre-filled OutCollided, we store the size
	const auto preSize = OutCollided.Num();
	// we can't use session counter in multi thread scenario so
	// we iterate over nodes 2 times, 1 for multi-cell (shared) nodes and 1 for single-cell nodes
	// this way we can use OutCollided array efficiently for checking duplication.
	bool multiCellPhase = true;
	do {
		for (const auto &it : cells)
		{
			const auto cellInd = IND_2D_TO_1D(it.Key, it.Value);
			// check nodes
			auto beginIdx = Grid[cellInd];
			while (beginIdx != INDEX_NONE)
			{
				auto &node = GridNodes[beginIdx];
				const auto &nodeData = GridData[node.DataIdx];
				beginIdx = node.NextIdx;

				// skip single-cell nodes in first phase
				if (multiCellPhase && !nodeData.bMultiCell)
					continue;

				// skip multi-cell nodes in second phase
				if (!multiCellPhase && nodeData.bMultiCell)
					continue;

				// check flag and disbality
				if (!CHECK_BIT_ANY(Flags, nodeData.Flag))
					continue;

				// check ignore list
				if (IgnoreList && AntInternal::IndexOfElement(*IgnoreList, nodeData.Handle, 0) != INDEX_NONE)
					continue;

				// init contact info
				FAntContactInfo info;
				info.Flag = nodeData.Flag;
				info.Handle = nodeData.Handle;
				info.InstanceID = nodeData.InstanceID;

				// duplication check for multi-cell nodes
				if (nodeData.bMultiCell && AntInternal::IndexOfElement(OutCollided, info, preSize) != INDEX_NONE)
					continue;

				if (FAntMath::PolygonCircle(sortedPointLists, FVector2f(nodeData.Cylinder.Base), nodeData.Cylinder.Radius))
				{
					info.Cylinder.Base = nodeData.Cylinder.Base;
					info.Cylinder.Radius = nodeData.Cylinder.Radius;

					// check agianst 3D volume
					if (convexVolume.IntersectPoint(FVector(nodeData.Cylinder.Base)))
						OutCollided.Push(info);
				}
			}
		}
		// switch phase
		multiCellPhase = !multiCellPhase;
	} while (!multiCellPhase);
}