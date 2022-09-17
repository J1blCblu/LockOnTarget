// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingHelperComponent.generated.h"

class ULockOnTargetComponent;
class FHelperVisualizer;
class UMeshComponent;
class USceneComponent;
class AActor;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOwnerCaptured, class ULockOnTargetComponent*, Invader, FName, Socket);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwnerReleased, class ULockOnTargetComponent*, OldInvader);

/** Determines focus point type. */
UENUM(BlueprintType)
enum class EFocusPoint : uint8
{
	Default			UMETA(ToolTip = "The captured socket's world location will be used."),
	CustomSocket	UMETA(ToolTip = "The custom socket's world location will be used."),
	Custom			UMETA(ToolTip = "The GetCustomFocusPoint() implementation will be called. ")
};

/**
 * Represents the Target that the LockOnTargetComponent can capture in addition to a specified Socket.
 * Has all component's benefits and acts like a Target specific storage.
 * 
 * Network Design:
 * Is not replicated directly. Invaders are added by the LockOnTargetComponent which is replicated.
 * The LockOnTargetComponent finds the Target locally and then notifies the server which Target and Socket are captured.
 * This means that the HelperComponent's state shouldn't be changed on the server, instead a client RPC or replicated systems callbacks should be used.
 * For example, don't set the bCanBeCaptured to false on the server when the Target becomes 'dead'.
 * Mark it with a something like 'health system's (which is probably replicated) OnDead event that is called on all clients.
 * 
 * @see ULockOnTargetComponent, UDefaultTargetHandler.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), HideCategories = (Components, Activation, ComponentTick, Cooking, Sockets, Collision), meta = (BlueprintSpawnableComponent, ChildCannotTick))
class LOCKONTARGET_API UTargetingHelperComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingHelperComponent();
	friend FHelperVisualizer;

public:
	/** 
	 * Can the Target be captured by the LockOnTargetComponent (e.g. register the Target's death by setting the value to false).
	 * The method CanBeCapturedCustom() can be overridden to register a custom capture state (e.g. don't capture teammates).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
	bool bCanBeCaptured;

private:
	/** Provides the LockOnTargetComponent a proper mesh component for finding Sockets and Widget attachment. If None then root component will be used. */
	UPROPERTY(EditAnywhere, Category = "Default Settings", meta = (GetOptions = "GetAvailableMeshes"))
	FName MeshName;

	/** Sockets that the LockOnTargetComponent can capture within the Target. If num == 0 then the Target can't be captured. */
	UPROPERTY(EditAnywhere, Category = "Default Settings", meta = (GetOptions = "GetAvailableSockets"))
	TSet<FName> Sockets;

public:
	/** Target capture radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Radius", meta = (ClampMin = 50.f, UIMin = 50.f, Units="cm"))
	float CaptureRadius;

	/** Target lost radius offset is added to the CaptureRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Radius", meta = (ClampMin = 0.f, UIMin = 0.f, Units="cm"))
	float LostOffsetRadius;

	/** May be helpful to avoid capturing the Target behind the Invader's back (distance is calculated from the camera location). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Radius", meta = (ClampMin = 0.f, UIMin = 0.f, Units="cm"))
	float MinimumCaptureRadius;

public:
	/** Specifies the focus point type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point")
	EFocusPoint FocusPoint;

	/** The custom socket for the focus point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point", meta = (GetOptions = "GetAvailableSockets", EditCondition = "FocusPoint == EFocusPoint::CustomSocket", EditConditionHides))
	FName FocusPointCustomSocket;

	/** Whether the offset will be applied in camera space or world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point")
	bool bFocusPointOffsetInCameraSpace;

	/** Offset applied to the focus location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Point")
	FVector FocusPointOffset;

public:
	/** Whether the Target wants to display any widget. */
	UPROPERTY(EditAnywhere, Category = "Widget")
	uint8 bWantsDisplayWidget : 1;

	/** Whether the Target wants to display the custom Widget. If null then the default one will be used. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bWantsDisplayWidget"))
	TSoftClassPtr<UUserWidget> CustomWidgetClass;

	/** Widget relative offset. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bWantsDisplayWidget"))
	FVector WidgetRelativeOffset;

	/** Notifies when the Target is captured by a LockOnTargetComponetn. */
	UPROPERTY(BlueprintAssignable, Transient, Category = "Targeting Helper")
	FOnOwnerCaptured OnOwnerCaptured;

	/** Notifies when the Target is released by a LockOnTargetComponetn. */
	UPROPERTY(BlueprintAssignable, Transient, Category = "Targeting Helper")
	FOnOwnerReleased OnOwnerReleased;

