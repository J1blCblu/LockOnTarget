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

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnTargetComponentCaptured, UTargetComponent, OnTargetComponentCaptured, class ULockOnTargetComponent*, Instigator);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnTargetComponentReleased, UTargetComponent, OnTargetComponentReleased, class ULockOnTargetComponent*, Instigator);

/** Determines focus point type. */
UENUM(BlueprintType)
enum class ETargetFocusPointType : uint8
{
	CapturedSocket	UMETA(ToolTip = "Captured socket world location will be used."),
	CustomSocket	UMETA(ToolTip = "Custom socket world location will be used."),
	Custom			UMETA(ToolTip = "GetCustomFocusPoint() will be called. ")
};

/**
 * TargetComponent gives LockOnTargetComponent the ability to capture it with one of available sockets.
 * Can be used as a storage for the Target specific data.
 * Has a special 'focus point' concept for tracking systems.
 * 
 * There is a small and simple TargetManager keeps track of registered Targets in game.
 * 
 * @Note: Not replicated. Take this into account when changing the internal state.
 * 
 * @see ULockOnTargetComponent.
 */
UCLASS(Blueprintable, ClassGroup = LockOnTarget, HideCategories = (Components, Activation, ComponentTick, Cooking, Sockets, Collision), meta = (BlueprintSpawnableComponent, ChildCannotTick))
class LOCKONTARGET_API UTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	//@TODO: Display a warning next to invalid sockets after changing AssociatedComponentName in details. Or maybe clear directly.

	UTargetComponent();
	friend class FTargetComponentDetails; //Details customization.
	static constexpr uint32 NumInlinedInvaders = 3;
	UTargetManager& GetTargetManager() const;

private: /** General */

	/** Whether the Target can be captured. SetCanBeCaptured(). */
	UPROPERTY(EditAnywhere, Category = "General")
	bool bCanBeCaptured;

	/** Specifies the associated component by name used for Socket lookup, attachment and etc. If 'None' then the root component will be used. */
	UPROPERTY(EditAnywhere, Category = "General")
	FName AssociatedComponentName;

	/** Sockets to capture. Add/RemoveSocket(). */
	UPROPERTY(EditAnywhere, Category = "General", meta = (EditFixedOrder, DisplayName = "Sockets Data", NoResetToDefault))
	TArray<FName> Sockets;

public: /** General */

	/** Whether to use the default capture radius or custom. */
	UPROPERTY(EditAnywhere, Category = "General", meta = (InlineEditConditionToggle))
	bool bForceCustomCaptureRadius;

	/** Radius in which the Target can be captured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta = (EditCondition = "bForceCustomCaptureRadius", ClampMin = 100.f, Units = "cm"))
	float CustomCaptureRadius;

	/** 0 - higher priority, 1 - lower priority. Some animals and bosses may need lower and higher priority respectively. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta = (ClampMin = 0.f, ClampMax = 1.f, Units = "x"))
	float Priority;

public: /** Focus Point */

	/** Specifies the FocusPoint type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point")
	ETargetFocusPointType FocusPointType;

	/** Specifies a custom socket for FocusPoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point", meta = (EditCondition = "FocusPointType == ETargetFocusPointType::CustomSocket", EditConditionHides))
	FName FocusPointCustomSocket;

	/** FocusPoint offset in actor space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point")
	FVector FocusPointRelativeOffset;

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

	/** Called if the Target has been successfully captured by LockOnTargetComponent. */
	UPROPERTY(BlueprintAssignable, Category = "Target|Delegates")
	FOnTargetComponentCaptured OnTargetComponentCaptured;

	/** Called if the Target has been successfully released by LockOnTargetComponent. */
	UPROPERTY(BlueprintAssignable, Category = "Target|Delegates")
	FOnTargetComponentReleased OnTargetComponentReleased;

