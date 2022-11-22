// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "TargetingHelperComponent.h"
#include "LockOnTargetComponent.h"
#include "LockOnTargetDefines.h"
#include "TemplateUtilities.h"

#include "Components/MeshComponent.h"
#include "Engine/SimpleConstructionScript.h"

UTargetingHelperComponent::UTargetingHelperComponent()
	: bCanBeCaptured(true)
	, MeshName(NAME_None)
	, CaptureRadius(1700.f)
	, LostOffsetRadius(100.f)
	, MinimumCaptureRadius(100.f)
	, FocusPoint(EFocusPoint::Default)
	, FocusPointCustomSocket(NAME_None)
	, bFocusPointOffsetInCameraSpace(false)
	, FocusPointOffset(0.f)
	, bWantsDisplayWidget(true)
	, WidgetRelativeOffset(0.f)
	, bWantsMeshInitialization(true)
{
	//Tick isn't allowed due to the purpose of this component which acts as a storage.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	Sockets.Add(NAME_None);
}

void UTargetingHelperComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bWantsMeshInitialization)
	{
		DesiredMeshComponent = MeshName == NAME_None ? GetRootComponent() : FindComponentByName<UMeshComponent>(GetOwner(), MeshName);
	}
}

void UTargetingHelperComponent::EndPlay(EEndPlayReason::Type Reason)
{
	//In an event-driven design, we have to disable bCanBeCaptured to avoid being captured again.
	//Previously, we handled this on the next tick in LockOnTargetComponent.
	bCanBeCaptured = false;
	OnTargetEndPlay.Broadcast(this, Reason);
	Super::EndPlay(Reason);
}

bool UTargetingHelperComponent::CanBeCaptured(const ULockOnTargetComponent* Instigator) const
{
	return  bCanBeCaptured && (Sockets.Num() > 0) && CanBeCapturedCustom(Instigator);
}

bool UTargetingHelperComponent::CanBeCapturedCustom_Implementation(const ULockOnTargetComponent* Instigator) const
{
	return true;
}

void UTargetingHelperComponent::CaptureTarget(ULockOnTargetComponent* Instigator, FName Socket)
{
	if (IsValid(Instigator))
	{
		bool bHasAlreadyBeen = false;
		Invaders.Add(Instigator, &bHasAlreadyBeen);

		if (!bHasAlreadyBeen)
		{
			//Most likely you removed the socket on one machine, and captured on another.
			checkf(Sockets.Contains(Socket), TEXT("Captured socket doesn't exists in the Target."));
			OnOwnerCaptured.Broadcast(Instigator, Socket);
			K2_OnCaptured(Instigator, Socket);
		}
	}
	else
	{
		LOG_WARNING("Invalid Instigator. Failed to capture the %s", *GetNameSafe(GetOwner()));
	}
}

void UTargetingHelperComponent::ReleaseTarget(ULockOnTargetComponent* Instigator)
{
	if (IsCaptured() && IsValid(Instigator) && Invaders.Remove(Instigator))
	{
		OnOwnerReleased.Broadcast(Instigator);
		K2_OnReleased(Instigator);
	}
}

USceneComponent* UTargetingHelperComponent::GetDesiredMesh() const
{
	return DesiredMeshComponent.IsValid() ? DesiredMeshComponent.Get() : GetRootComponent();
}

FVector UTargetingHelperComponent::GetSocketLocation(FName Socket) const
{
	FVector Location{ 0.f };

	if (DesiredMeshComponent.IsValid() && MeshName != NAME_None)
	{
		Location = DesiredMeshComponent->GetSocketLocation(Socket);
	}
	else if (GetOwner())
	{
		Location = GetOwner()->GetActorLocation();
	}
	else
	{
		LOG_WARNING("Failed to calculate the location.");
	}

	return Location;
}

FVector UTargetingHelperComponent::GetFocusLocation(const ULockOnTargetComponent* Instigator) const
{
	FVector FocusPointLocation{ 0.f };

	switch (FocusPoint)
	{
	case EFocusPoint::Default:
	{
		FocusPointLocation = GetSocketLocation(IsValid(Instigator) ? Instigator->GetCapturedSocket() : NAME_None);
		break;
	}
	case EFocusPoint::CustomSocket:
	{
		FocusPointLocation = GetSocketLocation(FocusPointCustomSocket);
		break;
	}
	case EFocusPoint::Custom:
	{
		FocusPointLocation = GetCustomFocusPoint(Instigator);
		break;
	}
	default:
	{
		LOG_ERROR("Unknown EFocusPoint is discovered.");
		checkNoEntry();
		break;
	}
	}

	if (bFocusPointOffsetInCameraSpace)
	{
		constexpr FVector::FReal Threshold = 1e-2;

		if (FocusPointOffset.Z > Threshold || FocusPointOffset.Y > Threshold || FocusPointOffset.X > Threshold)
		{
			FRotationMatrix RotMatrix(Instigator->GetCameraRotation());

			FocusPointLocation += FocusPointOffset.X * RotMatrix.GetUnitAxis(EAxis::X);
			FocusPointLocation += FocusPointOffset.Y * RotMatrix.GetUnitAxis(EAxis::Y);
			FocusPointLocation += FocusPointOffset.Z * RotMatrix.GetUnitAxis(EAxis::Z);
		}
	}
	else
	{
		FocusPointLocation += FocusPointOffset;
	}

	return FocusPointLocation;
}

