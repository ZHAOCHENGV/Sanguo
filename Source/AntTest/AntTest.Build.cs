/**
 * @file AntTest.Build.cs
 * @brief AntTest模块构建配置文件
 * @details 该文件定义了AntTest模块的构建规则和依赖关系
 *          指定了模块的公共依赖和私有依赖
 * @author AntTest Team
 * @date 2024
 */

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * @brief AntTest模块构建规则类
 * @details 定义AntTest模块的构建配置，包括依赖模块和编译选项
 */
public class AntTest : ModuleRules
{
	/**
	 * @brief 构造函数
	 * @param Target 目标构建规则
	 * @details 初始化AntTest模块的构建配置
	 */
	public AntTest(ReadOnlyTargetRules Target) : base(Target)
	{
		// 使用显式或共享预编译头文件
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// 公共依赖模块列表
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core",           // 核心模块
			"CoreUObject",    // 核心UObject模块
			"Engine",         // 引擎模块
			"InputCore",      // 输入核心模块
			"EnhancedInput",  // 增强输入模块
			"Ant",            // Ant代理系统模块
			"Skelot",         // Skelot骨骼动画模块
			"NavigationSystem", // 导航系统模块
			"BattleFrame",    // BattleFrame战斗框架模块
			"ApparatusRuntime", // Apparatus运行时模块
			"FlowFieldCanvas", // 流场画布模块
			"AnimToTexture"   // 动画到纹理模块
		});

		// 私有依赖模块列表（当前为空）
		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// 如果使用Slate UI，取消注释以下行
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// 如果使用在线功能，取消注释以下行
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// 要包含OnlineSubsystemSteam，请在uproject文件的plugins部分添加它，并将Enabled属性设置为true
	}
}
