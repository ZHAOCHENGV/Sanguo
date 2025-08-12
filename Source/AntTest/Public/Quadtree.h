/**
 * @file Quadtree.h
 * @brief 四叉树数据结构头文件
 * @details 该文件定义了用于空间分割的四叉树数据结构，用于高效地管理RTS游戏中的单位位置查询
 *          支持单位插入、移除和最近邻查找等操作
 * @author AntTest Team
 * @date 2024
 */

#pragma once

#include "CoreMinimal.h"
#include "AntFunctionLibrary.h"
#include "AntHandle.h"

/**
 * @brief 四叉树节点类
 * @details 实现了空间分割的四叉树数据结构，用于高效管理2D空间中的单位位置
 *          支持动态插入、移除和最近邻查找，适用于RTS游戏中的单位管理
 */
class QuadTreeNode
{
public:
	/**
	 * @brief 四叉树对象结构体
	 * @details 存储单位信息，包含Ant句柄指针和队伍ID
	 */
        struct FQuadTreeObject {
                /** @brief Ant句柄指针，指向具体的单位 */
                FAntHandle* Handle;
                /** @brief 单位所属的队伍ID */
                int Team;
                /** @brief 单位所属的组ID */
                int Group;

                /**
                 * @brief 构造函数
                 * @param InHandle Ant句柄指针
                 * @param InTeam 队伍ID
                 * @param InGroup 组ID
                 */
                FQuadTreeObject(FAntHandle* InHandle = nullptr, int InTeam = 0, int InGroup = 0)
                        : Handle(InHandle), Team(InTeam), Group(InGroup) {}
        };

	/** @brief 节点中心点坐标 */
	FVector2D Center;
	/** @brief 节点半宽度和半高度，定义节点的二维范围 */
	FVector2D HalfSize;
	/** @brief 节点容量阈值，超过此数量后会细分节点 */
	int Capacity;
	/** @brief 节点在树中的深度 */
	int Depth;
	/** @brief 是否为叶节点的标志 */
	bool bIsLeaf;
	/** @brief 当前节点存储的单位列表 */
	TArray<FQuadTreeObject> Objects;
	/** @brief 子节点指针数组，顺序为：西北、东北、西南、东南 */
	TSharedPtr<QuadTreeNode> Children[4];

public:
	/**
	 * @brief 构造函数
	 * @param InCenter 节点中心点
	 * @param InHalfSize 节点半尺寸
	 * @param InCapacity 节点容量阈值
	 * @param InDepth 节点深度，默认为0
	 * @details 初始化四叉树节点，设置中心点、尺寸和容量参数
	 */
	QuadTreeNode(const FVector2D& InCenter, const FVector2D& InHalfSize, int32 InCapacity, int32 InDepth = 0)
		: Center(InCenter), HalfSize(InHalfSize), Capacity(InCapacity), Depth(InDepth), bIsLeaf(true)
	{
		// 初始化所有子节点为空
		for (int i = 0; i < 4; i++)
			Children[i] = nullptr;
	}

	/**
	 * @brief 判断点是否在当前节点区域内
	 * @param Point 要检查的点坐标
	 * @return 如果点在节点区域内返回true，否则返回false
	 * @details 检查给定点是否在当前节点的边界范围内
	 */
	bool ContainsPoint(const FVector2D& Point) const
	{
		return (Point.X >= Center.X - HalfSize.X && Point.X <= Center.X + HalfSize.X &&
				Point.Y >= Center.Y - HalfSize.Y && Point.Y <= Center.Y + HalfSize.Y);
	}

