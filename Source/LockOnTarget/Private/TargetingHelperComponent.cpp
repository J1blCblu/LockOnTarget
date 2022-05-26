// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "TargetingHelperComponent.h"
#include "LockOnTargetComponent.h"
#include "LockOnTarget/LockOnTarget.h"
#include "TemplateUtilities.h"

#include "Components/WidgetComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"

UTargetingHelperComponent::UTargetingHelperComponent()
	: bCanBeTargeted(true)
	, MeshName(NAME_None)
	, CaptureRadius(1700.f)
	, LostOffsetRadius(100.f)
	, MinimumCaptureRadius(100.f)
	, TargetOffset(0.f)
	, bEnableWidget(true)
	, WidgetOffset(0.f)
	, bAsyncLoadWidget(true)
	, bWidgetWasInitialized(false)
	, bWantsMeshInitialization(true)
{
	//Tick isn't allowed due to the purpose of this component which acts as both storage and subject (for listeners).
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	//Request InitializeComponent().
	bWantsInitializeComponent = true;	//@TODO: Maybe wrap with the preprocessor #if !WITH_SERVER_CODE to exclude widget creation on the server.

	Sockets.Add(NAME_None);

	FRichCurve* Curve = HeightOffsetCurve.GetRichCurve();
	Curve->SetKeyInterpMode(Curve->AddKey(0.f, 50.f), RCIM_Cubic);
	Curve->SetKeyInterpMode(Curve->AddKey(2000.f, -400.f), RCIM_Cubic);

	FSoftClassPath WidgetPath = FString(TEXT("WidgetBlueprint'/LockOnTarget/WBP_Target.WBP_Target_C'"));
	WidgetClass = TSoftClassPtr<UUserWidget>(WidgetPath);
}

void UTargetingHelperComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (!GIsEditor || GIsPlayInEditorWorld)
	{
		InitWidgetComponent();
	}
}

void UTargetingHelperComponent::BeginPlay()
{
	Super::BeginPlay();
	InitMeshComponent();
}

void UTargetingHelperComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	//Notify listeners.
	for (ULockOnTargetComponent* Invader : Invaders)
	{
		OnOwnerReleased.Broadcast(Invader);
	}

	Invaders.Empty();
}

/*******************************************************************************************/
/*******************************  Component Interface  *************************************/
/*******************************************************************************************/

void UTargetingHelperComponent::CaptureTarget(ULockOnTargetComponent* const Instigator, const FName& Socket)
{
	if (IsValid(Instigator))
	{
		bool bHasAlreadyBeen = false;
		Invaders.Add(Instigator, &bHasAlreadyBeen);

		UpdateWidget(Socket, Instigator);

		if (!bHasAlreadyBeen)
		{
			OnOwnerCaptured.Broadcast(Instigator, Socket);
			OnCaptured();
		}
	}
}

void UTargetingHelperComponent::ReleaseTarget(ULockOnTargetComponent* const Instigator)
{
	if (IsTargeted() && IsValid(Instigator) && Invaders.Remove(Instigator))
	{
		OnOwnerReleased.Broadcast(Instigator);
		HideWidget(Instigator);
		OnReleased();
	}
}

/*******************************************************************************************/
/*******************************  Widget Handling  *****************************************/
/*******************************************************************************************/

void UTargetingHelperComponent::InitWidgetComponent()
{
	if (bEnableWidget)
	{
		WidgetComponent = NewObject<UWidgetComponent>(this, MakeUniqueObjectName(this, UWidgetComponent::StaticClass(), TEXT("LOT_Widget")));

		if (IsValid(WidgetComponent))
		{
			WidgetComponent->RegisterComponent();
			WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
			WidgetComponent->SetVisibility(false);
			WidgetComponent->SetDrawAtDesiredSize(true);
			bWidgetWasInitialized = true;
		}
	}
}

bool UTargetingHelperComponent::IsWidgetInitialized() const
{
	return bWidgetWasInitialized && ensureMsgf(IsValid(WidgetComponent), TEXT("TargetingHelperComponent: WidgetComponent was initialized but is invalid in the '%s'. Maybe it was removed manually."), *GetNameSafe(GetOwner()));
}

void UTargetingHelperComponent::UpdateWidget(const FName& Socket, const ULockOnTargetComponent* const Instigator)
{
#if LOC_INSIGHTS
	SCOPED_NAMED_EVENT(LOT_UpdatingWidget, FColor::White);
#endif

	if (!IsWidgetInitialized())
	{
		return;
	}

	if (!IsValid(Instigator) || !Instigator->IsOwnerLocallyControlled())
	{
		return;
	}

	if(WidgetClass)
	{
		SetWidgetClassOnWidgetComponent();
	}
	else
	{
		if (WidgetClass.IsNull())
		{
			UE_LOG(LogLockOnTarget, Warning, TEXT("TargetingHelperComponent: Widget class is nullptr. Fill the WidgetClass field in the %s Actor or disable the bEnableWidget field."), *GetNameSafe(GetOwner()));
			return;
		}

		if (WidgetClass.IsPending())
		{
			if (bAsyncLoadWidget)
			{
				UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(WidgetClass.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &ThisClass::SetWidgetClassOnWidgetComponent));
			}
			else
			{
				WidgetClass.LoadSynchronous();
				SetWidgetClassOnWidgetComponent();
			}
		}
	}

	WidgetComponent->AttachToComponent(GetWidgetParentComponent(Socket), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	WidgetComponent->SetRelativeLocation(WidgetOffset);
	WidgetComponent->SetVisibility(true);
}

USceneComponent* UTargetingHelperComponent::GetWidgetParentComponent(const FName& Socket) const
{
	return OwnerMeshComponent.IsValid() && Socket != NAME_None ? OwnerMeshComponent.Get() : GetRootComponent();
}

