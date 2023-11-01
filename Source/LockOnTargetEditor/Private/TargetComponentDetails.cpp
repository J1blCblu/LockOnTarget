// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

#include "TargetComponentDetails.h"
#include "TargetComponent.h"
#include "LockOnTargetTypes.h"
#include "SSocketSelector.h"

#include "Components/MeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"

#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Internationalization/Text.h"
#include "Misc/Attribute.h"

#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateIconFinder.h"
#include "GameFramework/Character.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"

void FTargetComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);

	if (!Objects.Num())
	{
		return;
	}

	EditedComponent = static_cast<UTargetComponent*>(Objects[0].Get());

	/***********************
	 * Default Category
	 ***********************/

	IDetailCategoryBuilder& DefaultCategoryBuilder = DetailLayout.EditCategory(TEXT("General"), FText::GetEmpty(), ECategoryPriority::Important);

	//CanBeCaptured
	{
		DefaultCategoryBuilder.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, bCanBeCaptured)));
	}

	//AssociatedComponentName
	{
		AssociatedComponentNamePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, AssociatedComponentName));

		if (AssociatedComponentNamePropertyHandle->IsValidHandle())
		{
			AssociatedComponentNamePropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FTargetComponentDetails::UpdateAssociatedComponentNameText));

			DefaultCategoryBuilder.AddProperty(AssociatedComponentNamePropertyHandle).CustomWidget()
				.NameContent()
				[
					AssociatedComponentNamePropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FTargetComponentDetails::OnGetMeshContent)
					.ContentPadding(0.0f)
					.ButtonStyle(FAppStyle::Get(), "ToggleButton")
					.ForegroundColor(FSlateColor::UseForeground())
					.VAlign(VAlign_Center)
					.ButtonContent()
					[
						SAssignNew(AssociatedComponentNameTextBox, SEditableTextBox)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.OnTextCommitted(this, &FTargetComponentDetails::OnCommitAssociatedComponentNameText)
					]
				];

			UpdateAssociatedComponentNameText();
		}
	}

	//Sockets
	{
		SocketsPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, Sockets));

		if (SocketsPropertyHandle->IsValidHandle())
		{
			auto DetailArrayBuilder = MakeShared<FDetailArrayBuilder>(SocketsPropertyHandle.ToSharedRef(), true, false);
			DetailArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FTargetComponentDetails::GenerateArrayElementWidget));
			DefaultCategoryBuilder.AddCustomBuilder(DetailArrayBuilder);
		}
	}

	/***********************
	 * Focus Point Category
	 ***********************/

	IDetailCategoryBuilder& FocusPointDetailBuilder = DetailLayout.EditCategory(TEXT("Focus Point"), FText::GetEmpty(), ECategoryPriority::Important);

	//FocusPointType
	{
		FocusPointDetailBuilder.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, FocusPointType)));
	}

	//FocusPointCustomSocket
	{
		TSharedRef<IPropertyHandle> CustomSocketPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, FocusPointCustomSocket));

		if (CustomSocketPropertyHandle->IsValidHandle())
		{
			FocusPointDetailBuilder.AddProperty(CustomSocketPropertyHandle).CustomWidget()
				.NameContent()
				[
					CustomSocketPropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					SNew(SSocketSelector)
					.PropertyHandle(CustomSocketPropertyHandle)
					.SceneComponent(this, &FTargetComponentDetails::GetAssociatedComponent)
				];
		}
	}

	//FocusPointRelativeOffset
	{
		FocusPointDetailBuilder.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, FocusPointRelativeOffset)));
	}
}

FName FTargetComponentDetails::GetAssociatedComponentNameValue() const
{
	FName MeshName = NAME_None;

	if (AssociatedComponentNamePropertyHandle->IsValidHandle())
	{
		AssociatedComponentNamePropertyHandle->GetValue(MeshName);
	}

	return MeshName;
}

