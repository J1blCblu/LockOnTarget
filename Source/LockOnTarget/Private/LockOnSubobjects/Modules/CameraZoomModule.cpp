// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/Modules/CameraZoomModule.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"
#include "TemplateUtilities.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"

UCameraZoomModule::UCameraZoomModule()
	: ZoomCurve(FSoftClassPath(FString(TEXT("CurveFloat'/LockOnTarget/LOT_FOV_Curve.LOT_FOV_Curve'"))))
	, ActiveCamera(nullptr)
	, CachedZoomValue(0.f)
{
	bWantsUpdate = true;
	bUpdateOnlyWhileLocked = false;
}

void UCameraZoomModule::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	if (AActor* const Owner = GetLockOnTargetComponent()->GetOwner())
	{
		TInlineComponentArray<UCameraComponent*> Cameras(Owner);

		for (UCameraComponent* const Camera : Cameras)
		{
			if(Camera && Camera->IsActive())
			{
				ActiveCamera = Camera;
				break;
			}
		}
	}

	if(ActiveCamera.IsValid())
	{
		checkf(!ZoomCurve.IsNull(), TEXT("Zoom curve isn't specified."));

		Timeline.AddInterpFloat(ZoomCurve.LoadSynchronous(), FOnTimelineFloatStatic::CreateUObject(this, &ThisClass::UpdateTimeline));
	}
	else
	{
		LOG_WARNING("CameraComponent isn't found.");
	}
}

void UCameraZoomModule::Deinitialize(ULockOnTargetComponent* Instigator)
{
	ClearZoomOnActiveCamera();

	Super::Deinitialize(Instigator);
}

void UCameraZoomModule::ClearZoomOnActiveCamera()
{
	if (ActiveCamera.IsValid() && Timeline.GetPlaybackPosition() > 0.f)
	{
		ActiveCamera->FieldOfView -= CachedZoomValue;
	}
}

void UCameraZoomModule::OnTargetLocked(UTargetingHelperComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);

	if(GetLockOnTargetComponent()->IsLocallyControlled())
	{
		Timeline.Play();
	}
}

void UCameraZoomModule::OnTargetUnlocked(UTargetingHelperComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	
	if(GetLockOnTargetComponent()->IsLocallyControlled())
	{
		Timeline.Reverse();
	}
}

void UCameraZoomModule::UpdateOverridable(FVector2D PlayerInput, float DeltaTime)
{
	if (ActiveCamera.IsValid())
	{
		Timeline.TickTimeline(DeltaTime);
	}
}

void UCameraZoomModule::UpdateTimeline(float Value)
{
	if(ActiveCamera.IsValid())
	{
		const float CurrentFOV = ActiveCamera->FieldOfView;
		ActiveCamera->SetFieldOfView(CurrentFOV - CachedZoomValue + Value);
		CachedZoomValue = Value;
	}
}

void UCameraZoomModule::SetActiveCamera(UCameraComponent* InActiveCamera)
{
	if(IsValid(InActiveCamera) && InActiveCamera != ActiveCamera.Get())
	{
		ClearZoomOnActiveCamera();
		ActiveCamera = InActiveCamera;
	}
}
