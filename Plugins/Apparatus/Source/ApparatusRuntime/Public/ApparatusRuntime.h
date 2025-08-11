/*
 * ░▒▓ APPARATUS ▓▒░
 * 
 * File: ApparatusRuntime.h
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
/**
 * @file 
 * @brief The main Apparatus runtime module.
 */

#pragma once

#include "More/type_traits"

#include "Runtime/Launch/Resources/Version.h"
#include "CoreMinimal.h"
#include "Engine/NetConnection.h"
#include "Modules/ModuleManager.h"
#include "HAL/UnrealMemory.h"

#include "Paradigm.h"


// Global forward declarations:
enum class EApparatusStatus : int8;
enum class EFlagmarkBit : uint8;
class UChunk;
class UBelt;
class AMechanism;
class UMachine;

template < typename ChunkItT, typename BeltItT, EParadigm Paradigm = EParadigm::Safe >
struct TChain;

/**
 * The main Apparatus runtime module.
 */
class APPARATUSRUNTIME_API FApparatusRuntimeModule : public IModuleInterface
{
	friend class UChunk;
	friend class UBelt;
	friend class AMechanism;
	friend class UMachine;

  public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

#if WITH_EDITOR

/**
 * Check if an editor transaction is currently active.
 * 
 * Always returns @c true in a runtime build.
 */
APPARATUSRUNTIME_API bool
IsEditorTransactionActive();

#else //#if !WITH_EDITOR

/**
 * Check if an editor transaction is currently active.
 * 
 * Always returns @c true in a runtime build.
 */
static constexpr bool
IsEditorTransactionActive()
{
	return false;
}

#endif // !WITH_EDITOR


APPARATUSRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogApparatus, Log, All);
