// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/TargetHandlers/TargetHandlerBase.h"
#include "Engine/EngineTypes.h"
#include <type_traits>
#include "DefaultTargetHandler.generated.h"

struct FTargetInfo;
struct FTargetModifier;
struct FTargetContext;
struct FFindTargetContext;
class UDefaultTargetHandler;
class ULockOnTargetComponent;
class UTargetingHelperComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnModifierCalculated, const FFindTargetContext& /*TargetContext*/, float /*Modifier*/);

/** Unlock reasons that are currently supported by DefaultTargetHandler. */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EUnlockReasonBitmask : uint8
{
	TargetInvalidation			= 1 << 0 UMETA(ToolTip = "Auto find a new Target when the previous was invalidated (e.g. HelperComponent or its owner is destroyed)."),
	OutOfLostDistance			= 1 << 1 UMETA(ToolTip = "Auto find a new Target when the previous has left the lost distance."),
	LineOfSightFail				= 1 << 2 UMETA(ToolTip = "Auto find a new Target when the Line of Sight timer has finished. If bLineOfSightCheck is enabled and LostTargetDelay > 0.f."),
	HelperComponentDiscard		= 1 << 3 UMETA(ToolTip = "Auto find a new Target when the CanBeCaptured() method has returned false (Can be overridden) in the TargetingHelperComponent."),
	CapturedSocketInvalidation	= 1 << 4 UMETA(ToolTip = "Auto find a new Target when the previous captured socket has been removed using the RemoveSocket() method.")
	//Reserved					= 1 << 5 UMETA(ToolTip = "")
};

/**
 * Holds a modifier associated with the Target.
 */
USTRUCT()
struct LOCKONTARGET_API FTargetModifier
{
	GENERATED_BODY()

public:
	FTargetModifier() = default;
	FTargetModifier(const FTargetContext& TargetContext);

public:
	UPROPERTY()
	FTargetInfo TargetInfo;

	UPROPERTY()
	float Modifier = FLT_MAX;
};