private:

	//LockOnTargetComponents that are currently capturing the Target.
	TArray<ULockOnTargetComponent*, TInlineAllocator<NumInlinedInvaders>> Invaders;

	//The component used for Socket lookup, attachment and etc.
	TWeakObjectPtr<USceneComponent> AssociatedComponent;

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
	bool IsCaptured() const { return GetInvadersNum() > 0; }

	/** Gets all ULockOnTargetsComponents that have captured the Target. Usually 1 in a standalone game. */
	UFUNCTION(BlueprintPure, Category = "Target")
	TArray<ULockOnTargetComponent*> GetInvaders() const { return TArray<ULockOnTargetComponent*>(Invaders); }

	/** Gets all ULockOnTargetsComponents that have captured the Target. Usually 1 in a standalone game. */
	UFUNCTION(BlueprintPure, Category = "Target")
	int32 GetInvadersNum() const { return Invaders.Num(); }

public: /** Associated Component */

	/** Returns the associated component. */
	UFUNCTION(BlueprintPure, Category = "Target|Mesh")
	USceneComponent* GetAssociatedComponent() const { return AssociatedComponent.IsValid() ? AssociatedComponent.Get() : nullptr; }

	/** 
	 * Updates the associated component.
	 * @Note: Existing sockets aren't verified.
	 */
	UFUNCTION(BlueprintCallable, Category = "Target|Mesh")
	void SetAssociatedComponent(USceneComponent* InAssociatedComponent);

public: /** Sockets */

	/** Does the given Socket exist in the Target. */
	UFUNCTION(BlueprintPure, Category = "Target")
	bool IsSocketValid(FName Socket) const { return Sockets.Contains(Socket); };

	/** Returns all available Sockets. */
	UFUNCTION(BlueprintPure, Category = "Target")
	const TArray<FName>& GetSockets() const { return Sockets; }

	/** Returns the world location of the given Socket. */
	UFUNCTION(BlueprintPure, Category = "Target")
	FVector GetSocketLocation(FName Socket) const;

	/** Updates the default Socket in 0 index. */
	UFUNCTION(BlueprintCallable, Category = "Target", meta = (AutoCreateRefTerm = "Socket"))
	void SetDefaultSocket(FName Socket = NAME_None);

	/** Updates the default Socket in 0 index. */
	UFUNCTION(BlueprintCallable, Category = "Target", meta = (AutoCreateRefTerm = "Socket"))
	FName GetDefaultSocket() const { return Sockets.IsEmpty() ? NAME_None : Sockets[0]; }

	/** Adds a Socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Target", meta = (AutoCreateRefTerm = "Socket"))
	bool AddSocket(FName Socket = NAME_None);

	/** Removes a Socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Target", meta = (AutoCreateRefTerm = "Socket"))
	bool RemoveSocket(FName Socket = NAME_None);

public: /** Focus Point */

	/** Returns the 'focus point' location for ULockOnTargetComponent. Mostly used by tracking systems. */
	UFUNCTION(BlueprintPure, Category = "Target|FocusPoint")
	FVector GetFocusPointLocation(const ULockOnTargetComponent* Instigator) const;

	/** Override to get a custom FocusPoint location. */
	UFUNCTION(BlueprintNativeEvent, Category = "Target|FocusPoint")
	FVector GetCustomFocusPoint(const ULockOnTargetComponent* Instigator) const;
	virtual FVector GetCustomFocusPoint_Implementation(const ULockOnTargetComponent* Instigator) const;

public: /** Communication */

	/** Called to inform the Target that it's been captured by ULockOnTargetComponent. */
	virtual void NotifyTargetCaptured(ULockOnTargetComponent* Instigator);

	/** Called to inform the Target that it's been captured by ULockOnTargetComponent. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Target", meta = (DisplayName = "On Target Captured"))
	void K2_OnCaptured(const ULockOnTargetComponent* Instigator);

	/** Called to inform the Target that it's been released by ULockOnTargetComponent. */
	virtual void NotifyTargetReleased(ULockOnTargetComponent* Instigator);

	/** Called to inform the Target that it's been released by ULockOnTargetComponent. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Target", meta = (DisplayName = "On Target Released"))
	void K2_OnReleased(const ULockOnTargetComponent* Instigator);

	//Dispatch an exception/interrupt message to the Invaders.
	void DispatchTargetException(ETargetExceptionType Exception);

public: /** Overrides */

	//UActorComponent
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
};
