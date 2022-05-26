// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "DetailsCustomization/LockOnComponentDetails.h"
#include "LockOnTargetComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LOC_DetailsCustomization"

TSharedRef<IDetailCustomization> FLockOnComponentDetails::MakeInstance()
{
	return MakeShareable(new FLockOnComponentDetails());
}

void FLockOnComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const FText LockOnDescription = LOCTEXT("LockOnDescriptionText", "Target must have TargetingHelperComponent.");
	
	IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Description", LOCTEXT("LockOnDescription", "Description"), ECategoryPriority::Important);
	Category.AddCustomRow(LOCTEXT("DescriptionRow", "DescriptionRow"))
		.WholeRowContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.MaxDesiredWidth(300.f)
		[
			SNew(SBorder)
			.Padding(5.f)
			.HAlign(HAlign_Center)
			.BorderBackgroundColor(FLinearColor(255.f, 152.f, 0))
			.Content()
			[
				SNew(STextBlock)
				.Text(LockOnDescription)
			]
		];
}

#undef LOCKEXT_NAMESPACE
