/**
 * @file RTSPlayerController.cpp
 * @brief RTS玩家控制器实现文件
 * @details 该文件实现了RTS游戏中的玩家控制器，处理鼠标和键盘输入，控制相机移动和缩放
 *          支持鼠标点击检测、相机移动、缩放等功能
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#include "Controllers/RTSPlayerController.h"

#include "Actors/CollisionActor.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "RTSHUD.h"
#include "GameFramework/SpringArmComponent.h"
#include "Pawns/RTSPawn.h"
#include "InputActionValue.h"
#include "GameFramework/FloatingPawnMovement.h"

/**
 * @brief 构造函数
 * @details 初始化RTS玩家控制器，设置鼠标光标显示
 */
ARTSPlayerController::ARTSPlayerController()
{
	// 显示鼠标光标
	SetShowMouseCursor(true);
}

void ARTSPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ARTSPawn* RTSPawn = Cast<ARTSPawn>(GetPawn()))
	{
		USpringArmComponent* SpringArm = RTSPawn->GetSpringArm();
		if (SpringArm)
		{
			// 平滑缩放到目标长度
			if (DesiredArmLength < 0.f)
			{
				DesiredArmLength = SpringArm->TargetArmLength;
			}
			SpringArm->TargetArmLength = FMath::FInterpTo(
				SpringArm->TargetArmLength,
				FMath::Clamp(DesiredArmLength, MinArmLength, MaxArmLength),
				DeltaSeconds,
				ZoomInterpSpeed);
		}
	}
}

/**
 * @brief 处理鼠标右键点击事件
 * @param MouseLocation 鼠标点击位置
 * @details 检测鼠标点击位置，判断是否点击到碰撞Actor，并触发相应事件
 */
void ARTSPlayerController::OnMouseRightButtonClicked(FVector2D MouseLocation)
{
	FHitResult	HitResult;

	// 将屏幕坐标转换为世界坐标
	FVector MouseWorldPosition;
	FVector MouseWorldDirection;
	DeprojectScreenPositionToWorld(MouseLocation.X, MouseLocation.Y, MouseWorldPosition, MouseWorldDirection);

	// 计算射线终点
	const FVector EndLocation = MouseWorldPosition + MouseWorldDirection * 10000.f;
	FCollisionQueryParams CollisionParams;
	//CollisionParams.bDebugQuery = true;
	
	// 执行射线检测
	GetWorld()->LineTraceSingleByChannel(HitResult, MouseWorldPosition, EndLocation, ECC_Visibility, CollisionParams);

	// 如果射线击中了物体
	if (HitResult.bBlockingHit)
	{
		// 检查是否击中了碰撞Actor
		if (ACollisionActor* CollisionActor = Cast<ACollisionActor>(HitResult.GetActor()))
		{
			// 如果未在绘制状态，则触发进入代理位置事件
			if (!bIsDraw)
			{
				CollisionActor->OnEnterAgentLocation();
			}
			// 广播鼠标左键按下事件（false表示未释放）
			OnLeftMouseDown.Broadcast(false);
			UE_LOG(LogTemp, Warning, TEXT("获取到 %s"), *HitResult.GetActor()->GetName());
			return;
		}
	}

	// 如果没有击中碰撞Actor，广播鼠标左键按下事件（true表示已释放）
	OnLeftMouseDown.Broadcast(true);
}

/**
 * @brief 游戏开始时调用
 * @details 初始化增强输入系统，添加输入映射上下文
 */
void ARTSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 检查输入映射上下文是否有效
	checkf(InputMappingContext, TEXT("InputMappingContext无效"));
	
	// 获取增强输入本地玩家子系统
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	// 添加输入映射上下文
	Subsystem->AddMappingContext(InputMappingContext, 0);

	// 初始化移动速度到基础速度
	if (ARTSPawn* RTSPawn = Cast<ARTSPawn>(GetPawn()))
	{
		if (UFloatingPawnMovement* MoveComp = RTSPawn->FindComponentByClass<UFloatingPawnMovement>())
		{
			MoveComp->MaxSpeed = BaseMoveSpeed;
		}
	}
}

