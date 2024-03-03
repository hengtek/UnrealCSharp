#include "Dynamic/FDynamicInterfaceGenerator.h"
#include "Bridge/FTypeBridge.h"
#include "Common/FUnrealCSharpFunctionLibrary.h"
#include "CoreMacro/ClassMacro.h"
#include "CoreMacro/NamespaceMacro.h"
#include "CoreMacro/PropertyAttributeMacro.h"
#include "Domain/FMonoDomain.h"
#include "Dynamic/FDynamicGeneratorCore.h"
#if WITH_EDITOR
#include "BlueprintActionDatabase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Dynamic/FDynamicGenerator.h"
#endif
#include "UEVersion.h"

TMap<FString, UClass*> FDynamicInterfaceGenerator::DynamicInterfaceMap;

TSet<UClass*> FDynamicInterfaceGenerator::DynamicInterfaceSet;

void FDynamicInterfaceGenerator::Generator()
{
	FDynamicGeneratorCore::Generator(CLASS_U_INTERFACE_ATTRIBUTE,
		[](MonoClass* InMonoClass)
		{
			Generator(InMonoClass);
		});
}

void FDynamicInterfaceGenerator::CodeAnalysisGenerator()
{
	static FString CSharpInterface = TEXT("CSharpInterface");

	auto InterfaceNames = FDynamicGeneratorCore::GetDynamic(
		FString::Printf(TEXT(
			"%s/%s.json"),
						*FUnrealCSharpFunctionLibrary::GetCodeAnalysisPath(),
						*CSharpInterface),
						CSharpInterface
	);

	for (const auto& InterfaceName : InterfaceNames)
	{
		if (!DynamicInterfaceMap.Contains(InterfaceName))
		{
			GeneratorInterface(FDynamicGeneratorCore::GetOuter(), InterfaceName, UInterface::StaticClass());
		}
	}
}

void FDynamicInterfaceGenerator::Generator(MonoClass* InMonoClass)
{
	if (InMonoClass == nullptr)
	{
		return;
	}

	const auto ClassName = FString(FMonoDomain::Class_Get_Name(InMonoClass));

	const auto Outer = FDynamicGeneratorCore::GetOuter();
	
	const auto ParentMonoClass = FMonoDomain::Class_Get_Parent(InMonoClass);

	const auto ParentMonoType = FMonoDomain::Class_Get_Type(ParentMonoClass);

	const auto ParentMonoReflectionType = FMonoDomain::Type_Get_Object(ParentMonoType);

	const auto ParentPathName = FTypeBridge::GetPathName(ParentMonoReflectionType);

	const auto ParentClass = LoadClass<UObject>(nullptr, *ParentPathName);

#if WITH_EDITOR
	const UClass* OldClass{};
#endif

	UClass* Class{};

	if (DynamicInterfaceMap.Contains(ClassName))
	{
		Class = DynamicInterfaceMap[ClassName];

		GeneratorInterface(ClassName, Class, ParentClass,
			[InMonoClass](UClass* InInterface)
		{
			ProcessGenerator(InMonoClass, InInterface);
		});

#if WITH_EDITOR
		OldClass = Class;
#endif
	}
	else
	{
		Class = GeneratorInterface(Outer, ClassName,ParentClass,
								   [InMonoClass](UClass* InInterface)
								   {
									   ProcessGenerator(InMonoClass, InInterface);
								   });
	}

#if WITH_EDITOR
	if (OldClass != nullptr)
	{
		ReInstance(Class);
	}
#endif
}

bool FDynamicInterfaceGenerator::IsDynamicInterface(MonoClass* InMonoClass)
{
	const auto AttributeMonoClass = FMonoDomain::Class_From_Name(
	COMBINE_NAMESPACE(NAMESPACE_ROOT, NAMESPACE_DYNAMIC), CLASS_U_INTERFACE_ATTRIBUTE);

	const auto Attrs = FMonoDomain::Custom_Attrs_From_Class(InMonoClass);

	return !!FMonoDomain::Custom_Attrs_Has_Attr(Attrs, AttributeMonoClass);
}

bool FDynamicInterfaceGenerator::IsDynamicInterface(const UClass* InClass)
{
	return DynamicInterfaceSet.Contains(InClass);
}

