// Fill out your copyright notice in the Description page of Project Settings.


#include "BFAntFunctionLibrary.h"

#include "BF/Actors/GeneralActorBase.h"
#include "BF/Traits/Group.h"
#include "BF/Traits/Unit.h"
#include "Traits/Death.h"
#include "Traits/PrimaryType.h"
#include "Traits/Team.h"

TArray<FSubjectHandle> UBFAntFunctionLibrary::GetFilterSurvivalAgent(const UObject* WorldContextObject,
	const FBFFilter& Filter)
{
	if (!GEngine) return TArray<FSubjectHandle>();
	
	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	
	TArray<FSubjectHandle> SubjectHandles;
	
	if (WorldContextObject)
	{
		auto Mechanism = UMachine::ObtainMechanism(World);

		if (Mechanism)
		{
			FFilter FoundFilter = FFilter::Make<FAgent>();
			FoundFilter.Include(Filter.IncludeTraits);
			FoundFilter.Exclude(Filter.ExcludeTraits);
			Mechanism->CollectSubjects(SubjectHandles, FoundFilter);
		}
		return SubjectHandles;
	}
	return TArray<FSubjectHandle>();
}

TArray<FSubjectHandle> UBFAntFunctionLibrary::FilterSurvivalAgentArray(const UObject* WorldContextObject,
                                                                       TArray<FSubjectHandle> SubjectHandles, UScriptStruct* Trait)
{
	if (!GEngine) return TArray<FSubjectHandle>();
	
	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	
	TArray<FSubjectHandle> FilterSubjectHandles;
	
	if (WorldContextObject)
	{
		auto Mechanism = UMachine::ObtainMechanism(World);

		if (Mechanism)
		{
			FFilter Filter = FFilter::Make<FAgent>();
			//Filter.Include(Traits);
			Filter.Exclude<FDying>();
			Filter.Exclude<FGeneral>();
			Mechanism->CollectSubjects(SubjectHandles, Filter);
		}
		return SubjectHandles;
	}
	return TArray<FSubjectHandle>();
}

UScriptStruct* UBFAntFunctionLibrary::GetTeamTrait(const int32 Index)
{
	TArray<UScriptStruct*> TeamStructs
	{
		FTeam0::StaticStruct(),
		FTeam1::StaticStruct(),
		FTeam2::StaticStruct(),
		FTeam3::StaticStruct(),
		FTeam4::StaticStruct(),
		FTeam5::StaticStruct(),
		FTeam6::StaticStruct(),
		FTeam7::StaticStruct(),
		FTeam8::StaticStruct(),
		FTeam9::StaticStruct()
	};
	
	return TeamStructs[Index];
}

UScriptStruct* UBFAntFunctionLibrary::GetGroupTrait(const int32 Index)
{
	TArray<UScriptStruct*> GroupStructs
	{
		FGroup0::StaticStruct(),
		FGroup1::StaticStruct(),
		FGroup2::StaticStruct(),
		FGroup3::StaticStruct(),
		FGroup4::StaticStruct(),
		FGroup5::StaticStruct(),
		FGroup6::StaticStruct(),
		FGroup7::StaticStruct(),
		FGroup8::StaticStruct(),
		FGroup9::StaticStruct()
	};

	return GroupStructs[Index];
}

UScriptStruct* UBFAntFunctionLibrary::GetUnitTrait(const int32 Index)
{
	TArray<UScriptStruct*> UnitStructs
	{
		FUnit0::StaticStruct(),
		FUnit1::StaticStruct(),
		FUnit2::StaticStruct(),
		FUnit3::StaticStruct(),
		FUnit4::StaticStruct(),
		FUnit5::StaticStruct(),
		FUnit6::StaticStruct(),
		FUnit7::StaticStruct(),
		FUnit8::StaticStruct(),
		FUnit9::StaticStruct()
	};

	return UnitStructs[Index];
}

int32 UBFAntFunctionLibrary::QueueCount = 0;

void UBFAntFunctionLibrary::SetQueueCount(const int32 InQueueCount)
{
	QueueCount = 0;
	QueueCount = InQueueCount;
}

void UBFAntFunctionLibrary::ReduceQueueCount(const int32 Value)
{
	QueueCount -= Value;
	QueueCount = QueueCount < 0 ? 0 : QueueCount;
}

int32 UBFAntFunctionLibrary::GetQueueCount()
{
	return QueueCount;
}
