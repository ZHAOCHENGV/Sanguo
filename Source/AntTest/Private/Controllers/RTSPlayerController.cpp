// Fill out your copyright notice in the Description page of Project Settings.


#include "Controllers/RTSPlayerController.h"

#include "Actors/CollisionActor.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "RTSHUD.h"
#include "GameFramework/SpringArmComponent.h"
#include "Pawns/RTSPawn.h"

ARTSPlayerController::ARTSPlayerController()
{
	SetShowMouseCursor(true);
}

void ARTSPlayerController::OnMouseRightButtonClicked(FVector2D MouseLocation)
{
	FHitResult	HitResult;

	FVector MouseWorldPosition;
	FVector MouseWorldDirection;
	DeprojectScreenPositionToWorld(MouseLocation.X, MouseLocation.Y, MouseWorldPosition, MouseWorldDirection);

	const FVector EndLocation = MouseWorldPosition + MouseWorldDirection * 10000.f;
	FCollisionQueryParams CollisionParams;
	//CollisionParams.bDebugQuery = true;
	GetWorld()->LineTraceSingleByChannel(HitResult, MouseWorldPosition, EndLocation, ECC_Visibility,CollisionParams);

	if (HitResult.bBlockingHit)
	{
		if (ACollisionActor* CollisionActor = Cast<ACollisionActor>(HitResult.GetActor()))
		{
			if (!bIsDraw)
			{
				CollisionActor->OnEnterAgentLocation();
			}
			OnLeftMouseDown.Broadcast(false);
			UE_LOG(LogTemp, Warning, TEXT("获取到 %s"), *HitResult.GetActor()->GetName());
			return;
		}
		
	}

	OnLeftMouseDown.Broadcast(true);
}

void ARTSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	checkf(InputMappingContext, TEXT("InputMappingContext无效"));
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	Subsystem->AddMappingContext(InputMappingContext, 0);
}

void ARTSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	check(IA_MoveAction);

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);
	EnhancedInputComponent->BindAction(IA_MoveAction, ETriggerEvent::Triggered, this, &ARTSPlayerController::Input_Move);
	EnhancedInputComponent->BindAction(IA_ZoomAction, ETriggerEvent::Triggered, this, &ARTSPlayerController::Input_Zoom);
	EnhancedInputComponent->BindAction(IA_LeftButton, ETriggerEvent::Started, this, &ARTSPlayerController::Input_LeftButtonPressed);
	EnhancedInputComponent->BindAction(IA_LeftButton, ETriggerEvent::Completed, this, &ARTSPlayerController::Input_LeftButtonReleased);
}

void ARTSPlayerController::Input_Move(const FInputActionValue& Value)
{
	FVector2D InputAxis2D = Value.Get<FVector2D>();

	const FRotator YawRotator = FRotator(0.0f, GetControlRotation().Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotator).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotator).GetUnitAxis(EAxis::Y);
	
	if (APawn* ControllerPawn = GetPawn())
	{
		ControllerPawn->AddMovementInput(ForwardDirection, InputAxis2D.X);
		ControllerPawn->AddMovementInput(RightDirection, InputAxis2D.Y);
	}
}

void ARTSPlayerController::Input_Zoom(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();

	if (ARTSPawn* RTSPawn = Cast<ARTSPawn>(GetPawn()))
	{
		float ZoomDesired = AxisValue * 2000.f;
		ZoomDesired = FMath::Clamp(ZoomDesired, -2000.f, 2000.f);
		RTSPawn->GetSpringArm()->TargetArmLength += ZoomDesired;
	}
}

void ARTSPlayerController::Input_LeftButtonPressed(const FInputActionValue& Value)
{
	OnLeftMouseDown.Broadcast(false);
}

void ARTSPlayerController::Input_LeftButtonReleased(const FInputActionValue& Value)
{
	FVector2D MouseClickedPosition;
	GetMousePosition(MouseClickedPosition.X, MouseClickedPosition.Y);
	OnMouseRightButtonClicked(MouseClickedPosition);
}
