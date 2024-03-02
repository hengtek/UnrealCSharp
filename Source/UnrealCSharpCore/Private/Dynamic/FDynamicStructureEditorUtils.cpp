#include "Dynamic/FDynamicStructureEditorUtils.h"
#if WITH_EDITOR
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#endif

UUserDefinedStruct* FDynamicStructureEditorUtils::CreateUserDefinedStruct(UObject* InOuter, const FString& InName,
                                                                          const EObjectFlags InFlags)
{
	UUserDefinedStruct* Struct = NewObject<UUserDefinedStruct>(InOuter, *InName, InFlags);

#if WITH_EDITOR
		Struct->EditorData = NewObject<UUserDefinedStructEditorData>(Struct, NAME_None, RF_Transactional);
#endif
	
		Struct->Guid = FGuid::NewGuid();
#if WITH_EDITOR
		Struct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
#endif
		Struct->Bind();
		Struct->StaticLink(true);
#if WITH_EDITOR
		Struct->Status = UDSS_Error;
#else
		Struct->Status = UDSS_UpToDate;
#endif

// #if WITH_EDITOR
// 	FStructureEditorUtils::AddVariable(Struct, FEdGraphPinType(UEdGraphSchema_K2::PC_Boolean, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType()));
// #endif
	
	return Struct;
}

#if WITH_EDITOR
bool FDynamicStructureEditorUtils::AddVariable(UUserDefinedStruct* Struct, const FEdGraphPinType& VarType,
	const FString& VarName)
{
	if (Struct)
	{
		FStructureEditorUtils::ModifyStructData(Struct);

		FStructVariableDescription NewVar;
		NewVar.VarName = *VarName;
		NewVar.FriendlyName = VarName;
		NewVar.SetPinType(VarType);
		NewVar.VarGuid = FGuid::NewGuid();
		
		FStructureEditorUtils::GetVarDesc(Struct).Add(NewVar);

		FStructureEditorUtils::OnStructureChanged(Struct, FStructureEditorUtils::EStructureEditorChangeInfo::AddedVariable);
		return true;
	}
	return false;
}
#endif

#if WITH_EDITOR
void FDynamicStructureEditorUtils::RecreateDefaultInstanceInEditorData(UUserDefinedStruct* Struct)
{
	UUserDefinedStructEditorData* StructEditorData = Struct ? CastChecked<UUserDefinedStructEditorData>(Struct->EditorData) : nullptr;
	if (StructEditorData)
	{
		StructEditorData->RecreateDefaultInstance();
	}
}
#endif
