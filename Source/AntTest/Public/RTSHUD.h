/**
 * @file RTSHUD.h
 * @brief RTS HUD头文件
 * @details 定义了RTS游戏的HUD系统，包括选择框绘制、单位血条显示等功能
 *          提供游戏界面渲染和用户交互功能
 * @author AntTest Team
 * @date 2024
 */

#pragma once

#include "Ant/public/AntHandle.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "RTSHUD.generated.h"

// 前向声明
class ARTSPlayerController;

/**
 * @brief RTS HUD类
 * @details 该类继承自AHUD，用于绘制RTS游戏的用户界面
 *          包括选择框、单位血条等UI元素
 */
UCLASS()
class ANTTEST_API ARTSHUD : public AHUD
{
	GENERATED_BODY()

public:
	/**
	 * @brief 绘制选择框
	 * @param Selection 选择框的2D边界
	 * @details 设置选择框并启用绘制标志
	 */
	void DrawSelectionBox(const FBox2f &Selection);

	/** @brief 选中的单位列表 */
	TSharedPtr<TArray<FAntHandle>> SelectedUnits;

protected:
	/**
	 * @brief HUD的主要绘制调用
	 * @details 绘制游戏界面元素，包括选中单位的血条和选择框
	 */
	virtual void DrawHUD() override;

private:
	/** @brief 选择框的2D边界 */
	FBox2f SelectionBox;
	/** @brief 是否绘制选择框的标志 */
	bool DrawSelectionbox = false;

public:
	/**
	 * @brief 获取选中的单位列表
	 * @return 返回选中单位的共享指针
	 */
	FORCEINLINE TSharedPtr<TArray<FAntHandle>> GetSelectedUnits() const { return SelectedUnits; }

	/**
	 * @brief 获取是否绘制选择框
	 * @return 返回绘制选择框的标志
	 */
	FORCEINLINE bool GetDrawSelectionbox() const { return DrawSelectionbox; }
};

/**
 * @brief RTS单位管理类
 * @details 该类继承自AActor，用于管理RTS游戏中的单位选择、移动等功能
 *          提供单位选择、移动和渲染功能
 */
UCLASS()
class ANTTEST_API ARTSUnits : public AActor
{
	GENERATED_BODY()
	
public:	
	/**
	 * @brief 构造函数
	 * @details 初始化RTSUnits，创建选中单位数组并启用Tick功能
	 */
	ARTSUnits();

	/**
	 * @brief 根据给定区域选择屏幕上的代理
	 * @param ScreenArea 屏幕选择区域
	 * @details 在指定屏幕区域内选择代理单位
	 */
	void SelectOnScreenAgents(const FBox2f &ScreenArea);

	/**
	 * @brief 将代理移动到指定位置
	 * @param WorldLocation 目标世界位置
	 * @details 命令选中的代理移动到指定位置
	 */
	void MoveAgentsToLocation(const FVector &WorldLocation);

	/**
	 * @brief 获取选中的单位列表
	 * @return 返回选中单位的数组引用
	 */
	UFUNCTION(BlueprintPure)
	TArray<FAntHandle>& GetSelectedUnits();
	
	/** @brief 地面碰撞通道 */
	UPROPERTY(Config, EditAnywhere)
	TEnumAsByte<ECollisionChannel> FloorChannel;

	/** @brief 单位网格资产 */
	UPROPERTY(EditAnywhere)
	UStaticMesh *UnitMesh = nullptr;

	/**
	 * @brief 处理鼠标左键释放事件
	 * @param bIsReleased 是否已释放
	 * @details 处理选择框绘制和单位选择逻辑
	 */
	UFUNCTION()
	void OnLeftMouseReleased(bool bIsReleased);

protected:
	/**
	 * @brief 游戏开始时调用
	 * @details 初始化单位管理系统
	 */
	virtual void BeginPlay() override;

	/**
	 * @brief 每帧更新函数
	 * @param DeltaTime 帧间隔时间
	 * @details 处理单位选择和更新逻辑
	 */
	virtual void Tick(float DeltaTime) override;

	/**
	 * @brief 更新单位选择
	 * @param DeltaTime 帧间隔时间
	 * @details 处理单位选择的更新逻辑
	 */
	void UpdateUnitSelection(float DeltaTime);

private:
	/** @brief RTS玩家控制器引用 */
	TObjectPtr<ARTSPlayerController> RTSPC;
	
	/**
	 * @brief 到达目标回调函数
	 * @param Agents 到达目标的代理数组
	 * @details 当代理到达目标位置时调用
	 */
	void OnDestReached(const TArray<FAntHandle> &Agents);
	
	/**
	 * @brief 速度超时回调函数
	 * @param Agents 速度超时的代理数组
	 * @details 当代理移动速度超时时调用
	 */
	void OnVelocityTimeout(const TArray<FAntHandle> &Agents);

	/**
	 * @brief 设置实例化网格组件
	 * @details 初始化用于渲染单位的实例化网格组件
	 */
	void SetupInstancedMeshComp();
	
	/**
	 * @brief 渲染单位
	 * @details 使用实例化网格组件渲染选中的单位
	 */
	void RenderUnits();

	/** @brief 鼠标左键是否已按下的标志 */
	bool MLBDown = false;
	
	/** @brief 鼠标右键是否已按下的标志 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly , meta = (allowPrivateAccess = "true"))
	bool MRBDown = false;

	/** @brief 是否正在绘制的标志 */
	bool bIsDraw;

	/** @brief 鼠标左键是否已释放的标志 */
	bool bIsLeftMouseReleased = false;

	/** @brief 按住时间 */
	float HoldTime = 0.0f;

	/** @brief 移动组标志，用于避免不同组之间的碰撞 */
	uint8 MoveGroupFlag = 2;

	/** @brief 屏幕选择矩形的起始位置 */
	FVector2f StartSelectionPos;

	/** @brief 当前选中单位的列表 */
	TSharedPtr<TArray<FAntHandle>> SelectedUnits;

	/** @brief 实例化静态网格组件，用于渲染单位 */
	UPROPERTY()
	UInstancedStaticMeshComponent* InstancedMeshComp = nullptr;
};
