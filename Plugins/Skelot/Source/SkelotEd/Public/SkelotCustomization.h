// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "SkelotComponent.h"
#include "SkelotAnimCollection.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "EditorCategoryUtils.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "Engine/Selection.h"
#include "Widgets/Images/SImage.h"
#include "IDetailChildrenBuilder.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimComposite.h"

#define LOCTEXT_NAMESPACE "Skelot"

struct FAssetData;
class IDetailLayoutBuilder;
class IPropertyHandle;
class SComboButton;

inline FText GetObjectNameSuffix(UObject* Obj)
{
	return Obj ? FText::Format(INVTEXT("#{0}"), FText::FromString(Obj->GetName())) : INVTEXT("#null");
}

#if 0 //shitcode
class FSkelotAnimSetDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	bool OnShouldFilterAnimAsset(const FAssetData& AssetData);

	USkeleton* GetValidSkeleton(IDetailLayoutBuilder& DetailBuilder) const;

	USkeleton* ValidSkeleton;
	FString SelectedSkeletonName;


	void GenerateMeshes(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder);
};


class FSkelotAnimSetCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	bool OnShouldFilterAnimAsset(const FAssetData& AssetData);

	USkeleton* GetValidSkeleton(TSharedRef<IPropertyHandle> InStructPropertyHandle) const;

	USkeleton* ValidSkeleton;
	FString SelectedSkeletonName;
};

#endif // 




class FSkelotSharedCustomization : public IPropertyTypeCustomization
{
public:
	void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		OuterObjects.Empty();
		StructPropertyHandle->GetOuterObjects(OuterObjects);

		ValidSkeleton = GetValidSkeleton(StructPropertyHandle);
		SelectedSkeletonName = FString();
		if (ValidSkeleton)
		{
			SelectedSkeletonName = FString::Printf(TEXT("%s'%s'"), *ValidSkeleton->GetClass()->GetPathName(), *ValidSkeleton->GetPathName());
		}
	}
	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{

	}
	bool OnShouldFilterSkeleton(const FAssetData& AssetData)
	{
		const FString SkeletonName = AssetData.GetTagValueRef<FString>(TEXT("Skeleton"));
		return ValidSkeleton == nullptr || SkeletonName != SelectedSkeletonName;
	}
	USkeleton* GetValidSkeleton(TSharedRef<IPropertyHandle> InStructPropertyHandle) const
	{
		USkeleton* Skeleton = nullptr;
		TArray<UObject*> Objects;
		InStructPropertyHandle->GetOuterObjects(Objects);

		for (UObject* ObjectIter : Objects)
		{
			USkelotAnimCollection* AnimSet = Cast<USkelotAnimCollection>(ObjectIter);
			if (!AnimSet || !AnimSet->Skeleton)
			{
				continue;
			}

			// If we've not come across a valid skeleton yet, store this one.
			if (!Skeleton)
			{
				Skeleton = AnimSet->Skeleton;
				continue;
			}

			// We've encountered a valid skeleton before.
			// If this skeleton is not the same one, that means there are multiple
			// skeletons selected, so we don't want to take any action.
			if (AnimSet->Skeleton != Skeleton)
			{
				return nullptr;
			}
		}

		return Skeleton;
	}

	TArray<UObject*> OuterObjects;
	USkeleton* ValidSkeleton;
	FString SelectedSkeletonName;
};