/**
 * Holds contextual information about the Target.
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FTargetContext
{
	GENERATED_BODY()

public:
	FTargetContext() = default;

public:
	//Actually the Target associated with the context. 
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	TObjectPtr<UTargetingHelperComponent> HelperComponent = nullptr;

	//Socket associated with the Target.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FName SocketName = NAME_None;
	
	//Socket's world location.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FVector SocketWorldLocation = FVector::ZeroVector;

	//The Socket's world location projected on the screen. Make sure to check bIsScreenPositionValid before access. 
	//Basically isn't calculated if bScreenCapture is not used or while not switching the Target.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FVector2D SocketScreenPosition = FVector2D::ZeroVector;

	//Can the ScreenPosition actually be used for calculations
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	bool bIsScreenPositionValid = false;
};

/**
 * Holds contextual information about current Target finding.
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FFindTargetContext
{
	GENERATED_BODY()

public:
	FFindTargetContext() = default;
	FFindTargetContext(UDefaultTargetHandler* Owner, ULockOnTargetComponent* LockOn, FVector2D PlayerInput);

public:
	//TargetHandler that has created this context.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<UDefaultTargetHandler> ContextOwner = nullptr;

	//TargetHandler owner.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<ULockOnTargetComponent> LockOnTargetComponent = nullptr;

	//Raw player input. Basically available only while switching the Target.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector2D PlayerRawInput = FVector2D(0.);
	
	//Current Target context. Basically available only while switching the Target.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FTargetContext CurrentTarget;

	//Iterative Target context. Treat as a candidate that can potentially be a new Target.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FTargetContext IteratorTarget;

	//Is any Target is locked by the LockOnTargetComponent.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	bool bIsSwitchingTarget = false;

public:
	void PrepareTargetSocket(FName Socket);

private:
	bool VectorToScreenPosition(const FVector& Location, FVector2D& OutScreenPosition) const;
};

/**
 * Native default implementation of the TargetHandler based on calculation and comparison of Targets modifiers.
 * The best Target will have the least modifier. Targets with several sockets will have several modifiers.
 * 
 * Find Target execution flow:
 * - FindTargetInternal() - Iterates over TargetingHelperComponents (hereinafter a Target), prepares context.
 *		- IsTargetable() - Checks wether the Target can be processed.
 *		- [BP] IsTargetableCustom() - Custom checks.
 *		- FindBestSocket() - Iterates over Target's sockets and finds the best one by calculating a modifier that will be associated with the Target.
 *			- [C++] PreModifierCalculation() - Checks wether the modifier should be calculated for the Socket.
 *			- [BP] CalculateModifier() - Calculates the modifier for the Socket.
 *			- [BP] PostModifierCalculation() - Will be called only if the Socket's modifier is lesser than the best Target's modifier. Expensive operations are intended to be here, e.g. Line of Sight is processed here.
 * 
 * (where [BP] means that a method can overridden in BP/C++, [C++] respectively for C++ only.)
 * 
 * @see UTargetHandlerBase.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Config=Game, DefaultConfig)
class LOCKONTARGET_API UDefaultTargetHandler : public UTargetHandlerBase
{
	GENERATED_BODY()

public:
	UDefaultTargetHandler();
	friend class FGDC_LockOnTarget; //To avoid ignoring PostModifierCalculationCheck() if the modifier isn't small enough.
	static_assert(TIsSame<std::underlying_type_t<EUnlockReasonBitmask>, uint8>::Value, "UDefaultTargetHandler::AutoFindTargetFlags must be of the same type as the EUnlockReasonBitmask underlying type.");

public: /** Config */

	/** Auto find a new Target on a certain flag failure. If the Target is destroyed and the TargetInvalidation flag is set then try to find a new Target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (BitMask, BitmaskEnum = "/Script/LockOnTarget.EUnlockReasonBitmask"))
	uint8 AutoFindTargetFlags;

	/** 
	 * Perform distance check to the Target.
	 * Warning: It's not recommended to disable as it can sort out most of the Targets very quickly on earlier stages!
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
	bool bDistanceCheck;

	/** Capture a Target that is only on the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
	bool bScreenCapture;

	/**
	 * Narrows the screen borders (x and y) from the both sides by a percentage when trying to find a new Target.
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

	//~-----------------------------------------------------------------------------------------------------------------------------

	/** Increases the influence of the distance to the Target's socket on the final modifier. */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Units = "x"))
	float DistanceWeight;

	/** Increases the influence of the angle to the Target's socket on the final modifier. */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Units="x"))
	float AngleWeightWhileFinding;

	/** Increases the influence of the angle to the Target's socket on the final modifier. */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Units="x"))
	float AngleWeightWhileSwitching;

	/** Increases the influence of the player's input to the Target's socket on the final modifier (while any Target is locked). */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Units="x"))
	float PlayerInputWeight;

	/** Transform the view rotation (basically a camera rotation) by this rotator. Used to adjust the screen center for the AngleWeightWhileFinding. */
	UPROPERTY(EditAnywhere, Category = "Default Solver", meta = (EditCondition = "AngleWeightWhileFinding > 0"))
	FRotator ViewRotationOffset;

	//~-----------------------------------------------------------------------------------------------------------------------------

	/** Targets within this angle range will be processed while switching. Added to both sides of the player's input direction (in screen space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, Units="deg"))
	float AngleRange;

	/**
	 * Whether the Target's socket should be traced. Will regularly trace to the captured socket.
	 * If the Target is out of the Line Of Sight, the timer starts running.
	 * The Target can return to the Line Of Sight and stop the timer.
	 * The Target will be unlocked after the timer expires.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Line Of Sight")
	bool bLineOfSightCheck;

	/** Object channels to trace. If the trace hits something then the Line of Sight fails. Target and Owner will be ignored. */
	UPROPERTY(EditDefaultsOnly, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck", EditConditionHides))
	TArray<TEnumAsByte<ECollisionChannel>> TraceObjectChannels;

	/** 
	 * Timer Delay on expiration of which the Target will be unlocked. Timer stops if the Target returns to the Line Of Sight.
	 * If <= 0.f, the captured Target won't be traced regularly, but will only be traced while finding a Target.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck", EditConditionHides, Units="s"))
	float LostTargetDelay;

	/** The captured Target will be traced within this interval. If <= 0.f, then will be traced each frame. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck && LostTargetDelay > 0", EditConditionHides, Units="s"))
	float CheckInterval;

	/** Multiply the CaptureRadius in the HelperComponent. May be useful for the progression. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Misc", meta = (UIMin = 0.f, ClampMin = 0.f, Units = "x", EditCondition = "bDistanceCheck", EditConditionHides))
	float TargetCaptureRadiusModifier;

	/** Will be called when any Target's modifier is calculated. Basically used by FGDC_LockOnTarget to simulate the TargetHandler. */
	FOnModifierCalculated OnModifierCalculated;

private: /** Advanced Config */

	//Modifier default value before weights are applied.
	UPROPERTY(Config)
	float PureDefaultModifier;

	//Precision below which the weight won't be taken into account.
	UPROPERTY(Config)
	float WeightPrecision;

	//Distance max factor above which the distance won't be taken into account (in uu).
	UPROPERTY(Config)
	float DistanceMaxFactor;

	//Angle max factor above which the angle won't be taken into account (in deg).
	UPROPERTY(Config)
	float AngleMaxFactor;

	//Due to the best Target should have the least modifier we want to avoid an exact match with the 0.f modifier.
	//e.g. don't capture the most centered Target on the screen at 10'000m while we have a Target in front of us.
	//So, we have something like a free zone (threshold), where several Target will have the same least modifier.
	UPROPERTY(Config)
	float MinimumThreshold;

protected: /** TargetHandlerBase overrides */
	virtual FTargetInfo FindTarget_Implementation(FVector2D PlayerInput) override;
	virtual bool CanContinueTargeting_Implementation() override;
	virtual void HandleTargetEndPlay_Implementation(UTargetingHelperComponent* HelperComponent) override;
	virtual void HandleSocketRemoval_Implementation(FName RemovedSocket) override;

protected: /** LockOnTargetModuleBase overrides */
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket) override;