AActor* FTargetComponentDetails::GetComponentEditorOwner() const
{
	AActor* Owner = nullptr;

	if (EditedComponent.IsValid())
	{
		if (EditedComponent->HasAnyFlags(RF_ArchetypeObject)) //In BP editor.
		{
			const UBlueprintGeneratedClass* BPGC;

			if (EditedComponent->HasAnyFlags(RF_DefaultSubObject))
			{
				//DefaultSubobject created during construction.
				BPGC = Cast<UBlueprintGeneratedClass>(EditedComponent->GetOuter()->GetClass());
			}
			else
			{
				//BP created component.
				BPGC = Cast<UBlueprintGeneratedClass>(EditedComponent->GetOuter());
			}

			if (BPGC)
			{
				if (const USimpleConstructionScript* const SCS = BPGC->SimpleConstructionScript)
				{
					Owner = SCS->GetComponentEditorActorInstance();
				}
			}
		}
		else if (AActor* const OwnerActor = EditedComponent->GetOwner()) //On the map.
		{
			Owner = OwnerActor;
		}
	}

	return Owner;
}

USceneComponent* FTargetComponentDetails::GetAssociatedComponent() const
{
	USceneComponent* Component = nullptr;

	if (EditedComponent.IsValid())
	{
		if (AActor* const Owner = GetComponentEditorOwner())
		{
			Component = FindComponentByName<USceneComponent>(Owner, GetAssociatedComponentNameValue());

			//If name is none then use the root component.
			if (!Component)
			{
				Component = Owner->GetRootComponent();
			}
		}
		else
		{
			//Try to use the cached component if it exists.
			Component = EditedComponent->GetAssociatedComponent();
		}
	}

	return Component;
}

void FTargetComponentDetails::UpdateAssociatedComponentNameText()
{
	OnCommitAssociatedComponentNameEntry(GetAssociatedComponentNameValue());
}

void FTargetComponentDetails::OnCommitAssociatedComponentNameEntry(FName MeshName)
{
	if (AssociatedComponentNamePropertyHandle->IsValidHandle())
	{
		AssociatedComponentNamePropertyHandle->SetValue(MeshName);
	}

	AssociatedComponentNameTextBox->SetText(FText::FromName(MeshName));
}

void FTargetComponentDetails::OnCommitAssociatedComponentNameText(const FText& ItemText, ETextCommit::Type CommitInfo)
{
	OnCommitAssociatedComponentNameEntry(FName(ItemText.ToString()));
}

TSharedRef<SWidget> FTargetComponentDetails::OnGetMeshContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	//Root component
	{
		const FUIAction Action(FExecuteAction::CreateSP(this, &FTargetComponentDetails::OnCommitAssociatedComponentNameEntry, FName(NAME_None)));
		MenuBuilder.AddMenuEntry(FText::FromString(TEXT("None (Root)")), FText::GetEmpty(), FSlateIconFinder::FindIconForClass(ACharacter::StaticClass()), Action);
	}

	MenuBuilder.BeginSection(NAME_None, FText::FromString(TEXT("Components")));

	//All other meshes
	if (AActor* const ComponentOwner = GetComponentEditorOwner())
	{
		for (auto* const Component : TInlineComponentArray<USceneComponent*>(ComponentOwner))
		{
			if (Component && !Component->IsEditorOnly())
			{
				const FUIAction Action(FExecuteAction::CreateSP(this, &FTargetComponentDetails::OnCommitAssociatedComponentNameEntry, Component->GetFName()));
				MenuBuilder.AddMenuEntry(FText::FromName(Component->GetFName()), FText::GetEmpty(), FSlateIconFinder::FindIconForClass(Component->GetClass()), Action);
			}
		}
	}

	return MenuBuilder.MakeWidget();
}

void FTargetComponentDetails::GenerateArrayElementWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	if (PropertyHandle->IsValidHandle())
	{
		ChildrenBuilder.AddProperty(PropertyHandle).CustomWidget()
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SSocketSelector)
				.PropertyHandle(PropertyHandle)
				.SceneComponent(this, &FTargetComponentDetails::GetAssociatedComponent)
			];
	}
}
