// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"

#include "Utilities/Enums.h"

#include "TargetingHelperComponent.generated.h"

class ULockOnTargetComponent;
class FHelperVisualizer;
class UMeshComponent;
class USceneComponent;
class AActor;
class UWidgetComponent;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOwnerCaptured, ULockOnTargetComponent*, Invader, const FName&, Socket);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwnerReleased, ULockOnTargetComponent*, OldInvader);

/**
 * Used by the LockOnTargetComponent to capture and provide specific information about the Target.
 * 
 * Some features:
 *  - Can be added to any AActor subclass (also during runtime).
 *	- Customizable settings for each Target.
 *	- Targets can have multiple sockets (e.g. head, body, calf, etc.).
 *  - Add custom rules for capturing the target (override the CanBeTargeted() method, e.g. don't capture 'allies').
 *  - Add and remove sockets at runtime.
 *  - Determine the mesh component by name.
 *  - Various useful information and methods that may be needed for the Target.
 *  - Implicitly adds a UWidgetComponent (use the GetWidgetComponent() to access it).
 * 
 * Is not replicated directly. Invaders are added by the LockOnTargetComponent which is replicated.
 * Also, the LockOnTargetComponent finds the Target locally and then notifies the server which Target and Sockets are captured.
 * Due to this every state update must be done using RPCs or leave the Component state static.
 * 
 * @see ULockOnTargetComponent.
 */
UCLASS(Blueprintable, ClassGroup = (LockOnTarget), HideCategories = (Components, Activation, ComponentTick, Cooking, Sockets, Collision), meta = (BlueprintSpawnableComponent))
class LOCKONTARGET_API UTargetingHelperComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingHelperComponent();
	friend FHelperVisualizer;

protected:
	// UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void InitializeComponent() override;

public:
	/** 
	 * Can the owner be captured by the LockOnTargetComponent.
	 * (e.g. registering the owner's death without destroying it by GC).
	 * 
	 * Also, the method CanBeTargeted() can be overridden to register a custom capture state.
	 * (e.g. register 'team system'. Don't capture 'allies').
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
	bool bCanBeTargeted;

private:
	/** 
	 * Provide the LockOnTargetComponent a proper mesh component for finding Sockets and Widget attachment.
	 * If the mesh component is None then will try to find the first MeshComponent in the Owner's components hierarchy.
	 * If the mesh component doesn't exists then will be used root component.
	 */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	FName MeshName;

	/** 
	 * Sockets for capturing.
	 * If num == 0 then the Target can't be captured.
	 * 'None' will attach the widget to the root component.
	 * 
	 * Due to performance reasons Sockets aren't validated!
	 */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	TSet<FName> Sockets;

public:
	/** Target capture radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (ClampMin = 50.f, UIMin = 50.f, Units="cm"))
	float CaptureRadius;

	/** Target lost radius offset is added to the CaptureRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (ClampMin = 0.f, UIMin = 0.f, Units="cm"))
	float LostOffsetRadius;

	/** May be helpful to avoid capturing the Target behind the Invader's back (distance is calculated from the camera location). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings", meta = (ClampMin = 0.f, UIMin = 0.f, Units="cm"))
	float MinimumCaptureRadius;

	/** Custom offset is added to the socket location while the target is locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Offset")
	EOffsetType OffsetType;

	/** Target offset in the camera space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Offset", meta = (EditCondition = "OffsetType == EOffsetType::EConstant", EditConditionHides))
	FVector TargetOffset;

	/** Curve distance based target height offset in the camera space. */
	UPROPERTY(EditAnywhere, Category = "Target Offset", meta = (EditCondition = "OffsetType == EOffsetType::EAdaptiveCurve", EditConditionHides, XAxisName="Distance", YAxisName="Height offset"))
	FRuntimeFloatCurve HeightOffsetCurve;

	/** Should the Target be marked with the Widget which is attached to the Socket. */
	UPROPERTY(EditAnywhere, Category = "Widget")
	bool bEnableWidget;

	/** Widget Class. Safe to fill and don't use. Uses the soft class and loads only if it's needed. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bEnableWidget", EditConditionHides))
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** Offset is applied to the Widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget", meta = (EditCondition = "bEnableWidget", EditConditionHides))
	FVector WidgetOffset;

	/** Load the Widget class async, otherwise sync. Note: AssetManager should be specified in project settings. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "Widget", meta = (EditCondition = "bEnableWidget", EditConditionHides))
	bool bAsyncLoadWidget;

#if WITH_EDITORONLY_DATA
public:
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bVisualizeDebugInfo = false;
#endif

/*******************************************************************************************/
/*******************************  Delegates  ***********************************************/
/*******************************************************************************************/
public:
	/** Notifies when the Owner has been targeted. */
	UPROPERTY(BlueprintAssignable)
	FOnOwnerCaptured OnOwnerCaptured;

	/** Notifies when the Owner has been unlocked. */
	UPROPERTY(BlueprintAssignable)
	FOnOwnerReleased OnOwnerReleased;

