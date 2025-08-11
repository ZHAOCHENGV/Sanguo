// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "Skelot.h"

#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY(LogSkelot);


#define LOCTEXT_NAMESPACE "FSkelotModule"

void FSkelotModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	AddShaderSourceDirectoryMapping(TEXT("/Plugin/Skelot"), FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("Skelot"))->GetBaseDir(), TEXT("Shaders")));

	//GEngine is not ready right now because of	"LoadingPhase": "PostConfigInit"
	FCoreDelegates::OnPostEngineInit.AddLambda([]() {
		extern void Skelot_PreRenderFrame(class FRDGBuilder&);
		extern void Skelot_PostRenderFrame(class FRDGBuilder&);

		check(GEngine);
		GEngine->GetPreRenderDelegateEx().AddStatic(&Skelot_PreRenderFrame);
		GEngine->GetPostRenderDelegateEx().AddStatic(&Skelot_PostRenderFrame);
	}); 

	FCoreDelegates::OnBeginFrame.AddStatic(&FSkelotModule::OnBeginFrame);
	FCoreDelegates::OnEndFrame.AddStatic(&FSkelotModule::OnEndFrame);
}

void FSkelotModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FSkelotModule::OnBeginFrame()
{
	extern int32 GSkelot_NumTransitionGeneratedThisFrame;
	GSkelot_NumTransitionGeneratedThisFrame = 0;
}

void FSkelotModule::OnEndFrame()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSkelotModule, Skelot)



