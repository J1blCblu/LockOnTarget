// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Types/SlateEnums.h"

class SEditableTextBox;
class USceneComponent;
class IPropertyHandle;

class SSocketSelector : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SSocketSelector)
	{
	}

	/** A component that contains sockets. */
	SLATE_ATTRIBUTE(USceneComponent*, SceneComponent)

	/** PropertyHandle. */
	SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, PropertyHandle)

	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);

private:

	TSharedRef<SWidget> OnGetSocketContent();

	void OnPropertyUpdated();
	void OnTextBoxCommitted(const FText& Text, ETextCommit::Type);
	void OnSocketSelected(FName Socket);

private:
	
	TSharedPtr<IPropertyHandle> PropertyHandle;
	TAttribute<USceneComponent*> SceneComponent;
	TSharedPtr<SEditableTextBox> TextBox;
};