/*******************************************************************************************/
/*******************************  BP Override  *********************************************/
/*******************************************************************************************/
public:
	/** 
	 * Can the owner be captured. By default uses the bCanBeTargeted value.
	 * BP override method for the custom logic.
	 * e.g. process the 'team system' for not capturing 'allies'.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "TargetingHelper")
	bool CanBeTargeted(ULockOnTargetComponent* Instigator) const;

	/** Provide the custom Target offset. The OffsetType should be set to the CustomOffset value. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TargetingHelper")
	FVector GetCustomTargetOffset(const ULockOnTargetComponent* Instigator) const;
	
	/** Called when the Owner is captured. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TargetingHelper")
	void OnCaptured();

	/** Called when the Owner is released. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TargetingHelper")
	void OnReleased();
	
/*******************************************************************************************/
/*******************************  BP Methods  **********************************************/
/*******************************************************************************************/
public:
	/** Get all the Invaders that are locked on the Owner. Usually 1 in a standalone game. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	const TSet<ULockOnTargetComponent*>& GetInvaders() const { return Invaders;}

	/** Whether the Owner is currently Targeted by any Invader.*/
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	bool IsTargeted() const { return Invaders.Num() > 0; }

	/** Get all sockets. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	const TSet<FName>& GetSockets() const { return Sockets; }

	/** Add a Socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper", meta = (AutoCreateRefTerm = "Socket"))
	bool AddSocket(const FName& Socket = NAME_None);

	/** Remove a socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper", meta = (AutoCreateRefTerm = "Socket"))
	bool RemoveSocket(const FName& Socket = NAME_None);

	/** Update the MeshComponent for calculating socket location and widget attachment. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	void SetMeshComponent(UMeshComponent* NewMeshComponent);

	/** Return the implicit UWidgetComponent. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	UWidgetComponent* GetWidgetComponent() const{ return WidgetComponent;}

/*******************************************************************************************/
/*******************************  Native  **************************************************/
/*******************************************************************************************/
private:
	/** All Invaders. Usually 1 in standalone. Multiple in Network. */
	UPROPERTY(Transient)
	TSet<ULockOnTargetComponent*> Invaders;

	/** Implicit WidgetComponent is used to indicate the Target. */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> WidgetComponent;

	/** Cached mesh component is used to calculate a socket location and widget attachment. */
	TWeakObjectPtr<USceneComponent> OwnerMeshComponent;

	/** WidgetComponent was created during initialization. */
	uint8 bWidgetWasInitialized : 1;

	/** Init MeshComponent by Name on BeginPlay. */
	uint8 bWantsMeshInitialization : 1;

public:
	/** Called to inform the TargetingHelperComponent that it's been Targeted. */
	virtual void CaptureTarget(ULockOnTargetComponent* const Instigator, const FName& Socket);

	/** Called to inform the TargetingHelperComponent that it's been Released. */
	virtual void ReleaseTarget(ULockOnTargetComponent* const Instigator);

	/** Returns world location of the socket. bWithoffset = true should be with an Instigator for the offset in the camera space. */
	FVector GetSocketLocation(const FName& Socket, bool bWithOffset = false, const ULockOnTargetComponent* const Instigator = nullptr) const;

private:
	void InitWidgetComponent();

	/** Is the WidgetComponent properly initialized and available now. */
	bool IsWidgetInitialized() const;

	/** Load and display the widget for the locally controlled Instigator. */
	void UpdateWidget(const FName& Socket, const ULockOnTargetComponent* const Instigator);
	
	/** Hide the widget for the locally controlled Instigator. */
	void HideWidget(const ULockOnTargetComponent* const Instigator);
	
	USceneComponent* GetWidgetParentComponent(const FName& Socket) const;
	void SetWidgetClassOnWidgetComponent();

private:
	void InitMeshComponent();
	UMeshComponent* GetFirstMeshComponent() const;
	USceneComponent* GetRootComponent() const;

private:
	/** Applying an offset to a socket location. */
	void AddOffset(FVector& Location, const ULockOnTargetComponent* const Instigator) const;
};
