// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"

class LOCKONTARGETEDITOR_API FHelperVisualizer : public FComponentVisualizer
{
protected:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* SceneView, FPrimitiveDrawInterface* DrawInterface) override;

};
