// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"
#include "Utilities/Enums.h"
#include <type_traits>
#include "DefaultTargetHandler.generated.h"

struct FTargetInfo;
class ULockOnTargetComponent;
class UTargetingHelperComponent;

/**
 * Native default implementation of TargetHandler based on calculation and comparison of Targets modifiers.
 * The best Target will have the least modifier.
 * 
 * You can override CalculateTargetModifier() to custom sort a set of Targets.
 * @see UTargetHandlerBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget))
class LOCKONTARGET_API UDefaultTargetHandler : public UTargetHandlerBase
{
	GENERATED_BODY()

public:
	UDefaultTargetHandler();
	static_assert(TIsSame<std::underlying_type_t<EUnlockReasonBitmask>, uint8>::Value, "UDefaultTargetHandler::AutoFindTargetFlags must be of the same type as the EUnlockReasonBitmask underlying type.");

public:
	/** TargetHandlerBase */
	virtual FTargetInfo FindTarget_Implementation() override;
	virtual bool SwitchTarget_Implementation(FTargetInfo& TargetInfo, FVector2D PlayerInput) override;
	virtual bool CanContinueTargeting_Implementation() override;
	virtual void OnTargetUnlockedNative() override;

public:
	/** 
	 * Auto find a new Target after a certain flag fails.
	 * If proper flag is set, try to find a new Target.
	 * i.e. if the 'TargetInvalidation' flag is only set then it will try to find a new Target after the Target destruction. 
	 * If the Target went out of LostDistance then it won't try to find a new Target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (BitMask, BitmaskEnum = "EUnlockReasonBitmask"))
	uint8 AutoFindTargetFlags;

	/** Multiply the CaptureRadius in the HelperComponent. May be useful for the progression. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (UIMin = 0.f, ClampMin = 0.f, Units="x"))
	float TargetCaptureRadiusModifier;

	/** Capture a Target that is only on the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
	bool bScreenCapture;

	/**
	 * Narrows the screen borders(x and y) from the both sides by a percentage when trying to find a new Target.
	 * Useful for not capturing a Target near the screen borders. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (EditCondition = "bScreenCapture", EditConditionHides))
	FVector2D FindingScreenOffset;

	/**
	 * Narrows the screen borders(x and y) from the both sides by a percentage when trying to switch the Target. 
	 * Useful for not capturing a Target near screen borders. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (EditCondition = "bScreenCapture", EditConditionHides))
	FVector2D SwitchingScreenOffset;

	/** Angle to Target relative to the forward vector of the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, EditCondition = "!bScreenCapture", EditConditionHides, Units="deg"))
	float CaptureAngle;

	/** Should modifier use a distance to the Target or a default modifier = 1000.f. */
	UPROPERTY(EditAnywhere, Category = "Default Solver")
	bool bCalculateDistance;

	/**
	 * The Default Solver that calculates the Target modifier. (The target with the smallest modifier will be captured as a result).
	 * Overriding the CalculateTargetModifier() method without calling the parent method will have no effect.
	 * 
	 * Angle Weight determines the impact of the angle modifier between the vector to the Target socket and the camera forward vector (basically without camera rotation clamping) while finding the best Target.
	 * i.e. a smaller value will reduce the angle impact on the resulting modifier and will increase the impact of the distance weight to the Target.
	 * 
	 * To find the closest Target this value should be 0.f. 
	 */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Units="x"))
	float AngleWeightWhileFinding;

	/**
	 * The Default Solver that calculates the Target modifier. (The target with the smallest modifier will be captured as a result).
	 * Overriding the CalculateTargetModifier() method without calling the parent method will have no effect.
	 * 
	 * Angle Weight determines the impact of the angle modifier between the vector to the Target socket and the vector to the captured Target socket while switching to a new Target.
	 * i.e. a smaller value will reduce the angle impact on the resulting modifier and will increase the impact of the distance weight to the Target.
	 * 
	 * To find the closest Target this value should be 0.f.
	 */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Units="x"))
	float AngleWeightWhileSwitching;

	/** 
	 * The Default Solver that calculates the Target modifier. (The target with the smallest modifier will be captured as a result).
	 * Overriding the CalculateTargetModifier() method without calling the parent method will have no effect.
	 * 
	 * Only have affect when switching to a new Target.
	 * This parameter will increase the impact of the player's input direction.
	 * The best Target will be the closest one to the trigonometric angle (the player input direction).
	 * i.e. moving the mouse/stick up(x = 0, y = 1) the analog input will be converted to the trigonometric angle 90deg.
	 */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Units="x"))
	float TrigonometricInputWeight;

	/** 
	 * Rotate the camera forward vector (== screen center) in the Angle calculations. 
	 * May be useful for finding the closest Target to the rotated camera forward vector. 
	 * 
	 * |2 2 2 2 2|-->|2 1 0 1 2|
	 * |2 1 1 1 2|-->|2 1 1 1 2|
	 * |2 1 0 1 2|-->|2 2 2 2 2|
	 * |2 1 1 1 2|-->|3 3 3 3 3|
	 * |2 2 2 2 2|-->|4 4 4 4 4|
	 * 
	 * On the picture the CameraRotationOffsetForCalculations pitch value is changed => The Best Target(0) is now higher on the screen.
	 */

	UPROPERTY(EditAnywhere, Category = "Default Solver")
	FRotator CameraRotationOffsetForCalculations;

	/** 
	 * Targets in this range will be processed.
	 * The TrigonometricAngleRange is added to both sides of the player analog input direction.
	 * The Player input is converted to a 2D space direction.
	 * The trigonometric Angle is calculated from the screen right X axis (x = 1, y = 0) and the player input direction.
	 * 
	 * i.e. player Analog input = 90deg = (x = 0, y = 1) and this value = 30deg,
	 * then the capturing trigonometric range(from, to) will be [60 , 120] => 60deg.
	 * Any Target outside this trigonometric range [60, 120] i.e. down side off the screen (270deg)
	 * can't be captured and the Target modifier won't be calculated for it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, Units="deg"))
	float TrigonometricAngleRange;

	/**
	 * Does a Target should be traced before capturing it.
	 * While the Target is locked, checks for a successful trace to it.
	 * If the captured Target is out of the Line Of Sight, the timer begins working.
	 * The Target may return to the Line Of Sight and stop the timer.
	 * The Target will be unlocked after the timer expires.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Line Of Sight")
	bool bLineOfSightCheck;

	/** Object channels for the trace. If trace hits something then the Line of Sight fails. Target and Owner will be ignored. */
	UPROPERTY(EditDefaultsOnly, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck", EditConditionHides))
	TArray<TEnumAsByte<ECollisionChannel>> TraceObjectChannels;

	/** 
	 * Timer Delay after which the Target will be unlocked. 
	 * Timer stops if the Target returns to the Line Of Sight.
	 * If <= 0.f, the Target won't be unlocked. This means the Line of Sight is used only to find the Target.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck", EditConditionHides, Units="s"))
	float LostTargetDelay;

#if WITH_EDITORONLY_DATA

	/** Display a Target temporary modifier for debugging. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDisplayModifier = false;

	/** Modifier Color. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (HideAlphaChannel, EditCondition = "bDisplayModifier", EditConditionHides))
	FColor ModifierColor = FColor::Black;

	/** Modifier display duration. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (EditCondition = "bDisplayModifier", EditConditionHides, ClampMin = 0.f, UIMin = 0.f, Units="s"))
	float ModifierDuration = 3.f;

#endif

/*******************************************************************************************/
/*******************************  BP Overridable  ******************************************/
/*******************************************************************************************/
protected:
	/**
	 * Used to find the best Target from the sorted Targets.
	 * Can be overridden via BP.
	 * 
	 * The best Target = modifier -> 0. (The clossest to zero)
	 * If the player input doesn't exist then its value < 0.f (when not switching).
	 * 
	 * @param Location - Target's socket location.
	 * @param TargetHelperComponent - HelperComponent is used to provide an useful info.
	 * @param PlayerInput - Player trigonometric input(0, 360). If >= 0 then the player performs to switch the Target/socket.
	 * @return - Target modifier should be > 0.f.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Default Target Handler")
	float CalculateTargetModifier(const FVector& Location, UTargetingHelperComponent* TargetHelperComponent, float PlayerInput) const;

	/**
	 * Used for CaptureRadius checks by default.
	 * 
	 * Can be overridden to provide custom rules.
	 * If you want to keep CaptureRadius checks, make sure to call the parent method.
	 * Note that the LineOfSight is handled in the native private method after this method returns true.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Default Target Handler")
	bool IsTargetableCustom(UTargetingHelperComponent* HelperComponent) const;

/*******************************************************************************************/
/*******************************  Native   *************************************************/
/*******************************************************************************************/
private:
	/** Finding target */
	FTargetInfo FindTargetNative(float PlayerInput = -1.f) const;
	bool IsTargetable(UTargetingHelperComponent* HelpComp) const;
	bool IsSocketValid(const FName& Socket, UTargetingHelperComponent* HelperComponent, float PlayerInput, const FVector& SocketLocation) const;

	bool SwitchTargetNative(FTargetInfo& TargetInfo, float PlayerInput) const;
	TPair<FName, float> TrySwitchSocket(float PlayerInput) const;

private:
	/** Line Of Sight handling */
	FTimerHandle LOSDelayHandler;
	virtual void StartLineOfSightTimer();
	virtual void StopLineOfSightTimer();
	virtual void OnLineOfSightFail();
	bool LineOfSightTrace(const AActor* const Target, const FVector& Location) const;

private:
	/** Helpers methods */
	bool HandleTargetClearing(EUnlockReasonBitmask UnlockReason);

/*******************************************************************************************/
/******************************* Debug and Editor Only *************************************/
/*******************************************************************************************/
#if WITH_EDITORONLY_DATA
private:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event);
#endif
};
