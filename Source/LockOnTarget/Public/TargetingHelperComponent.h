// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwnerCaptured, ULockOnTargetComponent*, Invader);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwnerReleased, ULockOnTargetComponent*, OldInvader);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInvadersUnlock, bool, ULockOnTargetComponent*);

/**
 * Used by Lock On Component for Capturing and providing useful information to it.
 * 
 * Some features:
 *  - Can be added to any AActor subclass(also during runtime).
 *	- Customizable settings for each Target.
 *	- Capturing Targets with multiple sockets (e.g. for capturing head, body or legs in large creatures (like Dragon) and switching between them).
 *  - Add custom rules for capturing the target (with overriding CanBeTargeted() method, e.g. not capturing teammates).
 *  - Adding and removing sockets at runtime.
 *  - Ability to Unlock All Invaders(LockOnTargetComponent) or specific one from it(Target).
 *  - Determine the mesh component by name.
 *  - Various useful information and methods that may be needed for Target.
 *  - Implicitly adds UWidgetComponent (use GetWidgetComponent() to access it).
 * 
 *	@see ULockOnTargetComponent, UTargetHandlerBase, UWithoutInterpolationRotationMode.
 */

UCLASS(Blueprintable, ClassGroup = (LockOnTarget), HideCategories = (Components, Activation, ComponentTick, Cooking, Sockets, ComponentReplication, Collision), meta = (BlueprintSpawnableComponent))
class LOCKONTARGET_API UTargetingHelperComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingHelperComponent();
	friend FHelperVisualizer;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

public:
	/** 
	 * Can owner be captured by LockOnTargetComponent.
	 * (e.g. registering owner's death without destroying it by GC).
	 * 
	 * Also method CanBeTarget() can be overriden for registering custom capture state.
	 * (e.g. team system. CanBeTargeted() can be overwritten for not capturing friendies.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
	bool bCanBeTargeted;

	/** 
	 * Provide LockOnTargetComponent proper mesh component for finding Sockets and Widget attachment.
	 * If mesh component invalid or None then will try find first MeshComponent in Owner's components hierarchy.
	 */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	FName MeshName;
	
	/** 
	 * Sockets for capturing.
	 * If num == 0 then Target can't be captured.
	 * NAME_None will attach widget to root component.
	 */
	UPROPERTY(EditAnywhere, Category = "Default Settings")
	TArray<FName> Sockets;

	/** Target capture radius. Should be < Lost radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Settings", meta = (ClampMin = 50.f, UIMin = 50.f))
	float CaptureRadius;

	/** Target lost radius. Should be > Capture radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Settings", meta = (ClampMin = 50.f, UIMin = 50.f))
	float LostRadius;

	/** Maybe helpful for not capturing Target behind owner back(distance calculates from camera location). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Settings", meta = (ClampMin = 0.f, UIMin = 0.f))
	float MinDistance;

	/** Custom offset added to socket location while target locked. */
	UPROPERTY(EditAnywhere, Category = "Target Offset")
	EOffsetType OffsetType;

	/** Target offset in camera space. */
	UPROPERTY(EditAnywhere, Category = "Target Offset", meta = (EditCondition = "OffsetType == EOffsetType::EConstant"))
	FVector TargetOffset;

	/** Curve based target height offset in camera space. */
	UPROPERTY(EditAnywhere, Category = "Target Offset", meta = (EditCondition = "OffsetType == EOffsetType::EAdaptiveCurve"))
	FRuntimeFloatCurve HeightOffsetCurve;

	/** Should Target be marked with Widget attached to Socket. */
	UPROPERTY(EditAnywhere, Category = "Widget")
	bool bEnableWidget;

	/** Widget Class. Safe to fill and not use. Uses soft class and load only if needed. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bEnableWidget"))
	TSoftClassPtr<class UUserWidget> WidgetClass;

	/** Offset applied to Widget. */
	UPROPERTY(EditAnywhere, Category = "Widget", meta = (EditCondition = "bEnableWidget"))
	FVector WidgetOffset;

	/** Load UUserWidget class async, otherwise sync. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "Widget", meta = (EditCondition = "bEnableWidget"))
	bool bAsyncLoadWidget;

#if WITH_EDITORONLY_DATA
protected:
	/** Visualize debug info. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "Default Settings")
	bool bVisualizeDebugInfo = false;
#endif

/*******************************************************************************************/
/*******************************  Delegates  ***********************************************/
/*******************************************************************************************/
public:
	/** Notify when Owner has been targeted. */
	UPROPERTY(BlueprintAssignable)
	FOnOwnerCaptured OnOwnerCaptured;

	/** Notify when Owner has been unlocked. */
	UPROPERTY(BlueprintAssignable)
	FOnOwnerReleased OnOwnerReleased;

	/** Used by LockOnTargetComponent for Clearing Target on Request. */
	FOnInvadersUnlock OnInvadersUnlock;

