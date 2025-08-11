#pragma once
#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ESoldierType : uint8
{
	CiKe UMETA(DisplayName = "刺客"),
	DaoDaoBing UMETA(DisplayName = "大刀兵"),
	DaoBing UMETA(DisplayName = "刀兵"),
	BaoQiBing UMETA(DisplayName = "刀骑兵"),
	FuQiBing UMETA(DisplayName = "斧骑兵"),
	GongBing UMETA(DisplayName = "弓兵"),
	GongQiBing UMETA(DisplayName = "弓骑兵"),
	HuBaoQi UMETA(DisplayName = "虎豹骑"),
	JinWeiJun UMETA(DisplayName = "禁卫军"),
	LangYaBangBing UMETA(DisplayName = "狼牙棒兵"),
	NuBing UMETA(DisplayName = "弩兵"),
	QiangQiBing UMETA(DisplayName = "枪骑兵"),
	ShuangDaoBing UMETA(DisplayName = "双刀兵"),
	ShuangQiangBing UMETA(DisplayName = "双枪兵"),
	TouMaoBing UMETA(DisplayName = "投矛兵"),
	XiangQiBing UMETA(DisplayName = "象骑兵"),
	ChangQiangBing UMETA(DisplayName = "长枪兵"),
	ZhongQiBing UMETA(DisplayName = "重骑兵")
};

UENUM(BlueprintType)
enum class EAgentAttackType : uint8
{
	Near UMETA(DisplayName = "近战攻击"),
	Range UMETA(DisplayName = "远程攻击")
};

UENUM(BlueprintType)
enum class EAgentSoldierType : uint8
{
	General UMETA(DisplayName = "将领"),
	Soldier UMETA(DisplayName = "士兵")
};
