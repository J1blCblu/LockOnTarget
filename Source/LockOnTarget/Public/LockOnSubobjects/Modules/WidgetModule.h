// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnSubobjects/LockOnTargetModuleBase.h"
#include "WidgetModule.generated.h"

class UWidgetComponent;
class UUserWidget;
struct FStreamableHandle;

/**
 * Displays a single widget attached to the captured Target socket.
 * Will only be displayed for the locally controlled player.
 */
UCLASS(Blueprintable)
class LOCKONTARGET_API UWidgetModule : public ULockOnTargetModuleBase
{
	GENERATED_BODY()

public:
	UWidgetModule();

public:
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
	uint8 bWidgetIsActive : 1;
	
	//Whether the widget was successfully initialized or not.
	uint8 bWidgetIsInitialized : 1;

protected: //ULockOnTargetModuleBase overrides
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetLocked(UTargetingHelperComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket) override;
	virtual void OnSocketChanged(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket) override;

public:
	bool IsWidgetInitialized() const;
	bool IsWidgetActive() const;

	//Only affects the locally controlled player.
	UFUNCTION(BlueprintCallable, Category = "Widget Module")
	void SetWidgetVisibility(bool bInVisibility);

	//Gets the displayed widget component.
	UFUNCTION(BlueprintPure, Category = "Widget Module")
	UWidgetComponent* GetWidget() const;

	//Sets a new Widget class.
	UFUNCTION(BlueprintCallable, Category = "Widget Module")
	void SetWidgetClass(const TSoftClassPtr<UUserWidget>& WidgetClass);

	void OnWidgetClassLoaded();
};
