// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "SSocketSelector.h"

#include "PropertyHandle.h"

#include "Internationalization/Text.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MINOR_VERSION <= 2 //&& ENGINE_MAJOR_VERSION == 5
#include "SceneOutliner/Private/SSocketChooser.h"
#else
#include "SSocketChooser.h"
#endif

#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"

void SSocketSelector::Construct(const FArguments& InArgs)
{
	PropertyHandle = InArgs._PropertyHandle;
	SceneComponent = InArgs._SceneComponent;
	check(SceneComponent.IsBound());

	if (PropertyHandle->IsValidHandle())
	{
		PropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &SSocketSelector::OnPropertyUpdated));
	}

	ChildSlot
	[
		SNew(SComboButton)
		.OnGetMenuContent(this, &SSocketSelector::OnGetSocketContent)
		.ContentPadding(0.0f)
		.ButtonStyle(FAppStyle::Get(), "ToggleButton")
		.ForegroundColor(FSlateColor::UseForeground())
		.VAlign(VAlign_Center)
		.ButtonContent()
		[
			SAssignNew(TextBox, SEditableTextBox)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.OnTextCommitted(this, &SSocketSelector::OnTextBoxCommitted)
		]
	];

	OnPropertyUpdated();
}

TSharedRef<SWidget> SSocketSelector::OnGetSocketContent()
{
	return	SNew(SSocketChooserPopup)
			.SceneComponent(SceneComponent.Get())
			.ProvideNoSocketOption(true)
			.OnSocketChosen(this, &SSocketSelector::OnSocketSelected);
}

void SSocketSelector::OnPropertyUpdated()
{
	FName NewSocket;

	if (PropertyHandle->IsValidHandle())
	{
		PropertyHandle->GetValue(NewSocket);
	}

	OnSocketSelected(NewSocket);
}

void SSocketSelector::OnTextBoxCommitted(const FText& Text, ETextCommit::Type)
{
	OnSocketSelected(FName(Text.ToString()));
}

void SSocketSelector::OnSocketSelected(FName Socket)
{
	TextBox->SetText(FText::FromName(Socket));

	if (PropertyHandle->IsValidHandle())
	{
		PropertyHandle->SetValue(Socket);
	}
}
