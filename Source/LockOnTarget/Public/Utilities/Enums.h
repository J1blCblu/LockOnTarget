// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <type_traits>
#include "Enums.generated.h"

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERot : uint8
{
	Roll = 1,
	Pitch = 1 << 1,
	Yaw = 1 << 2
};

using ERotType = std::underlying_type_t<ERot>;

constexpr inline ERotType operator|(ERotType A, ERot B)
{
	return A | static_cast<ERotType>(B);
}

inline ERotType operator|=(ERotType A, ERot B)
{
	return A |= static_cast<ERotType>(B);
}

constexpr inline ERotType operator&(ERotType A, ERot B)
{
	return A & static_cast<ERotType>(B);
}

UENUM(BlueprintType)
enum class EOffsetType : uint8
{
	ENone			UMETA(DisplayName = "None", ToolTip = "Offset isn't applied."),
	EConstant		UMETA(DisplayName = "Constant", ToolTip = "Constant offset is applied."),
	EAdaptiveCurve	UMETA(DisplayName = "AdaptiveCurve", ToolTip = "Offset is estimated depending on the distance to the Target."),
	ECustomOffset	UMETA(DisplayName = "CustomOffset", ToolTip = "Custom offset is applied via calling the GetCustomTargetOffset(), which can be overridden.")
};

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EUnlockReasonBitmask : uint8
{
	E_TargetInvalidation = 1 << 0			UMETA(DisplayName = "Target Invalidation", ToolTip = "Auto find a new Target when the previous was invalidated (e.g. HelperComponent is destroyed)."),
	E_OutOfLostDistance = 1 << 1			UMETA(DisplayName = "Out Of Lost Distance", ToolTip = "Auto find a new Target when the previous has left the lost distance."),
	E_LineOfSightFail = 1 << 2				UMETA(DisplayName = "Line Of Sight Fail", ToolTip = "Auto find a new Target when the Line of Sight timer has finished. If bLineOfSightCheck is enabled and LostTargetDelay > 0.f."),
	E_HelperComponentDiscard = 1 << 3		UMETA(DisplayName = "Helper Component Discard", ToolTip = "Auto find a new Target when the CanBeTargeted() method has returned false (Can be overridden) in the TargetingHelperComponent."),
	E_CapturedSocketInvalidation = 1 << 4	UMETA(DisplayName = "Captured Socket Invalidation", ToolTip = "Auto find a new Target when a previous captured socket has been removed using the RemoveSocket() method.")
};

//ENUM_CLASS_FLAGS(ERot);
//ENUM_CLASS_FLAGS(EUnlockReasonBitmask);
