// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Modules */

template<typename T>
constexpr bool TIsModule_V = TPointerIsConvertibleFromTo<T, class ULockOnTargetModuleBase>::Value;

template<typename T>
constexpr bool TIsRotationModule_V = TPointerIsConvertibleFromTo<T, class URotationModule>::Value;

template<typename T>
constexpr bool TIsTargetHandler_V = TPointerIsConvertibleFromTo<T, class UTargetHandlerBase>::Value;

/** Misc */

template<typename T>
constexpr bool TIsComponentPointer_V = TPointerIsConvertibleFromTo<T, class UActorComponent>::Value;

template<typename T>
typename TEnableIf<TIsComponentPointer_V<T>, T>::Type* FindComponentByName(class AActor* Actor, FName Name)
{
	T* ReturnComponent = nullptr;

	if (IsValid(Actor) && Name != NAME_None)
	{
		TInlineComponentArray<T*> Components(Actor);

		T** FoundComponent = Components.FindByPredicate(
			[Name](const auto* const Comp)
			{
				return Comp->GetFName() == Name;
			}
		);

		ReturnComponent = FoundComponent ? *FoundComponent : nullptr;
	}

	return ReturnComponent;
}
