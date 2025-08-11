// Copyright Epic Games, Inc. All Rights Reserved.

#include "AntTest.h"

#include "AntFunctionLibrary.h"
#include "Modules/ModuleManager.h"
#include "AntUtil.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, AntTest, "AntTest" );

void UAntHelperFunctionLibrary::CopyEnemyTranformsToSkelot(const UObject *WorldContextObject, USkelotComponent *SkelotCom, int32 Flags)
{
	auto *ant = WorldContextObject ? WorldContextObject->GetWorld()->GetSubsystem<UAntSubsystem>() : nullptr;;
	if (ant && SkelotCom)
		for (const auto &agent : ant->GetUnderlyingAgentsList())
			if (CHECK_BIT_ANY(agent.GetFlag(), Flags))
			{
				const auto &unitData = ant->GetAgentUserData(agent.GetHandle()).Get<FUnitData>();

				// set animation instance transform according to the current agent location
				const auto scale = SkelotCom->GetInstanceTransform(unitData.SkelotInstanceID).GetScale3D();
				const FTransform3f transform(FQuat4f(FVector3f::UpVector, agent.FaceAngle - RAD_90), agent.GetLocationLerped(), scale);
				SkelotCom->SetInstanceTransform(unitData.SkelotInstanceID, transform);

				// play animation in the first time right after the spawn
				if (SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.WalkAnim)
					SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.WalkAnim, true, 0, 1, 2.0f);


				// defualt play scale is 1
				SkelotCom->SetInstancePlayScale(unitData.SkelotInstanceID, 1.0f);

				// adjust play scale according to the current velocity
				if (agent.GetVelocity().SizeSquared() <= 10)
					SkelotCom->SetInstancePlayScale(unitData.SkelotInstanceID, 0.9f);

				if (agent.GetVelocity().SizeSquared() <= 7)
					SkelotCom->SetInstancePlayScale(unitData.SkelotInstanceID, 0.7f);

				if (agent.GetVelocity().SizeSquared() <= 4)
					SkelotCom->SetInstancePlayScale(unitData.SkelotInstanceID, 0.5f);
			}
}

void UAntHelperFunctionLibrary::CopyHeroTransformToSkelot(const UObject *WorldContextObject, USkelotComponent *SkelotCom, FAntHandle HeroHandle)
{
	auto *ant = WorldContextObject ? WorldContextObject->GetWorld()->GetSubsystem<UAntSubsystem>() : nullptr;;
	if (ant && SkelotCom && ant->IsValidAgent(HeroHandle))
	{
		const auto &unitData = ant->GetAgentUserData(HeroHandle).Get<FUnitData>();
		const auto &agentData = ant->GetAgentData(HeroHandle);

		// set animation instance transform according to the current agent location
		const auto scale = SkelotCom->GetInstanceTransform(unitData.SkelotInstanceID).GetScale3D();
		const FTransform3f transform(FQuat4f(FVector3f::UpVector, agentData.FaceAngle - RAD_90), agentData.GetLocationLerped(), scale);
		SkelotCom->SetInstanceTransform(unitData.SkelotInstanceID, transform);

		if (agentData.GetVelocity() != FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.WalkAnim)
			SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.WalkAnim, true, 0, 1, 0.0f);
		else if (agentData.GetVelocity() == FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.IdleAnim)
			SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.IdleAnim, true, 0, 1, 0.4f);
	}
}

void UAntHelperFunctionLibrary::CopyEnemyTranformsToSkelot_New(const UObject *WorldContextObject, USkelotComponent *SkelotCom, TArray<FAntHandle> Handle)
{
	auto *ant = WorldContextObject ? WorldContextObject->GetWorld()->GetSubsystem<UAntSubsystem>() : nullptr;;
	for (const auto& HeroHandle : Handle)
	{
		if (ant && SkelotCom && ant->IsValidAgent(HeroHandle))
		{
			const auto &unitData = ant->GetAgentUserData(HeroHandle).Get<FUnitData>();
			const auto &agentData = ant->GetAgentData(HeroHandle);
			
			// set animation instance transform according to the current agent location
			const auto scale = SkelotCom->GetInstanceTransform(unitData.SkelotInstanceID).GetScale3D();
			const FTransform3f transform(FQuat4f(FVector3f::UpVector, agentData.FaceAngle - RAD_90), agentData.GetLocationLerped(), scale);
			SkelotCom->SetInstanceTransform(unitData.SkelotInstanceID, transform);

			if (unitData.bIsPlayCheerAnim)
			{
				if (agentData.GetVelocity() == FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.CheerAnim)
				{
					SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.CheerAnim, false, 0, 1, 0.0f);
				}
			}
			else
			{
				if (agentData.GetVelocity() != FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.WalkAnim)
					SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.WalkAnim, true, 0, 1, 0.0f);
				else if (agentData.GetVelocity() == FVector3f::ZeroVector && SkelotCom->GetInstanceCurrentAnimSequence(unitData.SkelotInstanceID) != unitData.IdleAnim)
					SkelotCom->InstancePlayAnimation(unitData.SkelotInstanceID, unitData.IdleAnim, true, 0, 1, 0.4f);
			}
		}
	}
}


