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

	if(!Objects.Num())
	{
		return;
	}

	EditedComponent = static_cast<UTargetComponent*>(Objects[0].Get());

	/***********************
	 * Default Category
	 ***********************/

	IDetailCategoryBuilder& DefaultCategoryBuilder = DetailLayout.EditCategory(TEXT("Default Settings"), FText::GetEmpty(), ECategoryPriority::Important);

	//CanBeCaptured
	{
		DefaultCategoryBuilder.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, bCanBeCaptured)));
	}

	//TrackedMeshName
	{
		MeshPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, TrackedMeshName));

		if (MeshPropertyHandle->IsValidHandle())
		{
			MeshPropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FTargetComponentDetails::UpdateMeshPropertyText));

			DefaultCategoryBuilder.AddProperty(MeshPropertyHandle).CustomWidget()
				.NameContent()
				[
					MeshPropertyHandle->CreatePropertyNameWidget()
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
						SAssignNew(MeshTextBox, SEditableTextBox)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.OnTextCommitted(this, &FTargetComponentDetails::OnCommitMeshText)
					]
				];

			UpdateMeshPropertyText();
		}
	}

	//Sockets
	{
		SocketsPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, Sockets));

		if (SocketsPropertyHandle->IsValidHandle())
		{
			auto DetailArrayBuilder = MakeShared<FDetailArrayBuilder>(SocketsPropertyHandle.ToSharedRef());
			DetailArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FTargetComponentDetails::GenerateArrayElementWidget));
			DefaultCategoryBuilder.AddCustomBuilder(DetailArrayBuilder);
		}
	}

	/***********************
	 * Focus Point Category
	 ***********************/

	IDetailCategoryBuilder& FocusPointDetailBuilder = DetailLayout.EditCategory(TEXT("Focus Point"), FText::GetEmpty(), ECategoryPriority::Important);

	//Focus Point
	{
		FocusPointDetailBuilder.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, FocusPoint)));
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
					.SceneComponent(this, &FTargetComponentDetails::GetTrackedMeshComponent)
				];
		}
	}

	//Focus Point
	{
		FocusPointDetailBuilder.AddProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTargetComponent, FocusPointOffset)));
	}
}

FName FTargetComponentDetails::GetTrackedMeshName() const
{
	FName MeshName = NAME_None;

	if (MeshPropertyHandle->IsValidHandle())
	{
		MeshPropertyHandle->GetValue(MeshName);
	}

	return MeshName;
}

AActor* FTargetComponentDetails::GetComponentEditorOwner() const
{
	AActor* Owner = nullptr;

	if (EditedComponent.IsValid())
	{
		if (EditedComponent->HasAnyFlags(RF_ArchetypeObject))
		{
			if (const UBlueprintGeneratedClass* const BPGC = Cast<UBlueprintGeneratedClass>(EditedComponent->GetOuter()))
			{
				if (const USimpleConstructionScript* const SCS = BPGC->SimpleConstructionScript)
				{
					Owner = SCS->GetComponentEditorActorInstance();
				}
			}
		}
		else
		{
			//Owner, placed in the world, has all components, including those added in BP editor.
			Owner = EditedComponent->GetOwner();
		}
	}

	return Owner;
}

USceneComponent* FTargetComponentDetails::GetTrackedMeshComponent() const
{
	USceneComponent* Component = nullptr;
	
	if (AActor* const Owner = GetComponentEditorOwner())
	{
		Component = FindComponentByName<UMeshComponent>(GetComponentEditorOwner(), GetTrackedMeshName());

		if (!Component)
		{
			Component = Owner->GetRootComponent();
		}
	}

	return Component;
}

void FTargetComponentDetails::UpdateMeshPropertyText()
{
	OnCommitMeshEntry(GetTrackedMeshName());
}

void FTargetComponentDetails::OnCommitMeshEntry(FName MeshName)
{
	if (MeshPropertyHandle->IsValidHandle())
	{
		MeshPropertyHandle->SetValue(MeshName);
	}

	MeshTextBox->SetText(FText::FromName(MeshName));
}

void FTargetComponentDetails::OnCommitMeshText(const FText& ItemText, ETextCommit::Type CommitInfo)
{
	OnCommitMeshEntry(FName(ItemText.ToString()));
}

TSharedRef<SWidget> FTargetComponentDetails::OnGetMeshContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	//Root component
	{
		const FUIAction Action(FExecuteAction::CreateSP(this, &FTargetComponentDetails::OnCommitMeshEntry, FName(NAME_None)));
		MenuBuilder.AddMenuEntry(FText::FromString(TEXT("None (Root)")), FText::GetEmpty(), FSlateIconFinder::FindIconForClass(ACharacter::StaticClass()), Action);
	}

	MenuBuilder.BeginSection(NAME_None, FText::FromString(TEXT("Meshes")));

	//All other meshes
	if(AActor* const ComponentOwner = GetComponentEditorOwner())
	{
		for (auto* const Component : TInlineComponentArray<UMeshComponent*>(ComponentOwner))
		{
			if (Component && !Component->IsEditorOnly())
			{
				const FUIAction Action(FExecuteAction::CreateSP(this, &FTargetComponentDetails::OnCommitMeshEntry, Component->GetFName()));
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
				.SceneComponent(this, &FTargetComponentDetails::GetTrackedMeshComponent)
			];
	}
}
