/**
 * @file PointActor.cpp
 * @brief 点Actor实现文件
 * @details 该文件实现了用于标记位置的点Actor，提供简单的静态网格渲染功能
 *          通常用于标记目标点、路径点等位置信息
 * @author AntTest Team
 * @date 2024
 */

// Fill out your copyright notice in the Description page of Project Settings.

#include "BF/Actors/PointActor.h"

/**
 * @brief 构造函数
 * @details 初始化点Actor，创建静态网格组件用于渲染
 */
APointActor::APointActor()
{
	// 禁用Tick功能，因为不需要每帧更新
	PrimaryActorTick.bCanEverTick = false;

	// 创建静态网格组件
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(GetRootComponent());
}
