// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#if CPP
#include "SkelotBase.h"
#endif

#include "SkelotBaseBP.generated.h"


#if !CPP

USTRUCT(noexport, BlueprintType)
struct alignas(16) FBoxMinMaxFloat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Math")
	FVector3f Min;
	//
	float Unused;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Math")
	FVector3f Max;
	//
	float Unused2;
};

USTRUCT(noexport, BlueprintType)
struct alignas(16) FBoxMinMaxDouble
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Math")
	FVector Min;
	//
	double Unused;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Math")
	FVector Max;
	//
	double Unused2;
};

USTRUCT(noexport, BlueprintType)
struct FBoxCenterExtentFloat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Math")
	FVector3f Center;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Math")
	FVector3f Extent;
};

USTRUCT(noexport, BlueprintType)
struct FBoxCenterExtentDouble
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Math")
	FVector Center;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skelot|Math")
	FVector Extent;
};

#endif