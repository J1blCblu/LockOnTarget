// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetTypes.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetComponent.generated.h"

class ULockOnTargetComponent;
class UTargetComponent;
class USceneComponent;
class UTargetManager;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnOwnerCaptured, UTargetComponent, OnCaptured, class ULockOnTargetComponent*, Instigator);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnOwnerReleased, UTargetComponent, OnReleased, class ULockOnTargetComponent*, Instigator);

/** Determines focus point type. */
UENUM(BlueprintType)
enum class EFocusPoint : uint8
{
	CapturedSocket	UMETA(ToolTip = "Captured socket world location will be used."),
	CustomSocket	UMETA(ToolTip = "Custom socket world location will be used."),
	Custom			UMETA(ToolTip = "GetCustomFocusPoint() will be called. ")
};

/**
 * Represents a Target that ULockOnTargetComponent can capture in conjunction with a Socket.
 * It is kind of a dumping ground for anything LockOnTarget subsystems may need.
 * Has a special FocusPoint concept for tracking systems.
 * 
 * @Note: Not replicated. Take this into account when changing its state.
 * 
 * @see ULockOnTargetComponent.
 */
UCLASS(Blueprintable, ClassGroup = LockOnTarget, HideCategories = (Components, Activation, ComponentTick, Cooking, Sockets, Collision), meta = (BlueprintSpawnableComponent, ChildCannotTick))
class LOCKONTARGET_API UTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UTargetComponent();
	friend class FTargetComponentDetails; //Details customization.
	static constexpr uint32 NumInlinedInvaders = 1;
	UTargetManager& GetTargetManager() const;

private: /** Core Config */

	/** Can the Target be captured by ULockOnTargetComponent. */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	bool bCanBeCaptured;

	/** Specifies the TrackedMeshComponent by name, which is used to find Sockets, attach a Widget, etc. If 'None' then the root component will be used. */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	FName TrackedMeshName;

	/** Sockets that ULockOnTargetComponent can capture. Num should be > 0. */
	UPROPERTY(EditAnywhere, Category = "Default Settings", meta = (NoElementDuplicate, EditFixedOrder, DisplayName = "Sockets Data"))
	TArray<FName> Sockets;

public: /** Capture Radius */

	/** Radius in which the Target can be captured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Radius", meta = (ClampMin = 50.f, UIMin = 50.f, Units = "cm"))
	float CaptureRadius;

	/** LostRadius (CaptureRadius + LostOffsetRadius) in which the captured Target should be released. Helps avoid immediate release at the capture boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Radius", meta = (ClampMin = 0.f, UIMin = 0.f, Units = "cm"))
	float LostOffsetRadius;

public: /** Focus Point */

	/** Specifies FocusPoint type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point")
	EFocusPoint FocusPoint;

	/** Specifies a custom socket for FocusPoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point", meta = (EditCondition = "FocusPoint == EFocusPoint::CustomSocket", EditConditionHides))
	FName FocusPointCustomSocket;

	/** FocusPoint offset in global space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point")
	FVector FocusPointOffset;

public: /** Widget */

	/** Whether the Target wants to display any widget. */
	UPROPERTY(EditAnywhere, Category = "Widget")
	bool bWantsDisplayWidget;

	/** Custom Widget class. If null, then the default one should be used. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bWantsDisplayWidget"))
	TSoftClassPtr<UUserWidget> CustomWidgetClass;

	/** Widget relative offset. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bWantsDisplayWidget"))
	FVector WidgetRelativeOffset;

public: /** Callbacks */

	/** Called if the Target has been successfully captured by ULockOnTargetComponent. */
	UPROPERTY(BlueprintAssignable, Category = "Target")
	FOnOwnerCaptured OnCaptured;

	/** Called if the Target has been successfully released by ULockOnTargetComponent. */
	UPROPERTY(BlueprintAssignable, Category = "Target")
	FOnOwnerReleased OnReleased;

