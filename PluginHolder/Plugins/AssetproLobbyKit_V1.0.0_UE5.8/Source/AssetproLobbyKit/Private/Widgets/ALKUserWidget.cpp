// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKUserWidget.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Blueprint/WidgetTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKUserWidget)

void UALKUserWidget::NativeOnInitialized()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		bUsingDefaultLayout = true;
		NativeBuildDefaultLayout(*WidgetTree);
	}

	NativeOnLayoutReady();
	if (bUsingDefaultLayout)
	{
		OnDefaultLayoutBuilt();
	}
	Super::NativeOnInitialized();
}

void UALKUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HandleThemeChanged();
	if (!ThemeChangedHandle.IsValid())
	{
		ThemeChangedHandle = UALKTheme::OnAnyThemeChanged().AddUObject(this, &ThisClass::HandleThemeChanged);
	}
}

void UALKUserWidget::NativeDestruct()
{
	UALKTheme::OnAnyThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::NativeDestruct();
}

void UALKUserWidget::HandleThemeChanged()
{
	if (bApplyThemeStyles)
	{
		ApplyTheme(GetTheme());
	}
}

void UALKUserWidget::ApplyTheme_Implementation(const UALKTheme* Theme)
{
	if (Theme)
	{
		NativeApplyTheme(*Theme);
	}
}

const UALKTheme* UALKUserWidget::GetTheme() const
{
	return UALKDeveloperSettings::GetTheme();
}

const UALKMenuConfig* UALKUserWidget::GetMenuConfig() const
{
	return UALKDeveloperSettings::GetMenuConfig();
}
