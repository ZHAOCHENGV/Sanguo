/**
 * @file AntTest.Target.cs
 * @brief AntTest游戏目标构建配置文件
 * @details 该文件定义了AntTest游戏的目标构建规则
 *          指定了游戏类型、构建设置版本和包含的模块
 * @author AntTest Team
 * @date 2024
 */

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * @brief AntTest游戏目标构建规则类
 * @details 定义AntTest游戏的构建目标配置
 */
public class AntTestTarget : TargetRules
{
	/**
	 * @brief 构造函数
	 * @param Target 目标信息
	 * @details 初始化AntTest游戏目标的构建配置
	 */
	public AntTestTarget(TargetInfo Target) : base(Target)
	{
		// 设置为游戏类型目标
		Type = TargetType.Game;
		// 使用V5构建设置版本
		DefaultBuildSettings = BuildSettingsVersion.V5;
		// 使用Unreal 5.5包含顺序版本
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		// 添加AntTest模块
		ExtraModuleNames.Add("AntTest");
	}
}
