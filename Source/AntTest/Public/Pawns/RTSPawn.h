// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RTSPawn.generated.h"

class UFloatingPawnMovement;
class UCameraComponent;
class USpringArmComponent;

UCLASS()
class ANTTEST_API ARTSPawn : public APawn
{
	GENERATED_BODY()

public:
	ARTSPawn();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Root", meta = (AllowPrivateAccess = "true"))
	USceneComponent* Root;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowedCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	UFloatingPawnMovement* FloatingPawnMovement;

public:
	FORCEINLINE USpringArmComponent* GetSpringArm() const {return SpringArm;}
};
