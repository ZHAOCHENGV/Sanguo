/**
 * @file BFAntFunctionLibrary.cpp
 * @brief BattleFrame Ant函数库实现文件
 * @details 该文件实现了BattleFrame系统中与Ant代理相关的工具函数，包括代理过滤、特征获取和队列管理
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#include "BFAntFunctionLibrary.h"

#include "BF/Actors/GeneralActorBase.h"
#include "BF/Traits/Group.h"
#include "BF/Traits/Unit.h"
#include "Traits/Death.h"
#include "Traits/PrimaryType.h"
#include "Traits/Team.h"

/**
 * @brief 获取符合过滤条件的生存代理
 * @param WorldContextObject 世界上下文对象
 * @param Filter 过滤条件结构体
 * @return 返回符合过滤条件的代理句柄数组
 * @details 使用BattleFrame的过滤系统获取指定条件的生存代理
 */
TArray<FSubjectHandle> UBFAntFunctionLibrary::GetFilterSurvivalAgent(const UObject* WorldContextObject,
	const FBFFilter& Filter)
{
	// 检查引擎是否有效
	if (!GEngine) return TArray<FSubjectHandle>();
	
	// 从上下文对象获取世界
	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	
	TArray<FSubjectHandle> SubjectHandles;
	
	if (WorldContextObject)
	{
		// 获取BattleFrame机制
		auto Mechanism = UMachine::ObtainMechanism(World);

		if (Mechanism)
		{
			// 创建代理过滤器
			FFilter FoundFilter = FFilter::Make<FAgent>();
			// 包含指定的特征
			FoundFilter.Include(Filter.IncludeTraits);
			// 排除指定的特征
			FoundFilter.Exclude(Filter.ExcludeTraits);
			// 收集符合条件的主体
			Mechanism->CollectSubjects(SubjectHandles, FoundFilter);
		}
		return SubjectHandles;
	}
	return TArray<FSubjectHandle>();
}

/**
 * @brief 过滤生存代理数组
 * @param WorldContextObject 世界上下文对象
 * @param SubjectHandles 要过滤的主体句柄数组
 * @param Trait 特征结构体
 * @return 返回过滤后的主体句柄数组
 * @details 从给定的主体数组中过滤出生存的代理，排除死亡和通用主体
 */
TArray<FSubjectHandle> UBFAntFunctionLibrary::FilterSurvivalAgentArray(const UObject* WorldContextObject,
                                                                       TArray<FSubjectHandle> SubjectHandles, UScriptStruct* Trait)
{
	// 检查引擎是否有效
	if (!GEngine) return TArray<FSubjectHandle>();
	
	// 从上下文对象获取世界
	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	
	TArray<FSubjectHandle> FilterSubjectHandles;
	
	if (WorldContextObject)
	{
		// 获取BattleFrame机制
		auto Mechanism = UMachine::ObtainMechanism(World);

		if (Mechanism)
		{
			// 创建代理过滤器
			FFilter Filter = FFilter::Make<FAgent>();
			// 排除死亡特征
			Filter.Exclude<FDying>();
			// 排除通用特征
			Filter.Exclude<FGeneral>();
			// 收集符合条件的主体
			Mechanism->CollectSubjects(SubjectHandles, Filter);
		}
		return SubjectHandles;
	}
	return TArray<FSubjectHandle>();
}

/**
 * @brief 根据索引获取队伍特征
 * @param Index 队伍索引（0-9）
 * @return 返回对应的队伍特征结构体指针
 * @details 支持最多10个不同的队伍特征
 */
UScriptStruct* UBFAntFunctionLibrary::GetTeamTrait(const int32 Index)
{
	// 定义所有队伍特征结构体数组
	TArray<UScriptStruct*> TeamStructs
	{
		FTeam0::StaticStruct(),
		FTeam1::StaticStruct(),
		FTeam2::StaticStruct(),
		FTeam3::StaticStruct(),
		FTeam4::StaticStruct(),
		FTeam5::StaticStruct(),
		FTeam6::StaticStruct(),
		FTeam7::StaticStruct(),
		FTeam8::StaticStruct(),
		FTeam9::StaticStruct()
	};
	
	return TeamStructs[Index];
}

/**
 * @brief 根据索引获取组特征
 * @param Index 组索引（0-9）
 * @return 返回对应的组特征结构体指针
 * @details 支持最多10个不同的组特征
 */
UScriptStruct* UBFAntFunctionLibrary::GetGroupTrait(const int32 Index)
{
	// 定义所有组特征结构体数组
	TArray<UScriptStruct*> GroupStructs
	{
		FGroup0::StaticStruct(),
		FGroup1::StaticStruct(),
		FGroup2::StaticStruct(),
		FGroup3::StaticStruct(),
		FGroup4::StaticStruct(),
		FGroup5::StaticStruct(),
		FGroup6::StaticStruct(),
		FGroup7::StaticStruct(),
		FGroup8::StaticStruct(),
		FGroup9::StaticStruct()
	};

	return GroupStructs[Index];
}

/**
 * @brief 根据索引获取单位特征
 * @param Index 单位索引（0-9）
 * @return 返回对应的单位特征结构体指针
 * @details 支持最多10个不同的单位特征
 */
UScriptStruct* UBFAntFunctionLibrary::GetUnitTrait(const int32 Index)
{
	// 定义所有单位特征结构体数组
	TArray<UScriptStruct*> UnitStructs
	{
		FUnit0::StaticStruct(),
		FUnit1::StaticStruct(),
		FUnit2::StaticStruct(),
		FUnit3::StaticStruct(),
		FUnit4::StaticStruct(),
		FUnit5::StaticStruct(),
		FUnit6::StaticStruct(),
		FUnit7::StaticStruct(),
		FUnit8::StaticStruct(),
		FUnit9::StaticStruct()
	};

	return UnitStructs[Index];
}

/** @brief 静态队列计数器，用于管理全局队列状态 */
int32 UBFAntFunctionLibrary::QueueCount = 0;

/**
 * @brief 设置队列计数
 * @param InQueueCount 新的队列计数值
 * @details 重置并设置全局队列计数器
 */
void UBFAntFunctionLibrary::SetQueueCount(const int32 InQueueCount)
{
	// 先重置为0，然后设置新值
	QueueCount = 0;
	QueueCount = InQueueCount;
}

/**
 * @brief 减少队列计数
 * @param Value 要减少的值
 * @details 减少队列计数，确保不会小于0
 */
void UBFAntFunctionLibrary::ReduceQueueCount(const int32 Value)
{
	QueueCount -= Value;
	// 确保队列计数不会小于0
	QueueCount = QueueCount < 0 ? 0 : QueueCount;
}

/**
 * @brief 获取当前队列计数
 * @return 返回当前的队列计数值
 * @details 获取全局队列计数器的当前值
 */
int32 UBFAntFunctionLibrary::GetQueueCount()
{
	return QueueCount;
}
