
#pragma once

#include "CoreMinimal.h"
#include "BattleFrameInterface.h"
#include "AntTypes/AntStructs.h"
#include "GameFramework/Actor.h"
#include "Traits/Team.h"
#include "GeneralActorBase.generated.h"

class AFlowField;
class UBFSubjectiveAgentComponent;

UCLASS()
class ANTTEST_API AGeneralActorBase : public AActor, public IBattleFrameInterface
{
	GENERATED_BODY()
	
public:	
	
	AGeneralActorBase();

protected:

	virtual void BeginPlay() override;
	
private:
	void InitializeSubjectiveAgent();
	
	UPROPERTY()
	USceneComponent* Root;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SKeletalMesh", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* GeneralSkeletalMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SKeletalMesh", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* MountSkeletalMeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Subjective", meta = (AllowPrivateAccess = "true"))
	UBFSubjectiveAgentComponent* SubjectiveAgentComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	FGeneralData GeneralData;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ConfigData", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<AFlowField> FlowFieldToUse;

	UPROPERTY(Transient)
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
};
