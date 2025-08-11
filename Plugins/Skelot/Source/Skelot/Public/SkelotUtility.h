// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "SkelotComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "SkelotUtility.generated.h"



UCLASS()
class SKELOT_API USkelotBPUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*
	* iterate over all the instances and find the closest hit.  its not much optimal since there is not any broad phase.
	*/
	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static UPARAM(DisplayName="OutInstanceIndex") int LineTraceInstancesSingle(USkelotComponent* Component, const FVector& Start, const FVector& End, float Thickness, float DebugDrawTime, double& OutTime, FVector& OutPosition, FVector& OutNormal, int& OutBoneIndex);

	/*
	* @param bSetCustomPrimitiveDataFloat	 copy PerInstanceCustomDataFloat to each Component's CustomPrimitiveDataFloat ?
	*/
	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static USkeletalMeshComponent* ConstructSkeletalMeshComponentsFromInstance(const USkelotComponent* Component, int InstanceIndex, UObject* Outer = nullptr, bool bSetCustomPrimitiveDataFloat = true);

	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static void MoveAllInstancesConditional(USkelotComponent* Component, FVector Offset, UPARAM(meta = (Bitmask, BitmaskEnum = EInstanceUserFlags)) int32 FlagsToTest, bool bAllFlags, bool bInvert);


	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static void QueryLocationOverlappingBox(USkelotComponent* Component, const FBox& Box, TArray<int>& InstanceIndices)
	{
		if(IsValid(Component))
			Component->QueryLocationOverlappingBox(FBox3f(Box), InstanceIndices);
	}

	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static void QueryLocationOverlappingSphere(USkelotComponent* Component, const FVector& Center, float Radius, TArray<int>& InstanceIndices)
	{
		if(IsValid(Component))
			Component->QueryLocationOverlappingSphere(FVector3f(Center), Radius, InstanceIndices);
	}




	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static void QueryLocationOverlappingBoxAdvanced(USkelotComponent* Component, UPARAM(meta = (Bitmask, BitmaskEnum = EInstanceUserFlags)) int32 FlagsToTest, bool bAllFlags, bool bInvert, const FBox& Box, TArray<int>& InstanceIndices);

	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static void QueryLocationOverlappingSphereAdvanced(USkelotComponent* Component, UPARAM(meta = (Bitmask, BitmaskEnum = EInstanceUserFlags)) int32 FlagsToTest, bool bAllFlags, bool bInvert, const FVector& Center, float Radius, TArray<int>& InstanceIndices);

	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static void QueryLocationOverlappingComponentAdvanced(USkelotComponent* Component, UPrimitiveComponent* ComponentToTest, UPARAM(meta = (Bitmask, BitmaskEnum = EInstanceUserFlags)) int32 FlagsToTest, bool bAllFlags, bool bInvert, TArray<int>& InstanceIndices);



	UFUNCTION(BlueprintCallable, Category = "Skelot|Utility")
	static float InstancePlayAnimation(FSkelotInstanceRef InstanceHandle, FSkelotAnimPlayParams Params);
};

