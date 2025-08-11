/*
 * ░▒▓ APPARATUS ▓▒░
 * 
 * File: ApparatusCustomVersion.cpp
 * Created: 2023-03-05 19:28:02
 * Author: Vladislav Dmitrievich Turbanov (vladislav@turbanov.ru)
 * ───────────────────────────────────────────────────────────────────
 * 
 * The Apparatus source code is for your internal usage only.
 * Redistribution of this file is strictly prohibited.
 * 
 * Community forums: https://talk.turbanov.ru
 * 
 * Copyright 2019 - 2022, SP Vladislav Dmitrievich Turbanov
 * Made in Russia, Moscow City, Chekhov City ♡
 */

#include "More/HAL/UnrealMemory.h"

#if DEBUG_APPARATUS_MEMORY
FCriticalSection FMoreMemory::AllocationsCS;
std::unordered_set<void*> FMoreMemory::Allocations;
#endif
