// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotCommandlet.h"
#include "SkelotPrivate.h"
#include "SkelotPrivateUtils.h"
#include "SkelotSpanAllocator.h"


int32 USkelotCommandlet::Main(const FString& Params)
{
	for(int i = 0; i < 1024; i++)
	{
		FTransform3f TF(FRotator3f(FMath::FRand() * 360, FMath::FRand() * 360, FMath::FRand() * 360	), FVector3f::Zero(), FVector3f(-2, 1, 1));
		FMatrix44f Mtx = TF.ToMatrixWithScale();
		FVector3f SV = Mtx.GetScaleVector();
		FString Str = TF.ToHumanReadableString();
		float Det = Mtx.RotDeterminant();
		UE_LOGFMT(LogSkelot, Warning, "{Str} {Scale} {Det}", Str, SV.ToString(), Det);
	}

	return 0;
}
