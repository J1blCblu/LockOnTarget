// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class FLockOnComponentDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

public:
	void OnInitialSetupNavigate();
};
