/**
 * @file AntSkelotActor.cpp
 * @brief 骨骼动画角色实现文件
 * @details 该文件实现了基于Skelot系统的骨骼动画角色，用于在RTS游戏中生成和管理士兵单位
 * @author AntTest Team
 * @date 2024
 */

#include "Actors/AntSkelotActor.h"

#include "AntFunctionLibrary.h"
#include "NavigationSystem.h"
#include "SkelotComponent.h"
#include "AntTest/AntTest.h"
#include "Components/BillboardComponent.h"

/**
 * @brief 构造函数
 * @details 初始化骨骼动画角色的组件和属性
 */
AAntSkelotActor::AAntSkelotActor()
{
	// 启用Actor的Tick功能
	PrimaryActorTick.bCanEverTick = true;

	// 创建根组件
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	
	// 创建骨骼动画组件
	SoldierSkelotComponent = CreateDefaultSubobject<USkelotComponent>(TEXT("SoldierSkelot"));
	SoldierSkelotComponent->SetupAttachment(GetRootComponent());
	SoldierSkelotComponent->bReceivesDecals = false;

	// 创建广告牌组件用于编辑器显示
	SkelotBillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("SkelotBillboard"));
	SkelotBillboardComponent->SetupAttachment(GetRootComponent());
}

/**
 * @brief 游戏开始时调用
 * @details 初始化骨骼数据并设置生成计时器
 */
void AAntSkelotActor::BeginPlay()
{
	Super::BeginPlay();

	// 清空士兵Ant句柄数组
	SoldierAntHandles.Empty();
	
	// 初始化骨骼数据
	InitSkelotData();

	// 设置延迟生成计时器
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(StartSpawnTimerHandle, this, &ThisClass::SpawnAntHandle, 1.f, false);
	}
}

/**
 * @brief 每帧更新函数
 * @param DeltaTime 帧间隔时间
 * @details 更新骨骼动画组件的变换数据
 */
void AAntSkelotActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 如果存在骨骼组件且有士兵句柄且未播放欢呼动画，则更新变换
	if (SoldierSkelotComponent && SoldierAntHandles.Num() > 0 && !bIsPlayCheerAnim)
	{
		UAntHelperFunctionLibrary::CopyEnemyTranformsToSkelot_New(this, SoldierSkelotComponent, SoldierAntHandles);
	}
}

/**
 * @brief 获取指定士兵类型的骨骼数据
 * @param SoldierDataType 士兵类型枚举
 * @return 返回对应的骨骼数据指针，如果未找到则返回nullptr
 * @details 从数据表中查找指定士兵类型的骨骼动画配置数据
 */
FAntSkelotData* AAntSkelotActor::GetAntSkelotData(const ESoldierType& SoldierDataType)
{
	// 检查数据表是否有效
	checkf(AntSkelotDataTable, TEXT("AntSkelotDataTable无效"));
	
	// 获取士兵类型枚举
	const UEnum* EnumPtr = StaticEnum<ESoldierType>();
	
	// 获取枚举名称字符串
	FString SoldierName = EnumPtr->GetNameStringByIndex((int32)SoldierDataType);
		
	// 在数据表中查找对应的行数据
	FAntSkelotData* FoundData = AntSkelotDataTable->FindRow<FAntSkelotData>(FName(SoldierName), FString());

	if (FoundData)
	{
		return FoundData;
	}
	
	return nullptr;
}

/**
 * @brief 初始化骨骼数据
 * @details 根据士兵类型设置骨骼组件的动画集合、网格和材质
 */
void AAntSkelotActor::InitSkelotData()
{
	// 获取当前士兵类型的骨骼数据
	const FAntSkelotData* Data = GetAntSkelotData(SoldierType);

	// 如果骨骼组件和数据都存在，则进行初始化
	if (SoldierSkelotComponent && Data)
	{
		// 设置动画集合
		SoldierSkelotComponent->SetAnimCollection(Data->AnimCollection);
		// 设置骨骼网格
		SoldierSkelotComponent->SetSubmeshAsset(0, Data->SkeletalMesh);
		// 根据阵营设置材质（蓝色或红色）
		SoldierSkelotComponent->SetMaterial(0, bIsUesBlue ? Data->BlueMaterial : Data->RedMaterial);
	}
}

/**
 * @brief 生成Ant句柄
 * @details 在导航系统中随机位置生成指定数量的士兵单位
 */
void AAntSkelotActor::SpawnAntHandle()
{
	// 检查世界是否有效
	if (!GetWorld()) return;

	// 获取Ant子系统
	UAntSubsystem* AntSubsystem = GetWorld()->GetSubsystem<UAntSubsystem>();

	if (!AntSubsystem) return;
	
	// 检查生成数量是否有效
	if (SpawnCount == 0) return;

	// 获取骨骼数据
	FAntSkelotData* Data = GetAntSkelotData(SoldierType);

	if (!Data) return;
	
	FTransform SpawnTransform;
	
	// 循环生成指定数量的士兵
	for (int32 i = 0; i < SpawnCount; i++)
	{
		FVector SpawnLocation;
		// 在Actor周围随机可到达位置生成
		if (UNavigationSystemV1::K2_GetRandomReachablePointInRadius(this, GetActorLocation(), SpawnLocation, SpawnCount * 10))
		{
			SpawnTransform.SetLocation(SpawnLocation);
			
			// 在骨骼组件中添加实例
			int32 InstanceIndex = SoldierSkelotComponent->AddInstance(FTransform3f(SpawnTransform));

			// 播放待机动画
			SoldierSkelotComponent->InstancePlayAnimation(InstanceIndex, Data->IdleAnim);

			// 创建Ant代理句柄
			FAntHandle AntHandle = UAntFunctionLibrary::AddAgentAdvanced(this, SpawnLocation, 70.f, 0.f, 0.f, 1.f,
				1.f, 7.f, true, true, true, 1.f, Flags, Flags);

			// 如果Ant句柄有效，则设置单位数据
			if (AntSubsystem->IsValidAgent(AntHandle))
			{
				// 创建实例化结构体
				FInstancedStruct InstancedStruct;
				InstancedStruct.InitializeAs<FUnitData>();
				FUnitData& UnitData = InstancedStruct.GetMutable<FUnitData>();
				
				// 设置单位数据
				UnitData.SkelotInstanceID = InstanceIndex;
				UnitData.IdleAnim = Data->IdleAnim;
				UnitData.WalkAnim = Data->RunAnim;
				UnitData.CheerAnim = Data->CheerAnim;
				UnitData.Speed = AntSpeed;
				UnitData.SkelotComponent = SoldierSkelotComponent;
				
				// 将单位数据设置到Ant句柄
				UAntFunctionLibrary::SetAgentCustomInstancedStruct(this, AntHandle, InstancedStruct);
				SoldierAntHandles.Add(AntHandle);
			}
		}
	}
	
	// 输出生成的士兵数量日志
	UE_LOG(LogTemp, Warning, TEXT("%d"), SoldierAntHandles.Num());
}

