
#pragma once

#include "CoreMinimal.h"
#include "AntHandle.h"
#include "GameFramework/Actor.h"
#include "CollisionActor.generated.h"

class USphereComponent;
class UAntSubsystem;
class UBoxComponent;

UENUM(BlueprintType)
enum class ESpawnShape : uint8
{
	//方形
	Square = 0,
	//圆形
	Circle = 1,
	//三角形
	Triangle = 2
};

UCLASS()
class ANTTEST_API ACollisionActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ACollisionActor();

	void OnEnterAgentLocation();
	void OnEndAgentLocation();

	UFUNCTION(BlueprintCallable)
	void SpawnGrid(const FVector& CenterLocation, const FVector& TileSize, const FIntPoint TileCount);

	UFUNCTION(BlueprintCallable)
	void SpawnTriangle(const FVector& CenterLocation);

	UFUNCTION(BlueprintCallable)
	void SpawnCircle(const FVector& CenterLocation);

	void SetAgentID(FAntHandle& AntHandle, int32 ID);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "SetAgentID")
	void BP_SetAgentID(FAntHandle AntHandle, int32 ID);

	TArray<int32> CheckHasAddress();

	int32 GetInstancedCount();

	void DrawQueryConvexVolume(TArray<FAntHandle>& AntHandles);

	void SpawnFlagActor();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayCheerAnim();

	void RegisterTimer();
	void FollowChangeComponent();
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	
#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* DefaultRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	USphereComponent* SphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	UInstancedStaticMeshComponent* InstancedStaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Decal", meta = (AllowPrivateAccess = "true"))
	UDecalComponent* Decal;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	float CollisionRadius = 1000.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	bool bEnableAbsoluteCenter = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	bool bEnableRadiusSizeToBound = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	ESpawnShape SpawnShape = ESpawnShape::Square;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Default", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AActor> SpawnFlag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Handle", meta = (AllowPrivateAccess = "true"))
	TArray<FAntHandle> QueryResult;

	int32 CheckNum = 0;
	
	int32 RemainingQty = 0;
	
	int32 ExistingQty = 0;

	bool bIsClose = false;

	bool bIsSpawning = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	FVector GridCenterLocation;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	FVector GridTileSize = FVector(200.f, 200.f, 0);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Grid", meta = (AllowPrivateAccess = "true"))
	FIntPoint GridTileCount = FIntPoint(10, 10);

	FVector GridBottomLeftCornerLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map", meta = (AllowPrivateAccess = "true"))
	TMap<int32, bool> AddressMap;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	int32 CircleQty = 5;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	int32 CircleStartQty = 4;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	float CircleRadius = 100.f;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	float CircleInterval = 60.f;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Circle", meta = (AllowPrivateAccess = "true"))
	float CircleAngle = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector", meta = (MakeEditWidget = "true", AllowPrivateAccess = "true"))
	FVector CenterPoint;

	FVector QueryLocation;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Triangle", meta = (AllowPrivateAccess = "true"))
	int32 TriangleRowNum = 5;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Triangle", meta = (AllowPrivateAccess = "true"))
	FVector2D TriangleXYInterval = FVector2D(200.f, 200.f);
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Triangle", meta = (AllowPrivateAccess = "true"))
	float TriangleAngle = 0.f;

	FTimerHandle ChangeShapeTimer;
};