/**
 * @file RTSPawn.h
 * @brief RTS Pawn头文件
 * @details 定义了RTS游戏中的Pawn类，提供相机控制和移动功能
 *          包含弹簧臂相机系统，支持俯视角度和缩放功能
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RTSPawn.generated.h"

// 前向声明
class UFloatingPawnMovement;
class UCameraComponent;
class USpringArmComponent;

/**
 * @brief RTS Pawn类
 * @details 该类继承自APawn，专门用于RTS游戏的相机控制
 *          提供弹簧臂相机系统，支持俯视角度、缩放和移动功能
 */
UCLASS()
class ANTTEST_API ARTSPawn : public APawn
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details 初始化RTS Pawn的组件，包括根组件、弹簧臂、相机和移动组件
	 */
	ARTSPawn();

private:
	/** @brief 根组件，作为所有子组件的父级 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Root", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;
	
	/** @brief 弹簧臂组件，用于相机跟随和缩放控制 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* SpringArm;

	/** @brief 跟随相机组件，提供视角渲染 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowedCamera;

	/** @brief 浮动Pawn移动组件，提供移动功能 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	UFloatingPawnMovement* FloatingPawnMovement;

public:
	/**
	 * @brief 获取弹簧臂组件
	 * @return 返回弹簧臂组件的指针
	 * @details 提供访问弹簧臂组件的接口，用于外部控制相机缩放
	 */
	FORCEINLINE USpringArmComponent* GetSpringArm() const {return SpringArm;}
};
