/**
 * @file CollisionActor.h
 * @brief 碰撞Actor头文件
 * @details 定义了用于RTS游戏中的碰撞检测和形状生成的Actor类，支持多种形状的生成和管理
 *          包括网格、圆形、三角形等不同形状的碰撞区域和可视化效果
 * @author AntTest Team
 * @date 2024
 */

#pragma once

#include "CoreMinimal.h"
#include "AntHandle.h"
#include "GameFramework/Actor.h"
#include "CollisionActor.generated.h"

// 前向声明
class USphereComponent;
class UAntSubsystem;
class UBoxComponent;

/**
 * @brief 生成形状枚举
 * @details 定义了可生成的碰撞形状类型
 */
UENUM(BlueprintType)
enum class ESpawnShape : uint8
{
	/** @brief 方形形状 */
	Square = 0,
	/** @brief 圆形形状 */
	Circle = 1,
	/** @brief 三角形形状 */
	Triangle = 2
};

/**
 * @brief 碰撞Actor类
 * @details 该类继承自AActor，用于在RTS游戏中创建和管理不同形状的碰撞区域
 *          支持网格、圆形、三角形等多种形状的生成，并提供可视化效果和碰撞检测
 */
UCLASS()
class ANTTEST_API ACollisionActor : public AActor
{
	GENERATED_BODY()
	
public:	
	/**
	 * @brief 构造函数
	 * @details 初始化碰撞Actor的组件和属性
	 */
	ACollisionActor();

	/**
	 * @brief 进入代理位置时调用
	 * @details 当代理进入碰撞区域时触发的函数
	 */
	void OnEnterAgentLocation();
	
	/**
	 * @brief 离开代理位置时调用
	 * @details 当代理离开碰撞区域时触发的函数
	 */
	void OnEndAgentLocation();

	/**
	 * @brief 生成网格形状
	 * @param CenterLocation 网格中心位置
	 * @param TileSize 瓦片大小
	 * @param TileCount 瓦片数量
	 * @details 在指定位置生成指定大小的网格形状
	 */
	UFUNCTION(BlueprintCallable)
	void SpawnGrid(const FVector& CenterLocation, const FVector& TileSize, const FIntPoint TileCount);

	/**
	 * @brief 生成三角形形状
	 * @param CenterLocation 三角形中心位置
	 * @details 在指定位置生成三角形形状
	 */
	UFUNCTION(BlueprintCallable)
	void SpawnTriangle(const FVector& CenterLocation);

	/**
	 * @brief 生成圆形形状
	 * @param CenterLocation 圆形中心位置
	 * @details 在指定位置生成圆形形状
	 */
	UFUNCTION(BlueprintCallable)
	void SpawnCircle(const FVector& CenterLocation);

	/**
	 * @brief 设置代理ID
	 * @param AntHandle Ant句柄引用
	 * @param ID 代理ID
	 * @details 为指定的Ant代理设置ID
	 */
	void SetAgentID(FAntHandle& AntHandle, int32 ID);

	/**
	 * @brief 蓝图可实现的设置代理ID函数
	 * @param AntHandle Ant句柄
	 * @param ID 代理ID
	 * @details 为蓝图提供设置代理ID的接口
	 */
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "SetAgentID")
	void BP_SetAgentID(FAntHandle AntHandle, int32 ID);

	/**
	 * @brief 检查可用地址
	 * @return 返回可用的地址ID数组
	 * @details 检查并返回当前可用的地址ID列表
	 */
	TArray<int32> CheckHasAddress();

	/**
	 * @brief 获取实例数量
	 * @return 返回当前实例的数量
	 * @details 获取实例化静态网格组件的实例数量
	 */
	int32 GetInstancedCount();

	/**
	 * @brief 绘制查询凸包体积
	 * @param AntHandles Ant句柄数组
	 * @details 为指定的Ant句柄数组绘制查询凸包体积
	 */
	void DrawQueryConvexVolume(TArray<FAntHandle>& AntHandles);

	/**
	 * @brief 生成标志Actor
	 * @details 在指定位置生成标志Actor
	 */
	void SpawnFlagActor();

	/**
	 * @brief 播放欢呼动画
	 * @details 蓝图可实现的播放欢呼动画函数
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void PlayCheerAnim();

	/**
	 * @brief 注册形状变化计时器
	 * @details 设置延迟计时器来触发组件形状变化
	 */
	void RegisterTimer();
	
	/**
	 * @brief 跟随组件变化
	 * @details 根据生成的形状类型调整碰撞体和贴花组件的参数
	 */
	void FollowChangeComponent();

