/// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <type_traits>

namespace LOT_Math
{
	template<typename T> //@TODO: Maybe add sfinae or concepts.
	float GetAngle(const T& lhs, const T& rhs)
	{
		return FMath::RadiansToDegrees(FMath::Acos(lhs.GetSafeNormal() | rhs.GetSafeNormal()));
	}
}
