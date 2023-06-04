// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "DefaultModules/WidgetModule.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "LockOnTargetDefines.h"

#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/PlayerController.h"

UWidgetModule::UWidgetModule()
	: DefaultWidgetClass(FSoftClassPath(FString(TEXT("/Script/UMGEditor.WidgetBlueprint'/LockOnTarget/WBP_Target.WBP_Target_C'"))))
	, Widget(nullptr)
	, bWidgetIsActive(false)
	, bWidgetIsInitialized(false)
{
	//Do something.
}

void UWidgetModule::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	if ((Widget = NewObject<UWidgetComponent>(this, MakeUniqueObjectName(this, UWidgetComponent::StaticClass(), TEXT("LockOnTarget_Target_Widget")), RF_Transient)))
	{
		Widget->RegisterComponent();
		Widget->SetWidgetSpace(EWidgetSpace::Screen);
		Widget->SetVisibility(false);
		Widget->SetDrawAtDesiredSize(true);
		Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		bWidgetIsInitialized = true;
	}
}

void UWidgetModule::Deinitialize(ULockOnTargetComponent* Instigator)
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

void UWidgetModule::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);

	if (IsWidgetInitialized() && Target->bWantsDisplayWidget && GetController() && GetController()->IsLocalController())
	{
		if (const auto* const PC = GetPlayerController())
		{
			Widget->SetOwnerPlayer(PC->GetLocalPlayer());
		}

		Widget->AttachToComponent(Target->GetTrackedMeshComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		Widget->SetVisibility(true);
		Widget->SetRelativeLocation(Target->WidgetRelativeOffset);
		SetWidgetClass(Target->CustomWidgetClass.IsNull() ? DefaultWidgetClass : Target->CustomWidgetClass);
		bWidgetIsActive = true;
	}
}

void UWidgetModule::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);

	if (IsWidgetInitialized() && IsWidgetActive())
	{
		Widget->SetVisibility(false);
		Widget->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		bWidgetIsActive = false;
	}
}

void UWidgetModule::OnSocketChanged(UTargetComponent* CurrentTarget, FName NewSocket, FName OldSocket)
{
	Super::OnSocketChanged(CurrentTarget, NewSocket, OldSocket);

	if (IsWidgetInitialized() && IsWidgetActive())
	{
		Widget->AttachToComponent(CurrentTarget->GetTrackedMeshComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NewSocket);
	}
}

void UWidgetModule::SetWidgetClass(const TSoftClassPtr<UUserWidget>& WidgetClass)
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
		else if(WidgetClass.IsPending())
		{
			StreamableHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(WidgetClass.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &ThisClass::OnWidgetClassLoaded));
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
	if (IsWidgetInitialized())
	{
		//We can only show widget when it's active, though we can whenever hide it.
		Widget->SetVisibility(bInVisibility && IsWidgetActive());
	}
}
