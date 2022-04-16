// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "TargetingHelperComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StreamableManager.h"
#include "LockOnTargetComponent.h"

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
{
	//Tick isn't allowed due to the purpose of this component which acts as both storage and subject (for listeners).
	PrimaryComponentTick.bCanEverTick = false;

	//Request InitializeComponent().
	bWantsInitializeComponent = true;	//TODO: Maybe wrap with the preprocessor #if !WITH_SERVER_CODE to exclude widget creation on the server.

	Sockets.Add(NAME_None);

	FRichCurve* Curve = HeightOffsetCurve.GetRichCurve();
	Curve->SetKeyInterpMode(Curve->AddKey(0.f, 50.f), RCIM_Cubic);
	Curve->SetKeyInterpMode(Curve->AddKey(2000.f, -400.f), RCIM_Cubic);

	FSoftClassPath WidgetPath = TEXT("WidgetBlueprint'/LockOnTarget/WBP_Target.WBP_Target_C'");
	WidgetClass = TSoftClassPtr<UUserWidget>(WidgetPath);
}

void UTargetingHelperComponent::InitializeComponent()
{
	if(!GIsEditor || GIsPlayInEditorWorld)
	{
		WidgetComponent = bEnableWidget ? NewObject<UWidgetComponent>(this, MakeUniqueObjectName(this, UWidgetComponent::StaticClass(), TEXT("LOT_Widget"))) : nullptr;

		if (WidgetComponent)
		{
			WidgetComponent->RegisterComponent();
			WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
			WidgetComponent->SetVisibility(false);
			WidgetComponent->SetDrawAtDesiredSize(true);
		}
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

void UTargetingHelperComponent::CaptureTarget(ULockOnTargetComponent* Instigator, const FName& Socket)
{
	if (IsValid(Instigator))
	{
		bool bIsAlreadyBeen = false;
		Invaders.Add(Instigator, &bIsAlreadyBeen);
		
		if(!bIsAlreadyBeen)
		{
			OnOwnerCaptured.Broadcast(Instigator, Socket);
			UpdateWidget(Socket, Instigator);
		}
	}
}

void UTargetingHelperComponent::ReleaseTarget(ULockOnTargetComponent* Instigator)
{
	if (IsTargeted() && IsValid(Instigator) && Invaders.Remove(Instigator))
	{
		OnOwnerReleased.Broadcast(Instigator);
		HideWidget(Instigator);
	}
}

/*******************************************************************************************/
/*******************************  Widget Handling  *****************************************/
/*******************************************************************************************/

void UTargetingHelperComponent::UpdateWidget(const FName& Socket, ULockOnTargetComponent* Instigator)
{
	if (bEnableWidget && IsValid(Instigator) && Instigator->IsOwnerLocallyControlled())
	{
#if LOC_INSIGHTS
		SCOPED_NAMED_EVENT(LOC_UpdatingWidget, FColor::White);
#endif

		if (WidgetComponent)
		{
			if (!WidgetComponent->GetWidgetClass())
			{
				if (WidgetClass.IsNull())
				{
					UE_LOG(Log_LOC, Warning, TEXT("Widget class is nullptr. Fill the TargetingHelperComponent's WidgetClass field in the %s Actor or disable the bEnableWidget field."), *GetOwner()->GetName());
					return;
				}

				if (WidgetClass.IsPending())
				{
					bAsyncLoadWidget ? AsyncLoadWidget(MoveTemp(WidgetClass)) : SyncLoadWidget(WidgetClass);
				}
				else
				{
					WidgetComponent->SetWidgetClass(WidgetClass.Get());
				}
			}

			USceneComponent* ParentComp = Socket != NAME_None ? GetMeshComponent() : GetRootComponent();
			WidgetComponent->AttachToComponent(ParentComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
			WidgetComponent->SetRelativeLocation(WidgetOffset);
			WidgetComponent->SetVisibility(true);
		}
		else
		{
			UE_LOG(Log_LOC, Warning, TEXT("HelperComponent's UWidgetComponent is invalid in %s, maybe it was removed manualy."), *GetNameSafe(GetOwner()));
		}
	}
}

void UTargetingHelperComponent::HideWidget(ULockOnTargetComponent* Instigator)
{
	if (bEnableWidget && WidgetComponent && IsValid(Instigator) && Instigator->IsOwnerLocallyControlled())
	{
		WidgetComponent->SetVisibility(false);
	}
}

void UTargetingHelperComponent::SyncLoadWidget(TSoftClassPtr<UUserWidget>& Widget)
{
	if (Widget.IsPending())
	{
		Widget.LoadSynchronous();
	}

	WidgetComponent->SetWidgetClass(Widget.Get());
}

void UTargetingHelperComponent::AsyncLoadWidget(TSoftClassPtr<UUserWidget> Widget)
{
	if (Widget.IsPending())
	{
		static FStreamableManager Manager;
		TWeakObjectPtr<UTargetingHelperComponent> This = this;
		FSoftObjectPath SoftPath = Widget.ToSoftObjectPath();

		auto Callback = [Widget = MoveTemp(Widget), This = MoveTemp(This)]()
		{
			if (This.IsValid() && IsValid(This->WidgetComponent))
			{
				This->WidgetComponent->SetWidgetClass(Widget.Get());
			}
		};

		Manager.RequestAsyncLoad(SoftPath, MoveTemp(Callback));
	}
	else
	{
		WidgetComponent->SetWidgetClass(Widget.Get());
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

FVector UTargetingHelperComponent::GetSocketLocation(const FName& Socket, bool bWithOffset, const ULockOnTargetComponent* Instigator) const
{
	FVector Location;

	if (Socket == NAME_None && GetOwner())
	{
		Location = GetOwner()->GetActorLocation();
	}
	else if (OwnerMeshComponent.IsValid())
	{
		Location = OwnerMeshComponent->GetSocketLocation(Socket);
	}

	if (bWithOffset)
	{
		AddOffset(Location, Instigator);
	}

	return Location;
}

void UTargetingHelperComponent::AddOffset(FVector& Location, const ULockOnTargetComponent* Instigator) const
{
	switch (OffsetType)
	{
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
	}
}

void UTargetingHelperComponent::InitMeshComponent() const
{
	if (!OwnerMeshComponent.IsValid())
	{
		OwnerMeshComponent = FindMeshComponentByName<UMeshComponent>(GetOwner(), MeshName);
	}
}

void UTargetingHelperComponent::UpdateMeshComponent(UMeshComponent* NewComponent)
{
	if (IsValid(NewComponent) && NewComponent != OwnerMeshComponent.Get())
	{
		OwnerMeshComponent = NewComponent;
	}
}

USceneComponent* UTargetingHelperComponent::GetMeshComponent() const
{
	return OwnerMeshComponent.IsValid() ? OwnerMeshComponent.Get() : GetRootComponent();
}

USceneComponent* UTargetingHelperComponent::GetFirstMeshComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMeshComponent>() : nullptr;
}

USceneComponent* UTargetingHelperComponent::GetRootComponent() const
{
	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}
