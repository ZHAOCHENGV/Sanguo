/**
 * @file CollisionActor.cpp
 * @brief 碰撞Actor实现文件
 * @details 该文件实现了用于RTS游戏中的碰撞检测和形状生成的Actor，支持多种形状的生成和管理
 *          包括三角形、正方形等不同形状的碰撞区域和可视化效果
 * @author AntTest Team
 * @date 2024
 */

#include "Actors/CollisionActor.h"

#include "AntFunctionLibrary.h"
#include "RTSHUD.h"
#include "Actors/AntSkelotActor.h"
#include "AntTest/AntTest.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Containers/StaticArray.h"

/**
 * @brief 构造函数
 * @details 初始化碰撞Actor的组件，包括根组件、实例化静态网格、球形碰撞体和贴花组件
 */
ACollisionActor::ACollisionActor()
{
	// 启用Actor的Tick功能
	PrimaryActorTick.bCanEverTick = true;
	
	// 创建根组件
	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(DefaultRoot);
	
	// 创建实例化静态网格组件，用于渲染多个实例
	InstancedStaticMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedStaticMesh"));
	InstancedStaticMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	InstancedStaticMesh->SetupAttachment(GetRootComponent());
	
	// 创建球形碰撞体组件
	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SphereCollision->SetupAttachment(GetRootComponent());

	// 创建贴花组件，用于地面标记
	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(DefaultRoot);
}

/**
 * @brief 注册形状变化计时器
 * @details 设置延迟计时器来触发组件形状变化
 */
void ACollisionActor::RegisterTimer()
{
	GetWorld()->GetTimerManager().SetTimer(ChangeShapeTimer, this, &ThisClass::FollowChangeComponent, 0.5f, false);
}

/**
 * @brief 跟随组件变化
 * @details 根据生成的形状类型调整碰撞体和贴花组件的参数
 */
void ACollisionActor::FollowChangeComponent()
{
	// 创建动态材质实例
	UMaterialInstanceDynamic* DecalMaterialInstanceDynamic = Decal->CreateDynamicMaterialInstance();
	// 设置形状切换参数
	DecalMaterialInstanceDynamic->SetScalarParameterValue(FName("ShapeSwitch"), (float)SpawnShape);

	// 根据形状类型设置距离
	float Distance = SpawnShape == ESpawnShape::Triangle ? 100.f : 50.f;
	
	// 计算碰撞半径
	CollisionRadius = InstancedStaticMesh->Bounds.SphereRadius + Distance;
	
	// 设置球形碰撞体的半径和位置
	SphereCollision->SetSphereRadius(CollisionRadius);
	SphereCollision->SetWorldLocation(InstancedStaticMesh->Bounds.Origin);

	// 根据形状类型设置贴花大小
	if (SpawnShape == ESpawnShape::Square)
	{
		// 正方形形状的贴花设置
		Decal->DecalSize = FVector(500.f,
			InstancedStaticMesh->Bounds.BoxExtent.Y + (GridTileSize.Y * 2),
			InstancedStaticMesh->Bounds.BoxExtent.X + (GridTileSize.X * 2));
		
		// 设置材质参数
		DecalMaterialInstanceDynamic->SetScalarParameterValue(TEXT("ScaleX"), InstancedStaticMesh->Bounds.BoxExtent.X + (GridTileSize.X * 2) + 1000.f);
		DecalMaterialInstanceDynamic->SetScalarParameterValue(TEXT("ScaleY"), InstancedStaticMesh->Bounds.BoxExtent.Y + (GridTileSize.Y * 2));
	}
	else
	{
		// 其他形状的贴花设置
		Decal->DecalSize = FVector(500.f, CollisionRadius, CollisionRadius);
	}
	
	// 设置贴花位置
	Decal->SetWorldLocation(InstancedStaticMesh->Bounds.Origin);
}

/**
 * @brief 游戏开始时调用
 * @details 初始化地址映射表，设置Ant子系统的回调函数
 */
