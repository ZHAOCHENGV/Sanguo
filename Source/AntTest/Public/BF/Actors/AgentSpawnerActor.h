/**
 * @file AgentSpawnerActor.h
 * @brief 兵员生成器Actor头文件
 * @details 定义用于批量生成将领/士兵主体的Actor，支持读入配置并设置战斗、导航、动画等特征，
 *          以方阵方式在场景中布置。
 *
 * 使用方式（示例）：
 * 1. 在关卡中放置本Actor，设置`AgentSpawnerConfigDataAsset`（见`UAgentSpawnerConfigDataAsset`）。
 * 2. 选择生成类型（将领/士兵）与士兵细分类型；配置队/组/兵ID与是否为蓝队等。
 * 3. 调用`SpawnAgents`（可蓝图调用）或在`BeginPlay`中按需触发。
 *
 * @note 本类只负责“生成与初始特征配置”，后续行为（巡逻、AI状态机、任务系统）应由各系统自行驱动。
 * @see UAgentSpawnerConfigDataAsset, FAgentSpawnerData
 * @author AntTest Team
 * @date 2024
 */

#pragma once

#include "CoreMinimal.h"
#include "AgentSpawner.h"
#include "AntTypes/AntEnums.h"
#include "AntTypes/AntStructs.h"
#include "BF/Traits/Group.h"
#include "BF/Traits/Unit.h"
#include "Traits/Team.h"
#include "AgentSpawnerActor.generated.h"

/**
 * @brief 远程相关占位结构
 * @details 目前作为样例占位，后续可扩展特定远程配置。
 */
USTRUCT()
struct FRanged
{
	GENERATED_BODY()
};

class UAgentSpawnerConfigDataAsset;

/**
 * @brief 兵员生成器Actor
 * @details 继承自`AAgentSpawner`，用于基于数据资产批量生成将领/士兵主体，并下发各类特征。
 * @note 支持在编辑器中可视化阵型范围与朝向；运行时按方阵生成主体并激活。
 */
UCLASS()
class ANTTEST_API AAgentSpawnerActor : public AAgentSpawner
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details 初始化场景根、箭头、包围盒与广告牌组件。
	 */
	AAgentSpawnerActor();
	
protected:
	/**
	 * @brief 游戏开始回调
	 * @details 可在此加载配置并触发生成。
	 */
	virtual void BeginPlay() override;
	
#if WITH_EDITOR
	/**
	 * @brief 构造时回调（编辑器）
	 * @details 根据方阵配置更新包围盒可视化。
	 * @param Transform 放置时的世界变换。
	 */
	virtual void OnConstruction(const FTransform& Transform) override;
#endif
	
private:
	/**
	 * @brief 计算方阵阵型的世界变换列表
	 * @details 以Actor为中心，按`SquareSize`（行列）与`SquareSpacing`（间距）对称分布，
	 *          计算每个单元格的世界位置（仅设置Location）。会应用本Actor的世界旋转。
	 * @return 仅包含位置（Location）的变换数组，长度为`SquareSize.X * SquareSize.Y`。
	 * @warning 若`SquareSize`有任一分量小于等于0，返回数组可能为空。
	 */
	TArray<FTransform> SquareFormation();
	
	/**
	 * @brief 根据配置生成主体
	 * @details 按`AgentSoldierType`选择不同配置源，读取动画/渲染/索敌/攻击/追踪/睡眠等参数，
	 *          批量设置到主体上，并按方阵位置落点与激活。
	 * @param DataAsset 生成配置数据资产（需非空）。
	 * @note 蓝图可调用；若要在`BeginPlay`自动生成，可在其中调用本函数。
	 */
	UFUNCTION(BlueprintCallable)
	void SpawnAgents(UAgentSpawnerConfigDataAsset* DataAsset);

	/**
	 * @name 组件（编辑器可视化与组织）
	 * @{ 
	 */
	/** 根场景组件（世界层级的父节点） */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> DefaultRoot;

	/** 朝向箭头（仅编辑器可视化，指示生成队列朝向） */
	UPROPERTY(VisibleAnywhere)
	UArrowComponent* Arrow;

	/** 可视化包围盒（仅编辑器参考），Z大小固定，X/Y取决于阵型与间距 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Box;

	/** 广告牌（仅编辑器参考，方便定位） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Billboard", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBillboardComponent> Billboard;
	/** @} */
	
	/**
	 * @name 基础配置（影响生成类型与外观）
	 * @{ 
	 */
	/**
	 * @brief 生成配置数据资产
	 * @details 提供将领与士兵两套配置与特征模板。
	 * @note 需在编辑器中指定；运行时可选择同步加载。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UAgentSpawnerConfigDataAsset> AgentSpawnerConfigDataAsset;

	/**
	 * @brief 生成兵种类型
	 * @details `General` 走将领配置，`Soldier` 走士兵配置。
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	EAgentSoldierType AgentSoldierType = EAgentSoldierType::Soldier;

	/**
	 * @brief 士兵细分类
	 * @details 仅当`AgentSoldierType == Soldier`时生效；映射到`SoldierConfig`中的键。
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true", EditConditionHides, EditCondition = "(AgentSoldierType) == EAgentSoldierType::Soldier"))
	ESoldierType SoldierType = ESoldierType::DaoBing;
	/** @} */

	/**
	 * @name 阵营/标识（影响渲染/过滤/友伤等）
	 * @{ 
	 */
	/** 是否为蓝队（影响`FAnimating::Team`着色等） */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	bool bIsBlueTeam = false;

	/** 队伍ID [0,9]，用于Team特征筛选与友伤过滤 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData|ID", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "9"))
	int32 TeamID = 0;

	/** 组ID [0,9]，用于自定义分组（如编队/队列） */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData|ID", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "9"))
	int32 GroupID = 0;

	/** 单位ID [0,9]，用于自定义单位标识（如前排/后排） */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData|ID", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "9"))
	int32 UnitID = 0;
	/** @} */

	/**
	 * @name 行为开关与阵型参数
	 * @{ 
	 */
	/** 是否启用睡眠特征（`FSleep`） */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	bool bIsEnableSleep = false;
	
	/** 方阵行列（X=行数，Y=列数；总数量= X*Y） */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	FIntPoint SquareSize = FIntPoint(4, 8);

	/** 方阵间距（单位：cm；X=前后、Y=左右） */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	FVector2D SquareSpacing = FVector2D(100.f, 100.f);
	/** @} */

	/**
	 * @name 导航
	 * @{ 
	 */
	/** 使用的流场（导航图资源） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ConfigData|Navigation", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<AFlowField> FlowFieldToUse;
	/** @} */
};