protected:
	/**
	 * @brief 游戏开始时调用
	 * @details 初始化地址映射表，设置Ant子系统的回调函数
	 */
	virtual void BeginPlay() override;
	
	/**
	 * @brief 每帧更新函数
	 * @param DeltaSeconds 帧间隔时间
	 * @details 处理碰撞检测和形状更新逻辑
	 */
	virtual void Tick(float DeltaSeconds) override;
	
#if WITH_EDITOR
	/**
	 * @brief 构造时调用
	 * @param Transform 变换信息
	 * @details 在编辑器构造时初始化组件
	 */
	virtual void OnConstruction(const FTransform& Transform) override;
	
	/**
	 * @brief 属性改变后调用
	 * @param PropertyChangedEvent 属性改变事件
	 * @details 在编辑器属性改变后更新相关组件
	 */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** @brief 默认根组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* DefaultRoot;
	
	/** @brief 球形碰撞体组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	USphereComponent* SphereCollision;

	/** @brief 实例化静态网格组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	UInstancedStaticMeshComponent* InstancedStaticMesh;

	/** @brief 贴花组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decal", meta = (AllowPrivateAccess = "true"))
	UDecalComponent* Decal;

	/** @brief 碰撞半径 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	float CollisionRadius = 1000.f;

	/** @brief 是否启用绝对中心 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	bool bEnableAbsoluteCenter = false;

	/** @brief 是否启用半径大小到边界 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	bool bEnableRadiusSizeToBound = false;

	/** @brief 生成形状类型 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	ESpawnShape SpawnShape = ESpawnShape::Square;

	/** @brief 生成标志的Actor类 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AActor> SpawnFlag;

	/** @brief 查询结果数组 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Handle", meta = (AllowPrivateAccess = "true"))
	TArray<FAntHandle> QueryResult;

	/** @brief 检查数量计数器 */
	int32 CheckNum = 0;
	
	/** @brief 剩余数量 */
	int32 RemainingQty = 0;
	
	/** @brief 现有数量 */
	int32 ExistingQty = 0;

	/** @brief 是否关闭的标志 */
	bool bIsClose = false;

	/** @brief 是否正在生成的标志 */
	bool bIsSpawning = false;

	/** @brief 网格中心位置 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	FVector GridCenterLocation;

	/** @brief 网格瓦片大小 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	FVector GridTileSize = FVector(200.f, 200.f, 0);

	/** @brief 网格瓦片数量 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	FIntPoint GridTileCount = FIntPoint(10, 10);

	/** @brief 网格左下角位置 */
	FVector GridBottomLeftCornerLocation;

	/** @brief 地址映射表，存储地址ID和是否被占用的映射关系 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map", meta = (AllowPrivateAccess = "true"))
	TMap<int32, bool> AddressMap;

	/** @brief 圆形数量 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	int32 CircleQty = 5;
	
	/** @brief 圆形起始数量 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	int32 CircleStartQty = 4;
	
	/** @brief 圆形半径 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	float CircleRadius = 100.f;
	
	/** @brief 圆形间隔 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	float CircleInterval = 60.f;
	
	/** @brief 圆形角度 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	float CircleAngle = 360.f;

	/** @brief 中心点位置 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector", meta = (MakeEditWidget = "true", AllowPrivateAccess = "true"))
	FVector CenterPoint;

	/** @brief 查询位置 */
	FVector QueryLocation;

	/** @brief 三角形行数 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Triangle", meta = (AllowPrivateAccess = "true"))
	int32 TriangleRowNum = 5;
	
	/** @brief 三角形XY间隔 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Triangle", meta = (AllowPrivateAccess = "true"))
	FVector2D TriangleXYInterval = FVector2D(200.f, 200.f);
	
	/** @brief 三角形角度 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Triangle", meta = (AllowPrivateAccess = "true"))
	float TriangleAngle = 0.f;

	/** @brief 形状变化计时器句柄 */
	FTimerHandle ChangeShapeTimer;
};