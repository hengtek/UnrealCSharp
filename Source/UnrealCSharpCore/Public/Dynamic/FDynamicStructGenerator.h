#pragma once

#include "Engine/UserDefinedStruct.h"
#include "mono/metadata/object.h"

class FDynamicStructGenerator
{
public:
	static void Generator();

#if WITH_EDITOR
	static void CodeAnalysisGenerator();
#endif

	static void Generator(MonoClass* InMonoClass);

	static bool IsDynamicStruct(MonoClass* InMonoClass);

	static bool UNREALCSHARPCORE_API IsDynamicStruct(const UScriptStruct* InScriptStruct);

private:
	static void BeginGenerator(UUserDefinedStruct* InScriptStruct);

	static void ProcessGenerator(MonoClass* InMonoClass, UUserDefinedStruct* InScriptStruct);

	static void EndGenerator(UUserDefinedStruct* InScriptStruct);

	static void GeneratorScriptStruct(const FString& InName, UUserDefinedStruct* InScriptStruct,
	                                  const TFunction<void(UUserDefinedStruct*)>& InProcessGenerator);

	static UUserDefinedStruct* GeneratorCSharpScriptStruct(UPackage* InOuter, const FString& InName);

	static UUserDefinedStruct* GeneratorCSharpScriptStruct(UPackage* InOuter, const FString& InName,
	                                                  const TFunction<void(UUserDefinedStruct*)>& InProcessGenerator);

#if WITH_EDITOR
	static void GeneratorMetaData(MonoClass* InMonoClass, UUserDefinedStruct* InScriptStruct);
#endif

	static void GeneratorProperty(MonoClass* InMonoClass, UUserDefinedStruct* InScriptStruct);

	static TMap<FString, UUserDefinedStruct*> DynamicStructMap;

	static TSet<UUserDefinedStruct*> DynamicStructSet;
};
