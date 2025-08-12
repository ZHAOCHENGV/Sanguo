/**
 * @file BFAnimInstanceBase.h
 * @brief BattleFrame动画实例基类头文件
 * @details 定义了BattleFrame动画实例的基类，提供动画状态管理功能
 *          包括速度检测、欢呼动画状态等
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BFAnimInstanceBase.generated.h"

// 前向声明
class UBFSubjectiveAgentComponent;

/**
 * @brief BattleFrame动画实例基类
 * @details 该类继承自UAnimInstance，用于管理BattleFrame系统中的动画状态
 *          提供速度检测、欢呼动画状态等功能
 */
UCLASS()
class ANTTEST_API UBFAnimInstanceBase : public UAnimInstance
{
	GENERATED_BODY()

protected:
	/**
	 * @brief 本地初始化动画
	 * @details 在动画实例初始化时获取主观代理组件引用
	 */
	virtual void NativeInitializeAnimation() override;
	
	/**
	 * @brief 本地线程安全更新动画
	 * @param DeltaSeconds 帧间隔时间
	 * @details 在动画更新时检查主观代理组件的状态，更新速度和动画状态
	 */
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

private:
	/** @brief 所属的主观代理组件引用 */
	TObjectPtr<UBFSubjectiveAgentComponent> OwningSubjectiveAgent;

	/** @brief 当前移动速度，用于动画混合 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	float Speed = 0.f;

	/** @brief 是否正在播放欢呼动画的标志 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Anim", meta = (AllowPrivateAccess = "true"))
	bool bIsCheer = false;
};