/**
 * @brief 设置输入组件
 * @details 绑定各种输入动作到对应的处理函数
 */
void ARTSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 检查移动动作是否有效
	check(IA_MoveAction);

	// 获取增强输入组件
	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
	
	// 绑定各种输入动作
	EnhancedInputComponent->BindAction(IA_MoveAction, ETriggerEvent::Triggered, this, &ARTSPlayerController::Input_Move);
	EnhancedInputComponent->BindAction(IA_ZoomAction, ETriggerEvent::Triggered, this, &ARTSPlayerController::Input_Zoom);
	EnhancedInputComponent->BindAction(IA_LeftButton, ETriggerEvent::Started, this, &ARTSPlayerController::Input_LeftButtonPressed);
	EnhancedInputComponent->BindAction(IA_LeftButton, ETriggerEvent::Completed, this, &ARTSPlayerController::Input_LeftButtonReleased);
	if (IA_LookAction)
	{
		EnhancedInputComponent->BindAction(IA_LookAction, ETriggerEvent::Triggered, this, &ARTSPlayerController::Input_Look);
	}

	// 修饰键：Ctrl 旋转、Shift 加速
	if (IA_RotateModifier)
	{
		EnhancedInputComponent->BindAction(IA_RotateModifier, ETriggerEvent::Started, this, &ARTSPlayerController::Input_RotateModifierStarted);
		EnhancedInputComponent->BindAction(IA_RotateModifier, ETriggerEvent::Completed, this, &ARTSPlayerController::Input_RotateModifierCompleted);
	}
	if (IA_SpeedModifier)
	{
		EnhancedInputComponent->BindAction(IA_SpeedModifier, ETriggerEvent::Started, this, &ARTSPlayerController::Input_SpeedModifierStarted);
		EnhancedInputComponent->BindAction(IA_SpeedModifier, ETriggerEvent::Completed, this, &ARTSPlayerController::Input_SpeedModifierCompleted);
	}
}

/**
 * @brief 处理移动输入
 * @param Value 输入值
 * @details 处理WASD键的移动输入，控制Pawn的移动
 */
void ARTSPlayerController::Input_Move(const FInputActionValue& Value)
{
	// 获取2D输入轴值
	FVector2D InputAxis2D = Value.Get<FVector2D>();

	// 获取控制器的Yaw旋转
	const FRotator YawRotator = FRotator(0.0f, GetControlRotation().Yaw, 0.0f);
	// 计算前进和右方向向量
	const FVector ForwardDirection = FRotationMatrix(YawRotator).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotator).GetUnitAxis(EAxis::Y);
	
	// 如果控制器有Pawn，则添加移动输入
	if (APawn* ControllerPawn = GetPawn())
	{
		ControllerPawn->AddMovementInput(ForwardDirection, InputAxis2D.X);
		ControllerPawn->AddMovementInput(RightDirection, InputAxis2D.Y);
	}
}

/**
 * @brief 处理缩放输入
 * @param Value 输入值
 * @details 处理鼠标滚轮的缩放输入，控制相机的缩放
 */
void ARTSPlayerController::Input_Zoom(const FInputActionValue& Value)
{
	// 获取缩放轴值
	const float AxisValue = Value.Get<float>();

	// 如果控制器有RTSPawn，则调整弹簧臂长度
    if (ARTSPawn* RTSPawn = Cast<ARTSPawn>(GetPawn()))
	{
		USpringArmComponent* SpringArm = RTSPawn->GetSpringArm();
		if (!SpringArm)
		{
			return;
		}
		if (DesiredArmLength < 0.f)
		{
			DesiredArmLength = SpringArm->TargetArmLength;
		}
        const float ZoomInput = bInvertZoomAxis ? -AxisValue : AxisValue;
        DesiredArmLength = FMath::Clamp(
            DesiredArmLength - ZoomInput * ZoomSpeed,
			MinArmLength,
			MaxArmLength);
	}
}

