// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "Skelot.h"

DECLARE_STATS_GROUP(TEXT("Skelot"), STATGROUP_SKELOT, STATCAT_Advanced);




DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("ViewNumCulledInstance"), STAT_SKELOT_ViewNumCulled, STATGROUP_SKELOT, SKELOT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("ViewNumVisibleInstance"), STAT_SKELOT_ViewNumVisible, STATGROUP_SKELOT, SKELOT_API);

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("ShadowNumCulledInstance"), STAT_SKELOT_ShadowNumCulled, STATGROUP_SKELOT, SKELOT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("ShadowNumVisibleInstance"), STAT_SKELOT_ShadowNumVisible, STATGROUP_SKELOT, SKELOT_API);


DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("NumTransitionPoseGenerated"), STAT_SKELOT_NumTransitionPoseGenerated, STATGROUP_SKELOT, SKELOT_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("TotalInstances"), STAT_SKELOT_TotalInstances, STATGROUP_SKELOT, SKELOT_API);


#define SKELOT_SCOPE_CYCLE_COUNTER(CounterName) DECLARE_SCOPE_CYCLE_COUNTER(TEXT(#CounterName), STAT_SKELOT_##CounterName, STATGROUP_SKELOT)

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#define SKELOT_AUTO_CVAR_DEBUG(Type, Name, DefaultValue, Help, Flags) \
Type GSkelot_##Name = DefaultValue; \
FAutoConsoleVariableRef CVarSkelot_##Name(TEXT("skelot."#Name), GSkelot_##Name, TEXT(Help), Flags); \

#else

#define SKELOT_AUTO_CVAR_DEBUG(Type, Name, DefaultValue, Help, Flags) constexpr Type GSkelot_##Name = DefaultValue;



#endif

