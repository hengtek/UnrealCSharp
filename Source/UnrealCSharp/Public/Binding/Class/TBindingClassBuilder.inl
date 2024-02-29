#pragma once

#include "TClassBuilder.inl"
#include "Template/TIsUObject.inl"
#include "Template/TIsUStruct.inl"

template <typename T, typename Enable = void>
class TBindingClassBuilder
{
};

template <typename T>
class TBindingClassBuilder<T, std::enable_if_t<TIsUObject<T>::Value ||
                                               TIsUStruct<T>::Value ||
                                               TIsScriptStruct<T>::Value, T>> final :
	public TClassBuilder<T>
{
public:
	explicit TBindingClassBuilder(const FString& InImplementationNameSpace):
		TClassBuilder<T>(InImplementationNameSpace)
	{
	}

	virtual bool IsReflection() const override
	{
		return true;
	}
};

template <typename T>
class TBindingClassBuilder<T, std::enable_if_t<!TIsUObject<T>::Value &&
                                               !TIsUStruct<T>::Value &&
                                               !TIsScriptStruct<T>::Value, T>> final :
	public TClassBuilder<T>
{
public:
	explicit TBindingClassBuilder(const FString& InImplementationNameSpace):
		TClassBuilder<T>(InImplementationNameSpace)
	{
		if constexpr (std::is_default_constructible_v<T>)
		{
			TClassBuilder<T>::Constructor(BINDING_CONSTRUCTOR(T));
		}

		TClassBuilder<T>::Destructor(BINDING_DESTRUCTOR());
	}

	template <typename Class>
	TBindingClassBuilder& Inheritance(const FString& InImplementationNameSpace)
	{
		TClassBuilder<T>::GetBindingClass()->Inheritance(TClassName<Class>::Get(), InImplementationNameSpace,
														 TTypeInfo<Class>::Get());

		return *this;
	}
};
