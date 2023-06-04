// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "TargetHandlers/TargetHandlerBase.h"
#include "Engine/EngineTypes.h"
#include <type_traits>
#include "ThirdPersonTargetHandler.generated.h"

struct FTargetInfo;
struct FTargetContext;
struct FFindTargetContext;
class UThirdPersonTargetHandler;
class UTargetComponent;
class ULockOnTargetComponent;
class APlayerController;
class APawn;

/** Target unlock reasons. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EUnlockReason : uint8
{
	None				= 0 UMETA(Hidden),

	Destruction			= 1 << 0 UMETA(ToolTip = "Target is being removed from the level. AActor::Destroy(), UTargetComponent::DestroyComponent(), EndPlay()."),
	DistanceFail		= 1 << 1 UMETA(ToolTip = "Target is out of LostDistance. bDistanceCheck = true."),
	LineOfSightFail		= 1 << 2 UMETA(ToolTip = "Target has failed to return to Line of Sight. bLineOfSightCheck = true, LostTargetDelay > 0.f."),
	StateInvalidation	= 1 << 3 UMETA(ToolTip = "Target has entered an invalid state. UTargetComponent::SetCanBeCaptured(false)."),
	SocketInvalidation	= 1 << 4 UMETA(ToolTip = "Target has deleted a Socket. UTargetComponent::RemoveSocket(SocketToRemove)."),

	All					= 0b00011111 UMETA(Hidden)
};

constexpr bool IsAnyUnlockReasonSet(std::underlying_type_t<EUnlockReason> Flags, EUnlockReason Contains)
{
	return Flags & static_cast<std::underlying_type_t<EUnlockReason>>(Contains);
}

constexpr EUnlockReason ConvertTargetExceptionToUnlockReason(ETargetExceptionType Exception)
{
	switch(Exception)
	{
	case ETargetExceptionType::Destruction:			return EUnlockReason::Destruction;
	case ETargetExceptionType::StateInvalidation:	return EUnlockReason::StateInvalidation;
	case ETargetExceptionType::SocketInvalidation:	return EUnlockReason::SocketInvalidation;
	default:										return EUnlockReason::None;
	}
}

/**
 * Holds cached contextual information about the Target.
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FTargetContext
{
	GENERATED_BODY()

public:

	//Intentionally implicit.
	operator FTargetInfo() const
	{
		return { Target, Socket };
	}

public:

	//TargetComponent associated with this context.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	TObjectPtr<UTargetComponent> Target = nullptr;

	//Socket associated with the Target.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FName Socket = NAME_None;

	//World location of the Socket.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FVector Location = FVector::ZeroVector;

	//Vector to Socket location from ViewLocation.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FVector VectorToSocket = FVector::ForwardVector;

	//Normalized VectorToSocket.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FVector Direction = FVector::ForwardVector;
};

/**
 * Two possible modes of FindTargetContext.
 */
UENUM(BlueprintType)
enum class EContextMode : uint8
{
	Find	UMETA(ToolTip="Find a new Target."),
	Switch	UMETA(ToolTip="Switch the current Target.")
};

/**
 * FindTarget context information.
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FFindTargetContext
{
	GENERATED_BODY()

public:

	//TargetHandler that created this context.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<UThirdPersonTargetHandler> TargetHandler = nullptr;

	//LockOnTargetComponent. FindTarget Instigator.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<ULockOnTargetComponent> Instigator = nullptr;

	//Owner of Instigator.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<APawn> InstigatorPawn = nullptr;

	//PlayerController associated with Instigator.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<APlayerController> PlayerController = nullptr;

	//Passed data from the Player. By default only valid in Switch contexts.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector2D PlayerInput = FVector2D::ZeroVector;

	//Normalized PlayerInput. By default only valid in Switch contexts.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector2D PlayerInputDirection = FVector2D(1.f, 0.f);

	//Currently captured Target by Instigator. Only valid in Switch contexts.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FTargetContext CapturedTarget;

	//Iterative Target context. Treat as a candidate that can potentially be a new Target.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FTargetContext IteratorTarget;

	//View location for spatial calculations.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector ViewLocation = FVector::ZeroVector;

	//View direction for spatial calculations.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector ViewDirection = FVector::ForwardVector;

	//View direction adjusted by ViewRotationOffset.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector ViewDirectionWithOffset = FVector::ForwardVector;

	//Delta angle between PlayerInput and Direction2D between Targets. Only valid in Switch contexts.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	float DeltaAngle2D = 0.f;

	//Context mode. Some data is only valid for a certain mode.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	EContextMode Mode = EContextMode::Find;
};

/**
 * Special optimized TargetHandler, mainly designed for Third-Person games, based on the calculation and comparison of Target modifiers.
 * The best Target will have the least modifier. Targets with several sockets will have several modifiers.
 * All helpful information for Target finding is stored/cached inside FindTargetContext.
 * 
 * Basic FindTarget execution flow:
 * |FindTargetInternal() - Iterates over TargetComponents (hereinafter a Target).
 * |   |IsTargetable() - Rejects all invalid Targets.
 * |   |IsTargetableCustom() - Custom chance to reject the Target.
 * |   |FindBestSocket() - Finds the best Socket by comparing modifiers.
 * |   |   |PreModifierCalculation() - Early chance to reject the Socket.
 * |   |   |CalculateModifier() - Calculates the modifier for the Socket.
 * |   |   |PostModifierCalculation() - Last possible chance to reject the Socket.
 *
 * @see UTargetHandlerBase.
 */ 
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), Config=Game, DefaultConfig)
class LOCKONTARGET_API UThirdPersonTargetHandler : public UTargetHandlerBase
{
	GENERATED_BODY()

public:

