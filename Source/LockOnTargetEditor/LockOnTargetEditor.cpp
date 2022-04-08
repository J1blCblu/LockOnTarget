// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "LockOnTargetEditor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "DetailsCustomization/LockOnComponentDetails.h"
#include "TargetingHelperComponent.h"
#include "LockOnTargetComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "ComponentVisualizer/HelperVisualizer.h"

DEFINE_LOG_CATEGORY(Log_LOC_Editor);

#define LOCTEXT_NAMESPACE "FLockOnTargetModule"

void FLockOnTargetEditorModule::StartupModule()
{
	UE_LOG(Log_LOC_Editor, Log, TEXT("LockOnTarget_Editor startup."));
	//FString ContentDir = IPluginManager::Get().FindPlugin("LockOnTarget")->GetBaseDir();
	
	/** Icons and Thumbnail */
	FString ContentRoot = FPaths::EngineContentDir() / TEXT("Editor/Slate/Icons/AssetIcons");

	LockOnStyleSet = MakeShareable(new FSlateStyleSet(TEXT("LockOnTargetStyleSet")));
	LockOnStyleSet->SetContentRoot(ContentRoot);
	LockOnStyleSet->Set("ClassIcon.LockOnTargetComponent", new FSlateImageBrush(LockOnStyleSet->RootToContentDir(TEXT("ApplicationLifecycleComponent_16x.png")), {16.f, 16.f}));
	LockOnStyleSet->Set("ClassThumbnail.LockOnTargetComponent", new FSlateImageBrush(LockOnStyleSet->RootToContentDir(TEXT("ApplicationLifecycleComponent_64x.png")), {64.f, 64.f}));
	FSlateStyleRegistry::RegisterSlateStyle(*LockOnStyleSet.Get());

	TargetingHelperStyleSet = MakeShareable(new FSlateStyleSet(TEXT("TargetHelperStyleSet")));
	TargetingHelperStyleSet->SetContentRoot(ContentRoot);
	TargetingHelperStyleSet->Set("ClassIcon.TargetingHelperComponent", new FSlateImageBrush(TargetingHelperStyleSet->RootToContentDir(TEXT("TargetPoint_16x.png")), {16.f, 16.f}));
	TargetingHelperStyleSet->Set("ClassThumbnail.TargetingHelperComponent", new FSlateImageBrush(TargetingHelperStyleSet->RootToContentDir(TEXT("TargetPoint_64x.png")), {64.f, 64.f}));
	FSlateStyleRegistry::RegisterSlateStyle(*TargetingHelperStyleSet.Get());
	/** ~Icons and Thumbnail */

	/** Register Details Customization */
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomClassLayout(ULockOnTargetComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLockOnComponentDetails::MakeInstance));
	/** ~Register Details Customization */

	/** Register Component Visualizer */
	if (GUnrealEd)
	{
		TSharedPtr<FHelperVisualizer> HelperVis = MakeShared<FHelperVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UTargetingHelperComponent::StaticClass()->GetFName(), HelperVis);
		HelperVis->OnRegister();
	}
	/** ~Register Component visualizer */
}

void FLockOnTargetEditorModule::ShutdownModule()
{
	UE_LOG(Log_LOC_Editor, Log, TEXT("LockOnTarget_Editor shutdown."));

	/** Icons and Thumbnails */
	FSlateStyleRegistry::UnRegisterSlateStyle(*LockOnStyleSet.Get());
	FSlateStyleRegistry::UnRegisterSlateStyle(*TargetingHelperStyleSet.Get());
	/** ~Icons and Thumbnails */

	/** UnRegister Details Customization */
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.UnregisterCustomClassLayout(ULockOnTargetComponent::StaticClass()->GetFName());
	/** ~UnRegister Details Customization */

	/** UnRegister Component Visualizer */
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UTargetingHelperComponent::StaticClass()->GetFName());
	}
	/** ~UnRegister Component Visualizer */
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLockOnTargetEditorModule, LockOnTargetEditor)
