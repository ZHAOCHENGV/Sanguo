/**
 * @file BFAntFunctionLibrary.h
 * @brief BattleFrame Ant函数库头文件
 * @details 定义了BattleFrame系统中与Ant代理相关的工具函数库，提供代理过滤、特征获取和队列管理功能
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SubjectHandle.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Traits/Trace.h"
#include "BFAntFunctionLibrary.generated.h"

/**
 * @brief BattleFrame Ant函数库类
 * @details 该类继承自UBlueprintFunctionLibrary，提供了一系列静态工具函数
 *          用于在BattleFrame系统中处理Ant代理的过滤、特征管理和队列控制
 */
UCLASS()
class ANTTEST_API UBFAntFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief 获取符合过滤条件的生存代理
	 * @param WorldContextObject 世界上下文对象
	 * @param Filter 过滤条件结构体
	 * @return 返回符合过滤条件的代理句柄数组
	 * @details 使用BattleFrame的过滤系统获取指定条件的生存代理
	 */
	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary", meta = (WorldContext = "WorldContextObject"))
	static TArray<FSubjectHandle> GetFilterSurvivalAgent(const UObject* WorldContextObject, const FBFFilter& Filter);

	/**
	 * @brief 过滤生存代理数组
	 * @param WorldContextObject 世界上下文对象
	 * @param SubjectHandles 要过滤的主体句柄数组
	 * @param Trait 特征结构体
	 * @return 返回过滤后的主体句柄数组
	 * @details 从给定的主体数组中过滤出生存的代理，排除死亡和通用主体
	 */
	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary", meta = (WorldContext = "WorldContextObject"))
	static TArray<FSubjectHandle> FilterSurvivalAgentArray(const UObject* WorldContextObject, TArray<FSubjectHandle> SubjectHandles, UScriptStruct* Trait);

	/**
	 * @brief 根据索引获取队伍特征
	 * @param Index 队伍索引（0-9）
	 * @return 返回对应的队伍特征结构体指针
	 * @details 支持最多10个不同的队伍特征
	 */
	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary")
	static UScriptStruct* GetTeamTrait(const int32 Index);

	/**
	 * @brief 根据索引获取组特征
	 * @param Index 组索引（0-9）
	 * @return 返回对应的组特征结构体指针
	 * @details 支持最多10个不同的组特征
	 */
	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary")
	static UScriptStruct* GetGroupTrait(const int32 Index);

	/**
	 * @brief 根据索引获取单位特征
	 * @param Index 单位索引（0-9）
	 * @return 返回对应的单位特征结构体指针
	 * @details 支持最多10个不同的单位特征
	 */
	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary")
	static UScriptStruct* GetUnitTrait(const int32 Index);

	/**
	 * @brief 设置队列计数
	 * @param InQueueCount 新的队列计数值
	 * @details 重置并设置全局队列计数器
	 */
	UFUNCTION(BlueprintCallable, Category = "BFAntFunctionLibrary")
	static void SetQueueCount(const int32 InQueueCount);

	/**
	 * @brief 减少队列计数
	 * @param Value 要减少的值
	 * @details 减少队列计数，确保不会小于0
	 */
	UFUNCTION(BlueprintCallable, Category = "BFAntFunctionLibrary")
	static void ReduceQueueCount(const int32 Value);

	/**
	 * @brief 获取当前队列计数
	 * @return 返回当前的队列计数值
	 * @details 获取全局队列计数器的当前值
	 */
	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary")
	static int32 GetQueueCount();

private:
	/** @brief 静态队列计数器，用于管理全局队列状态 */
	static int32 QueueCount;
};