void UTargetingHelperComponent::SetWidgetClassOnWidgetComponent()
{
	if (IsWidgetInitialized())
	{
		if (WidgetClass.Get())
		{
			WidgetComponent->SetWidgetClass(WidgetClass.Get());
		}
		else
		{
			UE_LOG(LogLockOnTarget, Error, TEXT("TargetingHelperComponent: Failed to load '%s' WidgetClass in the %s"), *WidgetClass.GetLongPackageName(), *GetNameSafe(GetOwner()));
		}
	}
}

void UTargetingHelperComponent::HideWidget(const ULockOnTargetComponent* const Instigator)
{
	if (IsWidgetInitialized() && IsValid(Instigator) && Instigator->IsOwnerLocallyControlled())
	{
		WidgetComponent->SetVisibility(false);
	}
}

/*******************************************************************************************/
/*******************************  Other Methods  *******************************************/
/*******************************************************************************************/

bool UTargetingHelperComponent::CanBeTargeted_Implementation(ULockOnTargetComponent* Instigator) const
{
	return bCanBeTargeted && (Sockets.Num() > 0);
}

bool UTargetingHelperComponent::AddSocket(const FName& Socket)
{
	bool bReturn = false;

	if (OwnerMeshComponent.IsValid() && OwnerMeshComponent->DoesSocketExist(Socket))
	{
		Sockets.Add(Socket, &bReturn);
	}

	return bReturn;
}

bool UTargetingHelperComponent::RemoveSocket(const FName& Socket)
{
	return Sockets.Remove(Socket) > 0;
}

FVector UTargetingHelperComponent::GetSocketLocation(const FName& Socket, bool bWithOffset, const ULockOnTargetComponent* const Instigator) const
{
	FVector Location;

	if (OwnerMeshComponent.IsValid() && Socket != NAME_None)
	{
		Location = OwnerMeshComponent->GetSocketLocation(Socket);
	}
	else if (GetOwner())
	{
		Location = GetOwner()->GetActorLocation();
	}
	else
	{
		UE_LOG(LogLockOnTarget, Error, TEXT("TargetingHelperComponent: Failed to find a socket location. The owner is invalid."));
	}

	if (bWithOffset)
	{
		AddOffset(Location, Instigator);
	}

	return Location;
}

void UTargetingHelperComponent::AddOffset(FVector& Location, const ULockOnTargetComponent* const Instigator) const
{
	switch (OffsetType)
	{
	case EOffsetType::ENone:
	{
		break;
	}
	case EOffsetType::EConstant:
	{
		if (IsValid(Instigator))
		{
			Location += TargetOffset.X * Instigator->GetCameraForwardVector();
			Location += TargetOffset.Y * Instigator->GetCameraRightVector();
			Location += TargetOffset.Z * Instigator->GetCameraUpVector();
		}

		break;
	}
	case EOffsetType::EAdaptiveCurve:
	{
		if (IsValid(Instigator))
		{
			Location += Instigator->GetCameraUpVector() * HeightOffsetCurve.GetRichCurveConst()->Eval((Location - Instigator->GetOwnerLocation()).Size());
		}

		break;
	}
	case EOffsetType::ECustomOffset:
	{
		Location += GetCustomTargetOffset(Instigator);
		break;
	}
	default:
	{
		UE_LOG(LogLockOnTarget, Error, TEXT("TargetingHelperComponent: Unknown EOffsetType is discovered."));
		checkNoEntry();
		break;
	}
	}
}

/*******************************************************************************************/
/*******************************  Mesh Initialization  *************************************/
/*******************************************************************************************/

void UTargetingHelperComponent::InitMeshComponent()
{
	if (bWantsMeshInitialization)
	{
		OwnerMeshComponent = FindComponentByName<UMeshComponent>(GetOwner(), MeshName);

		if (!OwnerMeshComponent.IsValid())
		{
			OwnerMeshComponent = GetFirstMeshComponent();

			if (MeshName != NAME_None)
			{
				UE_LOG(LogLockOnTarget, Warning, TEXT("TargetingHelperComponent: The MeshComponent named '%s' doesn't exist in the '%s', '%s' will be used instead."), *MeshName.ToString(), *GetNameSafe(GetOwner()), *GetNameSafe(OwnerMeshComponent.Get()));
			}
		}
	}
}

void UTargetingHelperComponent::SetMeshComponent(UMeshComponent* NewComponent)
{
	if (IsValid(NewComponent) && NewComponent != OwnerMeshComponent.Get())
	{
		OwnerMeshComponent = NewComponent;

		if(HasBegunPlay())
		{
			//Socket location is updated on every frame so it's safe to change the MeshComponent if the Owner is captured.
			//But we need to manually update the widget.
			for(const auto* const Invader : Invaders)
			{
				UpdateWidget(Invader->GetCapturedSocket(), Invader);
			}
		}
		else
		{
			//Don't initialize MeshComponent by name if it's set before initialization. 
			bWantsMeshInitialization = 0;
		}
	}
}

UMeshComponent* UTargetingHelperComponent::GetFirstMeshComponent() const
{
	TInlineComponentArray<UMeshComponent*> ComponentsArray(GetOwner());

	//Exclude the TargetWidget from result.
	//@TODO: Maybe exclude it by name.
	UMeshComponent** MeshComponent = ComponentsArray.FindByPredicate([](const auto* const Component)
		{
			return !Component->IsA(UWidgetComponent::StaticClass());
		});

	return MeshComponent ? *MeshComponent : nullptr;
}

USceneComponent* UTargetingHelperComponent::GetRootComponent() const
{
	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}
