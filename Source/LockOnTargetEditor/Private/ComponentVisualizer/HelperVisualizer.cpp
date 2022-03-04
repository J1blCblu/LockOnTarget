// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#include "ComponentVisualizer/HelperVisualizer.h"
#include "TargetingHelperComponent.h"

void FHelperVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* SceneView, FPrimitiveDrawInterface* DrawInterface)
{
	const UTargetingHelperComponent* HC = CastChecked<UTargetingHelperComponent>(Component);
	if (HC->bVisualizeDebugInfo)
	{
		HC->InitMeshComponent();

		for (const FName& Socket : HC->Sockets)
		{
			DrawInterface->DrawPoint(HC->GetSocketLocation(Socket), FColor::White, 15.f, SDPG_Foreground);
		}

		const FTransform Transform = {FRotator(), HC->GetOwner()->GetActorLocation(), FVector::OneVector};
		DrawWireSphere(DrawInterface, Transform, FColor::Green, HC->CaptureRadius, 32, SDPG_Foreground, 3.f);
		DrawWireSphere(DrawInterface, Transform, FColor::Red, HC->LostRadius, 32, SDPG_Foreground, 3.f);
	}
}
