// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "SkelotComponent.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "UObject/Script.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkelotComponent)

#define LOCTEXT_NAMESPACE "Skelot"

void USkelotComponent::K2_GetInstanceCustomStruct(ESkelotValidity& ExecResult, int InstanceIndex, int32& InStruct)
{
	checkNoEntry();
}

void USkelotComponent::K2_SetInstanceCustomStruct(int InstanceIndex, const int32& InStruct)
{
	checkNoEntry();
}

DEFINE_FUNCTION(USkelotComponent::execK2_GetInstanceCustomStruct)
{
	USkelotComponent* SkelotComponent = P_THIS;
	P_GET_ENUM_REF(ESkelotValidity, ExecResult);
	P_GET_PROPERTY(FIntProperty, InstanceIndex);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* ValueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ExecResult = ESkelotValidity::NotValid;

	if (!ValueProp || !ValuePtr)
	{
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AbortExecution, INVTEXT("Failed to resolve the Value")));
		return;
	}

	if (!::IsValid(SkelotComponent))
	{
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AccessViolation, INVTEXT("Invalid Component")));
		return;
	}

	if (!SkelotComponent->IsInstanceValid(InstanceIndex))
	{
		//FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AccessViolation, INVTEXT("Invalid InstanceIndex")));
		return;
	}

	const UScriptStruct* ScriptStruct = SkelotComponent->PerInstanceScriptStruct;
	const bool bCompatible = ScriptStruct == ValueProp->Struct || (ScriptStruct && ScriptStruct->IsChildOf(ValueProp->Struct));
	if (!bCompatible)
	{
		//FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AccessViolation, INVTEXT("CustomPerInstanceStruct Mismatch")));
		return;
	}

	{
		P_NATIVE_BEGIN;
		const int StructSize = ScriptStruct->GetStructureSize();
		uint8* InstanceStructPtr = SkelotComponent->InstancesData.CustomPerInstanceStruct.GetData() + (StructSize * InstanceIndex);
		ValueProp->Struct->CopyScriptStruct(ValuePtr, InstanceStructPtr);
		ExecResult = ESkelotValidity::Valid;
		P_NATIVE_END;
	}
}

DEFINE_FUNCTION(USkelotComponent::execK2_SetInstanceCustomStruct)
{
	USkelotComponent* SkelotComponent = P_THIS;
	P_GET_PROPERTY(FIntProperty, InstanceIndex);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* ValueProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (!ValueProp || !ValuePtr)
	{
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AbortExecution, INVTEXT("Failed to resolve the Value")));
		return;
	}

	if (!::IsValid(SkelotComponent))
	{
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AccessViolation, INVTEXT("Invalid Component")));
		return;
	}

	if (!SkelotComponent->IsInstanceValid(InstanceIndex))
	{
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AccessViolation, INVTEXT("Invalid InstanceIndex")));
		return;
	}

	const UScriptStruct* ScriptStruct = SkelotComponent->PerInstanceScriptStruct;
	const bool bCompatible = ScriptStruct == ValueProp->Struct || (ScriptStruct && ValueProp->Struct->IsChildOf(ScriptStruct));
	if (!bCompatible)
	{
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, FBlueprintExceptionInfo(EBlueprintExceptionType::AccessViolation, INVTEXT("Incompatible Struct")));
		return;
	}

	{
		P_NATIVE_BEGIN;
		const int StructSize = ScriptStruct->GetStructureSize();
		uint8* InstanceStructPtr = SkelotComponent->InstancesData.CustomPerInstanceStruct.GetData() + (StructSize * InstanceIndex);
		ValueProp->Struct->CopyScriptStruct(InstanceStructPtr, ValuePtr);
		P_NATIVE_END;
	}
}

#undef LOCTEXT_NAMESPACE