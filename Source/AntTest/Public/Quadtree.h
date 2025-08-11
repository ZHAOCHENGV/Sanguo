#pragma once

#include "CoreMinimal.h"
#include "AntFunctionLibrary.h"
#include "AntHandle.h"

class QuadTreeNode
{
public:

	// 存储单位信息（使用弱指针避免持有过多引用）
	struct FQuadTreeObject {
		FAntHandle* Handle;
		int Team;
		FQuadTreeObject(FAntHandle* InHandle = nullptr, int InTeam = 0)
			: Handle(InHandle), Team(InTeam) {}
	};

	FVector2D Center;       // 节点中心点
	FVector2D HalfSize;     // 半宽度和半高度 (二维范围)
	int Capacity;           // 节点容量阈值
	int Depth;              // 节点深度
	bool bIsLeaf;           // 是否为叶节点
	TArray<FQuadTreeObject> Objects; // 当前节点存储的单位列表
	TSharedPtr<QuadTreeNode> Children[4]; // 子节点指针 (NW, NE, SW, SE)

public:
	// 构造函数：指定中心、半尺寸和容量
	QuadTreeNode(const FVector2D& InCenter, const FVector2D& InHalfSize, int32 InCapacity, int32 InDepth = 0)
		: Center(InCenter), HalfSize(InHalfSize), Capacity(InCapacity), Depth(InDepth), bIsLeaf(true)
	{
		for (int i = 0; i < 4; i++)
			Children[i] = nullptr;
	}

	// 判断点是否在当前节点区域内
	bool ContainsPoint(const FVector2D& Point) const
	{
		return (Point.X >= Center.X - HalfSize.X && Point.X <= Center.X + HalfSize.X &&
				Point.Y >= Center.Y - HalfSize.Y && Point.Y <= Center.Y + HalfSize.Y);
	}

	// 分裂当前节点为4个子节点
	void Subdivide()
	{
		FVector2D QuarterSize = HalfSize * 0.5f;
		// 顺序：0=NW, 1=NE, 2=SW, 3=SE
		Children[0] = MakeShareable(new QuadTreeNode(
			FVector2D(Center.X - QuarterSize.X, Center.Y + QuarterSize.Y), QuarterSize, Capacity, Depth + 1));
		Children[1] = MakeShareable(new QuadTreeNode(
			FVector2D(Center.X + QuarterSize.X, Center.Y + QuarterSize.Y), QuarterSize, Capacity, Depth + 1));
		Children[2] = MakeShareable(new QuadTreeNode(
			FVector2D(Center.X - QuarterSize.X, Center.Y - QuarterSize.Y), QuarterSize, Capacity, Depth + 1));
		Children[3] = MakeShareable(new QuadTreeNode(
			FVector2D(Center.X + QuarterSize.X, Center.Y - QuarterSize.Y), QuarterSize, Capacity, Depth + 1));
		bIsLeaf = false;
	}

	// 插入单位 (Actor指针和队伍ID)
	void Insert(FAntHandle* InHandle, int32 InTeam, const UObject *WorldContextObject)
	{
		if (!InHandle) return;
		FVector3f Location;
		UAntFunctionLibrary::GetAgentLocation(WorldContextObject, *InHandle, Location);
		FVector2D Point = FVector2D(Location.X, Location.Y);
		if (!ContainsPoint(Point)) return; // 不在范围内则忽略

		if (bIsLeaf)
		{
			// 叶节点且未满
			if (Objects.Num() < Capacity)
			{
				Objects.Emplace(InHandle, InTeam);
				return;
			}
			// 叶节点已满，需要分裂
			Subdivide();
			// 将当前叶节点的对象重新分配到子节点
			for (const FQuadTreeObject& Obj : Objects)
			{
				for (int i = 0; i < 4; i++)
				{
					FVector3f HandleLocation;
					UAntFunctionLibrary::GetAgentLocation(WorldContextObject, *Obj.Handle, HandleLocation);
					if (Children[i]->ContainsPoint(FVector2D(HandleLocation.X, HandleLocation.Y)))
					{
						Children[i]->Insert(Obj.Handle, Obj.Team, WorldContextObject);
						break;
					}
				}
			}
			Objects.Empty();
		}

		// 如果不是叶节点，则递归插入到对应子节点
		for (int i = 0; i < 4; i++)
		{
			if (Children[i] && Children[i]->ContainsPoint(Point))
			{
				Children[i]->Insert(InHandle, InTeam, WorldContextObject);
				return;
			}
		}
	}

