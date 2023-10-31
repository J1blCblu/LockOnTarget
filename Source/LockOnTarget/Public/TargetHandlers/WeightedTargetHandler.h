// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "TargetHandlers/TargetHandlerBase.h"
#include "Engine/EngineTypes.h"
#include <type_traits>
#include "WeightedTargetHandler.generated.h"

struct FTargetInfo;
struct FTargetContext;
struct FFindTargetContext;
class UWeightedTargetHandler;
class UTargetComponent;
class ULockOnTargetComponent;
class APlayerController;
class APawn;

/** Target unlock reasons. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETargetUnlockReason : uint8
{
	None				= 0 UMETA(Hidden),

	Destruction			= 1 << 0 UMETA(ToolTip = "Target is being removed from the level. AActor::Destroy(), UTargetComponent::DestroyComponent(), EndPlay()."),
	DistanceFailure		= 1 << 1 UMETA(ToolTip = "Target is out of LostDistance. bDistanceCheck = true."),
	LineOfSightFailure	= 1 << 2 UMETA(ToolTip = "Target has failed to return to Line of Sight. bLineOfSightCheck = true, LostTargetDelay > 0.f."),
	StateInvalidation	= 1 << 3 UMETA(ToolTip = "Target has entered an invalid state. UTargetComponent::SetCanBeCaptured(false)."),
	SocketInvalidation	= 1 << 4 UMETA(ToolTip = "Target has deleted a Socket. UTargetComponent::RemoveSocket(SocketToRemove)."),

	All					= 0b00011111 UMETA(Hidden)
};

constexpr bool IsAnyUnlockReasonSet(std::underlying_type_t<ETargetUnlockReason> Flags, ETargetUnlockReason Contains)
{
	return Flags & static_cast<std::underlying_type_t<ETargetUnlockReason>>(Contains);
}

constexpr ETargetUnlockReason ConvertTargetExceptionToUnlockReason(ETargetExceptionType Exception)
{
	switch(Exception)
	{
	case ETargetExceptionType::Destruction:			return ETargetUnlockReason::Destruction;
	case ETargetExceptionType::StateInvalidation:	return ETargetUnlockReason::StateInvalidation;
	case ETargetExceptionType::SocketInvalidation:	return ETargetUnlockReason::SocketInvalidation;
	default:										return ETargetUnlockReason::None;
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

	//The actual Target.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FTargetInfo Target;

	//World location of the Socket.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FVector Location = FVector::ZeroVector;

	//Direction from ViewLocation.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FVector Direction = FVector::ForwardVector;

	//The Direction2D from the CapturedTarget to this one.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	FVector2D DeltaDirection2D = FVector2D(1.f, 0.f);

	//Squared distance from ViewLocation.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	float DistanceSq = 0.f;

	//Delta angle between PlayerInput and DeltaDirection2D.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	float DeltaAngle2D = 0.f;

	//The weight calculated for the Target.
	UPROPERTY(BlueprintReadOnly, Category = "Target Context")
	float Weight = TNumericLimits<float>::Max();
};

/**
 * Possible modes of the FindTarget context.
 */
UENUM(BlueprintType)
enum class EFindTargetContextMode : uint8
{
	Find	UMETA(ToolTip = "Find a new Target."),
	Switch	UMETA(ToolTip = "Switch the current Target."),
};

/**
 * Contextual data used while finding a Target.
 */
USTRUCT(BlueprintType)
struct LOCKONTARGET_API FFindTargetContext
{
	GENERATED_BODY()

public: /** General */
	
	//Context mode. Some data is only valid for a certain mode.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	EFindTargetContextMode Mode = EFindTargetContextMode::Find;

	//Request params.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FFindTargetRequestParams RequestParams;

	//TargetHandler that created this context.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<UWeightedTargetHandler> TargetHandler = nullptr;

	//LockOnTargetComponent. FindTarget Instigator.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<ULockOnTargetComponent> Instigator = nullptr;

	//Owner of Instigator.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<APawn> InstigatorPawn = nullptr;

	//PlayerController associated with Instigator.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	TObjectPtr<APlayerController> PlayerController = nullptr;

	//Normalized PlayerInput.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector2D PlayerInputDirection = FVector2D(1.f, 0.f);

public: /** Captured Target */

	//Currently captured Target by the Instigator, if one exists.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FTargetContext CapturedTarget;

public: /** View Info */

