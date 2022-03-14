// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#include "DetailsCustomization/TargetingHelperComponentDetails.h"
#include "TargetingHelperComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "LOC_DetailsCustomization"

TSharedRef<IDetailCustomization> FTargetingHelperComponentDetails::MakeInstance()
{
	return MakeShareable(new FTargetingHelperComponentDetails());
}

void FTargetingHelperComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	//TSharedPtr<IPropertyHandle> Propetry = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetingHelperComponent, Sockets));
	//IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Description", LOCTEXT("HelperDescription", "TargetingHelperDescription"), ECategoryPriority::Important);
}

#undef LOCKEXT_NAMESPACE
