/**
 * @file DebugHelper.h
 * @brief 调试助手头文件
 * @details 该文件提供了调试相关的工具函数，用于在屏幕上显示调试信息
 *          包含打印函数等调试辅助功能
 * @author AntTest Team
 * @date 2024
 */

#pragma once

/**
 * @brief 调试工具命名空间
 * @details 包含各种调试相关的工具函数
 */
namespace Debug
{
	/**
	 * @brief 打印调试信息到屏幕
	 * @param Msg 要显示的调试消息
	 * @details 在屏幕上显示调试信息，使用随机颜色，显示时间为15秒
	 */
	FORCEINLINE void Print(const FString& Msg)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::MakeRandomColor(), Msg);
		}
	}
}
