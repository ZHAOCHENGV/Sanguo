/**
 * @file RTSPawn.cpp
 * @brief RTS Pawn实现文件
 * @details 该文件实现了RTS游戏中的Pawn类，提供相机控制和移动功能
 *          包含弹簧臂相机系统，支持俯视角度和缩放功能
 * @author AntTest Team
 * @date 2024
 */

#include "Pawns/RTSPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"

/**
 * @brief 构造函数
 * @details 初始化RTS Pawn的组件，包括根组件、弹簧臂、相机和移动组件
 */
ARTSPawn::ARTSPawn()
{
	// 禁用Tick功能，因为不需要每帧更新
	PrimaryActorTick.bCanEverTick = false;

	// 创建根组件
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 创建弹簧臂组件，用于相机跟随
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	// 设置弹簧臂的初始旋转角度，实现俯视效果
	SpringArm->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	// 设置弹簧臂的目标长度，控制相机距离
	SpringArm->TargetArmLength = 3000.f;

	// 创建跟随相机组件
	FollowedCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowedCamera"));
	FollowedCamera->SetupAttachment(SpringArm);

	// 创建浮动Pawn移动组件，提供移动功能
	FloatingPawnMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingPawnMovement"));
}

