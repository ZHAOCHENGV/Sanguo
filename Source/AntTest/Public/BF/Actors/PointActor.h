// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SubjectiveActor.h"
#include "PointActor.generated.h"

/**
 * 
 */
UCLASS()
class ANTTEST_API APointActor : public ASubjectiveActor
{
	GENERATED_BODY()

public:

	APointActor();

private:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* StaticMeshComponent;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	UStaticMesh* PointMesh;
};
