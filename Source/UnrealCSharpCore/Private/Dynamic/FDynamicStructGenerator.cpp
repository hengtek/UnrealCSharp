#include "Dynamic/FDynamicStructGenerator.h"
#include "Bridge/FTypeBridge.h"
#include "Common/FUnrealCSharpFunctionLibrary.h"
#include "CoreMacro/ClassMacro.h"
#include "CoreMacro/FunctionMacro.h"
#include "CoreMacro/MonoMacro.h"
#include "CoreMacro/NamespaceMacro.h"
#include "CoreMacro/PropertyAttributeMacro.h"
#include "Domain/FMonoDomain.h"
#include "Dynamic/FDynamicClassGenerator.h"
#include "Dynamic/FDynamicGeneratorCore.h"
#include "Template/TGetArrayLength.inl"
#if WITH_EDITOR
#include "K2Node_StructOperation.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintActionDatabase.h"
#endif
#include "EdMode.h"
#include "KismetCompilerMisc.h"
#include "UEVersion.h"
#include "Algo/Copy.h"
// #include "Editor/KismetCompiler/Public/KismetCompilerModule.h"
#include "KismetCompilerModule.h"
// #include "AnimGraph/Private/AnimBlueprintCompiler.h"
#include "Domain/UCSScriptStruct.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/ReloadUtilities.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UObject/FieldIterator.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

TMap<FString, UScriptStruct*> FDynamicStructGenerator::DynamicStructMap;

TSet<UScriptStruct*> FDynamicStructGenerator::DynamicStructSet;

void FDynamicStructGenerator::Generator()
{
	const auto AttributeMonoClass = FMonoDomain::Class_From_Name(
		COMBINE_NAMESPACE(NAMESPACE_ROOT, NAMESPACE_DYNAMIC), CLASS_U_STRUCT_ATTRIBUTE);

	const auto AttributeMonoType = FMonoDomain::Class_Get_Type(AttributeMonoClass);

	const auto AttributeMonoReflectionType = FMonoDomain::Type_Get_Object(AttributeMonoType);

	const auto UtilsMonoClass = FMonoDomain::Class_From_Name(
		COMBINE_NAMESPACE(NAMESPACE_ROOT, NAMESPACE_CORE_UOBJECT), CLASS_UTILS);

	void* InParams[2] = {
		AttributeMonoReflectionType,
		FMonoDomain::GCHandle_Get_Target_V2(FMonoDomain::AssemblyGCHandles[1])
	};

	const auto GetTypesWithAttributeMethod = FMonoDomain::Class_Get_Method_From_Name(
		UtilsMonoClass, FUNCTION_UTILS_GET_TYPES_WITH_ATTRIBUTE, TGetArrayLength(InParams));

	const auto Types = reinterpret_cast<MonoArray*>(FMonoDomain::Runtime_Invoke(
		GetTypesWithAttributeMethod, nullptr, InParams));

	const auto Length = FMonoDomain::Array_Length(Types);

	for (auto Index = 0; Index < Length; ++Index)
	{
		const auto ReflectionType = ARRAY_GET(Types, MonoReflectionType*, Index);

		const auto Type = FMonoDomain::Reflection_Type_Get_Type(ReflectionType);

		const auto Class = FMonoDomain::Type_Get_Class(Type);

		Generator(Class);
	}
}

#if WITH_EDITOR
void FDynamicStructGenerator::CodeAnalysisGenerator()
{
	static FString CSharpScriptStruct = TEXT("CSharpScriptStruct");

	auto StructNames = FDynamicGeneratorCore::GetDynamic(
		FString::Printf(TEXT(
			"%s/%s.json"),
		                *FUnrealCSharpFunctionLibrary::GetCodeAnalysisPath(),
		                *CSharpScriptStruct),
		CSharpScriptStruct
	);

	for (const auto& StructName : StructNames)
	{
		if (!DynamicStructMap.Contains(StructName))
		{
			GeneratorCSharpScriptStruct(FDynamicGeneratorCore::GetOuter(), StructName, nullptr);
		}
	}
}
#endif

void FDynamicStructGenerator::Generator(MonoClass* InMonoClass)
{
	if (InMonoClass == nullptr)
	{
		return;
	}

	const auto ClassName = FString(FMonoDomain::Class_Get_Name(InMonoClass));

	const auto Outer = FDynamicGeneratorCore::GetOuter();

	UScriptStruct* ParentClass{};

	if (const auto ParentMonoClass = FMonoDomain::Class_Get_Parent(InMonoClass))
	{
		if (ParentMonoClass != FMonoDomain::Get_Object_Class())
		{
			if (const auto ParentMonoType = FMonoDomain::Class_Get_Type(ParentMonoClass))
			{
				if (const auto ParentMonoReflectionType = FMonoDomain::Type_Get_Object(ParentMonoType))
				{
					const auto ParentPathName = FTypeBridge::GetPathName(ParentMonoReflectionType);

					ParentClass = LoadObject<UScriptStruct>(nullptr, *ParentPathName);
				}
			}
		}
	}

#if WITH_EDITOR
	UScriptStruct* OldScriptStruct{};
	
	UScriptStruct* ScriptStruct{};
	
	if (DynamicStructMap.Contains(ClassName))
	{
		OldScriptStruct = DynamicStructMap[ClassName];

		ScriptStruct = OldScriptStruct;
		//
		// DynamicStructSet.Remove(OldScriptStruct);
		//
		// OldScriptStruct->Rename(
		// 	*MakeUniqueObjectName(
		// 		OldScriptStruct->GetOuter(),
		// 		OldScriptStruct->GetClass())
		// 	.ToString(),
		// 	nullptr,
		// 	REN_DontCreateRedirectors);
		ScriptStruct->DestroyChildPropertiesAndResetPropertyLinks();
		
		GeneratorScriptStruct(ClassName, ScriptStruct, ParentClass, [InMonoClass](UScriptStruct* InScriptStruct)
														  {
															  ProcessGenerator(InMonoClass, InScriptStruct);
														  });

#if WITH_EDITOR
		ScriptStruct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		ScriptStruct->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
#endif
	}
	else
	{
#endif

		ScriptStruct = GeneratorCSharpScriptStruct(Outer, ClassName, ParentClass,
															  [InMonoClass](UScriptStruct* InScriptStruct)
															  {
																  ProcessGenerator(InMonoClass, InScriptStruct);
															  });
	}

#if WITH_EDITOR
	if (OldScriptStruct != nullptr)
	{
		ReInstance(OldScriptStruct, OldScriptStruct, InMonoClass);
	}
#endif
}

