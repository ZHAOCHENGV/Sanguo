 // Fill out your copyright notice in the Description page of Project Settings.


#include "AntTest/Public/RTSHUD.h"

#include "Ant/Public/AntSubsystem.h"
#include "Ant/Public/AntGrid.h"
#include "Ant/Public/AntMath.h"
#include "Ant/Public/AntUtil.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "AntTest/AntTest.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Controllers/RTSPlayerController.h"

 //#define ObstacleFlag 1u << 0
#define IdleAgentFlag 1u << 1

void ARTSHUD::DrawSelectionBox(const FBox2f & Selection)
{
	SelectionBox = Selection;
	DrawSelectionbox = true;
}

void ARTSHUD::DrawHUD()
{
	Super::DrawHUD();

	// draw UI for selected agents
	if (SelectedUnits)
	{
		auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
		for (const auto &handle : *SelectedUnits)
		{
			// skip non-agent units
			if (!ant->IsValidAgent(handle))
				continue;

			const FVector agentWorldLoc(ant->GetAgentData(handle).GetLocationLerped());
			const auto agentScreenLoc = Project(agentWorldLoc);
			const FBox2f uiArea(FVector2f(agentScreenLoc.X - 10.f, agentScreenLoc.Y - 25.f), FVector2f(agentScreenLoc.X + 10.f, agentScreenLoc.Y - 20.f));

			const float CurrentHealth = ant->GetAgentUserData(handle).Get<FUnitData>().Health;
			const float MaxHealth = ant->GetAgentUserData(handle).Get<FUnitData>().MaxHealth;
			const float HealthPercent = FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f);

			const float BarWidth = uiArea.Max.X - uiArea.Min.X;
			const float GreenBarWidth = BarWidth * HealthPercent;

			// fill color
			const FBox2f HealthRectBk = uiArea.ExpandBy(1);
			DrawRect(FLinearColor::Black, HealthRectBk.Min.X, HealthRectBk.Min.Y, HealthRectBk.Max.X - HealthRectBk.Min.X, HealthRectBk.Max.Y - HealthRectBk.Min.Y);
			DrawRect(FLinearColor::Green, uiArea.Min.X, uiArea.Min.Y, GreenBarWidth, uiArea.Max.Y - uiArea.Min.Y);
		}
	}

	// draw box selection on the screen
	if (DrawSelectionbox)
	{
		// fill color
		DrawRect(FLinearColor(0.f, 1.f, 0.f, 0.3f), SelectionBox.Min.X, SelectionBox.Min.Y, SelectionBox.Max.X - SelectionBox.Min.X, SelectionBox.Max.Y - SelectionBox.Min.Y);

		// draw border lines
		DrawLine(SelectionBox.Min.X, SelectionBox.Min.Y, SelectionBox.Max.X, SelectionBox.Min.Y, FLinearColor::Green, 1);
		DrawLine(SelectionBox.Max.X, SelectionBox.Min.Y, SelectionBox.Max.X, SelectionBox.Max.Y, FLinearColor::Green, 1);
		DrawLine(SelectionBox.Max.X, SelectionBox.Max.Y, SelectionBox.Min.X, SelectionBox.Max.Y, FLinearColor::Green, 1);
		DrawLine(SelectionBox.Min.X, SelectionBox.Max.Y, SelectionBox.Min.X, SelectionBox.Min.Y, FLinearColor::Green, 1);
	}

	// reset
	DrawSelectionbox = false;
}

// Sets default values
ARTSUnits::ARTSUnits() :
	SelectedUnits(new TArray<FAntHandle>)
{
 	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;
}

 void ARTSUnits::OnLeftMouseReleased(bool bIsReleased)
 {
	 if (bIsDraw)
	 {
		 if (MLBDown)
		 {
		 	float xMouse, yMouse;
		 	GetWorld()->GetFirstPlayerController()->GetMousePosition(xMouse, yMouse);
		 	SelectOnScreenAgents({ {FMath::Min(StartSelectionPos.X, xMouse), FMath::Min(StartSelectionPos.Y, yMouse)},
			{FMath::Max(StartSelectionPos.X, xMouse), FMath::Max(StartSelectionPos.Y, yMouse)} });
		 	
		 	bIsDraw = false;
		 	RTSPC->SetIsDraw(false);
		 }
	 }
	 else
	 {
	 	bIsLeftMouseReleased = bIsReleased;
	 	HoldTime = 0.0f;
	 }

	MLBDown = false;
 }

