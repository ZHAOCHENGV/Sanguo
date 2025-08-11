/*
 * ░▒▓ APPARATUS ▓▒░
 * 
 * File: Subjective.cpp
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

#include "Subjective.h"

#include "Machine.h"

TArray<FTraitRecord> ISubjective::EmptyTraits;
TArray<UDetail*> ISubjective::EmptyDetails;

const FFingerprint&
ISubjective::GetFingerprint() const
{
	if (LIKELY(Handle))
	{
		return Handle.GetFingerprint();
	}

	// The subjective is not registered just yet.
	// Assemble a temporary fingerprint as a workaround...
	static thread_local FFingerprint FingerprintTemp;
	FingerprintTemp.Reset(GetFlagmark());
	const auto& Traits = GetTraitRecordsRef();
	for (const auto& TraitRecord : Traits)
	{
		const auto TraitType = TraitRecord.GetType();
		if (LIKELY(TraitType))
		{
			FingerprintTemp.Add(TraitType);
		}
	}
	const auto& Details = GetDetailsRef();
	FingerprintTemp.Add(Details);
	return FingerprintTemp;
}
