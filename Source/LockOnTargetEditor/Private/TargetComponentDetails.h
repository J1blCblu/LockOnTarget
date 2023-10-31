// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"

class UTargetComponent;
class AActor;
class USceneComponent;

class IDetailLayoutBuilder;
class IDetailChildrenBuilder;
class IPropertyHandle;
class SEditableTextBox;
class SWidget;

/** Ref: CameraDetails. */
class FTargetComponentDetails final : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<FTargetComponentDetails>(); }

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:

	FName GetAssociatedComponentNameValue() const;
	AActor* GetComponentEditorOwner() const;
	USceneComponent* GetAssociatedComponent() const;

	void UpdateAssociatedComponentNameText();
	void OnCommitAssociatedComponentNameEntry(FName MeshName);
	void OnCommitAssociatedComponentNameText(const FText& ItemFText, ETextCommit::Type CommitInfo);

	TSharedRef<SWidget> OnGetMeshContent();
	void GenerateArrayElementWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);

private:

	TWeakObjectPtr<UTargetComponent> EditedComponent;
	TSharedPtr<IPropertyHandle> AssociatedComponentNamePropertyHandle;
	TSharedPtr<IPropertyHandle> SocketsPropertyHandle;
	TSharedPtr<SEditableTextBox> AssociatedComponentNameTextBox;
};
