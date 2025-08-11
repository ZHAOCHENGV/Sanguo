#pragma once


namespace Debug
{
	FORCEINLINE void Print(const FString& Msg)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::MakeRandomColor(), Msg);
		}
		
	}
}
