// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enums.generated.h"

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERot
{
	E_Roll = 0x01 UMETA(DisplayName = "Roll"),
	E_Pitch = 0x02 UMETA(DisplayName = "Pitch"),
	E_Yaw = 0x04 UMETA(DisplayName = "Yaw"),
};

ENUM_CLASS_FLAGS(ERot);

template <typename T>
T operator|(T A, ERot B)
{
	return A | static_cast<T>(B);
}

template <typename T>
T operator|=(T A, ERot B)
{
	return A |= static_cast<T>(B);
}

template <typename T>
T operator&(T A, ERot B)
{
	return A & static_cast<T>(B);
} 

UENUM(BlueprintType)
enum class EOffsetType : uint8
{
	ENone UMETA(DisplayName = "None", ToolTip = "Offset is not applied."),
	EConstant UMETA(DisplayName = "Constant", ToolTip = "Constant offset applied."),
	EAdaptiveCurve UMETA(DisplayName = "AdaptiveCurve", ToolTip = "Offset is estimated depending on the distance to the Target."),
	ECustomOffset UMETA(DisplayName = "CustomOffset", ToolTip = "Custom offset applied via calling GetCustomTargetOffset(), which can be overriden.")
};

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EUnlockReasonBitmask
{
	E_TargetInvalidation = 1 << 0 UMETA(DisplayName = "Target Invalidation", ToolTip = "Auto find new Target after previous invalidation (e.g. destroying Target/HelperComponent)."),
	E_OutOfLostDistance = 1 << 1 UMETA(DisplayName = "Out Of Lost Distance", ToolTip = "Auto find new Target after previous leaving Lost distance."),
	E_LineOfSightFail = 1 << 2 UMETA(DisplayName = "Line Of Sight Fail", ToolTip = "Auto find new Target after the end of Line of Sight Timer. If LineOfSight is enabled and LostTargetDelay >= 0.f."),
	E_HelperComponentDiscard = 1 << 3 UMETA(DisplayName = "Helper Component Discard", ToolTip = "Auto find new Target after negative method call CanBeTargeted() (Can be overridden) in HelperComponent."),
};

ENUM_CLASS_FLAGS(EUnlockReasonBitmask);
