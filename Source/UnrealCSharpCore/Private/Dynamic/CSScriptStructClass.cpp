#include "Domain/UCSScriptStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

void UUCSScriptStruct::PrepareCppStructOps()
{
	if (!CppStructOps)
	{
		CppStructOps = new FUSCppStructOps(GetStructureSize(), GetMinAlignment(), this);

		// EditorData = NewObject<UUserDefinedStructEditorData>(this, NAME_None, RF_Transactional);
	}
	
	UScriptStruct::PrepareCppStructOps();
}

