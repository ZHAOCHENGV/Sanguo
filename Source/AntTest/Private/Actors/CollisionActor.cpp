
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

ACollisionActor::ACollisionActor()
{
	PrimaryActorTick.bCanEverTick = true;
	
	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(DefaultRoot);
	
	InstancedStaticMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedStaticMesh"));
	InstancedStaticMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	InstancedStaticMesh->SetupAttachment(GetRootComponent());
	
	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SphereCollision->SetupAttachment(GetRootComponent());

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(DefaultRoot);
}

void ACollisionActor::RegisterTimer()
{
	GetWorld()->GetTimerManager().SetTimer(ChangeShapeTimer, this, &ThisClass::FollowChangeComponent, 0.5f, false);
}

void ACollisionActor::FollowChangeComponent()
{
	UMaterialInstanceDynamic* DecalMaterialInstanceDynamic = Decal->CreateDynamicMaterialInstance();
	DecalMaterialInstanceDynamic->SetScalarParameterValue(FName("ShapeSwitch"), (float)SpawnShape);

	float Distance = SpawnShape == ESpawnShape::Triangle ? 100.f : 50.f;
	
	CollisionRadius = InstancedStaticMesh->Bounds.SphereRadius + Distance;
	
	SphereCollision->SetSphereRadius(CollisionRadius);
	SphereCollision->SetWorldLocation(InstancedStaticMesh->Bounds.Origin);

	if (SpawnShape == ESpawnShape::Square)
	{
		Decal->DecalSize = FVector(500.f,
			InstancedStaticMesh->Bounds.BoxExtent.Y + (GridTileSize.Y * 2),
			InstancedStaticMesh->Bounds.BoxExtent.X + (GridTileSize.X * 2));
		
		DecalMaterialInstanceDynamic->SetScalarParameterValue(TEXT("ScaleX"), InstancedStaticMesh->Bounds.BoxExtent.X + (GridTileSize.X * 2) + 1000.f);
		DecalMaterialInstanceDynamic->SetScalarParameterValue(TEXT("ScaleY"), InstancedStaticMesh->Bounds.BoxExtent.Y + (GridTileSize.Y * 2));
	}
	else
	{
		Decal->DecalSize = FVector(500.f, CollisionRadius, CollisionRadius);
	}
	
	Decal->SetWorldLocation(InstancedStaticMesh->Bounds.Origin);
	
	
}

void ACollisionActor::BeginPlay()
{
	Super::BeginPlay();
	
	AddressMap.Empty();
	AddressMap.Reserve(GetInstancedCount());

	for (int i = 0; i < GetInstancedCount(); i++)
	{
		AddressMap.Emplace(i, false);
	}

	if (auto* AntSubsystem = GetWorld()->GetSubsystem<UAntSubsystem>())
	{
		AntSubsystem->OnMovementGoalReached.AddLambda([this](const TArray<FAntHandle>& Handles)
		{
			if (CheckHasAddress().IsEmpty())
			{
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
