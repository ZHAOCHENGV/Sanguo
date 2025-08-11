#pragma once

#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "SkelotComponent.h"

#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"


#define LOCTEXT_NAMESPACE "Skelot"


class FSkelotComponentDetails : public IDetailCustomization
{
public:

    void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
    {

    }
};

#undef LOCTEXT_NAMESPACE