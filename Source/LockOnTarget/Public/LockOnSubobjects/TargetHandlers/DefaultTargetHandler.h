// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"
#include "Utilities/Enums.h"
#include "DefaultTargetHandler.generated.h"

struct FTargetInfo;
class ULockOnTargetComponent;
class UTargetingHelperComponent;

/**
 * Native default implementation of TargetHandler based on calculation and comparison of the Targets modifiers.
 * Best Target will have least modifier.
 * 
 * You can override CalculateTargetModifier() for custom sorting set of Targets.
 * @see UTargetHandlerBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget))
class LOCKONTARGET_API UDefaultTargetHandler : public UTargetHandlerBase
{
	GENERATED_BODY()

public:
	UDefaultTargetHandler();

public:
	/** 
	 * Auto find Target on failing checked flags.
	 * If proper flag set then trying find new Target.
	 * i.e. if only checked TargetInvalidation then after Target Destroying will trying find new Target. 
	 * If Target goes out of LostDistance then auto Finding new Target will not be triggered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (BitMask, BitmaskEnum = "EUnlockReasonBitmask"))
	uint8 AutoFindTargetFlags;

	/** Capture Target that only on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
	bool bScreenCapture;

	/**
	 * Narrows the screen borders(x and y) from both sides as a percentage when finding new Target. 
	 * Useful for not capturing Target near screen border. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (EditCondition = "bScreenCapture"))
	FVector2D FindingScreenOffset;

	/**
	 * Narrows the screen borders(x and y) from both sides as a percentage when switching Target. 
	 * Useful for not capturing Target near screen border. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (EditCondition = "bScreenCapture"))
	FVector2D SwitchingScreenOffset;

	/** Angle to Target relative to the forward vector of the camera(if not exists owner Actor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, EditCondition = "!bScreenCapture"))
	float CaptureAngle;

	/** Should modifier use Distance to Target or Default modifier = 1000.f. */
	UPROPERTY(EditAnywhere, Category = "Default Solver")
	bool bCalculateDistance;

	/**
	 * Default Solver that calculate Target modifier. (Target with less modifier will be captured as the result).
	 * With CalculateTargetModifier override without calling parent Method will have no Affect.
	 * 
	 * Angle weight determines the effect of angle between captured Target's socket and screen center(basically, without camera rotation clamping) modifier while finding best Target.
	 * i.e. smaller value will reduce Angle affect on the resulted modifier and increase Distance weight to Target.
	 * 
	 * For finding closest Target this value should be 0.f. 
	 */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f))
	float AngleWeightWhileFinding;

	/**
	 * Default Solver that calculate Target modifier. (Target with less modifier will be captured as the result).
	 * With CalculateTargetModifier override without calling parent Method will have no Affect.
	 * 
	 * While switching this Angle will be between CapturedTarget and Candidate to be new Target.
	 * i.e. smaller value will reduce Angle affect on the resulted modifier and increase Distance weight to Target.
	 * 
	 * For finding closest Target this value should be 0.f. 
	 */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f))
	float AngleWeightWhileSwitching;

	/** 
	 * Default Solver that calculate Target modifier. (Target with less modifier will be captured as the result).
	 * With CalculateTargetModifier override without calling parent Method will have no Affect.
	 * 
	 * Only have affect while switching. 
	 * This parameter increase influence of player input switch direction.
	 * Better Targets will be closest to this trigonometric angle(of player input).
	 * i.e. moving up(x = 0, y = 1) analog input will be converted to trigonometric angle 90deg.
	 */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f))
	float TrigonometricInputWeight;

	/** Maybe useful for Capturing Target not closest to screen center. Measured in degrees. */
	UPROPERTY(EditAnywhere, Category = "Default Solver|Advanced")
	FVector2D ScreenCenterOffsetWhileFinding = FVector2D(0.f, 0.f);

	/** 
	 * The Targets in this range will be processed.
	 * Trigonometric Angle added to both side of player analog input direction.
	 * Player input converts to 2D space vector with direction value of player Analog input.
	 * Trigonometric Angle calculates from screen right X axis (x = 1, y = 0) and player Analog input.
	 * 
	 * i.e. player Analog input = 90deg = (x = 0, y = 1) and this value = 30deg.
	 * Then Capturing target range(from, to) will be [60 , 120] => 60deg.
	 * Any Target off this trigonometric range [60, 120] i.e. down side off screen (270deg)
	 * can't be captured and Target modifier will not be calculated for it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f))
	float TrigonometricAngleRange;

	/**
	 * Does Target should be Traced before capturing it.
	 * While Target locked checks successful trace to it.
	 * If Target while Targeting out of Line Of Sight the timer begins work.
	 * Target may return to Line Of Sight and stop timer.
	 * After timer finished Target will be unlocked.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Line Of Sight")
	bool bLineOfSightCheck;

	/** Object channel for tracing. If trace hit something then Line of Sight fails. Target and Owner will be ignored. */
	UPROPERTY(EditDefaultsOnly, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck"))
	TArray<TEnumAsByte<ECollisionChannel>> TraceObjectChannels;

	/** 
	 * The timer Delay after which the Target will be unlocked. 
	 * Timer stops if Target returns to Line Of Sight.
	 * If < 0.f will never unlock Target. This means Line of Sight used only for finding Target.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck"))
	float LostTargetDelay;

#if WITH_EDITORONLY_DATA

	/** Display Target temporary modifier for debugging while calculation. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (Bitmask, BitmaskEnum = "EDebugFlags"))
	bool bDisplayModifier = false;

	/** Modifier Color. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (HideAlphaChannel, EditCondition = "bDisplayModifier"))
	FColor ModifierColor = FColor::Black;

	/** Modifier display duration. */
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (EditCondition = "bDisplayModifier", ClampMin = 0.f, UIMin = 0.f))
	float ModifierDuration = 3.f;

