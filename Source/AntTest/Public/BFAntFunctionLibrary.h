// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SubjectHandle.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Traits/Trace.h"
#include "BFAntFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class ANTTEST_API UBFAntFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary", meta = (WorldContext = "WorldContextObject"))
	static TArray<FSubjectHandle> GetFilterSurvivalAgent(const UObject* WorldContextObject, const FBFFilter& Filter);

	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary", meta = (WorldContext = "WorldContextObject"))
	static TArray<FSubjectHandle> FilterSurvivalAgentArray(const UObject* WorldContextObject, TArray<FSubjectHandle> SubjectHandles, UScriptStruct* Trait);

	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary")
	static UScriptStruct* GetTeamTrait(const int32 Index);

	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary")
	static UScriptStruct* GetGroupTrait(const int32 Index);

	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary")
	static UScriptStruct* GetUnitTrait(const int32 Index);

	UFUNCTION(BlueprintCallable, Category = "BFAntFunctionLibrary")
	static void SetQueueCount(const int32 InQueueCount);

	UFUNCTION(BlueprintCallable, Category = "BFAntFunctionLibrary")
	static void ReduceQueueCount(const int32 Value);

	UFUNCTION(BlueprintPure, Category = "BFAntFunctionLibrary")
	static int32 GetQueueCount();

private:
	static int32 QueueCount;
};