void ACollisionActor::BeginPlay()
{
	Super::BeginPlay();
	
	// 清空并预分配地址映射表
	AddressMap.Empty();
	AddressMap.Reserve(GetInstancedCount());

	// 初始化地址映射表，所有地址初始为false
	for (int i = 0; i < GetInstancedCount(); i++)
	{
		AddressMap.Emplace(i, false);
	}

	// 获取Ant子系统并设置回调
	if (auto* AntSubsystem = GetWorld()->GetSubsystem<UAntSubsystem>())
	{
		// 设置移动目标到达的回调函数
		AntSubsystem->OnMovementGoalReached.AddLambda([this](const TArray<FAntHandle>& Handles)
		{
			// 检查是否有可用地址
			if (CheckHasAddress().IsEmpty())
			{
				// 如果查询结果数量等于地址映射表大小，且未在生成中，则开始生成标志Actor
				if (QueryResult.Num() == AddressMap.Num())
				{
					if (!bIsSpawning)
					{
						bIsSpawning = true;
						SpawnFlagActor();
					}
				}
			}
		});

		// 设置Ant句柄完成某事的回调函数
		AntSubsystem->OnAntHandleCompleteSomething.AddUniqueDynamic(this, &ThisClass::PlayCheerAnim);
	}

	QueryLocation = bEnableAbsoluteCenter ? InstancedStaticMesh->Bounds.Origin : GetActorTransform().TransformPosition(CenterPoint);
	Decal->SetWorldLocation(QueryLocation);
	Decal->DecalSize = FVector(500.f, CollisionRadius, CollisionRadius);
}

void ACollisionActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	QueryResult.Empty();
	
	UAntFunctionLibrary::QueryCylinder(this, FVector3f(QueryLocation), CollisionRadius, CollisionRadius,
		1, true, QueryResult);

	//UE_LOG(LogTemp, Warning, TEXT("%f"), CollisionRadius);
	//UE_LOG(LogTemp, Warning, TEXT("QueryResult : %d"), QueryResult.Num());
	if (CheckNum != 0 && !bIsClose)
	{
		//UE_LOG(LogTemp, Warning, TEXT("没关闭"));
		//UE_LOG(LogTemp, Warning, TEXT("QueryResult : %d"), QueryResult.Num());
		//UE_LOG(LogTemp, Warning, TEXT("CheckNum : %d"),CheckNum);
		if (QueryResult.Num() < CheckNum)
		{
			ExistingQty -= CheckNum - QueryResult.Num();
			RemainingQty += CheckNum - QueryResult.Num();
			OnEndAgentLocation();
		}
	}

	if (QueryResult.Num() > 0)
	{
		if (QueryResult.Num() > ExistingQty)
		{
			if (!bIsClose)
			{
				//OnEnterAgentLocation();
			}
		}
		else
		{
			bIsClose = false;
			if (QueryResult.Num() != CheckNum)
			{
				CheckNum = QueryResult.Num();
			}
		}
	}
	else if (bIsClose)
	{
		bIsClose = false;
	}
}

#if WITH_EDITOR
void ACollisionActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	switch (SpawnShape)
	{
	case ESpawnShape::Square :
		SpawnGrid(GetActorLocation(), GridTileSize, GridTileCount);
		break;
	case ESpawnShape::Circle :
		SpawnCircle(DefaultRoot->GetComponentToWorld().GetLocation());
		break;
	case ESpawnShape::Triangle :
		SpawnTriangle(GetActorLocation());
		break;
	}

	RegisterTimer();
}

