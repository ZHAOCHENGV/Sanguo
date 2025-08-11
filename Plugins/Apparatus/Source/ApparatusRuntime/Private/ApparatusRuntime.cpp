/*
 * ░▒▓ APPARATUS ▓▒░
 * 
 * File: ApparatusRuntime.cpp
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

#include "ApparatusRuntime.h"

#include "CoreGlobals.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h" // Checking for an active Editor transaction.
#endif


#define LOCTEXT_NAMESPACE "FApparatusRuntimeModule"

DEFINE_LOG_CATEGORY(LogApparatus);

#if WITH_EDITOR
bool
IsEditorTransactionActive()
{
	return IsInGameThread() && (GEditor != nullptr) && GEditor->IsTransactionActive();
}
#endif // WITH_EDITOR

void FApparatusRuntimeModule::StartupModule()
{
}

void FApparatusRuntimeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FApparatusRuntimeModule, ApparatusRuntime)

#undef LOCTEXT_NAMESPACE
