// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

template<typename T>
constexpr bool TIsComponentPointer_V = TPointerIsConvertibleFromTo<T, UActorComponent>::Value;

template<typename T>
T* FindComponentByName(AActor* Actor, const FName& Name)
{
	static_assert(TIsComponentPointer_V<T>, "'T' template parameter for FindComponentByName must be derived from UActorComponent.");

	T* ReturnComponent = nullptr;

	if (IsValid(Actor) && Name != NAME_None)
	{
		TInlineComponentArray<T*> Components(Actor);

		T** FoundComponent = Components.FindByPredicate(
			[&Name](const auto* const Comp)
			{
				return Comp->GetFName() == Name;
			}
		);

		ReturnComponent = FoundComponent ? *FoundComponent : nullptr;
	}

	return ReturnComponent;
}