void ACollisionActor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	/*if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ACollisionActor, CollisionRadius))
	{
		if (!bEnableRadiusSizeToBound)
		{
			SphereCollision->SetSphereRadius(CollisionRadius);
		}
	}

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ACollisionActor, CenterPoint))
	{
		if (!bEnableAbsoluteCenter)
		{
			SphereCollision->SetRelativeLocation(CenterPoint);
		}
	}

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ACollisionActor, bEnableAbsoluteCenter))
	{
		if (bEnableAbsoluteCenter)
		{
			SphereCollision->SetWorldLocation(InstancedStaticMesh->Bounds.Origin);
		}
		else
		{
			SphereCollision->SetRelativeLocation(CenterPoint);
		}
	}

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ACollisionActor, bEnableRadiusSizeToBound))
	{
		if (bEnableRadiusSizeToBound)
		{
			SphereCollision->SetSphereRadius(InstancedStaticMesh->Bounds.SphereRadius);
		}
		else
		{
			SphereCollision->SetSphereRadius(CollisionRadius);
		}
	}

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ACollisionActor, SpawnShape))
	{
		Decal->CreateDynamicMaterialInstance()->SetScalarParameterValue(FName("ShapeSwitch"), (float)SpawnShape);
	}*/
}
#endif

void ACollisionActor::OnEnterAgentLocation()
{
	if (!GetWorld()) return;
	
	if (UAntSubsystem* Ant = GetWorld()->GetSubsystem<UAntSubsystem>())
	{
		bIsClose = true;
		
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (!PC) return;
		
		ARTSHUD* RTSHUD = PC->GetHUD<ARTSHUD>();
		if (!RTSHUD) return;
		
		TArray<FAntHandle> Selected = *RTSHUD->GetSelectedUnits();
		if (Selected.IsEmpty()) return;
		
		TArray<int32> AddressIndex = CheckHasAddress();

		const int32 TileCount = GetInstancedCount();
		const int32 UnitCount = Selected.Num();
		
		if (AddressIndex.Num() <= 0)
		{
			return;
		}
		
		FTransform TargetLocation = FTransform();

		auto MoveAgentToTile = [&](int32 AgentIndex)
		{
			if (!Ant->IsValidAgent(Selected[AgentIndex])) return;
			if (AgentIndex >= AddressIndex.Num()) return;
			if (!AddressMap.Contains(AddressIndex[AgentIndex])) return;

			const auto& UnitData = Ant->GetAgentUserData(Selected[AgentIndex]).Get<FUnitData>();
			
			InstancedStaticMesh->GetInstanceTransform(AddressIndex[AgentIndex], TargetLocation, true);
			UAntFunctionLibrary::MoveAgentToLocation(this, Selected[AgentIndex], TargetLocation.GetLocation(),
				UnitData.Speed, 1.f, 0.6f, 10.f, 70.f,
				EAntPathFollowerType::FlowField, 500.f, 1000.f);

			SetAgentID(Selected[AgentIndex], AddressIndex[AgentIndex]);
			AddressMap[AddressIndex[AgentIndex]] = true;
		};
		
		if (UnitCount >= TileCount && RemainingQty == 0)
		{
			for (int32 i = UnitCount-1; i >= TileCount; --i)
			{
				if (!Ant->IsValidAgent(Selected[i]))
				{
					Selected.RemoveAt(i);
					continue;
				}
				Ant->RemoveAgentMovement(Selected[i]);
				Selected.RemoveAt(i);
			}
			
			for (int32 i = 0; i < Selected.Num(); i++)
			{
				MoveAgentToTile(i);
			}
		}
		else
		{
			bool bIsFirst = false;
			if (RemainingQty == 0)
			{
				RemainingQty = TileCount - UnitCount;
				bIsFirst = true;
			}

			if (!bIsFirst)
			{
				for (int32 i = UnitCount-1; i >= RemainingQty; --i)
				{
					if (!Ant->IsValidAgent(Selected[i]))
					{
						Selected.RemoveAt(i);
						continue;
					}
					Ant->RemoveAgentMovement(Selected[i]);
					Selected.RemoveAt(i);
				}
			}
			
			for (int32 i = 0; i < Selected.Num(); ++i)
			{
				MoveAgentToTile(i);
				if (!bIsFirst)
				{
					RemainingQty--;
				}
			}
			if (RemainingQty <= 0)
			{
				RemainingQty = 0;
			}
		}
		ExistingQty = TileCount - RemainingQty;
	}
}

