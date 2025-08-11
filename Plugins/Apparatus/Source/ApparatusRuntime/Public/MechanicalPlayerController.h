/*
 * ░▒▓ APPARATUS ▓▒░
 * 
 * File: MechanicalPlayerController.h
 * Created: 2021-09-21 21:03:27
 * Author: Vladislav Dmitrievich Turbanov (vladislav@turbanov.ru)
 * ───────────────────────────────────────────────────────────────────
 * 
 * The Apparatus source code is for your internal usage only.
 * Redistribution of this file is strictly prohibited.
 * 
 * Community forums: https://talk.turbanov.ru
 * 
 * Copyright 2019 - 2023, SP Vladislav Dmitrievich Turbanov
 * Made in Russia, Moscow City, Chekhov City ♡
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Machine.h"

#include "MechanicalPlayerController.generated.h"


/**
 * The mechanical player controller entity.
 */
UCLASS()
class APPARATUSRUNTIME_API AMechanicalPlayerController
  : public AActor
  , public IMechanical
{
	GENERATED_BODY()

	/**
	 * The steady update time interval.
	 */
	UPROPERTY(EditAnywhere, Category = Mechanism,
			  Meta = (AllowPrivateAccess = "true"))
	float SteadyDeltaTime = IMechanical_DefaultSteadyDeltaTime;

  public:

	/**
	 * Construct a new mechanism.
	 */
	AMechanicalPlayerController();

  protected:

	/**
	 * Begin executing the mechanism.
	 */
	OPTIONAL_FORCEINLINE virtual void
	BeginPlay() override
	{
		Super::BeginPlay();
		DoRegister();
	}

	/**
	 * End executing the mechanism.
	 */
	OPTIONAL_FORCEINLINE virtual void
	EndPlay(const EEndPlayReason::Type EndPlayReason) override
	{
		DoUnregister();
		Super::EndPlay(EndPlayReason);
	}

  public:

	virtual float
	GetSteadyDeltaTime() const override
	{
		return SteadyDeltaTime;
	}

	virtual float
	GetOwnTime() const override
	{
		return GetGameTimeSinceCreation();
	}

	OPTIONAL_FORCEINLINE virtual void
	Tick(float DeltaTime) override
	{
		DoTick(GetGameTimeSinceCreation(),
			   DeltaTime,
			   SteadyDeltaTime);
	}

	/**
	 * Get the time of the last processed steady frame.
	 */
	OPTIONAL_FORCEINLINE float
	GetProcessedSteadyTime() const
	{
		return IMechanical::GetProcessedSteadyTime_Implementation();
	}

	/**
	 * The current ratio within the steady frame.
	 * 
	 * This is in relation between the previous steady
	 * frame and the current next one.
	 * Should be used for interframe interpolation.
	 */
	OPTIONAL_FORCEINLINE float
	CalcSteadyFrameRatio() const
	{
		return IMechanical::CalcSteadyFrameRatio_Implementation();
	}

	/**
	 * The current steady frame.
	 */
	OPTIONAL_FORCEINLINE int64
	GetSteadyFrame() const
	{
		return SteadyFrame;
	}

	/**
	 * The total steady time elapsed.
	 */
	OPTIONAL_FORCEINLINE float
	GetSteadyTime() const
	{
		return IMechanical::GetSteadyTime_Implementation();
	}

	/**
	 * The current steady future factor.
	 * 
	 * This is in relation between the previous change time
	 * delta to the next steady frame change delta time.
	 */
	OPTIONAL_FORCEINLINE float
	CalcSteadyFutureFactor() const
	{
		return IMechanical::CalcSteadyFutureFactor_Implementation();
	}

}; //-class AMechanicalPlayerController

OPTIONAL_FORCEINLINE
AMechanicalPlayerController::AMechanicalPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
}