bool FDynamicStructGenerator::IsDynamicStruct(MonoClass* InMonoClass)
{
	const auto AttributeMonoClass = FMonoDomain::Class_From_Name(
		COMBINE_NAMESPACE(NAMESPACE_ROOT, NAMESPACE_DYNAMIC), CLASS_U_STRUCT_ATTRIBUTE);

	const auto Attrs = FMonoDomain::Custom_Attrs_From_Class(InMonoClass);

	return !!FMonoDomain::Custom_Attrs_Has_Attr(Attrs, AttributeMonoClass);
}

bool FDynamicStructGenerator::IsDynamicStruct(const UScriptStruct* InScriptStruct)
{
	return DynamicStructSet.Contains(InScriptStruct);
}

void FDynamicStructGenerator::BeginGenerator(UScriptStruct* InScriptStruct, UScriptStruct* InParentScriptStruct)
{
	if (InParentScriptStruct != nullptr)
	{
		InScriptStruct->SetSuperStruct(InParentScriptStruct);
	}
}

void FDynamicStructGenerator::ProcessGenerator(MonoClass* InMonoClass, UScriptStruct* InScriptStruct)
{
#if WITH_EDITOR
	GeneratorMetaData(InMonoClass, InScriptStruct);
#endif

	GeneratorProperty(InMonoClass, InScriptStruct);
}

void FDynamicStructGenerator::EndGenerator(UScriptStruct* InScriptStruct)
{
	InScriptStruct->Bind();

	InScriptStruct->StaticLink(true);

	// if (InScriptStruct->GetPropertiesSize() == 0)
	// {
	// 	InScriptStruct->SetPropertiesSize(1);
	// }

	// InScriptStruct->SetInternalFlags(EInternalObjectFlags::Native);
	//
	// InScriptStruct->StructFlags = STRUCT_Native;

#if WITH_EDITOR
	if (GEditor)
	{
		FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
	
		ActionDatabase.ClearAssetActions(InScriptStruct);
	
		ActionDatabase.RefreshAssetActions(InScriptStruct);
	}
#endif

	if(auto x1 = Cast<UUCSScriptStruct>(InScriptStruct))
	{
		x1->RecreateDefaults();

		// x1->InitializeDefaultValue(const_cast<uint8*>(x1->GetDefaultInstance()));
	}

#if UE_NOTIFY_REGISTRATION_EVENT
#if !WITH_EDITOR
	NotifyRegistrationEvent(*InScriptStruct->GetPackage()->GetName(),
	                        *InScriptStruct->GetName(),
	                        ENotifyRegistrationType::NRT_Class,
	                        ENotifyRegistrationPhase::NRP_Finished,
	                        nullptr,
	                        false,
	                        InScriptStruct);
#endif
#endif
}

void FDynamicStructGenerator::GeneratorScriptStruct(const FString& InName, UScriptStruct* InScriptStruct,
                                                    UScriptStruct* InParentScriptStruct,
                                                    const TFunction<void(UScriptStruct*)>& InProcessGenerator)
{
	DynamicStructMap.Add(InName, InScriptStruct);

	DynamicStructSet.Add(InScriptStruct);

	BeginGenerator(InScriptStruct, InParentScriptStruct);

	InProcessGenerator(InScriptStruct);

	EndGenerator(InScriptStruct);
}

UScriptStruct* FDynamicStructGenerator::GeneratorCSharpScriptStruct(UPackage* InOuter, const FString& InName,
                                                                    UScriptStruct* InParentScriptStruct)
{
	return GeneratorCSharpScriptStruct(InOuter, InName, InParentScriptStruct,
	                                   [](UScriptStruct*)
	                                   {
	                                   });
}

UScriptStruct* FDynamicStructGenerator::GeneratorCSharpScriptStruct(UPackage* InOuter, const FString& InName,
                                                                    UScriptStruct* InParentScriptStruct,
                                                                    const TFunction<void(UScriptStruct*)>&
                                                                    InProcessGenerator)
{
	// const auto ScriptStruct = NewObject<UScriptStruct>(InOuter, *InName.RightChop(1), RF_Public | RF_Standalone);

	const auto ScriptStruct = NewObject<UUCSScriptStruct>(InOuter, *InName, RF_Public | RF_Standalone);
	
	ScriptStruct->AddToRoot();

	ScriptStruct->EditorData = NewObject<UUserDefinedStructEditorData>(ScriptStruct, NAME_None, RF_Transactional);

// #if WITH_EDITOR
// 	ScriptStruct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
// 	ScriptStruct->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
// #endif

	GeneratorScriptStruct(InName, ScriptStruct, InParentScriptStruct, InProcessGenerator);

#if WITH_EDITOR
	ScriptStruct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
	ScriptStruct->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
#endif

	return ScriptStruct;
}