	//View location for spatial calculations.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector ViewLocation = FVector::ZeroVector;

	//View direction for spatial calculations.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FRotator ViewRotation = FRotator::ZeroRotator;

	//Axes storage.
	FMatrix ViewRotationMatrix;

	//View direction adjusted by ViewPitch/Yaw offsets.
	UPROPERTY(BlueprintReadOnly, Category = "Find Target Context")
	FVector SolverViewDirection = FVector::ForwardVector;
};

/**
 * An optimized TargetHandler subclass based on computing weights for each Target.
 * End users can customize the weights to suit their needs in a convenient way.
 * Weights can be visualized via the custom Gameplay Debugger category.
 *
 * Target finding is performed in 4 main passes:
 * 1. PrimarySampling - quickly rejects all invalid Targets.
 * 2. Solver - calculates weights for remaining Targets.
 * 3. Sort - sorts remaining Targets by weight in ascending order.
 * 4. SecondarySampling - finds the first Target that passes the remaining checks.
 * 
 * Override CalculateTargetWeight() to use custom weight calculation logic.
 * Override ShouldSkipTargetCustom() to add custom rejection logic.
 * 
 * Upon request, all targets with calculated weights can be stored in a detailed response.
 * To retrieve a detailed response, set FFindTargetRequestParams::bGenerateDetailedResponse to true.
 * Cast the payload object from the response to the UWeightedTargetHandlerDetailedResponse.
 * 
 * To enable profiling through Unreal Insights, add the -trace=default,lockontarget channel.
 *
 * @see UTargetHandlerBase, UWeightedTargetHandlerDetailedResponse.
 */ 
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), HideCategories = Tick)
class LOCKONTARGET_API UWeightedTargetHandler : public UTargetHandlerBase
{
	GENERATED_BODY()

public:

	UWeightedTargetHandler();
	static_assert(std::is_same_v<std::underlying_type_t<ETargetUnlockReason>, uint8>, "UWeightedTargetHandler::AutoFindTargetFlags must be of the same type as the EUnlockReason underlying type.");

public: /** Auto Find */

	/** Attempts to automatically find a new Target when a certain flag fails. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Auto Find", meta = (BitMask, BitmaskEnum = "/Script/LockOnTarget.ETargetUnlockReason"))
	uint8 AutoFindTargetFlags;

public: /** Weights */

	/** Increases the influence of the distance to the Target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = 0.f, ClampMax = 1.f, Delta = 0.05f, Units = "x"))
	float DistanceWeight;

	/** Increases the influence of the angle to the Target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = 0.f, ClampMax = 1.f, Delta = 0.05f, Units = "x"))
	float DeltaAngleWeight;

	/** Increases the influence of the player input to the Target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = 0.f, ClampMax = 1.f, Delta = 0.05f, Units = "x"))
	float PlayerInputWeight;

	/** Increases the influence of the Target priority. UTargetComponent::Priority. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weights", meta = (ClampMin = 0.f, ClampMax = 1.f, Delta = 0.05f, Units = "x"))
	float TargetPriorityWeight;

public: /** Solver */

	/** Scales the resulted weight. Can be considered as the maximum weight value. */
	UPROPERTY(EditDefaultsOnly, Category = "Solver", meta = (ClampMin = 10.f))
	float PureDefaultWeight;

	/** Limits the influence of distance. Can increase the impact of DeltaAngleWeight on distant Targets. */
	UPROPERTY(EditDefaultsOnly, Category = "Solver", meta = (ClampMin = 100.f, Units = "cm"))
	float DistanceMaxFactor;

	/** Limits the influence of angle. */
	UPROPERTY(EditDefaultsOnly, Category = "Solver", meta = (ClampMin = 10.f, ClampMax = 180.f, Units = "deg"))
	float DeltaAngleMaxFactor;

	/** The minimum factor value that will be applied to the final weight. Eliminates perfect hits with 0 factor. */
	UPROPERTY(EditDefaultsOnly, Category = "Solver", meta = (ClampMin = 0.f, ClampMax = 1.f, Units = "x"))
	float MinimumFactorThreshold;

public: /** Distance */

