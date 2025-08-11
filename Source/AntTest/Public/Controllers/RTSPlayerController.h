// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTSPlayerController.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeftMouseDown, bool, bIsReleased);

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
/**
 * 
 */
UCLASS()
class ANTTEST_API ARTSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ARTSPlayerController();

	UFUNCTION(BlueprintCallable)
	void OnMouseRightButtonClicked(FVector2D MouseLocation);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsLeftMouseReleased = false;

	UPROPERTY(BlueprintCallable)
	FOnLeftMouseDown OnLeftMouseDown;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:

	bool bIsDraw = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_ZoomAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_LeftButton;

	void Input_Move(const FInputActionValue& Value);
	void Input_Zoom(const FInputActionValue& Value);
	void Input_LeftButtonPressed(const FInputActionValue& Value);
	void Input_LeftButtonReleased(const FInputActionValue& Value);

public:
	FORCEINLINE void SetIsDraw(bool bIsDrawing) { bIsDraw = bIsDrawing;}
};
