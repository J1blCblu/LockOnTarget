// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class FTargetingHelperComponentDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

};
