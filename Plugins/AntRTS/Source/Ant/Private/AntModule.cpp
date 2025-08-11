// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "AntModule.h"

#define LOCTEXT_NAMESPACE "FAntModule"

DEFINE_LOG_CATEGORY(LogAnt);

DEFINE_STAT(STAT_ANT_QPS);
DEFINE_STAT(STAT_ANT_PxUpdate);
DEFINE_STAT(STAT_ANT_BFUpdate);
DEFINE_STAT(STAT_ANT_MovementsUpdate);
DEFINE_STAT(STAT_ANT_Pathfinding);
DEFINE_STAT(STAT_ANT_Queries);
DEFINE_STAT(STAT_ANT_PostPX);
DEFINE_STAT(STAT_ANT_TotalFrame);
DEFINE_STAT(STAT_ANT_NumAgents);
DEFINE_STAT(STAT_ANT_NumMovingAgents);
DEFINE_STAT(STAT_ANT_NumPaths);
DEFINE_STAT(STAT_ANT_NumAsyncQueries);

void FAntModule::StartupModule()
{
	// register per project plugin settings
	//if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	//	SettingsModule->RegisterSettings("Project", "Plugins", "AntPlugin_Settings",
	//		LOCTEXT("RuntimeSettingsName", "Ant Settiings"), LOCTEXT("RuntimeSettingsDescription", "Configure Ant Settings"),
	//		GetMutableDefault<UAntPlugin_Settings>());
}

void FAntModule::ShutdownModule()
{
	// unregister per project plugin settings
	//if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	//	SettingsModule->UnregisterSettings("Project", "Plugins", "AntPlugin_Settings");
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAntModule, Ant)

