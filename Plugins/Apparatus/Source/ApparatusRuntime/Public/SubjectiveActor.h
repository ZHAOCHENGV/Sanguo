/*
 * ░▒▓ APPARATUS ▓▒░
 * 
 * File: SubjectiveActor.h
 * Created: Friday, 23rd October 2020 7:00:48 pm
 * Author: Vladislav Dmitrievich Turbanov (vladislav@turbanov.ru)
 * ───────────────────────────────────────────────────────────────────
 * 
 * The Apparatus source code is for your internal usage only.
 * Redistribution of this file is strictly prohibited.
 * 
 * Community forums: https://talk.turbanov.ru
 * 
 * Copyright 2019 - 2023, SP Vladislav Dmitrievich Turbanov
 * Made in Russia, Moscow City, Chekhov City ♡
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Machine.h"

#include "SubjectiveActor.generated.h"


/**
 * An Unreal Engine actor as a subject with details.
 */
UCLASS(ClassGroup = "Apparatus")
class APPARATUSRUNTIME_API ASubjectiveActor
  : public AActor
  , public ISubjective
{
	GENERATED_BODY()

  private:

#pragma region Standard Subjective Block

	/**
	 * The list of traits.
	 */
	UPROPERTY(EditAnywhere, Category = Data,
			  Meta = (DisplayAfter = "Flagmark",
					  AllowPrivateAccess = "true"))
	TArray<FTraitRecord> Traits;

	/**
	 * The list of details.
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = Data,
			  Meta = (DisplayAfter = "Traits",
			  		  AllowPrivateAccess = "true"))
	TArray<UDetail*> Details;

	/**
	 * An optional preferred belt for the subject to be placed in.
	 */
	UPROPERTY(EditAnywhere, Category = Optimization,
			  Meta = (AllowPrivateAccess = "true"))
	UBelt* PreferredBelt = nullptr;

	/**
	 * The flagmark of the subjective.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Data,
			  Meta = (Bitmask, BitmaskEnum = "/Script/ApparatusRuntime.EFlagmark",
					  DisplayAfter = "PreferredBelt",
					  AllowPrivateAccess = "true"))
	int32 Flagmark = FM_None;

	/**
	 * The mechanism to use as a default one,
	 * when registering the subjective.
	 * 
	 * If not set, the default mechanism
	 * of the world will be used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Context,
			  Meta = (AllowPrivateAccess = "true"))
	AMechanism* MechanismOverride = nullptr;

  protected:

	/**
	 * The traits allowed to be received on the server.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Networking,
			  Meta = (DisplayAfter = "Details",
					  AllowPrivateAccess = "true"))
	FTraitmark TraitmarkPermit;

	UPROPERTY(Replicated)
	uint32 SubjectNetworkId = FSubjectNetworkState::InvalidId;

	END_STANDARD_NETWORKED_SUBJECTIVE_PROPERTY_BLOCK(ASubjectiveActor)

#pragma endregion Standard Subjective Block

  public:
	
	// Sets default values for this actor's properties
	ASubjectiveActor();

  protected:

	void
	GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;

	UFUNCTION(Client, Reliable)
	void
	ClientReceiveNetworkId(const uint32 NetworkId);

	OPTIONAL_FORCEINLINE void
	ClientReceiveNetworkId_Implementation(const uint32 NetworkId)
	{
		ISubjective::ClientReceiveNetworkId_Implementation(NetworkId);
	}

	UFUNCTION(Server, Reliable)
	void
	ServerRequestNetworkId();

	OPTIONAL_FORCEINLINE void
	ServerRequestNetworkId_Implementation()
	{
		ISubjective::ServerRequestNetworkId_Implementation();
	}

#pragma region Reliable

	UFUNCTION(Server, Reliable, WithValidation)
	void
	ServerReceiveTrait(UScriptStruct*       TraitType,
					   const TArray<uint8>& TraitData);

	OPTIONAL_FORCEINLINE void
	ServerReceiveTrait_Implementation(UScriptStruct*       TraitType,
									  const TArray<uint8>& TraitData)
	{
		ISubjective::PeerReceiveTrait_Implementation(EPeerRole::Server, TraitType, TraitData);
	}

	OPTIONAL_FORCEINLINE bool
	ServerReceiveTrait_Validate(UScriptStruct*       TraitType,
								const TArray<uint8>& TraitData)
	{
		return ISubjective::PeerReceiveTrait_Validate(EPeerRole::Server, TraitType, TraitData);
	}

	UFUNCTION(Client, Reliable, WithValidation)
	void
	ClientReceiveTrait(UScriptStruct*       TraitType,
					   const TArray<uint8>& TraitData);

	OPTIONAL_FORCEINLINE void
	ClientReceiveTrait_Implementation(UScriptStruct*       TraitType,
									  const TArray<uint8>& TraitData)
	{
		ISubjective::PeerReceiveTrait_Implementation(EPeerRole::Client, TraitType, TraitData);
	}

	OPTIONAL_FORCEINLINE bool
	ClientReceiveTrait_Validate(UScriptStruct*       TraitType,
								const TArray<uint8>& TraitData)
	{
		return ISubjective::PeerReceiveTrait_Validate(EPeerRole::Client, TraitType, TraitData);
	}

#pragma endregion Reliable

#pragma region Unreliable

	UFUNCTION(Server, Unreliable, WithValidation)
	void
	ServerReceiveTraitUnreliable(UScriptStruct*       TraitType,
								 const TArray<uint8>& TraitData);

	OPTIONAL_FORCEINLINE void
	ServerReceiveTraitUnreliable_Implementation(UScriptStruct*       TraitType,
												const TArray<uint8>& TraitData)
	{
		ISubjective::PeerReceiveTrait_Implementation(EPeerRole::Server, TraitType, TraitData);
	}

	OPTIONAL_FORCEINLINE bool
	ServerReceiveTraitUnreliable_Validate(UScriptStruct*       TraitType,
										  const TArray<uint8>& TraitData)
	{
		return ISubjective::PeerReceiveTrait_Validate(EPeerRole::Server, TraitType, TraitData);
	}

	UFUNCTION(Client, Unreliable, WithValidation)
	void
	ClientReceiveTraitUnreliable(UScriptStruct*       TraitType,
								 const TArray<uint8>& TraitData);

	OPTIONAL_FORCEINLINE void
	ClientReceiveTraitUnreliable_Implementation(UScriptStruct*       TraitType,
												const TArray<uint8>& TraitData)
	{
		ISubjective::PeerReceiveTrait_Implementation(EPeerRole::Client, TraitType, TraitData);
	}

	OPTIONAL_FORCEINLINE bool
	ClientReceiveTraitUnreliable_Validate(UScriptStruct*       TraitType,
										  const TArray<uint8>& TraitData)
	{
		return ISubjective::PeerReceiveTrait_Validate(EPeerRole::Client, TraitType, TraitData);
	}

#pragma endregion Unreliable

	OPTIONAL_FORCEINLINE void
	AssignNetworkIdOnClient(const uint32 NetworkId) override
	{
		SubjectNetworkId = NetworkId;
		if (GetNetConnection())
		{
			ClientReceiveNetworkId(NetworkId);
		}
	}

	OPTIONAL_FORCEINLINE void
	ObtainNetworkIdFromServer() override
	{
		if (LIKELY(SubjectNetworkId >= FSubjectNetworkState::FirstId))
		{
			// Already obtained via field replication...
			return;
		}
		if (GetNetConnection())
		{
			ServerRequestNetworkId();
		}
		else
		{
			auto* const World = GetWorld();
			if (World && (World->IsNetMode(NM_DedicatedServer) || World->IsNetMode(NM_ListenServer)))
			{
				check(Handle.IsValid());
				SubjectNetworkId = Handle.GetInfo().ObtainNetworkState()->ServerObtainId();
			}
			else
			{
				UE_LOG(LogApparatus, Error,
					   TEXT("Impossible to get a network identifier for: %s"),
					   *GetName());
			}
		}
	}

	OPTIONAL_FORCEINLINE bool
	ShouldBeReplicated() const override
	{
		return GetIsReplicated();
	}

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	OPTIONAL_FORCEINLINE void
	Serialize(FArchive& Archive) override
	{
		ISubjective::DoStartSerialization(Archive);
		Super::Serialize(Archive);
		ISubjective::DoFinishSerialization(Archive);
	}

	OPTIONAL_FORCEINLINE TPortableOutcome<>
	DoPushTrait(const EParadigm      Paradigm,
				UScriptStruct* const TraitType,
				const EPeerRole      PeerRole = EPeerRole::Auto,
				const bool           bReliable = true) const override
	{
		if (IsHarsh(Paradigm))
		{
			return (TPortableOutcome<>)ISubjective::template DoPushTrait<EParadigm::HarshSafe>(
				this, TraitType,
				PeerRole,
				bReliable,
				&ASubjectiveActor::ServerReceiveTrait,
				&ASubjectiveActor::ClientReceiveTrait,
				&ASubjectiveActor::ServerReceiveTraitUnreliable,
				&ASubjectiveActor::ClientReceiveTraitUnreliable);
		}
		else
		{
			return ISubjective::template DoPushTrait<EParadigm::PoliteSafe>(
				this, TraitType,
				PeerRole,
				bReliable,
				&ASubjectiveActor::ServerReceiveTrait,
				&ASubjectiveActor::ClientReceiveTrait,
				&ASubjectiveActor::ServerReceiveTraitUnreliable,
				&ASubjectiveActor::ClientReceiveTraitUnreliable);
		}
	}

	OPTIONAL_FORCEINLINE TPortableOutcome<>
	DoPushTrait(const EParadigm      Paradigm,
				UScriptStruct* const TraitType,
				const void* const    TraitData,
				const bool           bSetForLocal = false,
				const EPeerRole      PeerRole = EPeerRole::Auto,
				const bool           bReliable = true) override
	{
		if (IsHarsh(Paradigm))
		{
			return (TPortableOutcome<>)ISubjective::template DoPushTrait<EParadigm::HarshSafe>(
				this, TraitType, TraitData,
				bSetForLocal, PeerRole,
				bReliable,
				&ASubjectiveActor::ServerReceiveTrait,
				&ASubjectiveActor::ClientReceiveTrait,
				&ASubjectiveActor::ServerReceiveTraitUnreliable,
				&ASubjectiveActor::ClientReceiveTraitUnreliable);
		}
		else
		{
			return ISubjective::template DoPushTrait<EParadigm::PoliteSafe>(
				this, TraitType, TraitData,
				bSetForLocal, PeerRole,
				bReliable,
				&ASubjectiveActor::ServerReceiveTrait,
				&ASubjectiveActor::ClientReceiveTrait,
				&ASubjectiveActor::ServerReceiveTraitUnreliable,
				&ASubjectiveActor::ClientReceiveTraitUnreliable);
		}
	}

  public:

	OPTIONAL_FORCEINLINE AActor*
	GetActor() const override
	{
		return const_cast<ASubjectiveActor*>(this);
	}

	OPTIONAL_FORCEINLINE void
	NotifyHandleDespawned() override
	{
		ISubjective::NotifyHandleDespawned();
		Destroy();
	}

}; //-class ASubjectiveActor