	/** Target must be within a certain distance range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance")
	bool bDistanceCheck;

	/**
	 * Radius in which the Target can be captured.
	 * @note Can be customized per Target.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance", meta = (EditCondition = "bDistanceCheck", EditConditionHides, ClampMin = 100.f, Units = "cm"))
	float DefaultCaptureRadius;

	/** Radius [CaptureRadius * LostRadiusScale] in which the captured Target should be released. Helps avoid immediate release at the capture boundary. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance", meta = (EditCondition = "bDistanceCheck", EditConditionHides, ClampMin = 1.f, Units = "x"))
	float LostRadiusScale;

	/** Near clip radius. Rejects closer Targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance", meta = (EditCondition = "bDistanceCheck", EditConditionHides, ClampMin = 0.f, Units = "cm"))
	float NearClipRadius;

	/** Adjusts the final CaptureRadius. Useful for overall scaling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Distance", meta = (EditCondition = "bDistanceCheck", EditConditionHides, ClampMin = 0.f, Units = "x"))
	float CaptureRadiusScale;

public: /** View */

	/** The angle of the cone relative to the view direction within which the Target must be. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (ClampMin = 0.f, ClampMax = 180.f, Delta = 1.f, Units = "deg"))
	float ViewConeAngle;

	/** Transforms the view direction to calculate DeltaAngleWeight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (EditCondition = "DeltaAngleWeight > 0", Units = "Deg"))
	float ViewPitchOffset;

	/** Transforms the view direction to calculate DeltaAngleWeight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (EditCondition = "DeltaAngleWeight > 0", Units = "Deg"))
	float ViewYawOffset;

	/** Target must be successfully projected onto the screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View")
	bool bScreenCapture;

	/** Narrows the screen borders (x and y) from the both sides by a percentage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (EditCondition = "bScreenCapture", EditConditionHides, AllowPreserveRatio, ClampMin = 0.f, ClampMax = 50.f))
	FVector2D ScreenOffset;

	/** Target must be recently rendered. Note: AActor might be rendered even if it isn't seen on the screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View")
	bool bRecentRenderCheck;

	/** How many seconds ago the Target last render time can be and still count as having been "recently" rendered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "View", meta = (ClampMin = 0.f, ClampMax = 0.5f, Delta = 0.05f, EditCondition = "bRecentRenderCheck", Units = "s"))
	float RecentTolerance;

public: /** Target Switching */

	/** Rejects Targets outside this angular range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TargetSwitching", meta = (ClampMin = 0.f, ClampMax = 180.f, Units = "deg"))
	float PlayerInputAngularRange;

public: /** Line Of Sight */

	/** Target must be successfully traced without hitting any objects. The Target and Owner will be ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LineOfSight")
	bool bLineOfSightCheck;
	
	/** Line of Sight collision channel. */
	UPROPERTY(EditDefaultsOnly, Category = "LineOfSight", meta = (EditCondition = "bLineOfSightCheck", EditConditionHides))
	TEnumAsByte<ECollisionChannel> TraceCollisionChannel;

	/** Clears the Target if it wasn't successfully traced during this delay. Skips tracing between updates if <= 0.f. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LineOfSight", meta = (EditCondition = "bLineOfSightCheck", EditConditionHides, Units = "s"))
	float LostTargetDelay;

	/** Captured Target trace interval. Traces each frame if <= 0.f. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "LineOfSight", meta = (EditCondition = "bLineOfSightCheck && LostTargetDelay > 0", EditConditionHides, Units = "s"))
	float CheckInterval;

private: /** Internal */

	FTimerHandle LineOfSightExpirationHandle;
	float LineOfSightCheckTimer;

protected: /** Finding */

	/** The actual FindTarget() implementation. */
	FFindTargetRequestResponse FindTargetBatched(FFindTargetContext& Context);

	/** Quickly rejects all invalid Targets. */
	void PerformPrimarySamplingPass(FFindTargetContext& Context, TArray<FTargetContext>& OutTargetsData);

	/** Whether to skip the Target during the primary pass. */
	bool ShouldSkipTargetPrimaryPass(const FFindTargetContext& Context, const UTargetComponent* Target) const;

	/** Calculates the weight for each Target. */
	void PerformSolverPass(FFindTargetContext& Context, TArray<FTargetContext>& InOutTargetsData);

