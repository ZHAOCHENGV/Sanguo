/**
 * @file GeneralActorBase.h
 * @brief 将领Actor基类头文件
 * @details 定义了将领Actor的基类，提供将领单位的基本功能
 *          包括骨骼网格组件、主观代理组件和特征初始化等
 * @author AntTest Team
 * @date 2024
 */

#pragma once

#include "CoreMinimal.h"
#include "BattleFrameInterface.h"
#include "AntTypes/AntStructs.h"
#include "GameFramework/Actor.h"
#include "Traits/Team.h"
#include "GeneralActorBase.generated.h"

// 前向声明
class AFlowField;
class UBFSubjectiveAgentComponent;

/**
 * @brief 将领Actor基类
 * @details 该类继承自AActor并实现IBattleFrameInterface，用于创建将领单位
 *          提供将领的基本功能，包括坐骑和将领的骨骼网格、主观代理组件等
 */
UCLASS()
class ANTTEST_API AGeneralActorBase : public AActor, public IBattleFrameInterface
{
	GENERATED_BODY()
	
public:	
	/**
	 * @brief 构造函数
	 * @details 初始化将领Actor的组件，包括根组件、骨骼网格组件和主观代理组件
	 */
	AGeneralActorBase();

protected:
	/**
	 * @brief 游戏开始时调用
	 * @details 初始化主观代理组件，设置特征并激活代理
	 */
	virtual void BeginPlay() override;
	
private:
	/**
	 * @brief 初始化主观代理
	 * @details 设置将领的各种特征，包括血量、导航、索敌、伤害等
	 */
	void InitializeSubjectiveAgent();
	
	/** @brief 根组件，作为所有子组件的父级 */
	UPROPERTY()
	USceneComponent* Root;
	
	/** @brief 将领骨骼网格组件，渲染将领的3D模型 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SKeletalMesh", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* GeneralSkeletalMeshComponent;

	/** @brief 坐骑骨骼网格组件，渲染坐骑的3D模型 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SKeletalMesh", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* MountSkeletalMeshComponent;
	
	/** @brief 主观代理组件，处理将领的AI逻辑和特征 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Subjective", meta = (AllowPrivateAccess = "true"))
	UBFSubjectiveAgentComponent* SubjectiveAgentComponent;

	/** @brief 将领数据，包含血量、伤害等属性 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	FGeneralData GeneralData;
	
	/** @brief 使用的流场，用于导航 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<AFlowField> FlowFieldToUse;

	/** @brief 队伍结构体数组，用于标识不同队伍的特征 */
	UPROPERTY(Transient)
	TArray<UScriptStruct*> TeamStructs
	{
		FTeam0::StaticStruct(),
		FTeam1::StaticStruct(),
		FTeam2::StaticStruct(),
		FTeam3::StaticStruct(),
		FTeam4::StaticStruct(),
		FTeam5::StaticStruct(),
		FTeam6::StaticStruct(),
		FTeam7::StaticStruct(),
		FTeam8::StaticStruct(),
		FTeam9::StaticStruct()
	};
};