private: /** Internal */

	//LockOnTargetComponents that captures the Target.
	TArray<ULockOnTargetComponent*, TInlineAllocator<NumInlinedInvaders>> Invaders;

	//Not actually a UMeshComponent, cause we might want to store the root component.
	TWeakObjectPtr<USceneComponent> TrackedMeshComponent;

	//Should we skip the TrackedMeshComponent initialization by name.
	uint8 bSkipMeshInitializationByName : 1;

public: /** Target State */

	/** Can the Target be captured by ULockOnTargetComponent. */
	UFUNCTION(BlueprintCallable, Category = "Target")
	bool CanBeCaptured() const;

	bool CanBeReferencedOverNetwork() const;

	/** Updates the capture state of the Target. If false, ULockOnTargetComponent will clear the Target. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	void SetCanBeCaptured(bool bInCanBeCaptured);

	/** Whether the Target is captured by any ULockOnTargetComponent. */
	UFUNCTION(BlueprintPure, Category = "Target")
	bool IsCaptured() const;

	/** Gets all ULockOnTargetsComponents that have captured the Target. Usually 1 in a standalone game. */
	UFUNCTION(BlueprintPure, Category = "Target")
	TArray<ULockOnTargetComponent*> GetInvaders() const;

	/** Called to inform the Target that it's been captured by ULockOnTargetComponent. */
	virtual void CaptureTarget(ULockOnTargetComponent* Instigator);

	/** Called if the Target has been successfully captured by ULockOnTargetComponent. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TargetingHelper", meta = (DisplayName = "On Target Captured"))
	void K2_OnCaptured(const ULockOnTargetComponent* Instigator);

	/** Called to inform the Target that it's been released by ULockOnTargetComponent. */
	virtual void ReleaseTarget(ULockOnTargetComponent* Instigator);

	/** Called if the Target has been successfully released by ULockOnTargetComponent. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TargetingHelper", meta = (DisplayName = "On Target Released"))
	void K2_OnReleased(const ULockOnTargetComponent* Instigator);

	//Dispatch an exception/interrupt message from the Target to the Invaders.
	void DispatchTargetException(ETargetExceptionType Exception);

public: /** Sockets */

	/** Does the given Socket exist in the Target. */
	UFUNCTION(BlueprintPure, Category = "Target")
	bool IsSocketValid(FName Socket) const;

	/** Gets all Sockets of the Target. */
	UFUNCTION(BlueprintPure, Category = "Target")
	const TArray<FName>& GetSockets() const { return Sockets; }

	/** Returns the world location of the given Socket. */
	UFUNCTION(BlueprintPure, Category = "Target")
	FVector GetSocketLocation(FName Socket) const;

	/** Adds a Socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper", meta = (AutoCreateRefTerm = "Socket"))
	bool AddSocket(FName Socket = NAME_None);

	/** Removes a Socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper", meta = (AutoCreateRefTerm = "Socket"))
	bool RemoveSocket(FName Socket = NAME_None);

public: /** Focus Point */

	/** Returns the FocusPoint location for ULockOnTargetComponent. Mostly used by tracking systems. */
	UFUNCTION(BlueprintPure, Category = "Target")
	FVector GetFocusLocation(const ULockOnTargetComponent* Instigator) const;

	/** Override to get a custom FocusPoint location. */
	UFUNCTION(BlueprintNativeEvent, Category = "Targeting Helper")
	FVector GetCustomFocusPoint(const ULockOnTargetComponent* Instigator) const;
	virtual FVector GetCustomFocusPoint_Implementation(const ULockOnTargetComponent* Instigator) const;

public: /** Tracked Mesh */

	/** Returns TrackedMeshComponent. */
	UFUNCTION(BlueprintPure, Category = "Target")
	USceneComponent* GetTrackedMeshComponent() const;

	/** Updates the TrackedMeshComponent. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	void SetTrackedMeshComponent(USceneComponent* InTrackedComponent);

protected: /** Helpers */

	USceneComponent* FindMeshComponent() const;
	USceneComponent* GetRootComponent() const;

public: /** Overrides */

	//UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
};
