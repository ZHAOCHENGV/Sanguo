// Copyright 2024 Lazy Marmot Games. All Rights Reserved.

#include "../Public/SkelotCustomization.h"




#if 0
TSharedRef<IDetailCustomization> FSkelotAnimSetDetails::MakeInstance()
{
	return MakeShared<FSkelotAnimSetDetails>();
}

void FSkelotAnimSetDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	ValidSkeleton = GetValidSkeleton(DetailBuilder);
	SelectedSkeletonName = FString();
	if (ValidSkeleton)
	{
		SelectedSkeletonName = FString::Printf(TEXT("%s'%s'"), *ValidSkeleton->GetClass()->GetName(), *ValidSkeleton->GetPathName());
	}
	

	const FName SequencesFName(GET_MEMBER_NAME_CHECKED(USkelotAnimCollection, Sequences));
	const FName MeshesFName(GET_MEMBER_NAME_CHECKED(USkelotAnimCollection, Meshes));

	TSharedPtr<IPropertyHandleArray> SequencesPH = DetailBuilder.GetProperty(SequencesFName)->AsArray();
	check(SequencesPH.IsValid());

	{
		uint32 NumElement = 0;
		SequencesPH->GetNumElements(NumElement);

		for (uint32 i = 0; i < NumElement; i++)
		{
			TSharedRef<IPropertyHandle> ElementPH = SequencesPH->GetElement(i);
			uint32 TotalChildren = 0;
			ElementPH->GetNumChildren(TotalChildren);

			for (uint32 ChildIndex = 0; ChildIndex < TotalChildren; ++ChildIndex)
			{
				TSharedPtr<IPropertyHandle> ChildHandle = ElementPH->GetChildHandle(ChildIndex);

				/*if (ChildHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FSkelotSeqDef, Sequence))
				{
					//TSharedRef<SWidget> SequenceFieldWidget = ChildHandle->CreatePropertyValueWidget();
					//TSharedRef<SCompoundWidget> SequenceFieldWidgetCasted = StaticCastSharedRef<SCompoundWidget>(SequenceFieldWidget);

					//DetailBuilder.HideProperty(ChildHandle);	//hide FSkelotSeqDef.Sequence
					//SequenceFieldWidgetCasted->GetChildren()->GetSlotAt(0).AttachWidget();


					IDetailPropertyRow& Row = DetailBuilder.AddPropertyToCategory(ChildHandle);

					TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
						.ThumbnailPool(DetailBuilder.GetThumbnailPool())
						.PropertyHandle(ChildHandle)
						.AllowedClass(UAnimSequenceBase::StaticClass())
						.AllowClear(true)
						.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FSkelotAnimSetDetails::OnShouldFilterAnimAsset));

					FDetailWidgetRow& WidgetRow = Row.CustomWidget();
					WidgetRow.ValueContent().Widget = PropWidget;
				}*/
			};
		}
	}


	//DetailBuilder.AddPropertyToCategory();

	{
		TSharedRef<IPropertyHandle> MeshesPH = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkelotAnimCollection, Meshes));
		TSharedRef<FDetailArrayBuilder> MeshesBuilder = MakeShareable(new FDetailArrayBuilder(MeshesPH));
		MeshesBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FSkelotAnimSetDetails::GenerateMeshes));
		
		IDetailCategoryBuilder& DefaultCategory = DetailBuilder.EditCategory(MeshesPH->GetDefaultCategoryName());
		DefaultCategory.AddCustomBuilder(MeshesBuilder);

		//for (uint32 i = 0; i < NumElem; i++)
		//{
		//	TSharedPtr<IPropertyHandle> ChildPH = MeshesPH->GetChildHandle(i);
		//
		//	IDetailPropertyRow& Row = DetailBuilder.AddPropertyToCategory(ChildPH);
		//
		//	TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
		//		.ThumbnailPool(DetailBuilder.GetThumbnailPool())
		//		.PropertyHandle(ChildPH)
		//		.AllowedClass(USkeletalMesh::StaticClass())
		//		.AllowClear(true)
		//		.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FSkelotAnimSetDetails::OnShouldFilterAnimAsset));
		//
		//	FDetailWidgetRow& WidgetRow = Row.CustomWidget();
		//	WidgetRow.NameContent().Widget = ChildPH->CreatePropertyNameWidget();
		//	WidgetRow.ValueContent().Widget = PropWidget;
		//	WidgetRow.ValueContent().MinDesiredWidth(300).MaxDesiredWidth(600);
		//
		//	
		//}


	}

}

bool FSkelotAnimSetDetails::OnShouldFilterAnimAsset(const FAssetData& AssetData)
{
	const FString SkeletonName = AssetData.GetTagValueRef<FString>("Skeleton");
	return ValidSkeleton == nullptr || SkeletonName != SelectedSkeletonName;
}

USkeleton* FSkelotAnimSetDetails::GetValidSkeleton(IDetailLayoutBuilder& DetailBuilder) const
{
	USkeleton* Skeleton = nullptr;
	
	for (TWeakObjectPtr<UObject> ObjectIter : DetailBuilder.GetSelectedObjects())
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

void FSkelotAnimSetDetails::GenerateMeshes(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	IDetailPropertyRow& Row = ChildrenBuilder.AddProperty(PropertyHandle);


	TSharedRef<SWidget> PropWidget = SNew(SObjectPropertyEntryBox)
		.ThumbnailPool(ChildrenBuilder.GetParentCategory().GetParentLayout().GetThumbnailPool())
		.PropertyHandle(PropertyHandle)
		.AllowedClass(USkeletalMesh::StaticClass())
		.AllowClear(true)
		.OnShouldFilterAsset(FOnShouldFilterAsset::CreateSP(this, &FSkelotAnimSetDetails::OnShouldFilterAnimAsset));

	Row.CustomWidget(false).NameContent()[
		PropertyHandle->CreatePropertyNameWidget()
	].ValueContent().MaxDesiredWidth(TOptional<float>())[
		PropertyHandle->CreatePropertyValueWidget()
	];
}



TSharedRef<IPropertyTypeCustomization> FSkelotAnimSetCustomization::MakeInstance()
{
	return MakeShared<FSkelotAnimSetCustomization>();
}

void FSkelotAnimSetCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	//HeaderRow
	//.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()]
	//.ValueContent()[];
}

void FSkelotAnimSetCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 TotalChildren = 0;
	StructPropertyHandle->GetNumChildren(TotalChildren);

	for (uint32 ChildIndex = 0; ChildIndex < TotalChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle);
	}
}

bool FSkelotAnimSetCustomization::OnShouldFilterAnimAsset(const FAssetData& AssetData)
{
	return true;
}

USkeleton* FSkelotAnimSetCustomization::GetValidSkeleton(TSharedRef<IPropertyHandle> InStructPropertyHandle) const
{
	return ValidSkeleton;
}
#endif