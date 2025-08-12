/**
 * @file AntEnums.h
 * @brief Ant系统枚举定义头文件
 * @details 该文件定义了AntTest项目中使用的各种枚举类型，包括士兵类型、攻击类型和代理类型等
 * @author AntTest Team
 * @date 2024
 */

#pragma once
#include "CoreMinimal.h"

/**
 * @brief 士兵类型枚举
 * @details 定义了游戏中各种士兵单位的类型，包括不同武器和兵种的士兵
 */
UENUM(BlueprintType)
enum class ESoldierType : uint8
{
	/** @brief 刺客 - 高机动性的近战单位 */
	CiKe UMETA(DisplayName = "刺客"),
	/** @brief 大刀兵 - 使用大刀的重装步兵 */
	DaoDaoBing UMETA(DisplayName = "大刀兵"),
	/** @brief 刀兵 - 基础刀兵单位 */
	DaoBing UMETA(DisplayName = "刀兵"),
	/** @brief 刀骑兵 - 使用刀的骑兵单位 */
	BaoQiBing UMETA(DisplayName = "刀骑兵"),
	/** @brief 斧骑兵 - 使用斧头的骑兵单位 */
	FuQiBing UMETA(DisplayName = "斧骑兵"),
	/** @brief 弓兵 - 远程弓箭手单位 */
	GongBing UMETA(DisplayName = "弓兵"),
	/** @brief 弓骑兵 - 骑马的弓箭手单位 */
	GongQiBing UMETA(DisplayName = "弓骑兵"),
	/** @brief 虎豹骑 - 精锐骑兵单位 */
	HuBaoQi UMETA(DisplayName = "虎豹骑"),
	/** @brief 禁卫军 - 精英护卫单位 */
	JinWeiJun UMETA(DisplayName = "禁卫军"),
	/** @brief 狼牙棒兵 - 使用狼牙棒的重装步兵 */
	LangYaBangBing UMETA(DisplayName = "狼牙棒兵"),
	/** @brief 弩兵 - 使用弩的远程单位 */
	NuBing UMETA(DisplayName = "弩兵"),
	/** @brief 枪骑兵 - 使用长枪的骑兵单位 */
	QiangQiBing UMETA(DisplayName = "枪骑兵"),
	/** @brief 双刀兵 - 使用双刀的高机动性单位 */
	ShuangDaoBing UMETA(DisplayName = "双刀兵"),
	/** @brief 双枪兵 - 使用双枪的步兵单位 */
	ShuangQiangBing UMETA(DisplayName = "双枪兵"),
	/** @brief 投矛兵 - 投掷长矛的远程单位 */
	TouMaoBing UMETA(DisplayName = "投矛兵"),
	/** @brief 象骑兵 - 骑象的重装单位 */
	XiangQiBing UMETA(DisplayName = "象骑兵"),
	/** @brief 长枪兵 - 使用长枪的步兵单位 */
	ChangQiangBing UMETA(DisplayName = "长枪兵"),
	/** @brief 重骑兵 - 重装骑兵单位 */
	ZhongQiBing UMETA(DisplayName = "重骑兵"),
	/** @brief 重骑兵 - 重装骑兵单位 */
	TestZhongQiBingTest UMETA(DisplayName = "测试重骑兵")
};

/**
 * @brief 代理攻击类型枚举
 * @details 定义了代理单位的攻击方式类型
 */
UENUM(BlueprintType)
enum class EAgentAttackType : uint8
{
	/** @brief 近战攻击 - 需要近距离接触的攻击方式 */
	Near UMETA(DisplayName = "近战攻击"),
	/** @brief 远程攻击 - 可以远距离攻击的方式 */
	Range UMETA(DisplayName = "远程攻击")
};

/**
 * @brief 代理士兵类型枚举
 * @details 定义了代理单位的角色类型
 */
UENUM(BlueprintType)
enum class EAgentSoldierType : uint8
{
	/** @brief 将领 - 指挥官类型的单位 */
	General UMETA(DisplayName = "将领"),
	/** @brief 士兵 - 普通士兵单位 */
	Soldier UMETA(DisplayName = "士兵")
};
