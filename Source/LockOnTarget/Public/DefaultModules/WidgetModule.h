// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetModuleBase.h"
#include "WidgetModule.generated.h"

class UWidgetComponent;
class UUserWidget;
struct FStreamableHandle;

/**
 * Displays a single widget attached to the captured Target socket.
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

	//Gets the displayed widget component.
	UFUNCTION(BlueprintPure, Category = "LockOnTarget|Widget Module")
	UWidgetComponent* GetWidget() const;

	//Sets a new Widget class.
	UFUNCTION(BlueprintCallable, Category = "LockOnTarget|Widget Module")
	void SetWidgetClass(const TSoftClassPtr<UUserWidget>& WidgetClass);

protected:

	void OnWidgetClassLoaded();

protected: /** Overrides */

	//ULockOnTargetModuleBase
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
	virtual void OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket) override;
};
