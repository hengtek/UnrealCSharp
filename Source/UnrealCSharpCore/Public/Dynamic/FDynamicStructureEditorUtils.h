#pragma once

#include "CoreMinimal.h"
#include "Engine/UserDefinedStruct.h"

class FDynamicStructureEditorUtils
{
public:
	static UUserDefinedStruct* CreateUserDefinedStruct(UObject* InOuter, const FString& InName, EObjectFlags InFlags);

#if WITH_EDITOR
	static bool AddVariable(UUserDefinedStruct* Struct, const FEdGraphPinType& VarType, const FString& VarName);
#endif
	
#if WITH_EDITOR
	static void RecreateDefaultInstanceInEditorData(UUserDefinedStruct* Struct);
#endif
};
