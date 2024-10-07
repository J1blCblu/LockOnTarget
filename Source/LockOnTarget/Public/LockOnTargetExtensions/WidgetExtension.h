// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "WidgetExtension.generated.h"

class UWidgetComponent;
class UUserWidget;
struct FStreamableHandle;

/**
 * Visually indicates the captured Target by attaching a widget to the socket.
 */
UCLASS(Blueprintable, HideCategories = Tick)
class LOCKONTARGET_API UWidgetExtension : public ULockOnTargetExtensionBase
{
	GENERATED_BODY()

public:

	UWidgetExtension();

public:

	/** Whether to display the widget for a local player or for everyone. */
	UPROPERTY(EditDefaultsOnly, Category = "Widget")
	bool bIsLocalWidget;

	/** Widget soft class, loaded on demand. */
	UPROPERTY(EditDefaultsOnly, Category = "Widget")
	TSoftClassPtr<UUserWidget> DefaultWidgetClass;

private:

	//The actual widget to display.
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> Widget;

	//Stores the last async loaded widget class in memory.
	TSharedPtr<FStreamableHandle> StreamableHandle;

	//Whether the widget is active or not.
	bool bWidgetIsActive;
	
	//Whether the widget was successfully initialized or not.
	bool bWidgetIsInitialized;

public:

	bool IsWidgetInitialized() const;

	//Is widget shown.
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|Widget Module")
	bool IsWidgetActive() const;

	//Only affects the locally controlled player.
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|Widget Module")
	void SetWidgetVisibility(bool bInVisibility);

	//Returns the displayed widget component.
	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Widget Module")
	UWidgetComponent* GetWidget() const;

	//Sets a new Widget class.
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|Widget Module")
	void SetWidgetClass(const TSoftClassPtr<UUserWidget>& WidgetClass);

protected:

	void OnWidgetClassLoaded();

protected: /** Overrides */

	//ULockOnTargetExtensionBase
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
	virtual void OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket) override;
};
