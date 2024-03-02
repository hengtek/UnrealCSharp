#include "Dynamic/FDynamicStructGenerator.h"
#include "Bridge/FTypeBridge.h"
#include "Common/FUnrealCSharpFunctionLibrary.h"
#include "CoreMacro/ClassMacro.h"
#include "CoreMacro/FunctionMacro.h"
#include "CoreMacro/MonoMacro.h"
#include "CoreMacro/NamespaceMacro.h"
#include "CoreMacro/PropertyAttributeMacro.h"
#include "Domain/FMonoDomain.h"
#include "Dynamic/FDynamicGeneratorCore.h"
#include "Template/TGetArrayLength.inl"
#if WITH_EDITOR
#include "BlueprintActionDatabase.h"
#endif
#include "UEVersion.h"
#include "Dynamic/FDynamicStructureEditorUtils.h"

TMap<FString, UUserDefinedStruct*> FDynamicStructGenerator::DynamicStructMap;

TSet<UUserDefinedStruct*> FDynamicStructGenerator::DynamicStructSet;

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

#if WITH_EDITOR
	UUserDefinedStruct* OldScriptStruct{};

	if (DynamicStructMap.Contains(ClassName))
	{
		OldScriptStruct = DynamicStructMap[ClassName];

		// DynamicStructSet.Remove(OldScriptStruct);
		//
		// OldScriptStruct->Rename(
		// 	*MakeUniqueObjectName(
		// 		OldScriptStruct->GetOuter(),
		// 		OldScriptStruct->GetClass())
		// 	.ToString(),
		// 	nullptr,
		// 	REN_DontCreateRedirectors);

		GeneratorScriptStruct(ClassName, OldScriptStruct,
			[InMonoClass](UUserDefinedStruct* InScriptStruct)
													  {
														  ProcessGenerator(InMonoClass, InScriptStruct);
													  });
	}
	else
#endif
	{
		GeneratorCSharpScriptStruct(Outer, ClassName, 
													  [InMonoClass](UUserDefinedStruct* InScriptStruct)
													  {
														  ProcessGenerator(InMonoClass, InScriptStruct);
													  });
	}
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
	return DynamicStructSet.Contains(Cast<UUserDefinedStruct>(InScriptStruct));
}

void FDynamicStructGenerator::BeginGenerator(UUserDefinedStruct* InScriptStruct)
{
}

void FDynamicStructGenerator::ProcessGenerator(MonoClass* InMonoClass, UUserDefinedStruct* InScriptStruct)
{
// #if WITH_EDITOR
// 	GeneratorMetaData(InMonoClass, InScriptStruct);
// #endif

	GeneratorProperty(InMonoClass, InScriptStruct);
}

void FDynamicStructGenerator::EndGenerator(UUserDefinedStruct* InScriptStruct)
{
#if !WITH_EDITOR
	InScriptStruct->Bind();

	InScriptStruct->StaticLink(true);
#endif
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

void FDynamicStructGenerator::GeneratorScriptStruct(const FString& InName, UUserDefinedStruct* InScriptStruct,
                                                    const TFunction<void(UUserDefinedStruct*)>& InProcessGenerator)
{
	DynamicStructMap.Add(InName, InScriptStruct);

	DynamicStructSet.Add(InScriptStruct);

	BeginGenerator(InScriptStruct);

	InProcessGenerator(InScriptStruct);

	EndGenerator(InScriptStruct);
}

UUserDefinedStruct* FDynamicStructGenerator::GeneratorCSharpScriptStruct(UPackage* InOuter, const FString& InName)
{
	return GeneratorCSharpScriptStruct(InOuter, InName, 
	                                   [](UUserDefinedStruct*)
	                                   {
	                                   });
}

UUserDefinedStruct* FDynamicStructGenerator::GeneratorCSharpScriptStruct(UPackage* InOuter, const FString& InName,
                                                                    const TFunction<void(UUserDefinedStruct*)>&
                                                                    InProcessGenerator)
{
	const auto ScriptStruct = FDynamicStructureEditorUtils::CreateUserDefinedStruct(InOuter, *InName, RF_Public);

	ScriptStruct->AddToRoot();

	GeneratorScriptStruct(InName, ScriptStruct, InProcessGenerator);

	return ScriptStruct;
}

#if WITH_EDITOR
void FDynamicStructGenerator::GeneratorMetaData(MonoClass* InMonoClass, UUserDefinedStruct* InScriptStruct)
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

void FDynamicStructGenerator::GeneratorProperty(MonoClass* InMonoClass, UUserDefinedStruct* InScriptStruct)
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

#if WITH_EDITOR
				FDynamicStructureEditorUtils::AddVariable(InScriptStruct, FEdGraphPinType(UEdGraphSchema_K2::PC_Int, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType()), PropertyName);

				if(IsRunningCookCommandlet())
				{
					FDynamicGeneratorCore::SetPropertyFlags(CppProperty, Attrs);

					InScriptStruct->AddCppProperty(CppProperty);
				}
#else
				FDynamicGeneratorCore::SetPropertyFlags(CppProperty, Attrs);

				InScriptStruct->AddCppProperty(CppProperty);
#endif
			}
		}
	}
}