void ACollisionActor::OnEndAgentLocation()
{
	UAntSubsystem* Ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	if (!Ant) return;
	
	bIsClose = true;
	CheckNum = QueryResult.Num();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	ARTSHUD* RTSHUD = PC->GetHUD<ARTSHUD>();
	if (!RTSHUD) return;
	TArray<FAntHandle> Selected = *RTSHUD->GetSelectedUnits();
	if (!Ant->IsValidAgents(Selected)) return;
	ExistingQty -= CheckNum - QueryResult.Num();
	RemainingQty += CheckNum - QueryResult.Num();
	for (int32 i = 0; i < Selected.Num(); i++)
	{
		const auto& UnitData = Ant->GetAgentUserData(Selected[i]).Get<FUnitData>();
		
		if (AddressMap.Contains(UnitData.ID))
		{
			AddressMap[UnitData.ID] = false;
			
		}
	}
	//UE_LOG(LogTemp, Warning, TEXT("离开了"));
}

void ACollisionActor::SpawnGrid(const FVector& CenterLocation, const FVector& TileSize,
	const FIntPoint TileCount)
{
	if (!InstancedStaticMesh) return;
	
	GridCenterLocation = CenterLocation;
	GridTileSize = TileSize;
	GridTileCount = TileCount;

	InstancedStaticMesh->ClearInstances();
	
	GridBottomLeftCornerLocation = GridCenterLocation - FVector(GridTileCount.X / 2 * GridTileSize.X, GridTileCount.Y / 2 * GridTileSize.Y, 0.f);

	FIntPoint GridXY = FIntPoint(0,0);
	FTransform TileTransform;
	for (int32 i = 0; i < GridTileCount.X; i++)
	{
		GridXY.X = i;
		
		for (int32 j = 0; j < GridTileCount.Y; j++)
		{
			GridXY.Y = j;

			TileTransform.SetLocation(GridBottomLeftCornerLocation + FVector(GridTileSize.X * GridXY.X, GridTileSize.Y * GridXY.Y, 0.f));
			InstancedStaticMesh->AddInstance(TileTransform, true);
		}
	}
}

void ACollisionActor::SpawnTriangle(const FVector& CenterLocation)
{
	InstancedStaticMesh->ClearInstances();
	
	FTransform InTransform;

	// 计算整个阵列的尺寸
	float TotalDepth = (TriangleRowNum - 1) * TriangleXYInterval.X;
	float MaxRowWidth = (TriangleRowNum - 1) * TriangleXYInterval.Y;

	const FQuat RotationQuat = FRotator(0.f ,TriangleAngle, 0.f).Quaternion();
	
	for (int32 i = 0; i < TriangleRowNum; i++)
	{
		int32 NumInRow = i + 1;
		float RowWidth = (NumInRow - 1) * TriangleXYInterval.Y;
		float RowOffsetY = RowWidth * 0.5f;

		for (int32 j = 0; j < NumInRow; ++j)
		{
			// 局部位置（相对于阵型中心）
			FVector LocalLocation;
			LocalLocation.X = i * TriangleXYInterval.X - TotalDepth * 0.5f;
			LocalLocation.Y = j * TriangleXYInterval.Y - RowOffsetY;
			LocalLocation.Z = 0.f;

			// 应用旋转 & 平移到世界空间
			FVector WorldLocation = RotationQuat.RotateVector(LocalLocation) + CenterLocation;

			InTransform.SetLocation(WorldLocation);
			InTransform.SetRotation(RotationQuat); // 可选，如果希望单位朝向一致
			InstancedStaticMesh->AddInstance(InTransform, true);
		}
	}
}

