// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetExtensions/WidgetExtension.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "LockOnTargetDefines.h"

#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/PlayerController.h"

UWidgetExtension::UWidgetExtension()
	: bIsLocalWidget(true)
	, DefaultWidgetClass(FSoftClassPath(FString(TEXT("/Script/UMGEditor.WidgetBlueprint'/LockOnTarget/WBP_Target.WBP_Target_C'"))))
	, Widget(nullptr)
	, bWidgetIsActive(false)
	, bWidgetIsInitialized(false)
{
	DefaultWidgetClass.ToSoftObjectPath().PostLoadPath(nullptr);
	ExtensionTick.bCanEverTick = false;
}

void UWidgetExtension::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	//Don't create WidgetComponent on a dedicated server.
	if (!IsRunningDedicatedServer())
	{
		Widget = NewObject<UWidgetComponent>(this, MakeUniqueObjectName(this, UWidgetComponent::StaticClass(), TEXT("LockOnTarget_Target_Widget")), RF_Transient);

		if (Widget)
		{
			Widget->RegisterComponent();
			Widget->SetWidgetSpace(EWidgetSpace::Screen);
			Widget->SetVisibility(false);
			Widget->SetDrawAtDesiredSize(true);
			Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			bWidgetIsInitialized = true;
		}
	}
}

void UWidgetExtension::Deinitialize(ULockOnTargetComponent* Instigator)
{
	if (IsWidgetInitialized())
	{
		bWidgetIsInitialized = false;
		Widget->DestroyComponent();
	}

	if (StreamableHandle.IsValid())
	{
		StreamableHandle->ReleaseHandle();
		StreamableHandle.Reset();
	}

	Super::Deinitialize(Instigator);
}

void UWidgetExtension::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);

	if (IsWidgetInitialized() && Target->bWantsDisplayWidget)
	{
		const APlayerController* const PC = GetPlayerController();

		if (!bIsLocalWidget || (PC && PC->IsLocalController()))
		{
			if (bIsLocalWidget)
			{
				Widget->SetOwnerPlayer(PC->GetLocalPlayer());
			}

			Widget->AttachToComponent(Target->GetAssociatedComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
			Widget->SetVisibility(true);
			Widget->SetRelativeLocation(Target->WidgetRelativeOffset);
			SetWidgetClass(Target->CustomWidgetClass.IsNull() ? DefaultWidgetClass : Target->CustomWidgetClass);
			bWidgetIsActive = true;
		}
	}
}

void UWidgetExtension::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);

	if (IsWidgetInitialized() && IsWidgetActive())
	{
		Widget->SetVisibility(false);
		Widget->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		bWidgetIsActive = false;
	}
}

void UWidgetExtension::OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	Super::OnSocketChanged(CurrentTarget, NewSocket, OldSocket);

	if (IsWidgetInitialized() && IsWidgetActive())
	{
		Widget->AttachToComponent(CurrentTarget->GetAssociatedComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NewSocket);
	}
}

void UWidgetExtension::SetWidgetClass(const TSoftClassPtr<UUserWidget>& WidgetClass)
{
	if (WidgetClass.IsNull())
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
		else if (WidgetClass.IsPending())
		{
			StreamableHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(WidgetClass.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &ThisClass::OnWidgetClassLoaded));
		}
	}
}

void UWidgetExtension::OnWidgetClassLoaded()
{
	if (IsWidgetInitialized() && StreamableHandle.IsValid() && StreamableHandle->HasLoadCompleted())
	{
		//@TODO: Ensure that we've got the right class.
		Widget->SetWidgetClass(static_cast<UClass*>(StreamableHandle->GetLoadedAsset()));
	}
}

bool UWidgetExtension::IsWidgetInitialized() const
{
	return bWidgetIsInitialized && ensureMsgf(IsValid(Widget), TEXT("Widget was initialized but is invalid. Maybe it was removed manually."));
}

bool UWidgetExtension::IsWidgetActive() const
{
	return bWidgetIsActive;
}

UWidgetComponent* UWidgetExtension::GetWidget() const
{
	return Widget;
}

void UWidgetExtension::SetWidgetVisibility(bool bInVisibility)
{
	if (IsWidgetInitialized())
	{
		//We can only show widget when it's active, though we can whenever hide it.
		Widget->SetVisibility(bInVisibility && IsWidgetActive());
	}
}