class FSkelotSeqDefCustomization : public FSkelotSharedCustomization
{
public:
	void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		FSkelotSharedCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);

		HeaderRow.NameContent()[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[
				StructPropertyHandle->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot().VAlign(VAlign_Center)[
				SNew(STextBlock).Text(this, &FSkelotSeqDefCustomization::GetHeaderExtraText, StructPropertyHandle)
			]
		];
	}
	FText GetHeaderExtraText(TSharedRef<IPropertyHandle> StructPropertyHandle) const
	{
		if (OuterObjects.Num() == 1 && StructPropertyHandle->IsValidHandle())
		{
			FSkelotSequenceDef* StructPtr = (FSkelotSequenceDef*)StructPropertyHandle->GetValueBaseAddress((uint8*)OuterObjects[0]);
			if(StructPtr)
				return GetObjectNameSuffix(StructPtr->Sequence);
		}
		return FText();
	}
	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		uint32 TotalChildren = 0;
		StructPropertyHandle->GetNumChildren(TotalChildren);

		for (uint32 ChildIndex = 0; ChildIndex < TotalChildren; ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
			IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle);

			if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FSkelotSequenceDef, Sequence))
			{
				TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
					.ThumbnailPool(StructCustomizationUtils.GetThumbnailPool())
					.PropertyHandle(ChildHandle)
					.AllowedClass(UAnimSequenceBase::StaticClass())
					.AllowClear(true)
					.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FSkelotSeqDefCustomization::OnShouldFilterAnim));

				FDetailWidgetRow& WidgetRow = Row.CustomWidget();
				WidgetRow.NameContent().Widget = ChildHandle->CreatePropertyNameWidget();
				WidgetRow.ValueContent().Widget = PropWidget;
				WidgetRow.ValueContent().MinDesiredWidth(300).MaxDesiredWidth(0);
			}
//			else if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FSkelotSequenceDef, Blends))
// 			{
// 				TSharedPtr<IPropertyHandleMap> BlendsPH = ChildHandle->AsMap();
// 				uint32 NumElem = 0;
// 				if (BlendsPH->GetNumElements(NumElem) == FPropertyAccess::Success)
// 				{
// 					for (uint32 ElementIndex = 0; ElementIndex < NumElem; ElementIndex++)
// 					{
// 						TSharedRef<IPropertyHandle> ElementPH = ChildHandle->GetChildHandle(ElementIndex).ToSharedRef();
// 						IDetailPropertyRow& KeyRow = StructBuilder.AddProperty(ElementPH->GetKeyHandle().ToSharedRef());
// 					}
// 				}
// 			
// 				
// 			}

		};


		//FDetailWidgetRow& BlendsRow = StructBuilder.AddCustomRow(LOCTEXT("Blends", "Blends"));
		//BlendsRow.WholeRowContent()[
		//	
		//];
	}
	bool OnShouldFilterAnim(const FAssetData& AssetData)
	{
		for (UObject* OuterObj : OuterObjects)
		{
			if (USkelotAnimCollection* OuterAnimSet = Cast<USkelotAnimCollection>(OuterObj))
			{
				const bool bAlreadyTaken = OuterAnimSet->FindSequenceDefByPath(AssetData.ToSoftObjectPath()) != INDEX_NONE;
				if (bAlreadyTaken)
					return true;

				bool bClassIsSupported = AssetData.AssetClassPath == UAnimComposite::StaticClass()->GetClassPathName() || AssetData.AssetClassPath == UAnimSequence::StaticClass()->GetClassPathName();
				if(!bClassIsSupported)
					return true;

			}
		}

		return OnShouldFilterSkeleton(AssetData);
	}
};

class FSkelotMeshDefCustomization : public FSkelotSharedCustomization
{
public:
	void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		FSkelotSharedCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);

		HeaderRow.NameContent()[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[
					StructPropertyHandle->CreatePropertyNameWidget()
				]
				+ SHorizontalBox::Slot().VAlign(VAlign_Center)[
					SNew(STextBlock).Text(this, &FSkelotMeshDefCustomization::GetHeaderExtraText, StructPropertyHandle)
				]
		];
	}

	FText GetHeaderExtraText(TSharedRef<IPropertyHandle> StructPropertyHandle) const
	{
		if (OuterObjects.Num() == 1 && StructPropertyHandle->IsValidHandle())
		{
			FSkelotMeshDef* StructPtr = (FSkelotMeshDef*)StructPropertyHandle->GetValueBaseAddress((uint8*)OuterObjects[0]);
			if (StructPtr)
				return GetObjectNameSuffix(StructPtr->Mesh);
		}
		return FText();
	}
	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		uint32 TotalChildren = 0;
		StructPropertyHandle->GetNumChildren(TotalChildren);

		for (uint32 ChildIndex = 0; ChildIndex < TotalChildren; ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
			IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle);

			if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FSkelotMeshDef, Mesh))
			{
				TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
					.ThumbnailPool(StructCustomizationUtils.GetThumbnailPool())
					.PropertyHandle(ChildHandle)
					.AllowedClass(USkeletalMesh::StaticClass())
					.AllowClear(true)
					.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FSkelotMeshDefCustomization::OnShouldFilterMesh));

				FDetailWidgetRow& WidgetRow = Row.CustomWidget();
				WidgetRow.NameContent().Widget = ChildHandle->CreatePropertyNameWidget();
				WidgetRow.ValueContent().Widget = PropWidget;
				WidgetRow.ValueContent().MinDesiredWidth(300).MaxDesiredWidth(600);
			}
		};
	}
	bool OnShouldFilterMesh(const FAssetData& AssetData)
	{
		for (UObject* OuterObj : OuterObjects)
		{
			if (USkelotAnimCollection* OuterAnimSet = Cast<USkelotAnimCollection>(OuterObj))
			{
				const bool bAlreadyTaken = OuterAnimSet->FindMeshDefByPath(AssetData.ToSoftObjectPath()) != INDEX_NONE;
				if(bAlreadyTaken)
					return true;
			}
		}
		
		return OnShouldFilterSkeleton(AssetData);
	}
};