/**
 * @brief 处理鼠标左键按下事件
 * @param Value 输入值
 * @details 当鼠标左键按下时触发的事件
 */
void ARTSPlayerController::Input_LeftButtonPressed(const FInputActionValue& Value)
{
	// 广播鼠标左键按下事件（false表示未释放）
	OnLeftMouseDown.Broadcast(false);
}

/**
 * @brief 处理鼠标左键释放事件
 * @param Value 输入值
 * @details 当鼠标左键释放时触发的事件，调用鼠标右键点击处理函数
 */
void ARTSPlayerController::Input_LeftButtonReleased(const FInputActionValue& Value)
{
	// 获取鼠标点击位置
	FVector2D MouseClickedPosition;
	GetMousePosition(MouseClickedPosition.X, MouseClickedPosition.Y);
	// 调用鼠标右键点击处理函数
	OnMouseRightButtonClicked(MouseClickedPosition);
}

// 修饰键处理
void ARTSPlayerController::Input_RotateModifierStarted(const FInputActionValue& Value)
{
	bIsRotateModifierHeld = true;
	// 捕获鼠标用于旋转
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}

void ARTSPlayerController::Input_RotateModifierCompleted(const FInputActionValue& Value)
{
	bIsRotateModifierHeld = false;
	// 释放鼠标
	FInputModeGameAndUI InputMode;
	SetInputMode(InputMode);
}

void ARTSPlayerController::Input_SpeedModifierStarted(const FInputActionValue& Value)
{
	bIsSpeedModifierHeld = true;
	if (ARTSPawn* RTSPawn = Cast<ARTSPawn>(GetPawn()))
	{
		if (UFloatingPawnMovement* MoveComp = RTSPawn->FindComponentByClass<UFloatingPawnMovement>())
		{
			MoveComp->MaxSpeed = BaseMoveSpeed * SpeedBoostMultiplier;
		}
	}
}

void ARTSPlayerController::Input_SpeedModifierCompleted(const FInputActionValue& Value)
{
	bIsSpeedModifierHeld = false;
	if (ARTSPawn* RTSPawn = Cast<ARTSPawn>(GetPawn()))
	{
		if (UFloatingPawnMovement* MoveComp = RTSPawn->FindComponentByClass<UFloatingPawnMovement>())
		{
			MoveComp->MaxSpeed = BaseMoveSpeed;
		}
	}
}

void ARTSPlayerController::Input_Look(const FInputActionValue& Value)
{
	if (!bIsRotateModifierHeld)
	{
		return;
	}

	FVector2D LookAxis = Value.Get<FVector2D>();

    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
	{
		return;
	}

	// 调整Actor的Yaw，使相机绕场景旋转
    FRotator ActorRot = ControlledPawn->GetActorRotation();
	ActorRot.Yaw += LookAxis.X * RotateYawSpeed;
    ControlledPawn->SetActorRotation(ActorRot);

	// 调整俯仰：修改弹簧臂Pitch
    if (ARTSPawn* RTSPawn = Cast<ARTSPawn>(ControlledPawn))
	{
		if (USpringArmComponent* SpringArm = RTSPawn->GetSpringArm())
		{
			FRotator SpringRot = SpringArm->GetRelativeRotation();
			SpringRot.Pitch = FMath::Clamp(SpringRot.Pitch + LookAxis.Y * RotatePitchSpeed, MinPitch, MaxPitch);
			SpringArm->SetRelativeRotation(SpringRot);
		}
	}

	// 同步控制器旋转Yaw用于移动方向
	FRotator ControlRot = GetControlRotation();
	ControlRot.Yaw = ActorRot.Yaw;
	SetControlRotation(ControlRot);
}
