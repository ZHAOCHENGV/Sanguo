// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"

#include "SkelotSettings.generated.h"

UCLASS(Config = Skelot, defaultconfig, meta = (DisplayName = "Skelot"))
class SKELOT_API USkelotDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
public:

	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	int32 MaxTransitionGenerationPerFrame;

	USkelotDeveloperSettings(const FObjectInitializer& Initializer);
};