struct FUserDefinedStructureCompilerInner
{
	struct FBlueprintUserStructData
	{
		TArray<uint8> SkeletonCDOData;
		TArray<uint8> GeneratedCDOData;
	};

	static void ClearStructReferencesInBP(UBlueprint* FoundBlueprint, TMap<UBlueprint*, FBlueprintUserStructData>& BlueprintsToRecompile)
	{
		if (!BlueprintsToRecompile.Contains(FoundBlueprint))
		{
			FBlueprintUserStructData& BlueprintData = BlueprintsToRecompile.Add(FoundBlueprint);

			// Write CDO data to temp archive
			//FObjectWriter SkeletonMemoryWriter(FoundBlueprint->SkeletonGeneratedClass->GetDefaultObject(), BlueprintData.SkeletonCDOData);
			//FObjectWriter MemoryWriter(FoundBlueprint->GeneratedClass->GetDefaultObject(), BlueprintData.GeneratedCDOData);

			for (UFunction* Function : TFieldRange<UFunction>(FoundBlueprint->GeneratedClass, EFieldIteratorFlags::ExcludeSuper))
			{
				Function->Script.Empty();
			}
			FoundBlueprint->Status = BS_Dirty;
		}
	}

	static void ReplaceStructWithTempDuplicate(
		UUserDefinedStruct* StructureToReinstance, 
		TMap<UBlueprint*, FBlueprintUserStructData>& BlueprintsToRecompile,
		TArray<UUserDefinedStruct*>& ChangedStructs, MonoClass* InMonoClass)
	{
		if (StructureToReinstance)
		{
			UUserDefinedStruct* DuplicatedStruct = NULL;
			{
				const FString ReinstancedName = FString::Printf(TEXT("STRUCT_REINST_%s"), *StructureToReinstance->GetName());
				const FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), UUserDefinedStruct::StaticClass(), FName(*ReinstancedName));

				TGuardValue<bool> IsDuplicatingClassForReinstancing(GIsDuplicatingClassForReinstancing, true);
				DuplicatedStruct = (UUserDefinedStruct*)StaticDuplicateObject(StructureToReinstance, GetTransientPackage(), UniqueName, ~RF_Transactional); 
			}

			DuplicatedStruct->Guid = StructureToReinstance->Guid;
			FDynamicStructGenerator::ProcessGenerator(InMonoClass, DuplicatedStruct);
			DuplicatedStruct->Bind();
			DuplicatedStruct->StaticLink(true);
			DuplicatedStruct->PrimaryStruct = StructureToReinstance;
			DuplicatedStruct->Status = EUserDefinedStructureStatus::UDSS_Duplicate;
			DuplicatedStruct->SetFlags(RF_Transient);
			DuplicatedStruct->AddToRoot();

			CastChecked<UUserDefinedStructEditorData>(DuplicatedStruct->EditorData)->RecreateDefaultInstance();

			// List of unique classes and structs to regenerate bytecode and property referenced objects list
			TSet<UStruct*> StructsToRegenerateReferencesFor;

