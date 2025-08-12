/**
 * @file RTSPlayerController.h
 * @brief RTS玩家控制器头文件
 * @details 定义了RTS游戏中的玩家控制器类，处理鼠标和键盘输入，控制相机移动和缩放
 *          支持鼠标点击检测、相机移动、缩放等功能
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTSPlayerController.generated.h"

// 前向声明
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * @brief 鼠标左键按下委托
 * @param bIsReleased 是否已释放的标志
 * @details 当鼠标左键状态改变时触发的委托
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeftMouseDown, bool, bIsReleased);

/**
 * @brief RTS玩家控制器类
 * @details 该类继承自APlayerController，专门用于RTS游戏的玩家控制
 *          处理鼠标和键盘输入，控制相机移动和缩放，支持鼠标点击检测
 */
UCLASS()
class ANTTEST_API ARTSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details 初始化RTS玩家控制器，设置鼠标光标显示
	 */
	ARTSPlayerController();

	/**
	 * @brief 处理鼠标右键点击事件
	 * @param MouseLocation 鼠标点击位置
	 * @details 检测鼠标点击位置，判断是否点击到碰撞Actor，并触发相应事件
	 */
	UFUNCTION(BlueprintCallable)
	void OnMouseRightButtonClicked(FVector2D MouseLocation);

	/** @brief 鼠标左键是否释放的标志 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsLeftMouseReleased = false;

	/** @brief 鼠标左键按下事件委托 */
	UPROPERTY(BlueprintCallable)
	FOnLeftMouseDown OnLeftMouseDown;

protected:
	/**
	 * @brief 游戏开始时调用
	 * @details 初始化增强输入系统，添加输入映射上下文
	 */
	virtual void BeginPlay() override;
	
	/**
	 * @brief 设置输入组件
	 * @details 绑定各种输入动作到对应的处理函数
	 */
	virtual void SetupInputComponent() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZoomSpeed = 1000;

private:

	
	
	/** @brief 是否正在绘制的标志 */
	bool bIsDraw = false;
	
	/** @brief 输入映射上下文，定义输入映射关系 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InputMappingContext;

	/** @brief 移动动作，处理WASD键移动 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_MoveAction;

	/** @brief 缩放动作，处理鼠标滚轮缩放 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_ZoomAction;

	/** @brief 鼠标左键动作，处理鼠标左键点击 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_LeftButton;

	/**
	 * @brief 处理移动输入
	 * @param Value 输入值
	 * @details 处理WASD键的移动输入，控制Pawn的移动
	 */
	void Input_Move(const FInputActionValue& Value);
	
	/**
	 * @brief 处理缩放输入
	 * @param Value 输入值
	 * @details 处理鼠标滚轮的缩放输入，控制相机的缩放
	 */
	void Input_Zoom(const FInputActionValue& Value);
	
	/**
	 * @brief 处理鼠标左键按下事件
	 * @param Value 输入值
	 * @details 当鼠标左键按下时触发的事件
	 */
	void Input_LeftButtonPressed(const FInputActionValue& Value);
	
	/**
	 * @brief 处理鼠标左键释放事件
	 * @param Value 输入值
	 * @details 当鼠标左键释放时触发的事件，调用鼠标右键点击处理函数
	 */
	void Input_LeftButtonReleased(const FInputActionValue& Value);

public:
	/**
	 * @brief 设置绘制状态
	 * @param bIsDrawing 是否正在绘制
	 * @details 设置当前是否处于绘制状态
	 */
	FORCEINLINE void SetIsDraw(bool bIsDrawing) { bIsDraw = bIsDrawing;}
};
