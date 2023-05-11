// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "DefaultModules/CameraZoomModule.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Curves/CurveFloat.h"

UCameraZoomModule::UCameraZoomModule()
	: ZoomCurve(FSoftClassPath(FString(TEXT("CurveFloat'/LockOnTarget/LOT_FOV_Curve.LOT_FOV_Curve'"))))
	, ActiveCamera(nullptr)
	, CachedZoomValue(0.f)
{
	//Do something.
}

void UCameraZoomModule::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	if (AActor* const Owner = Instigator->GetOwner())
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

		if (UCurveFloat* const Curve = ZoomCurve.LoadSynchronous())
		{
			Timeline.AddInterpFloat(Curve, FOnTimelineFloatStatic::CreateUObject(this, &ThisClass::UpdateTimeline));
		}
	}
	else
	{
		LOG_WARNING("Failed to find any CameraComponent.");
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
		Timeline.SetPlaybackPosition(0.f, false, false);
		CachedZoomValue = 0.f;
	}
}

void UCameraZoomModule::OnTargetLocked(UTargetComponent* Target, FName Socket)
{
	Super::OnTargetLocked(Target, Socket);

	if (GetController() && GetController()->IsLocalController())
	{
		Timeline.Play();
	}
}

void UCameraZoomModule::OnTargetUnlocked(UTargetComponent* UnlockedTarget, FName Socket)
{
	Super::OnTargetUnlocked(UnlockedTarget, Socket);
	
	if(Timeline.GetPlaybackPosition() > 0.f)
	{
		Timeline.Reverse();
	}
}

void UCameraZoomModule::Update(float DeltaTime)
{
	Super::Update(DeltaTime);

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