			for (TAllFieldsIterator<FStructProperty> FieldIt(RF_NoFlags, EInternalObjectFlags::Garbage); FieldIt; ++FieldIt)
			{
				FStructProperty* StructProperty = *FieldIt;
				if (StructProperty && (StructureToReinstance == StructProperty->Struct))
				{
					if (UBlueprintGeneratedClass* OwnerClass = Cast<UBlueprintGeneratedClass>(StructProperty->GetOwnerClass()))
					{
						if (UBlueprint* FoundBlueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy))
						{
							ClearStructReferencesInBP(FoundBlueprint, BlueprintsToRecompile);
							StructProperty->Struct = DuplicatedStruct;
							StructsToRegenerateReferencesFor.Add(OwnerClass);
						}
					}
					else if (UUserDefinedStruct* OwnerStruct = Cast<UUserDefinedStruct>(StructProperty->GetOwnerStruct()))
					{
						check(OwnerStruct != DuplicatedStruct);
						const bool bValidStruct = (OwnerStruct->GetOutermost() != GetTransientPackage())
							&& IsValid(OwnerStruct)
							&& (EUserDefinedStructureStatus::UDSS_Duplicate != OwnerStruct->Status.GetValue());

						if (bValidStruct)
						{
							ChangedStructs.AddUnique(OwnerStruct);

							if (FStructureEditorUtils::FStructEditorManager::ActiveChange != FStructureEditorUtils::EStructureEditorChangeInfo::DefaultValueChanged)
							{
								// Don't change this for a default value only change, it won't get correctly replaced later
								StructProperty->Struct = DuplicatedStruct;
								StructsToRegenerateReferencesFor.Add(OwnerStruct);
							}
						}
					}
					else
					{
						// UE_LOG(LogK2Compiler, Error, TEXT("ReplaceStructWithTempDuplicate unknown owner"));
					}
				}
			}

			// Make sure we update the list of objects referenced by structs after we replaced the struct in FStructProperties
			for (UStruct* Struct : StructsToRegenerateReferencesFor)
			{
				Struct->CollectBytecodeAndPropertyReferencedObjects();
			}

			DuplicatedStruct->RemoveFromRoot();

			for (UBlueprint* Blueprint : TObjectRange<UBlueprint>(RF_ClassDefaultObject, /** bIncludeDerivedClasses */ true, /** InternalExcludeFlags */ EInternalObjectFlags::Garbage))
			{
				if (Blueprint && !BlueprintsToRecompile.Contains(Blueprint))
				{
					FBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(Blueprint);
					if (Blueprint->CachedUDSDependencies.Contains(StructureToReinstance))
					{
						ClearStructReferencesInBP(Blueprint, BlueprintsToRecompile);
					}
				}
			}
		}
	}

	static void CleanAndSanitizeStruct(UUserDefinedStruct* StructToClean)
	{
		check(StructToClean);

		if (UUserDefinedStructEditorData* EditorData = Cast<UUserDefinedStructEditorData>(StructToClean->EditorData))
		{
			// Ensure that editor data is in sync w/ the current default instance (if valid) so that it can be reinitialized later.
			EditorData->RefreshValuesFromDefaultInstance();

			EditorData->CleanDefaultInstance();
		}

		if (FStructureEditorUtils::FStructEditorManager::ActiveChange != FStructureEditorUtils::EStructureEditorChangeInfo::DefaultValueChanged)
		{
			StructToClean->SetSuperStruct(nullptr);
			StructToClean->Children = nullptr;
			StructToClean->DestroyChildPropertiesAndResetPropertyLinks();
			StructToClean->Script.Empty();
			StructToClean->MinAlignment = 0;
			StructToClean->ScriptAndPropertyObjectReferences.Empty();
			StructToClean->ErrorMessage.Empty();
			StructToClean->SetStructTrashed(true);
		}
	}

	static void LogError(UUserDefinedStruct* Struct, FCompilerResultsLog& MessageLog, const FString& ErrorMsg)
	{
		MessageLog.Error(*ErrorMsg);
		if (Struct && Struct->ErrorMessage.IsEmpty())
		{
			Struct->ErrorMessage = ErrorMsg;
		}
	}

	static void CreateVariables(UUserDefinedStruct* Struct, const class UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog)
	{
		check(Struct && Schema);

		//FKismetCompilerUtilities::LinkAddedProperty push property to begin, so we revert the order
		for (int32 VarDescIdx = FStructureEditorUtils::GetVarDesc(Struct).Num() - 1; VarDescIdx >= 0; --VarDescIdx)
		{
			FStructVariableDescription& VarDesc = FStructureEditorUtils::GetVarDesc(Struct)[VarDescIdx];
			VarDesc.bInvalidMember = true;

			FEdGraphPinType VarType = VarDesc.ToPinType();

			FString ErrorMsg;
			if(!FStructureEditorUtils::CanHaveAMemberVariableOfType(Struct, VarType, &ErrorMsg))
			{
				// LogError(
				// 	Struct,
				// 	MessageLog,
				// 	FText::Format(
				// 		LOCTEXT("StructureGeneric_ErrorFmt", "Structure: {0} Error: {1}"),
				// 		FText::FromString(Struct->GetFullName()),
				// 		FText::FromString(ErrorMsg)
				// 	).ToString()
				// );
				continue;
			}

			FProperty* VarProperty = nullptr;

			bool bIsNewVariable = false;
			if (FStructureEditorUtils::FStructEditorManager::ActiveChange == FStructureEditorUtils::EStructureEditorChangeInfo::DefaultValueChanged)
			{
				VarProperty = FindFProperty<FProperty>(Struct, VarDesc.VarName);
				if (!ensureMsgf(VarProperty, TEXT("Could not find the expected property (%s); was the struct (%s) unexpectedly sanitized?"), *VarDesc.VarName.ToString(), *Struct->GetName()))
				{
					VarProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Struct, VarDesc.VarName, VarType, NULL, CPF_None, Schema, MessageLog);
					bIsNewVariable = true;
				}
			}
			else
			{
				VarProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Struct, VarDesc.VarName, VarType, NULL, CPF_None, Schema, MessageLog);
				bIsNewVariable = true;
			}

			if (VarProperty == nullptr)
			{
				// LogError(
				// 	Struct,
				// 	MessageLog,
				// 	FText::Format(
				// 		LOCTEXT("VariableInvalidType_ErrorFmt", "The variable {0} declared in {1} has an invalid type {2}"),
				// 		FText::FromName(VarDesc.VarName),
				// 		FText::FromString(Struct->GetName()),
				// 		UEdGraphSchema_K2::TypeToText(VarType)
				// 	).ToString()
				// );
				continue;
			}
			else if (bIsNewVariable)
			{
				VarProperty->SetFlags(RF_LoadCompleted);
				FKismetCompilerUtilities::LinkAddedProperty(Struct, VarProperty);
			}
			
			VarProperty->SetPropertyFlags(CPF_Edit | CPF_BlueprintVisible);
			if (VarDesc.bDontEditOnInstance)
			{
				VarProperty->SetPropertyFlags(CPF_DisableEditOnInstance);
			}
			if (VarDesc.bEnableSaveGame)
			{
				VarProperty->SetPropertyFlags(CPF_SaveGame);
			}
			if (VarDesc.bEnableMultiLineText)
			{
				VarProperty->SetMetaData("MultiLine", TEXT("true"));
			}
			if (VarDesc.bEnable3dWidget)
			{
				VarProperty->SetMetaData(FEdMode::MD_MakeEditWidget, TEXT("true"));
			}
			VarProperty->SetMetaData(TEXT("DisplayName"), *VarDesc.FriendlyName);
			VarProperty->SetMetaData(FBlueprintMetadata::MD_Tooltip, *VarDesc.ToolTip);
			VarProperty->RepNotifyFunc = NAME_None;

			if (!VarDesc.DefaultValue.IsEmpty())
			{
				VarProperty->SetMetaData(TEXT("MakeStructureDefaultValue"), *VarDesc.DefaultValue);
			}
			VarDesc.CurrentDefaultValue = VarDesc.DefaultValue;

			VarDesc.bInvalidMember = false;

			if (VarProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference))
			{
				Struct->StructFlags = EStructFlags(Struct->StructFlags | STRUCT_HasInstancedReference);
			}

			if (VarType.PinSubCategoryObject.IsValid())
			{
				const UClass* ClassObject = Cast<UClass>(VarType.PinSubCategoryObject.Get());

				if (ClassObject && ClassObject->IsChildOf(AActor::StaticClass()) && (VarType.PinCategory == UEdGraphSchema_K2::PC_Object || VarType.PinCategory == UEdGraphSchema_K2::PC_Interface))
				{
					// NOTE: Right now the code that stops hard AActor references from being set in unsafe places is tied to this flag,
					// which is not generally respected in other places for struct properties
					VarProperty->PropertyFlags |= CPF_DisableEditOnTemplate;
				}
				else
				{
					// clear the disable-default-value flag that might have been present (if this was an AActor variable before)
					VarProperty->PropertyFlags &= ~(CPF_DisableEditOnTemplate);
				}
			}
		}
	}

	static void InnerCompileStruct(UUserDefinedStruct* Struct, const class UEdGraphSchema_K2* K2Schema, class FCompilerResultsLog& MessageLog)
	{
		check(Struct);
		const int32 ErrorNum = MessageLog.NumErrors;

		Struct->SetMetaData(FBlueprintMetadata::MD_Tooltip, *FStructureEditorUtils::GetTooltip(Struct));

		UUserDefinedStructEditorData* EditorData = CastChecked<UUserDefinedStructEditorData>(Struct->EditorData);

		CreateVariables(Struct, K2Schema, MessageLog);

		Struct->Bind();
		Struct->StaticLink(true);

		if (Struct->GetStructureSize() <= 0)
		{
			// LogError(
			// 	Struct,
			// 	MessageLog,
			// 	FText::Format(
			// 		LOCTEXT("StructurEmpty_ErrorFmt", "Structure '{0}' is empty "),
			// 		FText::FromString(Struct->GetFullName())
			// 	).ToString()
			// );
		}

		FString DefaultInstanceError;
		EditorData->RecreateDefaultInstance(&DefaultInstanceError);
		if (!DefaultInstanceError.IsEmpty())
		{
			LogError(Struct, MessageLog, DefaultInstanceError);
		}

		const bool bNoErrorsDuringCompilation = (ErrorNum == MessageLog.NumErrors);
		Struct->Status = bNoErrorsDuringCompilation ? EUserDefinedStructureStatus::UDSS_UpToDate : EUserDefinedStructureStatus::UDSS_Error;
	}

	static bool ShouldBeCompiled(const UUserDefinedStruct* Struct)
	{
		if (Struct && (EUserDefinedStructureStatus::UDSS_UpToDate == Struct->Status))
		{
			return false;
		}
		return true;
	}

	static void BuildDependencyMapAndCompile(const TArray<UUserDefinedStruct*>& ChangedStructs, FCompilerResultsLog& MessageLog)
	{
		struct FDependencyMapEntry
		{
			UUserDefinedStruct* Struct;
			TSet<UUserDefinedStruct*> StructuresToWaitFor;

			FDependencyMapEntry() : Struct(NULL) {}

			void Initialize(UUserDefinedStruct* ChangedStruct, const TArray<UUserDefinedStruct*>& AllChangedStructs) 
			{ 
				Struct = ChangedStruct;
				check(Struct);

				for (FStructVariableDescription& VarDesc : FStructureEditorUtils::GetVarDesc(Struct))
				{
					UUserDefinedStruct* StructType = Cast<UUserDefinedStruct>(VarDesc.SubCategoryObject.Get());
					if (StructType && (VarDesc.Category == UEdGraphSchema_K2::PC_Struct) && AllChangedStructs.Contains(StructType))
					{
						StructuresToWaitFor.Add(StructType);
					}
				}
			}
		};

		TArray<FDependencyMapEntry> DependencyMap;
		for (UUserDefinedStruct* ChangedStruct : ChangedStructs)
		{
			DependencyMap.Add(FDependencyMapEntry());
			DependencyMap.Last().Initialize(ChangedStruct, ChangedStructs);
		}

		while (DependencyMap.Num())
		{
			int32 StructureToCompileIndex = INDEX_NONE;
			for (int32 EntryIndex = 0; EntryIndex < DependencyMap.Num(); ++EntryIndex)
			{
				if(0 == DependencyMap[EntryIndex].StructuresToWaitFor.Num())
				{
					StructureToCompileIndex = EntryIndex;
					break;
				}
			}
			check(INDEX_NONE != StructureToCompileIndex);
			UUserDefinedStruct* Struct = DependencyMap[StructureToCompileIndex].Struct;
			check(Struct);

			// FUserDefinedStructureCompilerInner::CleanAndSanitizeStruct(Struct);
			// FUserDefinedStructureCompilerInner::InnerCompileStruct(Struct, GetDefault<UEdGraphSchema_K2>(), MessageLog);

			DependencyMap.RemoveAtSwap(StructureToCompileIndex);

			for (FDependencyMapEntry& MapEntry : DependencyMap)
			{
				MapEntry.StructuresToWaitFor.Remove(Struct);
			}
		}
	}
};