	UThirdPersonTargetHandler();
	friend class FGDC_LockOnTarget; //Gameplay Debugger.
	static_assert(std::is_same_v<std::underlying_type_t<EUnlockReason>, uint8>, "UThirdPersonTargetHandler::AutoFindTargetFlags must be of the same type as the EUnlockReason underlying type.");
	using FTargetModifier = TPair<FTargetInfo, float>; //Holds a modifier associated with the Target.

public: /** Auto Find */

	/** Attempts to automatically find a new Target when a certain flag fails. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Auto Find", meta = (BitMask, BitmaskEnum = "/Script/LockOnTarget.EUnlockReason"))
	uint8 AutoFindTargetFlags;

public: /** Weights */

	/** Increases the influence of the distance to the Target. */
	UPROPERTY(EditDefaultsOnly, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Delta = 0.05f, Units = "x"))
	float DistanceWeight;

	/** Increases the influence of the angle to the Target. */
	UPROPERTY(EditDefaultsOnly, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Delta = 0.05f, Units = "x"))
	float AngleWeight;

	/** Increases the influence of the player's input to the Target (while any Target is locked). */
	UPROPERTY(EditDefaultsOnly, Category = "Default Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, UIMin = 0.f, UIMax = 1.f, Delta = 0.05f, Units = "x"))
	float PlayerInputWeight;

public: /** Solver */

	/** Modifier default value before weights are applied. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Advanced Solver", meta = (UIMin = 0.f, ClampMin = 0.f))
	float PureDefaultModifier;

	/** Precision below which the weight won't be taken into account. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Advanced Solver", meta = (UIMin = 0.f, ClampMin = 0.f, UIMax = 1.f, ClampMax = 1.f, Units = "x"))
	float WeightPrecision;

	/** Distance max factor above which the distance won't be taken into account (in uu). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Advanced Solver", meta = (UIMin = 0.f, ClampMin = 0.f, Units = "cm"))
	float DistanceMaxFactor;

	/** Angle max factor above which the angle won't be taken into account (in deg). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Advanced Solver", meta = (UIMin = 0.f, ClampMin = 0.f, UIMax = 180.f, ClampMax = 180.f, Units = "deg"))
	float AngleMaxFactor;

	/** Minimum factor value to be applied to the modifier. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Advanced Solver", meta = (UIMin = 0.f, ClampMin = 0.f, UIMax = 1.f, ClampMax = 1.f, Units = "x"))
	float MinimumThreshold;

public: /** Distance */

	/** Target must be within a certain distance range. UTargetComponent::CaptureRadius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance")
	bool bDistanceCheck;

	/** Closer Targets will be ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance", meta = (EditCondition = "bDistanceCheck", EditConditionHides, ClampMin = 0.f, UIMin = 0.f, Units = "cm"))
	float MinimumRadius;

	/** Adjusts CaptureRadius of the Target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance", meta = (UIMin = 0.f, ClampMin = 0.f, Units = "x", EditCondition = "bDistanceCheck", EditConditionHides))
	float TargetCaptureRadiusModifier;

public: /** View */

	/** Tries to use the camera's point of view, otherwise the owner's one (AActor world location). UThirdPersonTargetHandler::GetPointOfView() can be overriden. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View")
	bool bUseCameraPointOfView;

	/** Target must be successfully projected onto the screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View")
	bool bScreenCapture;

	/** Narrows the screen borders (x and y) from the both sides by a percentage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (EditCondition = "bScreenCapture", EditConditionHides, AllowPreserveRatio, ClampMin = 0.f, ClampMax = 50.f, UIMin = 0.f, UIMax = 50.f))
	FVector2D ScreenOffset;

	/** The angle of the cone from the view direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, Delta = 1.f, EditCondition = "!bScreenCapture", EditConditionHides, Units = "deg"))
	float ViewAngle;

	/** Can be used to transform the ViewDirection to adjust the angle ratio distribution, e.g., the closest Target to the resulted direction will be captured instead of the most centered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (EditCondition = "AngleWeight > 0"))
	FRotator ViewRotationOffset;

	/** Target must be rendered. Note: AActor might be rendered even if it isn't seen on the screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (InlineEditConditionToggle))
	bool bRecentRenderCheck;

	/** Target must be rendered within this interval. Note: AActor might be rendered even if it isn't seen on the screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (ClampMin = 0.f, UIMin = 0.f, ClampMax = 0.5f, UIMax = 0.5f, Delta = 0.05f, EditCondition = "bRecentRenderCheck", Units = "s"))
	float RecentTolerance;

public: /** Target Switching */

	/** Targets within this angle range will be processed while switching. Added to both sides of the player's input direction (in screen space). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target Switching", meta = (ClampMin = 0.f, ClampMax = 180.f, UIMin = 0.f, UIMax = 180.f, Units = "deg"))
	float AngleRange;

public: /** Line Of Sight */

	/** Target must be successfully traced. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Line Of Sight")
	bool bLineOfSightCheck;

	/** Object channels to trace. If the trace hits something then the Line of Sight fails. Target and Owner will be ignored. */
	UPROPERTY(EditDefaultsOnly, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck", EditConditionHides))
	TArray<TEnumAsByte<ECollisionChannel>> TraceObjectChannels;

	/** 
	 * Timer Delay on expiration of which the Target will be unlocked. Timer stops if the Target returns to the Line Of Sight.
	 * If <= 0.f, the captured Target won't be traced regularly, but will only be traced while finding a Target.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck", EditConditionHides, Units = "s"))
	float LostTargetDelay;

	/** Captured Target trace interval. If <= 0.f, then will trace each frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Line Of Sight", meta = (EditCondition = "bLineOfSightCheck && LostTargetDelay > 0", EditConditionHides, Units = "s"))
	float CheckInterval;

public: /** Callbacks */

	/** Will be called when any Target's modifier is calculated. Basically used by FGDC_LockOnTarget to visualize Targets modifiers. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnModifierCalculated, const struct FFindTargetContext& /*TargetContext*/, float /*Modifier*/);
	FOnModifierCalculated OnModifierCalculated;

private: /** Internal */
	
	FTimerHandle LineOfSightExpirationHandle;
	float LineOfSightCheckTimer;

protected: /** Finding */

	/** Tries to find a new Target and passes it to LockOnTargetComponent. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|Third Person Target Handler", meta = (BlueprintProtected))
	void TryFindAndSetNewTarget(bool bClearTargetIfFailed = true);

	/** The actual FindTarget implementation. */
	FTargetInfo FindTargetInternal(FFindTargetContext& TargetContext);
	
	/** Mandatory early chance to deflect the Target. */
	bool IsTargetable(const FFindTargetContext& TargetContext) const;

	/** Rejects all invalid Targets. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Third Person Target Handler")
	bool IsTargetableCustom(const UTargetComponent* TargetComponent) const;
	virtual bool IsTargetableCustom_Implementation(const UTargetComponent* TargetComponent) const;

	/** Finds the best Socket by comparing modifiers. */
	void FindBestSocket(FTargetModifier& BestTarget, FFindTargetContext& TargetContext);

	/** Rejects the Target after processing the Socket. */
	virtual bool PreModifierCalculationCheck(const FFindTargetContext& TargetContext) const;

	/** Calculates the modifier for the Socket. Will be called multiple times if the Target has multiple sockets. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Third Person Target Handler")
	float CalculateTargetModifier(const FFindTargetContext& TargetContext) const;
	virtual float CalculateTargetModifier_Implementation(const FFindTargetContext& TargetContext) const;

	/** Last possible chance to reject the Socket. Expensive operations should be here. Some checks might spend more CPU time than calculating the modifier. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|Third Person Target Handler")
	bool PostModifierCalculationCheck(const FFindTargetContext& TargetContext) const;
	virtual bool PostModifierCalculationCheck_Implementation(const FFindTargetContext& TargetContext) const;

protected: /** Helpers */

	/** Creates and initially populates FindTargetContext. */
	FFindTargetContext CreateFindTargetContext(EContextMode Mode, FVector2D Input = FVector2D(0.f));

	/** Populates TargetContext. */
	void PrepareTargetContext(const FFindTargetContext& FindContext, FTargetContext& OutTargetContext, FName InSocket);

	/** Updates the Context after filling the TargetContext. */
	void UpdateContext(FFindTargetContext& Context);

	bool IsTargetOnScreen(const APlayerController* const PlayerController, FVector2D ScreenPosition) const;

	/** Gets a point of view for spatial calculations like visibility, tracing, distance and etc. */
	UFUNCTION(BlueprintNativeEvent, Category = "Context")
	void GetPointOfView(FVector& OutLocation, FVector& OutDirection) const;
	virtual void GetPointOfView_Implementation(FVector& OutLocation, FVector& OutDirection) const;

protected: /** Line Of Sight */

	virtual void StartLineOfSightTimer();
	virtual void StopLineOfSightTimer();
	virtual void OnLineOfSightExpiration();
	bool LineOfSightTrace(const FVector& From, const FVector& To, const AActor* const TargetToIgnore) const;

protected: /** Overrides */

	//TargetHandlerBase
	virtual FTargetInfo FindTarget_Implementation(FVector2D PlayerInput) override;
	virtual void CheckTargetState_Implementation(const FTargetInfo& Target, float DeltaTime) override;
	virtual void HandleTargetException_Implementation(const FTargetInfo& Target, ETargetExceptionType Exception) override;

	//LockOnTargetModuleBase
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
};
