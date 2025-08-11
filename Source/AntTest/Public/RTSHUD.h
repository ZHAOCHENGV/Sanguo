
#pragma once

#include "Ant/public/AntHandle.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "RTSHUD.generated.h"

class ARTSPlayerController;

UCLASS()
class ANTTEST_API ARTSHUD : public AHUD
{
	GENERATED_BODY()

public:
	void DrawSelectionBox(const FBox2f &Selection);

	// draw UI for selected units
	TSharedPtr<TArray<FAntHandle>> SelectedUnits;

protected:
	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	FBox2f SelectionBox;
	bool DrawSelectionbox = false;

public:
	FORCEINLINE TSharedPtr<TArray<FAntHandle>> GetSelectedUnits() const { return SelectedUnits; }

	FORCEINLINE bool GetDrawSelectionbox() const { return DrawSelectionbox; }
};

UCLASS()
class ANTTEST_API ARTSUnits : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARTSUnits();

	// Select agents on the screen by given area
	void SelectOnScreenAgents(const FBox2f &ScreenArea);

	// Move agents to the given location 
	void MoveAgentsToLocation(const FVector &WorldLocation);

	UFUNCTION(BlueprintPure)
	TArray<FAntHandle>& GetSelectedUnits();
	
	UPROPERTY(Config, EditAnywhere)
	TEnumAsByte<ECollisionChannel> FloorChannel;

	UPROPERTY(EditAnywhere)
	UStaticMesh *UnitMesh = nullptr;

	UFUNCTION()
	void OnLeftMouseReleased(bool bIsReleased);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// update unit selection
	void UpdateUnitSelection(float DeltaTime);

private:

	TObjectPtr<ARTSPlayerController> RTSPC;
	
	// movements callback
	void OnDestReached(const TArray<FAntHandle> &Agents);
	void OnVelocityTimeout(const TArray<FAntHandle> &Agents);

	// rendering
	void SetupInstancedMeshComp();
	void RenderUnits();

	// left mouse button is already down
	bool MLBDown = false;
	// right mouse button is already down
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly , meta = (allowPrivateAccess = "true"))
	bool MRBDown = false;

	bool bIsDraw;

	bool bIsLeftMouseReleased = false;

	float HoldTime = 0.0f;

	// we set a dedicated flag for each moving group to avoid collision between different groups
	uint8 MoveGroupFlag = 2;

	// start position of the screen selection rectangle
	FVector2f StartSelectionPos;

	// list of currently selected units
	TSharedPtr<TArray<FAntHandle>> SelectedUnits;

	UPROPERTY()
	UInstancedStaticMeshComponent* InstancedMeshComp = nullptr;


};