void ACollisionActor::SpawnCircle(const FVector& CenterLocation)
{
	InstancedStaticMesh->ClearInstances();
	FVector InVector = FVector(0, 0, 0);
	float Angle = 0.f;
	FTransform InTransform;
	for (int32 i = 1; i <= CircleQty * 2; ++i)
	{
		if (i % 2 > 0) continue;
		for (int32 j = CircleAngle == 360 ? 1 : 0; j <= CircleStartQty * i; ++j)
		{
			if (j % 2 > 0) continue;
			InVector.X = CircleRadius + (i * CircleInterval);
			Angle = (CircleAngle / (CircleStartQty * i)) * j;
			InTransform.SetLocation(CenterLocation + UKismetMathLibrary::RotateAngleAxis(InVector, Angle, FVector(0.f, 0.f, 1.f)));
			InstancedStaticMesh->AddInstance(InTransform, true);
		}
	}
}

void ACollisionActor::SetAgentID(FAntHandle& AntHandle, int32 ID)
{
	BP_SetAgentID(AntHandle, ID);
}

TArray<int32> ACollisionActor::CheckHasAddress()
{
	bool bHasAddress = false;
	TArray<int32> AddressIndex;
	AddressIndex.Empty();
	for (int32 i = 0; i < AddressMap.Num(); i++)
	{
		if (AddressMap.Contains(i))
		{
			bHasAddress = *AddressMap.Find(i);

			if (!bHasAddress)
			{
				AddressIndex.Add(i);
			}
		}
	}
	return AddressIndex;
}

int32 ACollisionActor::GetInstancedCount()
{
	return InstancedStaticMesh->GetInstanceCount();
}

void ACollisionActor::DrawQueryConvexVolume(TArray<FAntHandle>& AntHandles)
{
	const FVector BoxExtent = InstancedStaticMesh->Bounds.BoxExtent;
	const FVector Origin = InstancedStaticMesh->Bounds.Origin;
	TStaticArray<FVector, 4> NearPlane;
	TStaticArray<FVector, 4> FarPlane;

	NearPlane[0] = FVector(-BoxExtent.X, -BoxExtent.Y, -BoxExtent.Z) + Origin;
	NearPlane[1] = FVector(BoxExtent.X, -BoxExtent.Y, -BoxExtent.Z) + Origin;
	NearPlane[2] = FVector(-BoxExtent.X, BoxExtent.Y, -BoxExtent.Z) + Origin;
	NearPlane[3] = FVector(BoxExtent.X, BoxExtent.Y, -BoxExtent.Z) + Origin;

	FarPlane[0] = FVector(-BoxExtent.X, -BoxExtent.Y, BoxExtent.Z) + Origin;
	FarPlane[1] = FVector(BoxExtent.X, -BoxExtent.Y, BoxExtent.Z) + Origin;
	FarPlane[2] = FVector(-BoxExtent.X, BoxExtent.Y, BoxExtent.Z) + Origin;
	FarPlane[3] = FVector(BoxExtent.X, BoxExtent.Y, BoxExtent.Z) + Origin;
	
	UAntFunctionLibrary::QueryConvexVolume(this, NearPlane, FarPlane, CollisionRadius, AntHandles);
}

void ACollisionActor::SpawnFlagActor()
{
	if (SpawnFlag == nullptr)
	{
		return;
	}
	
	FTransform SpawnTransform;
	SpawnTransform.SetLocation(GetActorTransform().TransformPosition(CenterPoint));
	SpawnTransform.SetRotation(FRotator(0.f, 0.f, 0.f).Quaternion());
	SpawnTransform.SetScale3D(FVector(7.f, 7.f, 7.f));
	AActor* FlagActor = GetWorld()->SpawnActorDeferred<AActor>(SpawnFlag, SpawnTransform);
	UGameplayStatics::FinishSpawningActor(FlagActor, SpawnTransform);
	
}
