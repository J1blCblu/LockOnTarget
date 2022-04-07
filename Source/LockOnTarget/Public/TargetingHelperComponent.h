// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Utilities/Enums.h"
#include "Curves/CurveFloat.h"
#include "TargetingHelperComponent.generated.h"

class ULockOnTargetComponent;
class FHelperVisualizer;
class UMeshComponent;
class USceneComponent;
class AActor;

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

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif
	//~UActorComponent

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

	/** 
	 * Provide the LockOnTargetComponent the proper mesh component for finding the Sockets and the Widget attachment.
	 * If the mesh component is invalid or None then will try to find the first MeshComponent in the Owner's components hierarchy.
	 */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	FName MeshName;
	
	/** 
	 * Sockets for capturing.
	 * If num == 0 then the Target can't be captured.
	 * NAME_None will attach the widget to the root component.
	 * 
	 * Due to performance reasons Sockets aren't validated!
	 */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	TSet<FName> Sockets;

	/** Target capture radius. Should be < Lost radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Settings", meta = (ClampMin = 50.f, UIMin = 50.f))
	float CaptureRadius;

	/** Target lost radius. Should be > Capture radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Settings", meta = (ClampMin = 50.f, UIMin = 50.f))
	float LostRadius;

	/** May be helpful to avoid capturing the Target behind the Invader's back (distance is calculated from the camera location). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Settings", meta = (ClampMin = 0.f, UIMin = 0.f))
	float MinDistance;

	/** Custom offset is added to the socket location while the target is locked. */
	UPROPERTY(EditAnywhere, Category = "Target Offset")
	EOffsetType OffsetType;

	/** Target offset in the camera space. */
	UPROPERTY(EditAnywhere, Category = "Target Offset", meta = (EditCondition = "OffsetType == EOffsetType::EConstant"))
	FVector TargetOffset;

	/** Curve distance based target height offset in the camera space. */
	UPROPERTY(EditAnywhere, Category = "Target Offset", meta = (EditCondition = "OffsetType == EOffsetType::EAdaptiveCurve"))
	FRuntimeFloatCurve HeightOffsetCurve;

	/** Should the Target be marked with the Widget which is attached to the Socket. */
	UPROPERTY(EditAnywhere, Category = "Widget")
	bool bEnableWidget;

	/** Widget Class. Safe to fill and don't use. Uses the soft class and loads only if it's needed. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bEnableWidget"))
	TSoftClassPtr<class UUserWidget> WidgetClass;

	/** Offset is applied to the Widget. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bEnableWidget"))
	FVector WidgetOffset;

	/** Load the Widget class async, otherwise sync. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "Widget", meta = (EditCondition = "bEnableWidget"))
	bool bAsyncLoadWidget;

#if WITH_EDITORONLY_DATA
protected:
	/** Visualize the debug info. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "Default Settings")
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
	
/*******************************************************************************************/
/*******************************  BP Methods  **********************************************/
/*******************************************************************************************/
public:
	/** Get all the Invaders that are locked on the Owner. Usually 1 in a standalone game. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	TSet<ULockOnTargetComponent*> GetInvaders() const { return Invaders;}

	/** Whether the Owner is currently Targeted by any Invader.*/
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	bool IsTargeted() const { return Invaders.Num() > 0; }

	/** Add a Socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	bool AddSocket(UPARAM(ref) FName& Socket);

	/** Remove a socket at runtime. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	bool RemoveSocket(const FName& Socket);

	/** Update the MeshComponent for capturing the sockets. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	void UpdateMeshComponent(UMeshComponent* NewMeshComponent);

	/** Return the implicit UWidgetComponent. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	UWidgetComponent* GetWidgetComponent() const{ return WidgetComponent;}

	/** Get all sockets. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	TSet<FName> GetSockets() const {return Sockets; }

	/** Update the CaptureRadius, LostRadius, MinDistance. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	void ChangeRadius(float NewCaptureRadius = 2000.f, float NewLostRadius = 2100.f, float NewMinDistance = 100.f);

/*******************************************************************************************/
/*******************************  Native Fields  *******************************************/
/*******************************************************************************************/
private:
	//All Invaders. Usually 1 in standalone. Multiple in Network.
	UPROPERTY(Transient)
	TSet<ULockOnTargetComponent*> Invaders;

/*******************************************************************************************/
/*******************************  Native Methods  ******************************************/
/*******************************************************************************************/

	/** Class Interface. */
public:
	/** Called to inform the TargetingHelperComponent that it's been Targeted.*/
	virtual void CaptureTarget(ULockOnTargetComponent* Instigator, const FName& Socket);

	/** Called to inform the TargetingHelperComponent that it's been Released.*/
	virtual void ReleaseTarget(ULockOnTargetComponent* Instigator);

	/** @return	- World socket location. bWithoffset = true should be with the instigator for the camera space offset. */
	FVector GetSocketLocation(const FName& Socket, bool bWithOffset = false, const ULockOnTargetComponent* Instigator = nullptr) const;
	/** ~Class Interface. */

public:
	/** Widget Handling. */
	void UpdateWidget(const FName& Socket, ULockOnTargetComponent* Instigator);
	void HideWidget(ULockOnTargetComponent* Instigator);

private:
	//TODO: Maybe wrap the UWidgetComponent with the preprocessor #if !WITH_SERVER_CODE
	UPROPERTY(Transient)
	class UWidgetComponent* WidgetComponent;

	void SyncLoadWidget(TSoftClassPtr<UUserWidget>& Widget);
	void AsyncLoadWidget(TSoftClassPtr<UUserWidget> Widget);
	/** ~Widget Handling. */

private:
	/** Cached mesh component is used to calculate the socket location and the widget attachment. */
	mutable TWeakObjectPtr<UMeshComponent> OwnerMeshComponent;
	USceneComponent* GetMeshComponent() const;
	USceneComponent* GetFirstMeshComponent() const;
	USceneComponent* GetRootComponent() const;

private:
	void AddOffset(FVector& Location, const ULockOnTargetComponent* Instigator) const;
	void InitMeshComponent() const;
};

template <typename T>
T* FindMeshComponentByName(AActor* Actor, FName Name)
{
	static_assert(TPointerIsConvertibleFromTo<T, const UMeshComponent>::Value, "'T' template parameter for TryFindMeshComponentByName must be derived from UMeshComponent");

	if (!Actor)
		return nullptr;

	UActorComponent* Component = nullptr;

	if (Name != NAME_None) //Find MeshComponent by valid Name.
	{
		TArray<UActorComponent*> Array;
		Actor->GetComponents(T::StaticClass(), Array);

		//T* TArray<T,Alloc>::FindByPredicate() => return Type**
		UActorComponent** FoundComponent = Array.FindByPredicate([&Name](const UActorComponent* Comp)
			{ return *Comp->GetName() == Name; });

		Component = FoundComponent ? *FoundComponent : nullptr;
	}

	if (!Component) //Find first component if argument's name invalid.
	{
		Component = Actor->FindComponentByClass<T>();
	}

	//Cast to template typename.
	return Cast<T>(Component);
}
