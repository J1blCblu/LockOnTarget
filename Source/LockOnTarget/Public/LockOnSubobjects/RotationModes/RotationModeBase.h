// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnSubobjectBase.h"
#include "Utilities/Enums.h"
#include "RotationModeBase.generated.h"

/**
 * Base rotation mode which return rotation to Target's socket location.
 * Override GetRotation().
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), EditInlineNew, DefaultToInstanced)
class LOCKONTARGET_API URotationModeBase : public ULockOnSubobjectBase
{
	GENERATED_BODY()

public:
	URotationModeBase();

	/** Axes of NewRotation to change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta = (Bitmask, BitmaskEnum = "ERot"))
	uint8 RotationAxes;

	/** Clamps NewRotation Pitch value. Where x - min value, y - max value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta = (EditCondition = "RotationAxes & ERot::E_Pitch"))
	FVector2D PitchClamp;

	/** Offset applied to final Rotation. Regardless of Target offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config")
	FRotator OffsetRotation;

	UPROPERTY(EditAnywhere, Category = "Base Config")
	bool bIsEnabled = true;

/*******************************************************************************************/
/*******************************  Class Methods   ******************************************/
/*******************************************************************************************/
public:
	/**
	 * Get rotation for LockOnComponent's Control/Owner rotation.
	 * 
	 * @param CurrentRotation - Current control/owner's rotation.
	 * @param InstigatorLocation - LockOnComponent's camera/owner location.
	 * @param TargetLocation - Captured location of Target's socket with world offset.
	 * @param DeltaTime - WorldDeltaTime.
	 * @return - Rotation to be applied to Control/Owner rotation.
	 */
	UFUNCTION(BlueprintNativeEvent)
	FRotator GetRotation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode")
	bool IsEnabled() const { return bIsEnabled;}

	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|Rotation Mode")
	void SetIsEnabled(bool bEnable) { bIsEnabled = bEnable;}

	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode")
	FRotator GetRotationToTarget(const FVector& LocationFrom, const FVector& LocationTo) const;

	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode")
	FRotator GetClampedRotationToTarget(const FVector& LocationFrom, const FVector& LocationTo) const;

	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode")
	void AddOffsetToRotation(FRotator& Rotator) const;

/*******************************************************************************************/
/*******************************  Native Methods   *****************************************/
/*******************************************************************************************/
protected:
	/** Apply only required axes to final rotation. */
	void ApplyRotationAxes(const FRotator& CurrentRotation, FRotator& NewRotation) const;

	/** Clamp rotator's pitch with PitchClamp values. */
	void ClampPitch(FRotator& Rotation) const;

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif
};
