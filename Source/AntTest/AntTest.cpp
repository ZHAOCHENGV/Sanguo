/**
 * @file AntTest.cpp
 * @brief AntTest模块主实现文件
 * @details 该文件实现了AntTest模块的主要功能，包括游戏模块实现和Ant辅助函数
 *          提供将Ant代理变换复制到Skelot组件的功能
 * @author AntTest Team
 * @date 2024
 */

// Copyright Epic Games, Inc. All Rights Reserved.

#include "AntTest.h"

#include "AntFunctionLibrary.h"
#include "Modules/ModuleManager.h"
#include "AntUtil.h"

/**
 * @brief 实现主游戏模块
 * @details 使用默认游戏模块实现AntTest模块
 */
IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, AntTest, "AntTest" );

/**
 * @brief 将敌人变换复制到Skelot组件
 * @param WorldContextObject 世界上下文对象
 * @param SkelotCom Skelot组件
 * @param Flags 标志位，用于过滤代理
 * @details 将指定标志位的Ant代理的变换信息复制到Skelot组件中，实现骨骼动画同步
 */
void UAntHelperFunctionLibrary::CopyEnemyTranformsToSkelot(const UObject *WorldContextObject, USkelotComponent *SkelotCom, int32 Flags)
{
	// 获取Ant子系统
	auto *ant = WorldContextObject ? WorldContextObject->GetWorld()->GetSubsystem<UAntSubsystem>() : nullptr;;
	if (ant && SkelotCom)
		// 遍历所有底层代理
		for (const auto &agent : ant->GetUnderlyingAgentsList())
			// 检查代理标志位是否匹配
			if (CHECK_BIT_ANY(agent.GetFlag(), Flags))
			{
				// 获取代理的用户数据
				const auto &unitData = ant->GetAgentUserData(agent.GetHandle()).Get<FUnitData>();

				// 根据当前代理位置设置动画实例变换
				const auto scale = SkelotCom->GetInstanceTransform(unitData.SkelotInstanceID).GetScale3D();
				const FTransform3f transform(FQuat4f(FVector3f::UpVector, agent.FaceAngle - RAD_90), agent.GetLocationLerped(), scale);
				SkelotCom->SetInstanceTransform(unitData.SkelotInstanceID, transform);

				// 在生成后第一次播放动画
				if (SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.WalkAnim)
					SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.WalkAnim, true, 0, 1, 2.0f);

				// 默认播放缩放为1
				SkelotCom->SetInstancePlayScale(unitData.SkelotInstanceID, 1.0f);

				// 根据当前速度调整播放缩放
				if (agent.GetVelocity().SizeSquared() <= 10)
					SkelotCom->SetInstancePlayScale(unitData.SkelotInstanceID, 0.9f);

				if (agent.GetVelocity().SizeSquared() <= 7)
					SkelotCom->SetInstancePlayScale(unitData.SkelotInstanceID, 0.7f);

				if (agent.GetVelocity().SizeSquared() <= 4)
					SkelotCom->SetInstancePlayScale(unitData.SkelotInstanceID, 0.5f);
			}
}

/**
 * @brief 将英雄变换复制到Skelot组件
 * @param WorldContextObject 世界上下文对象
 * @param SkelotCom Skelot组件
 * @param HeroHandle 英雄代理句柄
 * @details 将指定英雄代理的变换信息复制到Skelot组件中，实现英雄骨骼动画同步
 */
void UAntHelperFunctionLibrary::CopyHeroTransformToSkelot(const UObject *WorldContextObject, USkelotComponent *SkelotCom, FAntHandle HeroHandle)
{
	// 获取Ant子系统
	auto *ant = WorldContextObject ? WorldContextObject->GetWorld()->GetSubsystem<UAntSubsystem>() : nullptr;;
	if (ant && SkelotCom && ant->IsValidAgent(HeroHandle))
	{
		// 获取代理的用户数据和代理数据
		const auto &unitData = ant->GetAgentUserData(HeroHandle).Get<FUnitData>();
		const auto &agentData = ant->GetAgentData(HeroHandle);

		// 根据当前代理位置设置动画实例变换
		const auto scale = SkelotCom->GetInstanceTransform(unitData.SkelotInstanceID).GetScale3D();
		const FTransform3f transform(FQuat4f(FVector3f::UpVector, agentData.FaceAngle - RAD_90), agentData.GetLocationLerped(), scale);
		SkelotCom->SetInstanceTransform(unitData.SkelotInstanceID, transform);

		// 根据速度状态播放相应动画
		if (agentData.GetVelocity() != FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.WalkAnim)
			SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.WalkAnim, true, 0, 1, 0.0f);
		else if (agentData.GetVelocity() == FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.IdleAnim)
			SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.IdleAnim, true, 0, 1, 0.4f);
	}
}

/**
 * @brief 将敌人变换复制到Skelot组件（新版本）
 * @param WorldContextObject 世界上下文对象
 * @param SkelotCom Skelot组件
 * @param Handle 代理句柄数组
 * @details 将指定代理句柄数组中的代理变换信息复制到Skelot组件中，支持欢呼动画状态
 */
void UAntHelperFunctionLibrary::CopyEnemyTranformsToSkelot_New(const UObject *WorldContextObject, USkelotComponent *SkelotCom, TArray<FAntHandle> Handle)
{
	// 获取Ant子系统
	auto *ant = WorldContextObject ? WorldContextObject->GetWorld()->GetSubsystem<UAntSubsystem>() : nullptr;;
	// 遍历所有代理句柄
	for (const auto& HeroHandle : Handle)
	{
		if (ant && SkelotCom && ant->IsValidAgent(HeroHandle))
		{
			// 获取代理的用户数据和代理数据
			const auto &unitData = ant->GetAgentUserData(HeroHandle).Get<FUnitData>();
			const auto &agentData = ant->GetAgentData(HeroHandle);
			
			// 根据当前代理位置设置动画实例变换
			const auto scale = SkelotCom->GetInstanceTransform(unitData.SkelotInstanceID).GetScale3D();
			const FTransform3f transform(FQuat4f(FVector3f::UpVector, agentData.FaceAngle - RAD_90), agentData.GetLocationLerped(), scale);
			SkelotCom->SetInstanceTransform(unitData.SkelotInstanceID, transform);

			// 检查是否播放欢呼动画
			if (unitData.bIsPlayCheerAnim)
			{
				// 如果速度为0且当前动画不是欢呼动画，则播放欢呼动画
				if (agentData.GetVelocity() == FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.CheerAnim)
				{
					SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.CheerAnim, false, 0, 1, 0.0f);
				}
			}
			else
			{
				// 根据速度状态播放相应动画
				if (agentData.GetVelocity() != FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.WalkAnim)
					SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.WalkAnim, true, 0, 1, 0.0f);
				else if (agentData.GetVelocity() == FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.IdleAnim)
					SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.IdleAnim, true, 0, 1, 0.4f);
			}
		}
	}
}