/*******************************************************************************************/
/*******************************  BP Override  *********************************************/
/*******************************************************************************************/
public:
	/** 
	 * Can owner be Captured. By default use bCanBeTargeted value.
	 * BP Override method for custom logic.
	 * e.g. process Team system for not capturing allies.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "TargetingHelper")
	bool CanBeTargeted(ULockOnTargetComponent* Instigator) const;

	/** Provide Custom Target offset. Offset type should be set to CustomOffset. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TargetingHelper")
	FVector GetCustomTargetOffset(ULockOnTargetComponent* Instigator) const;
	
/*******************************************************************************************/
/*******************************  BP Methods  **********************************************/
/*******************************************************************************************/
public:
	/** Get all Invaders locked on Owner Actor. Usually 1 single player. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	TArray<ULockOnTargetComponent*> GetInvaders() const { return Invaders;}

	/** Is Owner Actor at this moment Targeted by any Invader.*/
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	bool IsTargeted() const { return Invaders.Num() > 0; }

	/** Add socket without all Invaders unlocking if captured. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	bool AddSocket(UPARAM(ref) FName& Socket);

	/** Remove socket and unlock all Invaders if captured. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	bool RemoveSocket(const FName& Socket);

	/** Unlock specific Invader if exists. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	void UnlockInvader(ULockOnTargetComponent* Invader);

	/** Unlock all invaders. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	void UnlockAllInvaders();

	/** Return implicit UWidgetComponent. */
	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	UWidgetComponent* GetWidgetComponent() const{ return WidgetComponent;}

	UFUNCTION(BlueprintPure, Category = "TargetingHelper")
	TArray<FName> GetSockets() const {return Sockets; }

	/** @return - Is successfully changed. */
	UFUNCTION(BlueprintCallable, Category = "TargetingHelper")
	bool ChangeRadius(float NewCaptureRadius, float NewLostRadius, float NewMinDistance = 100.f);

/*******************************************************************************************/
/*******************************  Native Fields  *******************************************/
/*******************************************************************************************/
private:
	//All Invaders. Usually one in standalone. Multiple in Network.
	UPROPERTY(Transient)
	TArray<ULockOnTargetComponent*> Invaders;

/*******************************************************************************************/
/*******************************  Native Methods  ******************************************/
/*******************************************************************************************/

	/** Class Interface. */
public:
	/** Called for inform TargetingHelperComponent that it has been Targeted.*/
	void CaptureTarget(ULockOnTargetComponent* Instigator, const FName& Socket);

	/** Called for inform TargetingHelperComponent that it has been Released.*/
	void ReleaseTarget(ULockOnTargetComponent* Instigator);

	/** @return	- World socket location. bWithoffset = true should be with instigator for camera space offsets. */
	FVector GetSocketLocation(const FName& Socket, bool bWithOffset = false, ULockOnTargetComponent* Instigator = nullptr) const;
	/** ~Class Interface. */

	/** Widget Handling. */
public:
	void UpdateWidget(const FName& Socket);
	void HideWidget();

private:
	UPROPERTY(Transient)
	class UWidgetComponent* WidgetComponent;

	void SyncLoadWidget(TSoftClassPtr<UUserWidget>& Widget);
	void AsyncLoadWidget(TSoftClassPtr<UUserWidget> Widget);
	/** ~Widget Handling. */

	/** Cached mesh component for socket location and widget attachment. */
private:
	mutable TWeakObjectPtr<UMeshComponent> OwnerMeshComponent;
	USceneComponent* GetMeshComponent() const;
	USceneComponent* GetFirstMeshComponent() const;
	USceneComponent* GetRootComponent() const;
	/** ~Cached mesh component for socket location and widget attachment. */

private:
	void AddOffset(FVector& Location, ULockOnTargetComponent* Instigator) const;
	void InitMeshComponent() const;
	void VerifySockets();
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
