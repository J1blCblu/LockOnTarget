// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnSubobjects/Modules/CameraZoomModule.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"
#include "TemplateUtilities.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"

constexpr float DefaultUninitializedFOV = 90.f;

UCameraZoomModule::UCameraZoomModule()
	: CameraName(NAME_None)
	, bUseAbsoluteValue(false)
	, Camera(nullptr)
	, DefaultFOV(DefaultUninitializedFOV)
{
	bWantsUpdate = true;
	bUpdateOnlyWhileLocked = false;

	if(FRichCurve* const Curve = FieldOfViewCurve.GetRichCurve())
	{
		Curve->AddKey(0.f, 0.f);
		Curve->AddKey(0.25f, -3.f);
	}
}

void UCameraZoomModule::Initialize(ULockOnTargetComponent* Instigator)
{
	Super::Initialize(Instigator);

	if (AActor* const Owner = GetLockOnTargetComponent()->GetOwner())
	{
		Camera = CameraName == NAME_None ? Owner->FindComponentByClass<UCameraComponent>() : FindComponentByName<UCameraComponent>(Owner, CameraName);

		if(Camera.IsValid())
		{
			DefaultFOV = Camera->FieldOfView;
			//Timeline doesn't accept FRichCurve.
			UCurveFloat* const Curve = NewObject<UCurveFloat>(this, FName(TEXT("ZoomCurve_Temp")));
			Curve->FloatCurve = *FieldOfViewCurve.GetRichCurve();
			Timeline.AddInterpFloat(Curve, FOnTimelineFloatStatic::CreateUObject(this, &UCameraZoomModule::UpdateTimeline));
		}
		else
		{
			if(CameraName != NAME_None)
			{
				LOG_WARNING("Failed to find the camera named %s", *CameraName.ToString());
			}
		}
	}
}

void UCameraZoomModule::Deinitialize(ULockOnTargetComponent* Instigator)
{
	if (Camera.IsValid() && Timeline.GetPlaybackPosition() > 0.f)
	{
		//Update the Camera FOV manual.
		if(const FRichCurve* const Curve = FieldOfViewCurve.GetRichCurveConst())
		{
			UpdateTimeline(Curve->Eval(0.f));
		}
	}

	Super::Deinitialize(Instigator);
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
	if (Camera.IsValid())
	{
		Timeline.TickTimeline(DeltaTime);
	}
}

void UCameraZoomModule::UpdateTimeline(float Value)
{
	Camera->FieldOfView = bUseAbsoluteValue ? Value : DefaultFOV + Value;
}
