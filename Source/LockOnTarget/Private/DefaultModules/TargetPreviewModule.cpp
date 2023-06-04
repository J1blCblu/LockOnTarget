// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "DefaultModules/TargetPreviewModule.h"
#include "LockOnTargetComponent.h"
#include "TargetComponent.h"
#include "TargetHandlers/TargetHandlerBase.h"
#include "LockOnTargetDefines.h"

#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/PlayerController.h"

UTargetPreviewModule::UTargetPreviewModule()
	: WidgetClass(FSoftClassPath(FString(TEXT("/Script/UMGEditor.WidgetBlueprint'/LockOnTarget/WBP_PreviewTarget.WBP_PreviewTarget_C'"))))
	, UpdateRate(0.1f)
	, Widget(nullptr)
	, bIsPreviewActive(true)
	, bWidgetIsInitialized(false)
	, UpdateTimer(0.f)
{
	//Do something.
}

void UTargetPreviewModule::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	if ((Widget = NewObject<UWidgetComponent>(this, MakeUniqueObjectName(this, UWidgetComponent::StaticClass(), TEXT("LockOnTarget_TargetPreview_Widget")), RF_Transient)))
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
	}
}

void UTargetPreviewModule::OnWidgetClassLoaded()
{
	if (IsWidgetInitialized() && StreamableHandle.IsValid() && StreamableHandle->HasLoadCompleted())
	{
		//@TODO: Ensure that we've got the right class.
		Widget->SetWidgetClass(StaticCast<UClass*>(StreamableHandle->GetLoadedAsset()));
	}
}

void UTargetPreviewModule::Deinitialize(ULockOnTargetComponent* Instigator)
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

bool UTargetPreviewModule::IsWidgetInitialized() const
{
	return bWidgetIsInitialized && ensureMsgf(IsValid(Widget), TEXT("Widget was initialized but is invalid. Maybe it was removed manually."));
}

void UTargetPreviewModule::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);
	SetPreviewActive(false);
}

void UTargetPreviewModule::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	SetPreviewActive(true);
}

void UTargetPreviewModule::SetPreviewActive(bool bInActive)
{
	if (IsPreviewActive() != bInActive)
	{
		if (!bInActive || !GetLockOnTargetComponent()->IsTargetLocked())
		{
			bIsPreviewActive = bInActive;

			if (!IsPreviewActive())
			{
				StopTargetPreview(PreviewTarget);
				UpdateTimer = 0.f;
			}
		}
	}
}

void UTargetPreviewModule::Update(float DeltaTime)
{
	Super::Update(DeltaTime);

	if (IsPreviewActive() && IsWidgetInitialized() && GetController() && GetController()->IsLocalController() && GetLockOnTargetComponent()->CanCaptureTarget())
	{
		UpdateTimer += DeltaTime;

		if (UpdateTimer > UpdateRate)
		{
			UpdateTimer -= UpdateRate;
			UpdateTargetPreview();
		}
	}
}

void UTargetPreviewModule::UpdateTargetPreview()
{
	const ULockOnTargetComponent* const Owner = GetLockOnTargetComponent();

	if (UTargetHandlerBase* const TargetHandler = Owner->GetTargetHandler())
	{
		const FTargetInfo Preview = TargetHandler->FindTarget();

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
		LOG_WARNING("TargetPreviewModule failed to find TargetHandler in %s", *GetFullNameSafe(Owner));
	}
}

void UTargetPreviewModule::BeginTargetPreview(const FTargetInfo& Target)
{
	PreviewTarget = Target;

	if (IsWidgetInitialized())
	{
		if (const auto* const PC = GetPlayerController())
		{
			Widget->SetOwnerPlayer(PC->GetLocalPlayer());
		}

		Widget->AttachToComponent(Target.TargetComponent->GetTrackedMeshComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Target.Socket);
		Widget->SetVisibility(true);
		Widget->SetRelativeLocation(Target.TargetComponent->WidgetRelativeOffset);
	}
}

void UTargetPreviewModule::StopTargetPreview(const FTargetInfo& Target)
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
