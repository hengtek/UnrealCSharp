// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectSaveContext.h"
#include "UCSScriptStruct.generated.h"

/**
 * 
 */
UCLASS()
class UNREALCSHARPCORE_API UUCSScriptStruct : public UUserDefinedStruct
{
	GENERATED_BODY()

public:

	void RecreateDefaults()
	{
		DefaultStructInstance.Recreate(this);
	}

	//UScriptStruct interface implementation
	virtual void PrepareCppStructOps() override;
	//End of implementation

#if WITH_EDITOR
	// UObject interface.
	virtual void PostDuplicate(bool bDuplicateForPIE) override { UScriptStruct::PostDuplicate(bDuplicateForPIE); };
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override { UScriptStruct::GetAssetRegistryTags(OutTags); };
	virtual void PostLoad() override { UScriptStruct::PostLoad(); };
	virtual void PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext) override { UScriptStruct::PreSaveRoot(ObjectSaveContext); };
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override { UScriptStruct::PostSaveRoot(ObjectSaveContext); };
	// End of UObject interface.
	
#endif	// WITH_EDITOR

	// UObject interface.
	virtual void SerializeTaggedProperties(FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad = nullptr) const override
	{
		UScriptStruct::SerializeTaggedProperties(Slot, Data, DefaultsStruct, Defaults, BreakRecursionIfFullyLoad);
	};
	
	virtual FString GetAuthoredNameForField(const FField* Field) const override { return UScriptStruct::GetAuthoredNameForField(Field); };
	// End of UObject interface.

	// UScriptStruct interface.
	virtual void InitializeStruct(void* Dest, int32 ArrayDim) const override { return UScriptStruct::InitializeStruct(Dest, ArrayDim); }
	virtual uint32 GetStructTypeHash(const void* Src) const override { return UScriptStruct::GetStructTypeHash(Src); }
	virtual void RecursivelyPreload() override { return UScriptStruct::RecursivelyPreload(); }
	virtual FGuid GetCustomGuid() const override { return UScriptStruct::GetCustomGuid(); }
	virtual FString GetStructCPPName() const override { return UScriptStruct::GetStructCPPName(); }
	virtual FProperty* CustomFindProperty(const FName Name) const override { return UScriptStruct::CustomFindProperty(Name); }
	// End of  UScriptStruct interface.

};

struct FUSCppStructOps : UScriptStruct::ICppStructOps
{
	FUSCppStructOps(int32 InSize, int32 InAlignment, UUCSScriptStruct* InScriptStruct): ICppStructOps(InSize, InAlignment), ScriptStruct(InScriptStruct)
	{
		
	}

	//UScriptStruct::ICppStructOps interface implementation
	// virtual FCapabilities GetCapabilities() const override
	// {
	// 	FCapabilities Capabilities;
	// 	FMemory::Memzero(Capabilities);
	// 	Capabilities.HasDestructor = true;
	// 	Capabilities.HasCopy = true;
	// 	return Capabilities;
	// }

	virtual void Construct(void* Dest) override
	{
		
	}

	virtual void ConstructForTests(void* Dest) override
	{
		Construct(Dest);
	}

	virtual void Destruct(void *Dest) override
	{
		
	}

	virtual bool Copy(void* Dest, void const* Src, int32 ArrayDim) override
	{
		if (!Dest || !Src)
		{
			return false;
		}

		const int32 StructureSize = GetSize();
		for (int32 i = 0; i < ArrayDim; ++i)
		{
			uint8* DestElem = static_cast<uint8*>(Dest) + i * StructureSize;
			const uint8* SrcElem = static_cast<const uint8*>(Src) + i * StructureSize;
			FMemory::Memcpy(DestElem, SrcElem, StructureSize);
		}

		return true;
	}

	virtual bool Identical(const void* A, const void* B, uint32 PortFlags, bool& bOutResult) override
	{
		return true;
	}
	
	virtual bool Serialize(FArchive& Ar, void *Data) override { return false; }
	virtual bool Serialize(FStructuredArchive::FSlot Slot, void *Data) override { return false; }
	virtual void PostSerialize(const FArchive& Ar, void *Data) override {}
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess, void *Data) override { return false; }
	virtual bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms, void *Data) override { return false; }
	virtual bool ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) override { return false; }
	virtual bool ImportTextItem(const TCHAR*& Buffer, void* Data, int32 PortFlags, class UObject* OwnerObject, FOutputDevice* ErrorText) override { return false; }
	virtual bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar, void *Data) override { return false; }
	virtual bool StructuredSerializeFromMismatchedTag(struct FPropertyTag const& Tag, FStructuredArchive::FSlot Slot, void *Data) override { return false; }
	virtual void PostScriptConstruct(void *Data) override {}
	virtual void GetPreloadDependencies(void* Data, TArray<UObject*>& OutDeps) override {}
	virtual uint32 GetStructTypeHash(const void* Src) override { return 0; }
	virtual TPointerToAddStructReferencedObjects AddStructReferencedObjects() override { return nullptr; }
	
	// #if WITH_EDITOR
	// virtual bool CanEditChange(const FEditPropertyChain& PropertyChain, const void* Data) const { return true; }
	// #endif
	//End of implementation

	void GetCppTraits(bool& OutHasConstructor, bool& OutHasDestructor, bool& OutHasAssignmentOperator, bool& OutHasCopyConstructor) const
	{
		OutHasConstructor        = true;
		OutHasDestructor         = true;
		OutHasAssignmentOperator = true;
		OutHasCopyConstructor    = true;
	}

	virtual bool HasNoopConstructor() override {return false;}
	virtual bool HasZeroConstructor() override {return true;}
	virtual bool HasDestructor() override {return true;}
	virtual bool HasSerializer() override {return false;}
	virtual bool HasStructuredSerializer() override {return false;}
	virtual bool HasPostSerialize() override {return false;}
	virtual bool HasNetSerializer() override {return false;}
	virtual bool HasNetSharedSerialization() override {return false;}
	virtual bool HasNetDeltaSerializer() override {return false;}
	virtual bool HasPostScriptConstruct() override {return false;}
	virtual bool IsPlainOldData() override {return false;}
	virtual bool IsUECoreType() override {return false;}
	virtual bool IsUECoreVariant() override {return false;}
	virtual bool HasCopy() override {return true;}
	virtual bool HasIdentical() override {return false;}
	virtual bool HasExportTextItem() override {return false;}
	virtual bool HasImportTextItem() override {return false;}
	virtual bool HasAddStructReferencedObjects() override {return false;}
	virtual bool HasSerializeFromMismatchedTag() override {return false;}
	virtual bool HasStructuredSerializeFromMismatchedTag() override {return false;}
	virtual bool HasGetTypeHash() override {return false;}
	virtual EPropertyFlags GetComputedPropertyFlags() const override{return CPF_None;};
	virtual bool IsAbstract() const override{return false;}

	// virtual bool FindInnerPropertyInstance(FName PropertyName, const void* Data, const FProperty*& OutProp, const void*& OutData) const override { return false; };

	UUCSScriptStruct* ScriptStruct = nullptr;
	
};
