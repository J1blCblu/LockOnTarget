// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "ComponentVisualizer/HelperVisualizer.h"
#include "TargetingHelperComponent.h"
#include "SceneManagement.h"
#include "Gameframework/Actor.h"

void FHelperVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* SceneView, FPrimitiveDrawInterface* DrawInterface)
{
	const auto* HC = StaticCast<const UTargetingHelperComponent*>(Component);

	if (IsValid(HC) && HC->bVisualizeDebugInfo)
	{
		const FTransform Transform = {FRotator(), HC->GetOwner()->GetActorLocation(), FVector::OneVector};

		DrawWireSphere(DrawInterface, Transform, FColor::Green, HC->CaptureRadius, 32, SDPG_Foreground, 3.f);
		DrawWireSphere(DrawInterface, Transform, FColor::Yellow, HC->CaptureRadius + HC->LostOffsetRadius, 32, SDPG_Foreground, 3.f);
		DrawWireSphere(DrawInterface, Transform, FColor::Red, HC->MinimumCaptureRadius, 32, SDPG_Foreground, 3.f);
	}
}
