// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLockOnTargetEditor, All, All);

class FSlateStyleSet;

class FLockOnTargetEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterStyles();
	void UnregisterStyles();
	TSharedPtr<FSlateStyleSet> LockOnTargetStyleSet;
};