	// 从四叉树中移除单位（返回是否成功）
	bool Remove(FAntHandle* InHandle, const UObject *WorldContextObject)
	{
		if (!InHandle) return false;
		FVector3f Location;
		UAntFunctionLibrary::GetAgentLocation(WorldContextObject, *InHandle, Location);
		FVector2D Point = FVector2D(Location.X, Location.Y);

		if (bIsLeaf)
		{
			// 在叶节点直接查找并移除
			for (int i = 0; i < Objects.Num(); i++)
			{
				if (Objects[i].Handle == InHandle)
				{
					Objects.RemoveAt(i);
					return true;
				}
			}
			return false;
		}

		// 非叶节点，则递归到对应子节点
		for (int i = 0; i < 4; i++)
		{
			if (Children[i] && Children[i]->ContainsPoint(Point))
			{
				bool Removed = Children[i]->Remove(InHandle, WorldContextObject);
				// 如果子节点被移除后变成空叶节点，可回收
				if (Removed && Children[i]->bIsLeaf && Children[i]->Objects.Num() == 0)
				{
					Children[i].Reset();
					// 检查所有子节点是否已回收，若是则把当前节点标记为叶节点
					bool AllNull = true;
					for (int j = 0; j < 4; j++)
					{
						if (Children[j].IsValid()) { AllNull = false; break; }
					}
					if (AllNull) bIsLeaf = true;
				}
				return Removed;
			}
		}
		return false;
	}

	// 在四叉树中查找离给定点最近的敌方单位
	FAntHandle* FindNearest(const FVector2D& Point, int32 MyTeam, float& OutNearestDistSq, const UObject *WorldContextObject)
	{
		FAntHandle* Nearest = nullptr;
		
		if (bIsLeaf)
		{
			// 叶节点遍历所有对象
			for (const FQuadTreeObject& Obj : Objects)
			{
				//UE_LOG(LogTemp, Warning, TEXT("%d---%d"), Obj.Team, MyTeam);
				if (Obj.Handle && Obj.Team != MyTeam)
				{
					FVector3f Location;
					UAntFunctionLibrary::GetAgentLocation(WorldContextObject, *Obj.Handle, Location);
					FVector2D ObjPos = FVector2D(Location.X, Location.Y);
					float DistSq = FVector2D::DistSquared(Point, ObjPos);
					if (DistSq < OutNearestDistSq)
					{
						OutNearestDistSq = DistSq;
						Nearest = Obj.Handle;
						
					}
				}
			}
			return Nearest;
		}
		// 内部节点则对每个子节点进行空间剪枝检查后递归
		for (int i = 0; i < 4; i++)
		{
			if (!Children[i]) continue;
			// 计算查询点到子节点区域边界的最近距离 (下界剪枝)
			FVector2D ChildMin = Children[i]->Center - Children[i]->HalfSize;
			FVector2D ChildMax = Children[i]->Center + Children[i]->HalfSize;
			float dx = FMath::Max(FMath::Max(ChildMin.X - Point.X, 0.0f), Point.X - ChildMax.X);
			float dy = FMath::Max(FMath::Max(ChildMin.Y - Point.Y, 0.0f), Point.Y - ChildMax.Y);
			float DistSqBox = dx * dx + dy * dy;
			if (DistSqBox <= OutNearestDistSq)
			{
				FAntHandle* ChildNearest = Children[i]->FindNearest(Point, MyTeam, OutNearestDistSq,WorldContextObject);
				if (ChildNearest)
					Nearest = ChildNearest;
			}
		}
		return Nearest;
	}
};