#endif

	/*******************************************************************************************/
	/*******************************  BP Overridable  ******************************************/
	/*******************************************************************************************/

protected:
	/**
	 * Use for find Best Target from sorted Targets.
	 * Can be overridden via BP.
	 * 
	 * Best Target = Modifier -> 0. (Clossest to zero)
	 * If player input doesn't exist then it value < 0.f(when not switching).
	 * 
	 * @param Location - Target's socket location.
	 * @param TargetHelperComponent - HelperComponent for useful info.
	 * @param PlayerInput - Player trigonometric input(0, 360). If >= 0 then player perform Switch Target/socket.
	 * @return - Target modifier which should be > 0.f.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget")
	float CalculateTargetModifier(const FVector& Location, UTargetingHelperComponent* TargetHelperComponent, float PlayerInput) const;

	/*******************************************************************************************/
	/******************************* Target Handler Interface **********************************/
	/*******************************************************************************************/
public:
	/** TargetHandlerBase */
	virtual FTargetInfo FindTarget_Implementation() override;
	virtual bool SwitchTarget_Implementation(FTargetInfo& TargetInfo, float PlayerInput) override;
	virtual bool CanContinueTargeting_Implementation() override;
	virtual void OnTargetUnlockedNative() override;
	/** ~TargetHandlerBase */

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
	/** ~Finding target */

	/** Line Of Sight handling */
	FTimerHandle LOSDelayHandler;
	void StartLineOfSightTimer();
	void StopLineOfSightTimer();
	void OnLineOfSightFail();
	bool LineOfSightTrace(const AActor* const Target, const FVector& Location) const;
	/** ~Line Of Sight handling */

	/** Helpers methods */
	bool HandleTargetClearing(EUnlockReasonBitmask UnlockReason);
	/** ~Helpers methods */

/*******************************************************************************************/
/******************************* Debug and Editor Only  ************************************/
/*******************************************************************************************/
#if WITH_EDITORONLY_DATA
protected:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event);
#endif
};