#if WITH_EDITOR
void FDynamicStructGenerator::ReInstance(UScriptStruct* InOldScriptStruct, UScriptStruct* InNewScriptStruct,MonoClass* InMonoClass)
{
	
	auto x  = Cast<UUserDefinedStruct>(InOldScriptStruct);
	
	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	
	FCompilerResultsLog Results;

	Results.SetSourcePath(x->GetPathName());

	// FStructureEditorUtils::CompileStructure(x);


	// if (FStructureEditorUtils::UserDefinedStructEnabled() && x)
	// {
		TArray<UUserDefinedStruct*> ChangedStructs; 
		{
			ChangedStructs.Add(x);
		}

		TMap<UBlueprint*, FUserDefinedStructureCompilerInner::FBlueprintUserStructData> BlueprintsToRecompile;
		for (int32 StructIdx = 0; StructIdx < ChangedStructs.Num(); ++StructIdx)
		{
			UUserDefinedStruct* ChangedStruct = ChangedStructs[StructIdx];
			if (ChangedStruct)
			{
				// FStructureEditorUtils::BroadcastPreChange(ChangedStruct);
				FUserDefinedStructureCompilerInner::ReplaceStructWithTempDuplicate(ChangedStruct, BlueprintsToRecompile, ChangedStructs, InMonoClass);
				ChangedStruct->Status = EUserDefinedStructureStatus::UDSS_Dirty;
			}
		}

		// COMPILE IN PROPER ORDER
		
		FUserDefinedStructureCompilerInner::BuildDependencyMapAndCompile(ChangedStructs, Results);
		
		// UPDATE ALL THINGS DEPENDENT ON COMPILED STRUCTURES
		TSet<UScriptStruct*> ChangedStructsSet;
		ChangedStructsSet.Reserve(ChangedStructs.Num());
		Algo::Copy(ChangedStructs, ChangedStructsSet);
		TSet<UBlueprint*> BlueprintsThatHaveBeenRecompiled;
		FBlueprintEditorUtils::FindScriptStructsInNodes(ChangedStructsSet, [&BlueprintsThatHaveBeenRecompiled, &BlueprintsToRecompile](UBlueprint* Blueprint, UK2Node* Node)
			{
				// We need to recombine any nested subpins on this node, otherwise there will be an
				// unexpected amount of pins during reconstruction. 
				FBlueprintEditorUtils::RecombineNestedSubPins(Node);
		
				if (Blueprint)
				{
					// The blueprint skeleton needs to be updated before we reconstruct the node
					// or else we may have member references that point to the old skeleton
					if (!BlueprintsThatHaveBeenRecompiled.Contains(Blueprint))
					{
						BlueprintsThatHaveBeenRecompiled.Add(Blueprint);
						BlueprintsToRecompile.Remove(Blueprint);
		
						// Reapply CDO data
		
						FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
					}
					Node->ReconstructNode();
				}
			}
		);

		for (TPair<UBlueprint*, FUserDefinedStructureCompilerInner::FBlueprintUserStructData>& Pair : BlueprintsToRecompile)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Pair.Key);
		}

		for (UUserDefinedStruct* ChangedStruct : ChangedStructs)
		{
			if (ChangedStruct)
			{
				FStructureEditorUtils::BroadcastPostChange(ChangedStruct);
				ChangedStruct->MarkPackageDirty();
			}
		}

	x->Status = EUserDefinedStructureStatus::UDSS_UpToDate;
	// }
	
	return;
	

	// x->Status = UDSS_Dirty;
	
	for (TObjectIterator<UBlueprintGeneratedClass> ObjectIterator; ObjectIterator; ++ObjectIterator)
	{
		if(const auto Blueprint = Cast<UBlueprint>(ObjectIterator->ClassGeneratedBy))
		{
			if(Blueprint->GetName().Contains("NewBlueprint"))
			{
				auto x1111 = 10;
			}
					
			TArray<UK2Node*> AllNodes;
	                
			FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);

			auto x231312 = 10;
		}
	}
	TArray<UClass*> DynamicClasses;
	
	TArray<UBlueprintGeneratedClass*> BlueprintGeneratedClasses;
	
	FDynamicGeneratorCore::IteratorObject<UClass>(
		[InOldScriptStruct](const TObjectIterator<UClass>& InClass)
		{
			for (TFieldIterator<FProperty> It(*InClass, EFieldIteratorFlags::ExcludeSuper,
											  EFieldIteratorFlags::ExcludeDeprecated); It; ++It)
			{
				if (const auto StructProperty = CastField<FStructProperty>(*It))
				{
					if (StructProperty->Struct == InOldScriptStruct)
					{
						return true;
					}
				}
			}
	
			return false;
		},
		[&DynamicClasses, &BlueprintGeneratedClasses](const TObjectIterator<UClass>& InClass)
		{
			if (FDynamicClassGenerator::IsDynamicClass(*InClass))
			{
				DynamicClasses.AddUnique(*InClass);
			}
			else if (const auto BlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(*InClass))
			{
				if (!FUnrealCSharpFunctionLibrary::IsSpecialClass(*InClass))
				{
					BlueprintGeneratedClasses.AddUnique(BlueprintGeneratedClass);
				}
			}
		});

	for (const auto BlueprintGeneratedClass : BlueprintGeneratedClasses)
	{
		const auto Blueprint = Cast<UBlueprint>(BlueprintGeneratedClass->ClassGeneratedBy);

		for (const auto& Variable : Blueprint->NewVariables)
		{
			if (Variable.VarType.PinSubCategoryObject == InOldScriptStruct)
			{
				// auto NewVarType = Variable.VarType;
				//
				// NewVarType.PinSubCategoryObject = InNewScriptStruct;
				//
				// FBlueprintEditorUtils::ChangeMemberVariableType(Blueprint, Variable.VarName, NewVarType);
				
				// Blueprint->Status = BS_Dirty;

				FBlueprintEditorUtils::RefreshVariables(Blueprint);

				FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
				
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				
				constexpr auto BlueprintCompileOptions = EBlueprintCompileOptions::SkipGarbageCollection |
					EBlueprintCompileOptions::SkipSave;
				
				FKismetEditorUtilities::CompileBlueprint(Blueprint, BlueprintCompileOptions);
		
				// bIsRefresh = true;
			}
		}

		x->Status = UDSS_UpToDate;

		// FBlueprintEditorUtils::RefreshVariables(Blueprint);

		//
		// FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		//
		// FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		//
		// constexpr auto BlueprintCompileOptions = EBlueprintCompileOptions::SkipGarbageCollection |
		// 	EBlueprintCompileOptions::SkipSave;
		//
		// FKismetEditorUtilities::CompileBlueprint(Blueprint, BlueprintCompileOptions);
	}

	// FStructureEditorUtils::CompileStructure(Cast<UUserDefinedStruct>(InNewScriptStruct));
	// return;
	// TUniquePtr<FReload> Reload = MakeUnique<FReload>(EActiveReloadType::Reinstancing, TEXT(""), *GLog);
	//
	// Reload->NotifyChange(InNewScriptStruct, InOldScriptStruct);
	//
	// Reload->Reinstance();
	//
	// Reload->Finalize();
	//
	// CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	
	// TArray<UClass*> DynamicClasses;
	//
	// TArray<UBlueprintGeneratedClass*> BlueprintGeneratedClasses;
	//
	// FDynamicGeneratorCore::IteratorObject<UClass>(
	// 	[InOldScriptStruct](const TObjectIterator<UClass>& InClass)
	// 	{
	// 		for (TFieldIterator<FProperty> It(*InClass, EFieldIteratorFlags::ExcludeSuper,
	// 		                                  EFieldIteratorFlags::ExcludeDeprecated); It; ++It)
	// 		{
	// 			if (const auto StructProperty = CastField<FStructProperty>(*It))
	// 			{
	// 				if (StructProperty->Struct == InOldScriptStruct)
	// 				{
	// 					return true;
	// 				}
	// 			}
	// 		}
	//
	// 		return false;
	// 	},
	// 	[&DynamicClasses, &BlueprintGeneratedClasses](const TObjectIterator<UClass>& InClass)
	// 	{
	// 		if (FDynamicClassGenerator::IsDynamicClass(*InClass))
	// 		{
	// 			DynamicClasses.AddUnique(*InClass);
	// 		}
	// 		else if (const auto BlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(*InClass))
	// 		{
	// 			if (!FUnrealCSharpFunctionLibrary::IsSpecialClass(*InClass))
	// 			{
	// 				BlueprintGeneratedClasses.AddUnique(BlueprintGeneratedClass);
	// 			}
	// 		}
	// 	});
	//
	// for (const auto BlueprintGeneratedClass : BlueprintGeneratedClasses)
	// {
	// 	const auto Blueprint = Cast<UBlueprint>(BlueprintGeneratedClass->ClassGeneratedBy);
	//
	// 		for (const auto& Variable : Blueprint->NewVariables)
	// 		{
	// 			if (Variable.VarType.PinSubCategoryObject == InOldScriptStruct)
	// 			{
	// 				auto NewVarType = Variable.VarType;
	// 	
	// 				NewVarType.PinSubCategoryObject = InNewScriptStruct;
	// 	
	// 				// FBlueprintEditorUtils::ChangeMemberVariableType(Blueprint, Variable.VarName, NewVarType);
	// 	
	// 				// bIsRefresh = true;
	// 			}
	// 		}
	// 			//
	// 			// FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
	// 			//
	// 			// FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	// 			//
	// 			// constexpr auto BlueprintCompileOptions = EBlueprintCompileOptions::SkipGarbageCollection |
	// 			// 	EBlueprintCompileOptions::SkipSave;
	// 			//
	// 			// FKismetEditorUtilities::CompileBlueprint(Blueprint, BlueprintCompileOptions);
	// }
	//
	// FDynamicGeneratorCore::IteratorObject<UBlueprintGeneratedClass>(
	// 	[](const TObjectIterator<UBlueprintGeneratedClass>& InBlueprintGeneratedClass)
	// 	{
	// 		return FDynamicClassGenerator::IsDynamicBlueprintGeneratedSubClass(*InBlueprintGeneratedClass);
	// 	},
	// 	[&BlueprintGeneratedClasses](const TObjectIterator<UBlueprintGeneratedClass>& InBlueprintGeneratedClass)
	// 	{
	// 		if (!FUnrealCSharpFunctionLibrary::IsSpecialClass(*InBlueprintGeneratedClass))
	// 		{
	// 			BlueprintGeneratedClasses.AddUnique(*InBlueprintGeneratedClass);
	// 		}
	// 	});
	//
	// for (const auto BlueprintGeneratedClass : BlueprintGeneratedClasses)
	// {
	// 	const auto Blueprint = Cast<UBlueprint>(BlueprintGeneratedClass->ClassGeneratedBy);
	//
	// 	auto bIsRefresh = false;
	//
	// 	TArray<UK2Node*> AllNodes;
	//
	// 	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);
	//
	// 	for (const auto Node : AllNodes)
	// 	{
	// 		if (const auto StructOperation = Cast<UK2Node_StructOperation>(Node))
	// 		{
	// 			if (StructOperation->StructType == InOldScriptStruct)
	// 			{
	// 				StructOperation->StructType = InNewScriptStruct;
	//
	// 				bIsRefresh = true;
	// 			}
	// 		}
	// 		else
	// 		{
	// 			for (const auto Pin : Node->Pins)
	// 			{
	// 				if (Pin->PinType.PinSubCategoryObject == InOldScriptStruct)
	// 				{
	// 					Pin->PinType.PinSubCategoryObject = InNewScriptStruct;
	//
	// 					Pin->Modify();
	//
	// 					bIsRefresh = true;
	// 				}
	// 			}
	// 		}
	// 	}
	//
	// 	for (const auto& Variable : Blueprint->NewVariables)
	// 	{
	// 		if (Variable.VarType.PinSubCategoryObject == InOldScriptStruct)
	// 		{
	// 			auto NewVarType = Variable.VarType;
	//
	// 			NewVarType.PinSubCategoryObject = InNewScriptStruct;
	//
	// 			FBlueprintEditorUtils::ChangeMemberVariableType(Blueprint, Variable.VarName, NewVarType);
	//
	// 			bIsRefresh = true;
	// 		}
	// 	}
	//
	// 	if (bIsRefresh)
	// 	{
	// 		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
	//
	// 		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	//
	// 		constexpr auto BlueprintCompileOptions = EBlueprintCompileOptions::SkipGarbageCollection |
	// 			EBlueprintCompileOptions::SkipSave;
	//
	// 		FKismetEditorUtilities::CompileBlueprint(Blueprint, BlueprintCompileOptions);
	// 	}
	// }
	//
	// for (const auto DynamicClass : DynamicClasses)
	// {
	// 	if (const auto FoundMonoClass = FMonoDomain::Class_From_Name(
	// 		FUnrealCSharpFunctionLibrary::GetClassNameSpace(DynamicClass),
	// 		FUnrealCSharpFunctionLibrary::GetFullClass(DynamicClass)))
	// 	{
	// 		FDynamicClassGenerator::Generator(FoundMonoClass);
	// 	}
	// }
	//
	// InOldScriptStruct->RemoveFromRoot();
	//
	// InOldScriptStruct->MarkAsGarbage();
}

