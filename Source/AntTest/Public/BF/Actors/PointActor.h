/**
 * @file PointActor.h
 * @brief 点Actor头文件
 * @details 定义了用于标记位置的点Actor类，提供简单的静态网格渲染功能
 *          通常用于标记目标点、路径点等位置信息
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SubjectiveActor.h"
#include "PointActor.generated.h"

/**
 * @brief 点Actor类
 * @details 该类继承自ASubjectiveActor，用于在场景中标记特定的位置点
 *          提供简单的静态网格渲染，通常用于标记目标点、路径点等
 */
UCLASS()
class ANTTEST_API APointActor : public ASubjectiveActor
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数
	 * @details 初始化点Actor，创建静态网格组件用于渲染
	 */
	APointActor();

private:
	/** @brief 静态网格组件，用于渲染点Actor的3D模型 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* StaticMeshComponent;

	/** @brief 点网格资产，定义点Actor的3D模型 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	UStaticMesh* PointMesh;
};
