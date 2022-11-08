#pragma once

#include "mono/metadata/appdomain.h"

struct FMonoDomainInitializeParams
{
	FString Domain;

	FString Assembly;

	FMonoDomainInitializeParams(const FString& InDomain, const FString& InAssembly):
		Domain(InDomain),
		Assembly(InAssembly)
	{
	}
};

class FMonoDomain
{
public:
	explicit FMonoDomain(const FMonoDomainInitializeParams& Params);

	~FMonoDomain();

public:
	void Initialize(const FMonoDomainInitializeParams& Params);

	void Deinitialize();

public:
	MonoObject* Object_New(MonoClass* InMonoClass) const;

	MonoObject* Object_New(MonoClass* InMonoClass, const int32 InParamCount, void** InParams) const;

	void Runtime_Object_Init(MonoObject* InMonoObject) const;

	MonoClass* Class_From_Name(const FString& InNameSpace, const FString& InMonoClassName) const;

	MonoMethod* Class_Get_Method_From_Name(MonoClass* InMonoClass, const FString& InFunctionName,
	                                       int32 InParamCount) const;

	mono_bool Class_Is_Subclass_Of(MonoClass* InMonoClass, MonoClass* InSuperMonoClass, mono_bool bCheckInterfaces);

	MonoType* Class_Get_Type(MonoClass* InMonoClass);

	MonoReflectionType* Type_Get_Object(MonoType* InMonoType);

	MonoObject* Runtime_Invoke(MonoMethod* InFunction, void* InMonoObject, void** InParams, MonoObject** InExc) const;

	MonoClass* Object_Get_Class(MonoObject* InMonoObject);

	MonoObject* Value_Box(MonoClass* InMonoClass, void* InValue) const;

	void* Object_Unbox(MonoObject* InMonoObject) const;

	MonoString* String_New(const char* InText) const;

	MonoString* Object_To_String(MonoObject* InMonoObject, MonoObject** InExc) const;

	char* String_To_UTF8(MonoString* InMonoString) const;

	MonoClass* Get_Byte_Class() const;

	MonoClass* Get_UInt16_Class() const;;

	MonoClass* Get_UInt32_Class() const;

	MonoClass* Get_UInt64_Class() const;

	MonoClass* Get_Int16_Class() const;

	MonoClass* Get_Int32_Class() const;

	MonoClass* Get_Int64_Class() const;

	MonoClass* Get_Boolean_Class() const;

	MonoClass* Get_Single_Class() const;

	MonoClass* Get_Enum_Class() const;

	MonoClass* Get_Double_Class() const;

private:
	void RegisterMonoTrace();

	void RegisterReflectionPropertyImplementation();

	void RegisterReflectionFunctionImplementation();

	void RegisterReflectionStructImplementation();

	void RegisterReflectionContainerImplementation();

	void RegisterLog();

private:
	MonoDomain* Domain;

	MonoAssembly* Assembly;

	MonoImage* Image;
};
