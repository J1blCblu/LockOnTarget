// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include <type_traits>
#include "RotationModeBase.generated.h"

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERot : uint8
{
	Roll	= 0b00000001,
	Pitch	= 0b00000010,
	Yaw		= 0b00000100,
};

ENUM_CLASS_FLAGS(ERot);

inline constexpr bool HasAnyRotFlags(std::underlying_type_t<ERot> Flags, ERot Contains)
{
	return Flags & static_cast<std::underlying_type_t<ERot>>(Contains);
}

inline constexpr void SetRotFlags(std::underlying_type_t<ERot> Flags, ERot ToSet)
{
	Flags |= static_cast<std::underlying_type_t<ERot>>(ToSet);
}

/**
 * Returns an accurate rotation from the InstigatorLocation to the TargetLocation specified in GetRotation().
 * GetRotation() should be overridden.
 * 
 * @see ULazyInterpolationMode, URotationModules.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class LOCKONTARGET_API URotationModeBase : public UObject
{
	GENERATED_BODY()

public:
	URotationModeBase();
	static_assert(TIsSame<uint8, std::underlying_type_t<ERot>>::Value, "RotationAxes must be of the same type as the ERot underlying type.");

public:
	/** Clamps the returned rotation's pitch. Where x - min value, y - max value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta = (EditCondition = "RotationAxes & ERot::Pitch", EditConditionHides, DisplayAfter=RotationAxes))
	FVector2D PitchClamp;

	/** Offset applied to the returned rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta=(DisplayAfter = PitchClamp))
	FRotator OffsetRotation;
	
	/** Axes applied to the returned rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta = (Bitmask, BitmaskEnum = "ERot"))
	uint8 RotationAxes;

public:
	/**
	 * Returns the desired rotation for URotationModule subclasses.
	 * 
	 * @param CurrentRotation - Current updated rotation's value.
	 * @param InstigatorLocation - Instigator's world location (camera/owner).
	 * @param TargetLocation - Target's focus point world location.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Lock On Target|Rotation Mode")
	FRotator GetRotation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime);

	/** Native implementation of the above. */
	virtual FRotator GetRotation_Implementation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime);

protected:
	/** Returns rotation from to with the OffsetRotation. */
	FRotator GetRotationUnclamped(const FVector& From, const FVector& To) const;

	/** Returns clamped rotation from to. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target|Rotation Mode", meta = (BlueprintProtected))
	void ClampPitch(UPARAM(ref) FRotator& Rotation) const;

	bool IsPitchClamped(const FRotator& Rotation) const;

	/** Update only required axes in NewRotation. */
	UFUNCTION(BlueprintPure, Category = "Lock On Target|Rotation Mode", meta = (BlueprintProtected))
	void ApplyRotationAxes(const FRotator& CurrentRotation, UPARAM(ref) FRotator& NewRotation) const;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif
};
