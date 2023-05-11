// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "LockOnComponentDetails.h"
#include "LockOnTargetComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SHyperlink.h"

#define LOCTEXT_NAMESPACE "LOC_DetailsCustomization"

TSharedRef<IDetailCustomization> FLockOnComponentDetails::MakeInstance()
{
	return MakeShared<FLockOnComponentDetails>();
}

void FLockOnComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const FText LOT_Documentation = LOCTEXT("LOT_Doc", "Documentation");
	const FText LOT_Doc_Initial = LOCTEXT("LOT_Doc_Initial", "Initial Setup");
	const FText LOT_Doc_LockOn = LOCTEXT("LOT_Doc_LockOn", "Component Overview");
	const FText LOT_Doc_Debugger = LOCTEXT("LOT_Doc_Debugger", "Debugger");
	
	IDetailCategoryBuilder& DescriptionCategory = DetailLayout.EditCategory(TEXT("Description"), LOCTEXT("LOT_DescRow", "Description"), ECategoryPriority::Important);

	DescriptionCategory.AddCustomRow(LOCTEXT("LOT_ComponentDesc", "DescriptionRow"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOT_Documentation)
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(5.f, 2.f)
			.AutoWidth()
			[
				SNew(SHyperlink)
				.Text(LOT_Doc_Initial)
				.OnNavigate(this, &FLockOnComponentDetails::OnInitialSetupNavigate)
			]

			+ SHorizontalBox::Slot()
			.Padding(5.f, 2.f)
			.AutoWidth()
			[
				SNew(SHyperlink)
				.Text(LOT_Doc_LockOn)
				.OnNavigate(this, &FLockOnComponentDetails::OnLockOnOverviewNavigate)
			]

			+ SHorizontalBox::Slot()
			.Padding(5.f, 2.f)
			.AutoWidth()
			[
				SNew(SHyperlink)
				.Text(LOT_Doc_Debugger)
				.OnNavigate(this, &FLockOnComponentDetails::OnDebuggerNavigate)
			]
		];
}

void FLockOnComponentDetails::OnInitialSetupNavigate()
{
	FPlatformProcess::LaunchURL(TEXT("https://github.com/J1blCblu/LockOnTarget/wiki/1.-Initial-Setup"), nullptr, nullptr);
}

void FLockOnComponentDetails::OnLockOnOverviewNavigate()
{
	FPlatformProcess::LaunchURL(TEXT("https://github.com/J1blCblu/LockOnTarget/wiki/2.-LockOnTargetComponent-Overview"), nullptr, nullptr);
}

void FLockOnComponentDetails::OnDebuggerNavigate()
{
	FPlatformProcess::LaunchURL(TEXT("https://github.com/J1blCblu/LockOnTarget/wiki/4.-Gameplay-Debugger-Overview"), nullptr, nullptr);
}

#undef LOCTEXT_NAMESPACE
