// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(Log_LOC_Editor, All, All);

class FLockOnTargetEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<class FSlateStyleSet> LockOnStyleSet;
	TSharedPtr<FSlateStyleSet> TargetingHelperStyleSet;
};
