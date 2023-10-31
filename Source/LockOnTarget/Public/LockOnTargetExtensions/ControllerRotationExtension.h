// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "ControllerRotationExtension.generated.h"

/**
 * Smoothly orients the owning Controller rotation to face the Target.
 * Designed for a vertically aligned player representation, just like ACharacter.
 */
UCLASS(Blueprintable, HideCategories = Tick)
class LOCKONTARGET_API UControllerRotationExtension : public ULockOnTargetExtensionBase
{
	GENERATED_BODY()

public:

	UControllerRotationExtension();

public: //Input

	/** Whether to block the look input of the Controller while the Target is locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bBlockLookInput;

public: //Correction

	/** Tries to approximately predict the Target location using velocity. Adds additional smoothness with fast Targets and helps to achieve 'lag-free' rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction")
	bool bUseLocationPrediction;

	/** The prediction time. The default is 0.083s ~= 5 frames at 60 fps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction", meta = (ClampMin = 0.f, ClampMax = 0.5f, Units = "s", EditCondition = "bUseLocationPrediction", EditConditionHides))
	float PredictionTime;

	/** The maximum angular deviation of the predicted location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction", meta = (ClampMin = 0.f, ClampMax = 30.f, Units = "deg", EditCondition = "bUseLocationPrediction", EditConditionHides))
	float MaxAngularDeviation;

	/** Smoothies the Target oscillations in Actor's relative location using critically damped spring interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction")
	bool bUseOscillationSmoothing;

	/** The oscillation damping factor. The larger the value, the stiffer the spring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Correction", meta = (ClampMin = 0.f, ClampMax = 20.f, EditCondition = "bUseOscillationSmoothing", EditConditionHides))
	float OscillationDampingFactor;

public: //Limits

	/** The angular tolerance from the world Z-axis within which the rotation update is skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = 0.f, ClampMax = 20.f, Units = "deg"))
	float DeadZonePitchTolerance;

	/** Offsets the resulting yaw value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = -30.f, ClampMax = 30.f, Units = "deg"))
	float YawOffset;

	/** Clamps the resulting yaw value within this range relative to the Target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = 10.f, ClampMax = 90.f, Units = "deg"))
	float YawClampRange;

	/** Offsets the resulting pitch value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = -30.f, ClampMax = 30.f, Units = "deg"))
	float PitchOffset;

	/** Clamps the resulting pitch value in [X, Y] range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits", meta = (ClampMin = -90.f, ClampMax = 90.f, Units = "deg"))
	FVector2D PitchClamp;

public: //Interpolation

	/** The maximum interpolation speed to update the rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 0.f, ClampMax = 30.f))
	float InterpolationSpeed;

	/** The angular tolerance within which the rotation update is skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 0.f, ClampMax = 10.f, Units = "deg"))
	float AngularSleepTolerance;

	/** The angular range within which the interpolation speed is 'interpolated' by the ease-in algorithm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 1.f, ClampMax = 30.f, Units = "deg"))
	float InterpEasingRange;

	/** The exponent of the easing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 0.f, ClampMax = 5.f))
	float InterpEasingExponent;

	/** The minimum interpolation speed to update the rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interpolation", meta = (ClampMin = 0.f, ClampMax = 30.f))
	float MinInterpSpeed;

private:

	//Spring data.
	FVector SpringVelocity;
	TOptional<FVector> SpringLocation;

public:

	/** Resets all cached spring data. */
	void ResetSpringInterpData();

public:

	/** Calculates and returns the rotation to be applied to the Controller. */
	UFUNCTION(BlueprintNativeEvent, Category = "Controller Rotation")
	FRotator CalcRotation(const AController* Controller, float DeltaTime);
	virtual FRotator CalcRotation_Implementation(const AController* Controller, float DeltaTime);

	/** Returns the raw Target location. */
	UFUNCTION(BlueprintNativeEvent, Category = "Controller Rotation")
	FVector GetTargetFocusLocation() const;
	virtual FVector GetTargetFocusLocation_Implementation() const;

	/** Returns the view location of the Controller. */
	UFUNCTION(BlueprintNativeEvent, Category = "Controller Rotation")
	FVector GetViewLocation(const AController* Controller) const;
	virtual FVector GetViewLocation_Implementation(const AController* Controller) const;

protected:

	/** Returns the corrected Target location. */
	virtual FVector GetCorrectedTargetLocation(const FVector& TargetLocation, float Distance2D, float DeltaTime);

	/** Returns the desired rotation to the Target. */
	virtual FRotator GetTargetRotation(const FVector& ViewLocation, const FVector& TargetLocation, const FRotator& CurrentRotation);

	/** Returns the interpolated rotation between the current and the desired rotation. */
	virtual FRotator InterpTargetRotation(const FRotator& TargetRotation, const FRotator& CurrentRotation, float DeltaTime);

protected: /** Overrides */

	//ULockOnTargetExtensionBase
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void Update(float DeltaTime) override;
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
	virtual void OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket) override;
};