void FDynamicStructGenerator::GeneratorMetaData(MonoClass* InMonoClass, UScriptStruct* InScriptStruct)
{
	if (InMonoClass == nullptr || InScriptStruct == nullptr)
	{
		return;
	}

	const auto AttributeMonoClass = FMonoDomain::Class_From_Name(
		COMBINE_NAMESPACE(NAMESPACE_ROOT, NAMESPACE_DYNAMIC), CLASS_U_STRUCT_ATTRIBUTE);

	if (const auto Attrs = FMonoDomain::Custom_Attrs_From_Class(InMonoClass))
	{
		if (!!FMonoDomain::Custom_Attrs_Has_Attr(Attrs, AttributeMonoClass))
		{
			FDynamicGeneratorCore::SetMetaData(InScriptStruct, Attrs);
		}
	}
}
#endif

void FDynamicStructGenerator::GeneratorProperty(MonoClass* InMonoClass, UScriptStruct* InScriptStruct)
{
	if (InMonoClass == nullptr || InScriptStruct == nullptr)
	{
		return;
	}

	const auto AttributeMonoClass = FMonoDomain::Class_From_Name(
		COMBINE_NAMESPACE(NAMESPACE_ROOT, NAMESPACE_DYNAMIC), CLASS_U_PROPERTY_ATTRIBUTE);

	void* Iterator = nullptr;

	while (const auto Property = FMonoDomain::Class_Get_Properties(InMonoClass, &Iterator))
	{
		if (const auto Attrs = FMonoDomain::Custom_Attrs_From_Property(InMonoClass, Property))
		{
			if (!!FMonoDomain::Custom_Attrs_Has_Attr(Attrs, AttributeMonoClass))
			{
				const auto PropertyName = FMonoDomain::Property_Get_Name(Property);

				const auto PropertyType = FMonoDomain::Property_Get_Type(Property);

				const auto ReflectionType = FMonoDomain::Type_Get_Object(PropertyType);

				const auto CppProperty = FTypeBridge::Factory(ReflectionType, InScriptStruct, PropertyName,
				                                              EObjectFlags::RF_Public);

				FDynamicGeneratorCore::SetPropertyFlags(CppProperty, Attrs);

				InScriptStruct->AddCppProperty(CppProperty);
			}
		}
	}
}