void FDynamicInterfaceGenerator::BeginGenerator(UClass* InClass, UClass* InParentClass)
{
	InClass->PropertyLink = InParentClass->PropertyLink;

	InClass->ClassWithin = InParentClass->ClassWithin;

	InClass->ClassConfigName = InParentClass->ClassConfigName;

	InClass->SetSuperStruct(InParentClass);

#if UE_CLASS_ADD_REFERENCED_OBJECTS
	InClass->ClassAddReferencedObjects = InParentClass->ClassAddReferencedObjects;
#endif

	InClass->ClassCastFlags |= InParentClass->ClassCastFlags;
}

void FDynamicInterfaceGenerator::ProcessGenerator(MonoClass* InMonoClass, UClass* InClass)
{
#if WITH_EDITOR
	GeneratorMetaData(InMonoClass, InClass);
#endif

	GeneratorFunction(InMonoClass, InClass);
}

void FDynamicInterfaceGenerator::EndGenerator(UClass* InClass)
{
	InClass->ClearInternalFlags(EInternalObjectFlags::Native);

	InClass->Bind();

	InClass->StaticLink(true);

	InClass->AssembleReferenceTokenStream();

	InClass->ClassDefaultObject = StaticAllocateObject(InClass, InClass->GetOuter(),
													   *InClass->GetDefaultObjectName().ToString(),
													   RF_Public | RF_ClassDefaultObject | RF_ArchetypeObject,
													   EInternalObjectFlags::None,
													   false);

	(*InClass->ClassConstructor)(FObjectInitializer(InClass->ClassDefaultObject,
													InClass->GetSuperClass()->GetDefaultObject(),
													EObjectInitializerOptions::None));

	InClass->SetInternalFlags(EInternalObjectFlags::Native);

#if WITH_EDITOR
	if (GEditor)
	{
		FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();

		ActionDatabase.ClearAssetActions(InClass);

		ActionDatabase.RefreshClassActions(InClass);
	}
#endif

#if UE_NOTIFY_REGISTRATION_EVENT
#if !WITH_EDITOR
	NotifyRegistrationEvent(*InClass->ClassDefaultObject->GetPackage()->GetName(),
							*InClass->ClassDefaultObject->GetName(),
							ENotifyRegistrationType::NRT_ClassCDO,
							ENotifyRegistrationPhase::NRP_Finished,
							nullptr,
							false,
							InClass->ClassDefaultObject);


	NotifyRegistrationEvent(*InClass->GetPackage()->GetName(),
							*InClass->GetName(),
							ENotifyRegistrationType::NRT_Class,
							ENotifyRegistrationPhase::NRP_Finished,
							nullptr,
							false,
							InClass);
#endif
#endif
}

void FDynamicInterfaceGenerator::GeneratorInterface(const FString& InName, UClass* InClass, UClass* InParentClass,
	const TFunction<void(UClass*)>& InProcessGenerator)
{
	DynamicInterfaceMap.Add(InName, InClass);

	DynamicInterfaceSet.Add(InClass);

	BeginGenerator(InClass, InParentClass);

	InProcessGenerator(InClass);

	EndGenerator(InClass);
}

UClass* FDynamicInterfaceGenerator::GeneratorInterface(UPackage* InOuter, const FString& InName, UClass* InParentClass)
{
	return GeneratorInterface(InOuter, InName, InParentClass,
		[](UClass* InUClass)
		{
		});
}

UClass* FDynamicInterfaceGenerator::GeneratorInterface(UPackage* InOuter, const FString& InName, UClass* InParentClass,
                                                       const TFunction<void(UClass*)>& InProcessGenerator)
{
	const auto Class = NewObject<UClass>(InOuter, *InName.RightChop(1), RF_Public);

	Class->AddToRoot();

	GeneratorInterface(InName, Class, InParentClass, InProcessGenerator);

	return Class;
}

#if WITH_EDITOR
void FDynamicInterfaceGenerator::ReInstance(UClass* InClass)
{
}

void FDynamicInterfaceGenerator::GeneratorMetaData(MonoClass* InMonoClass, UClass* InClass)
{
}

#endif

void FDynamicInterfaceGenerator::GeneratorFunction(MonoClass* InMonoClass, UClass* InClass)
{
}