	/**
	 * @brief 分裂当前节点为4个子节点
	 * @details 将当前节点分割为四个相等的子区域，创建西北、东北、西南、东南四个子节点
	 */
	void Subdivide()
	{
		// 计算子节点的半尺寸
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

	/**
	 * @brief 插入单位到四叉树
	 * @param InHandle Ant句柄指针
	 * @param InTeam 单位所属的队伍ID
	 * @param WorldContextObject 世界上下文对象
	 * @details 将单位插入到四叉树的适当位置，如果节点已满则进行细分
	 */
        void Insert(FAntHandle* InHandle, int32 InTeam, int32 InGroup, const UObject *WorldContextObject)
        {
                if (!InHandle) return;
		
		// 获取单位位置
		FVector3f Location;
		UAntFunctionLibrary::GetAgentLocation(WorldContextObject, *InHandle, Location);
		FVector2D Point = FVector2D(Location.X, Location.Y);
		
		// 检查点是否在节点范围内
		if (!ContainsPoint(Point)) return;

		if (bIsLeaf)
		{
			// 叶节点且未满，直接添加
                        if (Objects.Num() < Capacity)
                        {
                                Objects.Emplace(InHandle, InTeam, InGroup);
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
                                                Children[i]->Insert(Obj.Handle, Obj.Team, Obj.Group, WorldContextObject);
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
                                        Children[i]->Insert(InHandle, InTeam, InGroup, WorldContextObject);
                                        return;
                                }
                }
        }

	/**
	 * @brief 从四叉树中移除单位
	 * @param InHandle 要移除的Ant句柄指针
	 * @param WorldContextObject 世界上下文对象
	 * @return 如果成功移除返回true，否则返回false
	 * @details 从四叉树中移除指定单位，如果子节点变为空则进行回收
	 */
	bool Remove(FAntHandle* InHandle, const UObject *WorldContextObject)
	{
		if (!InHandle) return false;
		
		// 获取单位位置
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

	/**
	 * @brief 在四叉树中查找离给定点最近的敌方单位
	 * @param Point 查询点坐标
	 * @param MyTeam 当前单位所属的队伍ID
	 * @param OutNearestDistSq 输出最近距离的平方值
	 * @param WorldContextObject 世界上下文对象
	 * @return 返回最近敌人的Ant句柄指针，如果未找到则返回nullptr
	 * @details 使用空间剪枝优化最近邻查找，忽略同队单位
	 */
        FAntHandle* FindNearest(const FVector2D& Point, int32 MyTeam, float& OutNearestDistSq, const UObject *WorldContextObject,
                                                    int32 TargetTeam = -1, int32 TargetGroup = -1)
        {
                FAntHandle* Nearest = nullptr;
		
		if (bIsLeaf)
		{
			// 叶节点遍历所有对象
			for (const FQuadTreeObject& Obj : Objects)
			{
				// 检查是否为敌方单位
                                if (Obj.Handle && Obj.Team != MyTeam &&
                                        (TargetTeam == -1 || Obj.Team == TargetTeam) &&
                                        (TargetGroup == -1 || Obj.Group == TargetGroup))
                                {
                                        // 计算距离
                                        FVector3f Location;
					UAntFunctionLibrary::GetAgentLocation(WorldContextObject, *Obj.Handle, Location);
					FVector2D ObjPos = FVector2D(Location.X, Location.Y);
					float DistSq = FVector2D::DistSquared(Point, ObjPos);
					
					// 更新最近距离
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
			
			// 计算查询点到子节点区域边界的最近距离（下界剪枝）
			FVector2D ChildMin = Children[i]->Center - Children[i]->HalfSize;
			FVector2D ChildMax = Children[i]->Center + Children[i]->HalfSize;
			float dx = FMath::Max(FMath::Max(ChildMin.X - Point.X, 0.0f), Point.X - ChildMax.X);
			float dy = FMath::Max(FMath::Max(ChildMin.Y - Point.Y, 0.0f), Point.Y - ChildMax.Y);
			float DistSqBox = dx * dx + dy * dy;
			
			// 如果子节点区域可能包含更近的单位，则递归查找
			if (DistSqBox <= OutNearestDistSq)
			{
                                FAntHandle* ChildNearest = Children[i]->FindNearest(Point, MyTeam, OutNearestDistSq, WorldContextObject, TargetTeam, TargetGroup);
				if (ChildNearest)
					Nearest = ChildNearest;
			}
		}
		return Nearest;
	}
};
