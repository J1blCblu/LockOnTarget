// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "LockOnTargetExtensions/LockOnTargetExtensionBase.h"
#include "LockOnTargetTypes.h"
#include "TargetPreviewExtension.generated.h"

class UUserWidget;
class UWidgetComponent;
struct FStreamableHandle;

/**
 * Tries to predictively find a new Target and mark it.
 */
UCLASS(Blueprintable, HideCategories = Tick)
class LOCKONTARGET_API UTargetPreviewExtension : public ULockOnTargetExtensionBase
{
	GENERATED_BODY()

public:

	UTargetPreviewExtension();

public: /** Config */

	/** Widget class to display. */
	UPROPERTY(EditDefaultsOnly, Category = "Target Preview")
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** Preview update rate. */
	UPROPERTY(EditDefaultsOnly, Category="Target Preview", meta = (ClampMin = 0.f, ClampMax = 1.f, Units = "s"))
	float UpdateRate;

private: /** Internal */

	//Current preview Target.
	UPROPERTY(Transient)
	FTargetInfo PreviewTarget;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> Widget;

	TSharedPtr<FStreamableHandle> StreamableHandle;

	//Whether the widget was successfully initialized or not.
	bool bWidgetIsInitialized;

public:

	bool IsWidgetInitialized() const;

	/** Whether the preview is active. */
	UFUNCTION(BlueprintPure, Category = "Target Preview")
	bool IsPreviewActive() const { return IsTickEnabled(); }

	/** Sets preview active state. */
	UFUNCTION(BlueprintCallable, Category = "Target Preview")
	void SetPreviewActive(bool bInActive);

	/** Whether the PreviewTarget is valid. */
	UFUNCTION(BlueprintPure, Category = "Target Preview")
	bool IsPreviewTargetValid() const { return PreviewTarget != FTargetInfo::NULL_TARGET; }

	/** Gets current Target preview. */
	UFUNCTION(BlueprintPure, Category = "Target Preview")
	FTargetInfo GetPreviewTarget() const { return PreviewTarget; }

protected:

	virtual void UpdateTargetPreview();
	virtual void BeginTargetPreview(const FTargetInfo& Target);
	virtual void StopTargetPreview(const FTargetInfo& Target);

	void OnWidgetClassLoaded();

public: /** Overrides */

	//ULockOnTargetExtensionBase
	virtual void Initialize(ULockOnTargetComponent* Instigator) override;
	virtual void Deinitialize(ULockOnTargetComponent* Instigator) override;
	virtual void OnTargetLocked(UTargetComponent* Target, FName Socket) override;
	virtual void OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket) override;
	virtual void Update(float DeltaTime) override;
};