// Called when the game starts or when spawned
void ARTSUnits::BeginPlay()
{
	Super::BeginPlay();

	// bind to the movement callback to get notify
	GetWorld()->GetSubsystem<UAntSubsystem>()->OnMovementGoalReached.AddUObject(this, &ARTSUnits::OnDestReached);
	GetWorld()->GetSubsystem<UAntSubsystem>()->OnMovementMissingVelocity.AddUObject(this, &ARTSUnits::OnVelocityTimeout);

	// set selection list pointer for UI drawing purpose
	GetWorld()->GetFirstPlayerController()->GetHUD<ARTSHUD>()->SelectedUnits = SelectedUnits;

	// setup render stuff
	SetupInstancedMeshComp();

	if (RTSPC == nullptr)
	{
		RTSPC = Cast<ARTSPlayerController>(GetWorld()->GetFirstPlayerController());
		RTSPC->OnLeftMouseDown.AddUniqueDynamic(this, &ARTSUnits::OnLeftMouseReleased);
	}
	else
	{
		RTSPC->OnLeftMouseDown.AddUniqueDynamic(this, &ARTSUnits::OnLeftMouseReleased);
	}
	
}

void ARTSUnits::OnDestReached(const TArray<FAntHandle> &Agents)
{
	// we can stop agents at the destination
	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	for (const auto &agent : Agents)
	{
		ant->RemoveAgentMovement(agent);
		ant->GetMutableAgentData(agent).IgnoreButWakeUpFlag = 0;
		ant->GetMutableAgentData(agent).IgnoreFlag = 0;
		//ant->SetAgentFlag(agent, IdleAgentFlag);
	}
}

void ARTSUnits::OnVelocityTimeout(const TArray<FAntHandle>& Agents)
{
	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();

	TArray<FVector> locations;
	for (const auto &agent : Agents)
		locations.Add(ant->GetAgentMovement(agent).GetDestinationLocation());

	// re-path and move again
	UAntUtil::MoveAgentsToLocations(GetWorld(), Agents, locations,
		{ ant->GetAgentMovement(Agents[0]).MaxSpeed }, { ant->GetAgentMovement(Agents[0]).Acceleration }, { ant->GetAgentMovement(Agents[0]).Deceleration },
		0, 70, ant->GetAgentMovement(Agents[0]).PathFollowerType, 300,
		ant->GetAgentMovement(Agents[0]).MissingVelocityTimeout, false);
}

void ARTSUnits::SelectOnScreenAgents(const FBox2f &ScreenArea)
{
	// clear old list
	SelectedUnits->Reset(0);

	// query agents according to the given area on the screen
	UAntUtil::QueryOnScreenAgents(GetWorld()->GetFirstPlayerController(), ScreenArea, 40000, FloorChannel, -1, *SelectedUnits);

	// change flag
	MoveGroupFlag = MoveGroupFlag >= 30 ? 2 : MoveGroupFlag + 1;
}

void ARTSUnits::MoveAgentsToLocation(const FVector &WorldLocation)
{
	auto *ant = GetWorld()->GetSubsystem<UAntSubsystem>();
	// there are no selected agents
	if (SelectedUnits->IsEmpty())
		return;

	// build agents formation at the destination location
	TArray<FVector> formationLoc;
	float squareDim = 400;
	UAntUtil::SortAgentsBySquare(GetWorld(), WorldLocation, *SelectedUnits, 80, squareDim, formationLoc);

	TArray<FAntHandle> AntHandles = *SelectedUnits;
	TArray<float> AntSpeeds;
	for (const auto &agent : AntHandles)
	{
		const auto& UnitData = ant->GetAgentUserData(agent).Get<FUnitData>();
		AntSpeeds.Add(UnitData.Speed);
	}
	
	// Move agents
	UAntUtil::MoveAgentsToLocations(GetWorld(), *SelectedUnits, formationLoc, AntSpeeds, { 1.f }, { 0.6f }, 10, 70, EAntPathFollowerType::FlowField, 400, 1000, false);

	// check which agents successfully moved
	for (const auto &it : *SelectedUnits)
		if (ant->IsValidMovement(it))
		{
			// set agent flag to moving flag
			//ant->SetAgentFlag(it, 1u << MoveGroupFlag);
			// ignore other moving groups
			ant->GetMutableAgentData(it).IgnoreFlag = -1 & ~((1u << MoveGroupFlag) | IdleAgentFlag);
			// avoid stucking at entrance of tight bottlenecks while moving
			ant->GetMutableAgentData(it).bCanPierce = true;
			// moving agents ignore idle agents but force them to move away
			ant->GetMutableAgentData(it).IgnoreButWakeUpFlag = IdleAgentFlag;
			// path node acceptance radius 
			ant->GetMutableAgentMovement(it).PathNodeRadiusSquared = (ant->GetAgentData(it).GetRadius() * 1.7f) * (ant->GetAgentData(it).GetRadius() * 1.7f);
		}
}

