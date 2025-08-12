/**
 * @file AntTestEditor.Target.cs
 * @brief AntTest编辑器目标构建配置文件
 * @details 该文件定义了AntTest编辑器的目标构建规则
 *          指定了编辑器类型、构建设置版本和包含的模块
 * @author AntTest Team
 * @date 2024
 */

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * @brief AntTest编辑器目标构建规则类
 * @details 定义AntTest编辑器的构建目标配置
 */
public class AntTestEditorTarget : TargetRules
{
	/**
	 * @brief 构造函数
	 * @param Target 目标信息
	 * @details 初始化AntTest编辑器目标的构建配置
	 */
	public AntTestEditorTarget( TargetInfo Target) : base(Target)
	{
		// 设置为编辑器类型目标
		Type = TargetType.Editor;
		// 使用V5构建设置版本
		DefaultBuildSettings = BuildSettingsVersion.V5;
		// 使用Unreal 5.5包含顺序版本
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		// 添加AntTest模块
		ExtraModuleNames.Add("AntTest");
	}
}
