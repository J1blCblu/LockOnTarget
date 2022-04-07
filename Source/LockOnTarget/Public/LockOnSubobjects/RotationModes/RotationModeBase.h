// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnSubobjectBase.h"
#include "Utilities/Enums.h"
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

	/** Axes applied to returned rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta = (Bitmask, BitmaskEnum = "ERot"))
	uint8 RotationAxes;

	/** Clamps the return value of the pitch. Where x - min value, y - max value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config", meta = (EditCondition = "RotationAxes & ERot::E_Pitch"))
	FVector2D PitchClamp;

	/** Offset applied to returned rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config")
	FRotator OffsetRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Config")
	bool bIsEnabled = true;

/*******************************************************************************************/
/*******************************  Class Methods   ******************************************/
/*******************************************************************************************/
public:
	/**
	 * Used to return the rotation to the LockOnComponent.
	 * 
	 * @param CurrentRotation - Current control/owner's rotation.
	 * @param InstigatorLocation - LockOnComponent's camera/owner location.
	 * @param TargetLocation - Captured location of the Target socket with the world offset.
	 * @param DeltaTime - WorldDeltaTime.
	 * @return - Rotation applied to the Control/Owner rotation.
	 */
	UFUNCTION(BlueprintNativeEvent)
	FRotator GetRotation(const FRotator& CurrentRotation, const FVector& InstigatorLocation, const FVector& TargetLocation, float DeltaTime);

protected:

	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode", meta = (BlueprintProtected))
	FRotator GetRotationToTarget(const FVector& LocationFrom, const FVector& LocationTo) const;

	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode", meta = (BlueprintProtected))
	FRotator GetClampedRotationToTarget(const FVector& LocationFrom, const FVector& LocationTo) const;

	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Rotation Mode", meta = (BlueprintProtected))
	void AddOffsetToRotation(FRotator& Rotator) const;

/*******************************************************************************************/
/*******************************  Native Methods   *****************************************/
/*******************************************************************************************/
protected:
	/** Update only required axes in NewRotation. */
	void ApplyRotationAxes(const FRotator& CurrentRotation, FRotator& NewRotation) const;

	/** Clamp the pitch value. */
	void ClampPitch(FRotator& Rotation) const;

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif
};