TArray<FAntHandle>& ARTSUnits::GetSelectedUnits()
{
	return *SelectedUnits;
}

// Called every frame
void ARTSUnits::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// manage unit selection by mouse and draw its UI
	UpdateUnitSelection(DeltaTime);

	// render units
	RenderUnits();
}

void ARTSUnits::UpdateUnitSelection(float DeltaTime)
{
	// listening for mouse press event
	const auto isLeftButtonDown = GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::LeftMouseButton);
	const auto isRightButtonDown = GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::RightMouseButton);

	// start unit selection
	if (isRightButtonDown)
	{
		float xMouse, yMouse;
		GetWorld()->GetFirstPlayerController()->GetMousePosition(xMouse, yMouse);
		SelectOnScreenAgents({ {FMath::Min(xMouse, xMouse), FMath::Min(yMouse, yMouse)},
			{FMath::Max(xMouse, xMouse), FMath::Max(yMouse, yMouse)} });
		
	}
	if (isLeftButtonDown && !MLBDown)
	{
		GetWorld()->GetFirstPlayerController()->GetMousePosition(StartSelectionPos.X, StartSelectionPos.Y);

		MLBDown = true;
	}
	
	// draw unit selection UI
	if (MLBDown)
	{
		float xMouse, yMouse;
		GetWorld()->GetFirstPlayerController()->GetMousePosition(xMouse, yMouse);
		GetWorld()->GetFirstPlayerController()->GetHUD<ARTSHUD>()->DrawSelectionBox({ {StartSelectionPos.X, StartSelectionPos.Y}, {xMouse, yMouse} });
		
		if (xMouse - StartSelectionPos.X > 10.f || yMouse - StartSelectionPos.Y > 10.f)
		{
			bIsDraw = true;
			RTSPC->SetIsDraw(true);
		}
		else if(xMouse - StartSelectionPos.X < -10.f || yMouse - StartSelectionPos.Y < -10.f)
		{
			bIsDraw = true;
			RTSPC->SetIsDraw(true);
		}
	}

	// end unit selection, we can do actual query here
	/*if (!isLeftButtonDown && MLBDown && bIsDraw)
	{
		float xMouse, yMouse;
		GetWorld()->GetFirstPlayerController()->GetMousePosition(xMouse, yMouse);
		SelectOnScreenAgents({ {FMath::Min(StartSelectionPos.X, xMouse), FMath::Min(StartSelectionPos.Y, yMouse)},
			{FMath::Max(StartSelectionPos.X, xMouse), FMath::Max(StartSelectionPos.Y, yMouse)} });

		MLBDown = false;
	}*/

	if (!isRightButtonDown && MRBDown)
		MRBDown = false;

	// move units to mouse cursor position
	if (bIsLeftMouseReleased && !isRightButtonDown && !MRBDown && !MLBDown)
	{
		FHitResult hitResult;
		float xMouse, yMouse;
		GetWorld()->GetFirstPlayerController()->GetMousePosition(xMouse, yMouse);
		if (GetWorld()->GetFirstPlayerController()->GetHitResultAtScreenPosition(FVector2D(xMouse, yMouse), FloorChannel, false, hitResult))
			MoveAgentsToLocation(hitResult.ImpactPoint);

		//MRBDown = true;
		bIsLeftMouseReleased = false;
		HoldTime = 0.0f;
	}
}

void ARTSUnits::SetupInstancedMeshComp()
{
	// initialize instanced static mesh components
	InstancedMeshComp = NewObject<UInstancedStaticMeshComponent>();
	InstancedMeshComp->RegisterComponentWithWorld(GetWorld());
	InstancedMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InstancedMeshComp->CastShadow = false;
	InstancedMeshComp->SetStaticMesh(UnitMesh);
}

void ARTSUnits::RenderUnits()
{
	// Ant is a world subsystem
	auto *antSubsysem = GetWorld()->GetSubsystem<UAntSubsystem>();

	// clear all instances
	InstancedMeshComp->ClearInstances();

	// iterate over agents
	for (const auto &agent : antSubsysem->GetUnderlyingAgentsList())
	{
		const FVector center(agent.GetLocationLerped());
		InstancedMeshComp->AddInstance(FTransform(FRotator::ZeroRotator, center, FVector::One()));
	}
}
