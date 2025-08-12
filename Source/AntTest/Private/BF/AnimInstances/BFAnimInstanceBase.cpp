/**
 * @file BFAnimInstanceBase.cpp
 * @brief BattleFrame动画实例基类实现文件
 * @details 该文件实现了BattleFrame动画实例的基类，提供动画状态管理功能
 *          包括速度检测、欢呼动画状态等
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#include "BF/AnimInstances/BFAnimInstanceBase.h"

#include "BFSubjectiveAgentComponent.h"
#include "DebugHelper.h"

/**
 * @brief 本地初始化动画
 * @details 在动画实例初始化时获取主观代理组件引用
 */
void UBFAnimInstanceBase::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 如果还没有主观代理组件引用，则尝试获取
	if (!OwningSubjectiveAgent)
	{
		if (UBFSubjectiveAgentComponent* OwningComponent = GetOwningActor()->GetComponentByClass<UBFSubjectiveAgentComponent>())
		{
			OwningSubjectiveAgent = OwningComponent;
		}
	}
}

/**
 * @brief 本地线程安全更新动画
 * @param DeltaSeconds 帧间隔时间
 * @details 在动画更新时检查主观代理组件的状态，更新速度和动画状态
 */
void UBFAnimInstanceBase::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// 如果还没有主观代理组件引用，则尝试获取
	if (!OwningSubjectiveAgent)
	{
		if (UBFSubjectiveAgentComponent* OwningComponent = GetOwningActor()->GetComponentByClass<UBFSubjectiveAgentComponent>())
		{
			OwningSubjectiveAgent = OwningComponent;
		}
	}

	// 如果没有主观代理组件，则直接返回
	if (!OwningSubjectiveAgent) return;
	
	// 检查是否有移动特征，更新速度
	if (OwningSubjectiveAgent->HasTrait(FMoving::StaticStruct()))
	{
		FMoving MovingTrait;
		OwningSubjectiveAgent->GetTrait(MovingTrait);
		// 计算当前速度大小
		Speed = MovingTrait.CurrentVelocity.Size();
	}

	// 检查是否有动画特征，更新欢呼状态
	if (OwningSubjectiveAgent->HasTrait(FAnimating::StaticStruct()))
	{
		FAnimating AnimatingTrait;
		OwningSubjectiveAgent->GetTrait(AnimatingTrait);
		// 检查当前蒙太奇槽位是否为4（欢呼动画槽位）
		bIsCheer = AnimatingTrait.CurrentMontageSlot == 4 ? true : false;
	}
}
