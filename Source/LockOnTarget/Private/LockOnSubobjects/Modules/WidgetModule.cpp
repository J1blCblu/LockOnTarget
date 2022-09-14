// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/Modules/WidgetModule.h"
#include "LockOnTargetComponent.h"
#include "TargetingHelperComponent.h"
#include "LockOnTargetDefines.h"
#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"

UWidgetModule::UWidgetModule()
	: DefaultWidgetClass(FSoftClassPath(FString(TEXT("WidgetBlueprint'/LockOnTarget/WBP_Target.WBP_Target_C'"))))
	, Widget(nullptr)
	, bWidgetIsActive(false)
	, bWidgetIsInitialized(false)
{
	bWantsUpdate = false;
}

void UWidgetModule::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	Widget = NewObject<UWidgetComponent>(this, MakeUniqueObjectName(this, UWidgetComponent::StaticClass(), TEXT("LOT_Target_Widget")));

	if (ensureMsgf(Widget, TEXT("Failed to create a Widget.")))
	{
		Widget->RegisterComponent();
		Widget->SetWidgetSpace(EWidgetSpace::Screen);
		Widget->SetVisibility(false);
		Widget->SetDrawAtDesiredSize(true);
		bWidgetIsInitialized = true;
	}
}

void UWidgetModule::Deinitialize(ULockOnTargetComponent* Instigator)
{
	if(IsWidgetInitialized())
	{
		bWidgetIsInitialized = false;
		Widget->DestroyComponent();
	}

	if(StreamableHandle.IsValid())
	{
		StreamableHandle->ReleaseHandle();
		StreamableHandle.Reset();
	}

	Super::Deinitialize(Instigator);
}

void UWidgetModule::OnTargetLocked(UTargetingHelperComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);

	//@TODO: Will be shown if controlled by a local AIController. Maybe leave it as a feature or display only for the player.
	if(IsWidgetInitialized() && GetLockOnTargetComponent()->IsLocallyControlled() && IsValid(Target) && Target->bWantsDisplayWidget)
	{
		Widget->AttachToComponent(Target->GetDesiredMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		Widget->SetVisibility(true);
		Widget->SetRelativeLocation(Target->WidgetRelativeOffset);
		SetWidgetClass(Target->CustomWidgetClass.IsNull() ? DefaultWidgetClass : Target->CustomWidgetClass);
		bWidgetIsActive = true;
	}
}

void UWidgetModule::SetWidgetClass(const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	if(WidgetClass.IsNull())
	{
		LOG_WARNING("Widget class is null.");
		return;
	}

	if (IsWidgetInitialized())
	{
		if (WidgetClass)
		{
			Widget->SetWidgetClass(WidgetClass.Get());
		}
		else
		{
			if (WidgetClass.IsPending())
			{
				StreamableHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(WidgetClass.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &UWidgetModule::OnWidgetClassLoaded));
			}
		}
	}
}

void UWidgetModule::OnWidgetClassLoaded()
{
	if (IsWidgetInitialized() && StreamableHandle.IsValid() && StreamableHandle->HasLoadCompleted())
	{
		//@TODO: Ensure that we've got the right class.
		Widget->SetWidgetClass(StaticCast<UClass*>(StreamableHandle->GetLoadedAsset()));
	}
}

void UWidgetModule::OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);

	if(IsWidgetInitialized() && IsWidgetActive())
	{
		Widget->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Widget->SetVisibility(false);
		bWidgetIsActive = false;
	}
}

void UWidgetModule::OnSocketChanged(UTargetingHelperComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	Super::OnSocketChanged(CurrentTarget, NewSocket, OldSocket);

	if(IsWidgetInitialized() && IsWidgetActive() && IsValid(CurrentTarget))
	{
		Widget->AttachToComponent(CurrentTarget->GetDesiredMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NewSocket);
	}
}

bool UWidgetModule::IsWidgetInitialized() const
{
	return bWidgetIsInitialized && ensureMsgf(IsValid(Widget), TEXT("Widget was initialized but is invalid. Maybe it was removed manually."));
}

bool UWidgetModule::IsWidgetActive() const
{
	return bWidgetIsActive;
}

UWidgetComponent* UWidgetModule::GetWidget() const
{
	return Widget;
}

void UWidgetModule::SetWidgetVisibility(bool bInVisibility)
{
	if(IsWidgetInitialized())
	{
		//We can only show widget when it's active, though we can whenever hide it.
		Widget->SetVisibility(bInVisibility && IsWidgetActive());
	}
}