#if 0
class FSkelotBlendDefCustomization : public FSkelotSharedCustomization
{
public:
	void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		FSkelotSharedCustomization::CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);

		HeaderRow.NameContent()[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[
					StructPropertyHandle->CreatePropertyNameWidget()
				]
				+ SHorizontalBox::Slot().VAlign(VAlign_Center)[
					SNew(STextBlock).Text(this, &FSkelotBlendDefCustomization::GetHeaderExtraText, StructPropertyHandle)
				]
		];
	}
	FText GetHeaderExtraText(TSharedRef<IPropertyHandle> StructPropertyHandle) const
	{
		if (OuterObjects.Num() == 1 && StructPropertyHandle->IsValidHandle())
		{
			FSkelotBlendDef* StructPtr = (FSkelotBlendDef*)StructPropertyHandle->GetValueBaseAddress((uint8*)OuterObjects[0]);
			if(StructPtr)
				return GetObjectNameSuffix(StructPtr->BlendTo);
		}
		return FText();
	}
	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		uint32 TotalChildren = 0;
		StructPropertyHandle->GetNumChildren(TotalChildren);

		for (uint32 ChildIndex = 0; ChildIndex < TotalChildren; ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
			IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle);

			if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FSkelotBlendDef, BlendTo))
			{
				TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
					.ThumbnailPool(StructCustomizationUtils.GetThumbnailPool())
					.PropertyHandle(ChildHandle)
					.AllowedClass(UAnimSequenceBase::StaticClass())
					.AllowClear(true)
					.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FSkelotBlendDefCustomization::OnShouldFilterAnim, StructPropertyHandle));

				FDetailWidgetRow& WidgetRow = Row.CustomWidget();
				WidgetRow.NameContent().Widget = ChildHandle->CreatePropertyNameWidget();
				WidgetRow.ValueContent().Widget = PropWidget;
				WidgetRow.ValueContent().MinDesiredWidth(300).MaxDesiredWidth(0);
			}
		};
	}
	bool OnShouldFilterAnim(const FAssetData& AssetData, TSharedRef<IPropertyHandle> StructPropertyHandle)
	{
		for (UObject* OuterObj : OuterObjects)
		{
			if (USkelotAnimCollection* OuterAnimSet = Cast<USkelotAnimCollection>(OuterObj))
			{
				int SeqIndex = OuterAnimSet->FindSequenceDefByPath(AssetData.ToSoftObjectPath());
				const bool bIsInSequenceArray = SeqIndex != INDEX_NONE;
				if (!bIsInSequenceArray)
					return true;

				//bool bIsInBlendArray = OuterAnimSet->Sequences[SeqIndex].IndexOfBlendDefByPath(AssetData.ToSoftObjectPath()) != INDEX_NONE;
				//if(bIsInBlendArray)
				//	return true;
			}
		}

		return OnShouldFilterSkeleton(AssetData);
	}
};
#endif






#undef  LOCTEXT_NAMESPACE