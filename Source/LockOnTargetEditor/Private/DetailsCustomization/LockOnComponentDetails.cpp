// Copyright 2021-2022 Ivan Baktenkov. All Rights Reserved.

#include "DetailsCustomization/LockOnComponentDetails.h"
#include "LockOnTargetComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "LOC_DetailsCustomization"

TSharedRef<IDetailCustomization> FLockOnComponentDetails::MakeInstance()
{
	return MakeShareable(new FLockOnComponentDetails());
}

void FLockOnComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	//TSharedPtr<IPropertyHandle> Property = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(ULockOnTargetComponent, Sockets));
	//check(Property.IsValid());

	const FText LockOnDescription = LOCTEXT("LockOnDescriptionText", R"(LockOnComponent responsible for:
+ Target storage(Helper Component and Socket).
+ Processing input.
+ Delegating work to it subobjects(which can be changed via BP/C++):
+	- TargetHandler used for handle Target(find, switch, maintenance).
+	- Rotation mode return final rotation for Control Rotation/Owner Rotation.

Target should have Targeting Helper Component.

Also note that you may need control yourself next fields:
+ bOrientRotationToMovement in UCharacterMovementComponent.
+ bUseControllerRotationYaw in APawn.
(e.g. via OnTargetLocked/OnTargetUnlocked delegates))");
	
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