FVector UTargetingHelperComponent::GetCustomFocusPoint_Implementation(const ULockOnTargetComponent* Instigator) const
{
	LOG_WARNING("Default implementation is called. Please override in child classes.");
	return FVector{ 0.f };
}

bool UTargetingHelperComponent::AddSocket(FName Socket)
{
	bool bIsAlreadySet = true;

	if (GetDesiredMesh() && GetDesiredMesh()->DoesSocketExist(Socket))
	{
		Sockets.Add(Socket, &bIsAlreadySet);
	}

	return !bIsAlreadySet;
}

bool UTargetingHelperComponent::RemoveSocket(FName Socket)
{
	bool bResult = Sockets.Remove(Socket) > 0;

	if(bResult)
	{
		OnSocketRemoved.Broadcast(Socket);
	}

	return bResult;
}

void UTargetingHelperComponent::UpdateDesiredMesh(UMeshComponent* NewComponent)
{
	if (IsValid(NewComponent) && NewComponent != DesiredMeshComponent.Get())
	{
		DesiredMeshComponent = NewComponent;

		if (!HasBegunPlay())
		{
			//Don't initialize MeshComponent by name if it's set before initialization. 
			bWantsMeshInitialization = false;
		}
	}
}

USceneComponent* UTargetingHelperComponent::GetRootComponent() const
{
	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

#if WITH_EDITOR

TArray<FString> UTargetingHelperComponent::GetAvailableSockets() const
{
	//@TODO: Maybe use inline allocator.
	TArray<FString> AvailableSockets = { FString(TEXT("None")) };
	AvailableSockets.Reserve(50);
	const UMeshComponent* Mesh = nullptr;

	if (HasAnyFlags(RF_ArchetypeObject))
	{
		//We can't just find components added in the BP editor. We need to find them in BP generated class.
		//In the actor BP editor an outer is an owner class.
		if (const UBlueprintGeneratedClass* const BPGC = Cast<UBlueprintGeneratedClass>(GetOuter()))
		{
			if (const USimpleConstructionScript* const SCS = BPGC->SimpleConstructionScript)
			{
				Mesh = FindComponentByName<UMeshComponent>(SCS->GetComponentEditorActorInstance(), MeshName);
			}
		}
	}
	else
	{
		//Owner, placed in the world, has all components, including those added in BP editor.
		Mesh = FindComponentByName<UMeshComponent>(GetOwner(), MeshName);
	}

	if (Mesh)
	{
		const TArray<FName> SocketsName = Mesh->GetAllSocketNames();

		for (const FName Socket : SocketsName)
		{
			AvailableSockets.Add(Socket.ToString());
		}
	}

	return AvailableSockets;
}

TArray<FString> UTargetingHelperComponent::GetAvailableMeshes() const
{
	//@TODO: Maybe use inline allocator.
	TArray<FString> Meshes = { FString(TEXT("None")) };
	Meshes.Reserve(10);
	const AActor* Owner = nullptr;

	if (HasAnyFlags(RF_ArchetypeObject))
	{
		//We can't just find components added in the BP editor. We need to find them in BP generated class.
		//In the actor BP editor an outer is an owner class.
		if (const UBlueprintGeneratedClass* const BPGC = Cast<UBlueprintGeneratedClass>(GetOuter()))
		{
			if (const USimpleConstructionScript* const SCS = BPGC->SimpleConstructionScript)
			{
				Owner = SCS->GetComponentEditorActorInstance();
			}
		}
	}
	else
	{
		//Owner, placed in the world, has all components, including those added in BP editor.
		Owner = GetOwner();
	}

	const TInlineComponentArray<UMeshComponent*> MeshComponents(Owner);

	for (const UMeshComponent* const MeshComponent : MeshComponents)
	{
		if (MeshComponent && !MeshComponent->IsEditorOnly())
		{
			Meshes.Add(MeshComponent->GetFName().ToString());
		}
	}

	return Meshes;
}

#endif