private:
	/** All Invaders. Usually 1 in standalone. Multiple in Network. */
	UPROPERTY(Transient)
	TSet<ULockOnTargetComponent*> Invaders;

	/** Desired mesh component is used to calculate a socket location, focus location, widget attachment, etc. */
	TWeakObjectPtr<USceneComponent> DesiredMeshComponent;	//Not actually a MeshComponent, cause we may want to store a root component.

	/** 
	 * Whether we need to initialize the desired mesh or not. 
	 * We don't need to use the MeshName in c++, we just can update the mesh via UpdateDesiredMesh() in constructor.
	 * MeshName is used for convenience in BP.
	 */
	uint8 bWantsMeshInitialization : 1;

public: //UActorComponent overrides.
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

public:
	/** Whether the HelperComponent can be captured or not. */
	UFUNCTION(BlueprintCallable, Category = "Targeting Helper")
	bool CanBeCaptured(const ULockOnTargetComponent* Instigator) const;

	/** Called to inform the Target that it's been captured by the LockOnTargetComponent. */
	virtual void CaptureTarget(ULockOnTargetComponent* Instigator, FName Socket);

	/** Called to inform the Target that it's been released by the LockOnTargetComponent. */
	virtual void ReleaseTarget(ULockOnTargetComponent* Instigator);

	/** Returns the Socket world location within the Target's DesiredMesh. */
	UFUNCTION(BlueprintPure, Category = "Targeting Helper")
	FVector GetSocketLocation(FName Socket) const;

	/** Returns the focus location. Mostly used by tracking systems. */
	UFUNCTION(BlueprintPure, Category = "Targeting Helper")
	FVector GetFocusLocation(const ULockOnTargetComponent* Instigator) const;

	/** Returns the DesiredMesh. */
	UFUNCTION(BlueprintPure, Category = "Targeting Helper")
	USceneComponent* GetDesiredMesh() const;

	/** Get all the Invaders that have captured the Target. Usually 1 in a standalone game. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	const TSet<ULockOnTargetComponent*>& GetInvaders() const { return Invaders;}

	/** Whether the Target is captured by any Invader.*/
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	bool IsCaptured() const { return Invaders.Num() > 0; }

	/** Get all sockets. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	const TSet<FName>& GetSockets() const { return Sockets; }

	/** Add a Socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper", meta = (AutoCreateRefTerm = "Socket"))
	bool AddSocket(FName Socket = NAME_None);

	/** Remove a socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper", meta = (AutoCreateRefTerm = "Socket"))
	bool RemoveSocket(FName Socket = NAME_None);

	/** Update the DesiredMesh. Which is mostly used in socket location calculations. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	void UpdateDesiredMesh(UMeshComponent* NewMeshComponent);

protected:
	USceneComponent* GetRootComponent() const;

protected: /** BP */

	/** Override to provide custom 'can be captured' rules (e.g. don't capture teammates). */
	UFUNCTION(BlueprintNativeEvent, Category = "Targeting Helper")
	bool CanBeCapturedCustom(const ULockOnTargetComponent* Instigator) const;

	virtual bool CanBeCapturedCustom_Implementation(const ULockOnTargetComponent* Instigator) const;

	/** Override to get a custom Focus point world location. */
	UFUNCTION(BlueprintNativeEvent, Category = "Targeting Helper")
	FVector GetCustomFocusPoint(const ULockOnTargetComponent* Instigator) const;

	virtual FVector GetCustomFocusPoint_Implementation(const ULockOnTargetComponent* Instigator) const;

	/** Called when the Target is captured. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TargetingHelper", meta = (DisplayName = "On Target Captured"))
	void K2_OnCaptured(const ULockOnTargetComponent* Instigator, FName Socket);

	/** Called when the Target is released. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TargetingHelper", meta = (DisplayName = "On Target Released"))
	void K2_OnReleased(const ULockOnTargetComponent* Instigator);

#if WITH_EDITOR
	UFUNCTION()
	virtual TArray<FString> GetAvailableSockets() const;

	UFUNCTION()
	virtual TArray<FString> GetAvailableMeshes() const;
#endif
};
