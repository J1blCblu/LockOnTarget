// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetExtensions/TargetPreviewExtension.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"

#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/PlayerController.h"

UTargetPreviewExtension::UTargetPreviewExtension()
	: WidgetClass(FSoftClassPath(FString(TEXT("/Script/UMGEditor.WidgetBlueprint'/LockOnTarget/WBP_PreviewTarget.WBP_PreviewTarget_C'"))))
	, UpdateRate(0.1f)
	, Widget(nullptr)
	, bWidgetIsInitialized(false)
{
	WidgetClass.ToSoftObjectPath().PostLoadPath(nullptr);
	ExtensionTick.TickGroup = TG_PostPhysics;
	ExtensionTick.bCanEverTick = true;
	ExtensionTick.bStartWithTickEnabled = false; //Update only if we've successfully created the widget.
	ExtensionTick.bAllowTickOnDedicatedServer = false;
}

void UTargetPreviewExtension::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	//Don't create WidgetComponent on a dedicated server.
	if (!IsRunningDedicatedServer())
	{
		Widget = NewObject<UWidgetComponent>(this, MakeUniqueObjectName(this, UWidgetComponent::StaticClass(), TEXT("LockOnTarget_TargetPreview_Widget")), RF_Transient);

		if (Widget)
		{
			Widget->RegisterComponent();
			Widget->SetWidgetSpace(EWidgetSpace::Screen);
			Widget->SetVisibility(false);
			Widget->SetDrawAtDesiredSize(true);
			Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			if (WidgetClass)
			{
				Widget->SetWidgetClass(WidgetClass.Get());
			}
			else
			{
				if (WidgetClass.IsPending())
				{
					StreamableHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(WidgetClass.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &ThisClass::OnWidgetClassLoaded));
				}
			}

			bWidgetIsInitialized = true;

			ExtensionTick.TickInterval = UpdateRate;
			SetTickEnabled(true);
		}
	}
}

void UTargetPreviewExtension::OnWidgetClassLoaded()
{
	if (IsWidgetInitialized() && StreamableHandle.IsValid() && StreamableHandle->HasLoadCompleted())
	{
		//@TODO: Ensure that we've got the right class.
		Widget->SetWidgetClass(static_cast<UClass*>(StreamableHandle->GetLoadedAsset()));
	}
}

void UTargetPreviewExtension::Deinitialize(ULockOnTargetComponent* Instigator)
{
	SetPreviewActive(false);

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

bool UTargetPreviewExtension::IsWidgetInitialized() const
{
	return bWidgetIsInitialized && ensureMsgf(IsValid(Widget), TEXT("Widget was initialized but is invalid. Maybe it was removed manually."));
}

void UTargetPreviewExtension::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);
	SetPreviewActive(false);
	SetTickEnabled(false);
}

void UTargetPreviewExtension::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	SetPreviewActive(true);
	SetTickEnabled(true);
}

void UTargetPreviewExtension::SetPreviewActive(bool bInActive)
{
	if (IsPreviewActive() != bInActive)
	{
		if (!bInActive || !GetLockOnTargetComponent()->IsTargetLocked())
		{
			SetTickEnabled(bInActive);

			if (!IsPreviewActive())
			{
				StopTargetPreview(PreviewTarget);
			}
		}
	}
}

void UTargetPreviewExtension::Update(float DeltaTime)
{
	Super::Update(DeltaTime);

	const AController* const Controller = GetInstigatorController();

	if (IsWidgetInitialized() 
		&& Controller && Controller->IsLocalController() 
		&& GetLockOnTargetComponent()->CanCaptureTarget())
	{
		UpdateTargetPreview();
	}
}

void UTargetPreviewExtension::UpdateTargetPreview()
{
	const ULockOnTargetComponent* const Owner = GetLockOnTargetComponent();

	if (UTargetHandlerBase* const TargetHandler = Owner->GetTargetHandler())
	{
		const FFindTargetRequestResponse Response = TargetHandler->FindTarget();
		const FTargetInfo Preview = Response.Target;

		if (Owner->IsTargetValid(Preview.TargetComponent))
		{
			if (Preview != GetPreviewTarget())
			{
				StopTargetPreview(GetPreviewTarget());
				BeginTargetPreview(Preview);
			}
		}
		else
		{
			StopTargetPreview(PreviewTarget);
		}
	}
	else
	{
		LOG_WARNING("TargetPreviewExtension failed to find TargetHandler in %s", *GetFullNameSafe(Owner));
	}
}

void UTargetPreviewExtension::BeginTargetPreview(const FTargetInfo& Target)
{
	PreviewTarget = Target;

	if (IsWidgetInitialized())
	{
		if (const auto* const PC = GetPlayerController())
		{
			Widget->SetOwnerPlayer(PC->GetLocalPlayer());
		}

		Widget->AttachToComponent(Target.TargetComponent->GetAssociatedComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Target.Socket);
		Widget->SetVisibility(true);
		Widget->SetRelativeLocation(Target.TargetComponent->WidgetRelativeOffset);
	}
}

void UTargetPreviewExtension::StopTargetPreview(const FTargetInfo& Target)
{
	if (IsPreviewTargetValid())
	{
		PreviewTarget = FTargetInfo::NULL_TARGET;

		if (IsWidgetInitialized())
		{
			Widget->SetVisibility(false);
			Widget->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}
