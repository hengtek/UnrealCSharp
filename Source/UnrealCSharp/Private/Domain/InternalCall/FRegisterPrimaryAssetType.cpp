﻿#include "Binding/Class/TBindingClassBuilder.inl"
#include "Binding/ScriptStruct/TScriptStruct.inl"
#include "Macro/NamespaceMacro.h"

struct FRegisterPrimaryAssetType
{
	FRegisterPrimaryAssetType()
	{
		TBindingClassBuilder<FPrimaryAssetType, FPrimaryAssetType>(NAMESPACE_BINDING)
			.Constructor(BINDING_CONSTRUCTOR(FPrimaryAssetType, FName),
			             {"InName"})
			.Function("GetName", BINDING_FUNCTION(&FPrimaryAssetType::GetName))
			.Function("IsValid", BINDING_FUNCTION(&FPrimaryAssetType::IsValid))
			.Function("ToString", BINDING_FUNCTION(&FPrimaryAssetType::ToString),
			          {}, EFunctionInteract::New)
			.Register();
	}
};

static FRegisterPrimaryAssetType RegisterPrimaryAssetType;
