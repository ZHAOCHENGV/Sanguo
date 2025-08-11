// Fill out your copyright notice in the Description page of Project Settings.


#include "BF/Actors/PointActor.h"

APointActor::APointActor()
{
	PrimaryActorTick.bCanEverTick = false;

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(GetRootComponent());
}
