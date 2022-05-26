// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnSubobjectBase.h"
#include "Utilities/Enums.h"
#include <type_traits>
#include "RotationModeBase.generated.h"

/**
 * Base rotation mode.
 * Returns the rotation to the Target's socket location.
 * 
 * GetRotation() should be overridden.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), EditInlineNew, DefaultToInstanced)
class LOCKONTARGET_API URotationModeBase : public ULockOnSubobjectBase
{
	GENERATED_BODY()

public:
	URotationModeBase();
	static_assert(TIsSame<std::underlying_type_t<ERot>, uint8>::Value, "URotationModeBase::RotationAxes must be of the same type as the ERot underlying type.");

public:
	/** Axes applied to returned rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta = (Bitmask, BitmaskEnum = "ERot"))
	uint8 RotationAxes;

	/** Clamps the return value of the pitch. Where x - min value, y - max value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta = (EditCondition = "RotationAxes & ERot::E_Pitch", EditConditionHides))
	FVector2D PitchClamp;

	/** Offset applied to returned rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config")
	FRotator OffsetRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config")
	bool bIsEnabled = true;

public:
	/**
	 * Used to return the rotation to the LockOnTargetComponent.
	 * 
	 * @param CurrentRotation - Current control/owner's rotation.
	 * @param InstigatorLocation - LockOnTargetComponent's camera/owner location.
	 * @param TargetLocation - Captured location of the Target socket with the world offset.
	 * @param DeltaTime - WorldDeltaTime.
	 * @return - Rotation applied to the Control/Owner rotation.
	 */
	UFUNCTION(BlueprintNativeEvent)
	FRotator GetRotation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime);

protected:

	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode", meta = (BlueprintProtected))
	FRotator GetRotationToTarget(const FVector& LocationFrom, const FVector& LocationTo) const;

	/** Update only required axes in NewRotation. */
	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode", meta = (BlueprintProtected))
	void ApplyRotationAxes(const FRotator& CurrentRotation, FRotator& NewRotation) const;

protected:
#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif
};