protected:
	bool HandleTargetClearing(EUnlockReasonBitmask UnlockReason);

protected:
	/** The actual implementation. */
	FTargetInfo FindTargetInternal(FFindTargetContext& TargetContext);
	
	/** Whether the HelperComponent can be captured. Provides only necessary checks. */
	bool IsTargetable(UTargetingHelperComponent* HelperComponent) const;

	/** Can be overridden to provide a custom target check. Provides the CaptureRadius checks by default. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Default Target Handler")
	bool IsTargetableCustom(const UTargetingHelperComponent* HelperComponent) const;

	/** Native implementation of the above. */
	virtual bool IsTargetableCustom_Implementation(const UTargetingHelperComponent* HelperComponent) const;

	/** Finds the best socket within the Target. */
	void FindBestSocket(FTargetModifier& TargetModifier, FFindTargetContext& TargetContext);

	/** Checks whether the modifier should be calculated for the Socket. */
	virtual bool PreModifierCalculationCheck(const FFindTargetContext& TargetContext) const;

	/** Calculates the modifier for the Socket. Will be called multiple times if the Target has multiple sockets. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Default Target Handler")
	float CalculateTargetModifier(const FFindTargetContext& TargetContext) const;

	/** Native implementation of the above. */
	virtual float CalculateTargetModifier_Implementation(const FFindTargetContext& TargetContext) const;

	/** Expensive operations are intended to be here, e.g. Line of Sight is processed here. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Default Target Handler")
	bool PostModifierCalculationCheck(const FFindTargetContext& TargetContext) const;

	/** Native implementation of the above. */
	virtual bool PostModifierCalculationCheck_Implementation(const FFindTargetContext& TargetContext) const;

protected: /** Misc */
	float GetAngleWeight() const;
	bool IsTargetOnScreen(FVector2D ScreenPosition) const;
	FVector2D GetScreenOffset() const;
	void CheckConfigInvariants() const;

protected: /** Line of Sight handling */
	virtual void StartLineOfSightTimer();
	virtual void StopLineOfSightTimer();
	virtual void OnLineOfSightExpiration();
	bool ShouldTargetBeTraced();
	bool LineOfSightTrace(const AActor* const Target, const FVector& Location) const;
	
private:
	FTimerHandle LOSDelayHandler;
	float CheckTimer;

#if WITH_EDITOR
private:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif
};