	/** Calculates the weight for the Target. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|WeightedTargetHandler")
	float CalculateTargetWeight(const FFindTargetContext& Context, const FTargetContext& TargetContext) const;
	virtual float CalculateTargetWeight_Implementation(const FFindTargetContext& Context, const FTargetContext& TargetContext) const;

	/** Finds the first Target that passes the remaining checks. */
	FFindTargetRequestResponse PerformSecondarySamplingPass(FFindTargetContext& Context, TArray<FTargetContext>& InTargetsData);

	/** Whether to skip the Target during the secondary pass. */
	bool ShouldSkipTargetSecondaryPass(const FFindTargetContext& Context, const FTargetContext& TargetContext) const;

	/** Whether to skip the Target during the secondary pass. */
	UFUNCTION(BlueprintNativeEvent, Category = "LockOnTarget|WeightedTargetHandler")
	bool ShouldSkipTargetCustom(const FFindTargetContext& Context, const FTargetContext& TargetContext) const;
	virtual bool ShouldSkipTargetCustom_Implementation(const FFindTargetContext& Context, const FTargetContext& TargetContext) const { return false; };

	/** Generates a detailed response based on the data. */
	virtual UWeightedTargetHandlerDetailedResponse* GenerateDetailedResponse(const FFindTargetContext& Context, TArray<FTargetContext>& InTargetsData);

protected: /** Helpers */

	/** Handles target unlock events. */
	virtual void HandleTargetUnlock(ETargetUnlockReason UnlockReason);

	/** Tries to find a new Target and capture it if found. */
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|WeightedTargetHandler", meta = (BlueprintProtected))
	void TryFindTarget(bool bClearTargetIfFailed = true);

	/** Creates and initially populates FindTargetContext. */
	FFindTargetContext CreateFindTargetContext(EFindTargetContextMode Mode, const FFindTargetRequestParams& RequestParams);

	/** Creates and initially populates TargetContext. */
	FTargetContext CreateTargetContext(const FFindTargetContext& Context, const FTargetInfo& InTarget);

	/** Calculates the delta angle 2D between the player's input and the direction towards the Target. */
	void CalcDeltaAngle2D(const FFindTargetContext& Context, FTargetContext& OutTargetContext) const;

	/** Returns the capture radius for the Target. */
	float GetTargetCaptureRadius(const UTargetComponent* InTarget) const;

	/** Whether the Target is on the screen. */
	bool IsTargetOnScreen(const FFindTargetContext& Context, const FTargetContext& TargetContext) const;

	/** Gets a point of view for spatial calculations like visibility, tracing, distance and etc. */
	UFUNCTION(BlueprintNativeEvent, Category = "Context")
	void GetPointOfView(FVector& OutLocation, FRotator& OutRotation) const;
	virtual void GetPointOfView_Implementation(FVector& OutLocation, FRotator& OutRotation) const;

protected: /** Line Of Sight */

	virtual void StartLineOfSightTimer();
	virtual void StopLineOfSightTimer();
	virtual void OnLineOfSightTimerExpired();
	bool LineOfSightTrace(const FVector& From, const FVector& To, const AActor* const TargetToIgnore) const;

protected: /** Overrides */

	//TargetHandlerBase
	virtual FFindTargetRequestResponse FindTarget_Implementation(const FFindTargetRequestParams& RequestParams) override;
	virtual void CheckTargetState_Implementation(const FTargetInfo& Target, float DeltaTime) override;
	virtual void HandleTargetException_Implementation(const FTargetInfo& Target, ETargetExceptionType Exception) override;

	//LockOnTargetModuleBase
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
};

/**
 * A payload object generated by the UWeightedTargetHandler in response to FFindTargetRequestParams::bGenerateDetailedResponse.
 * Contains the targets data with calculated weights and the context used while finding the Target.
 * 
 * @see UWeightedTargetHandler.
 */
UCLASS(NotBlueprintable, BlueprintType, ClassGroup = (LockOnTarget), HideDropdown, Transient)
class LOCKONTARGET_API UWeightedTargetHandlerDetailedResponse : public UObject
{
	GENERATED_BODY()

public:

	/** The context used while finding the Target. */
	UPROPERTY(BlueprintReadOnly, Category = "DetailedResponse")
	FFindTargetContext Context;

	/** All sampled Targets with weights in ascending order. */
	UPROPERTY(BlueprintReadOnly, Category = "DetailedResponse")
	TArray<FTargetContext> TargetsData;
};
