// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKPressStartScreen.h"
#include "Widgets/ALKTitleText.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPressStartScreen)

UALKPressStartScreen::UALKPressStartScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCloseOnBack = false;
	// The title screen shows the same key art as the main menu it leads to.
	bShowKeyArt = true;
}

void UALKPressStartScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("TitleColumn"));
	FALKWidgetBuilder::AddToOverlay(Root, Column, HAlign_Center, VAlign_Center, FMargin(0.f, 0.f, 0.f, 120.f));

	LogoImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("LogoImage"));
	LogoImage->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, LogoImage, FMargin(), HAlign_Center);

	TitleText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	TitleText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleText, FMargin(), HAlign_Center);

	TitleAccent = FALKWidgetBuilder::Make<UBorder>(Tree, TEXT("TitleAccent"));
	FALKWidgetBuilder::AddToVerticalBox(Column, FALKWidgetBuilder::WrapInSizeBox(Tree, TitleAccent, 96.f, 4.f), FMargin(0.f, 14.f, 0.f, 0.f), HAlign_Center);

	SubtitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("SubtitleText"));
	SubtitleText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, SubtitleText, FMargin(0.f, 16.f, 0.f, 0.f), HAlign_Center);

	PromptText = FALKWidgetBuilder::MakeText(Tree, TEXT("PromptText"));
	PromptText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToOverlay(Root, PromptText, HAlign_Center, VAlign_Bottom, FMargin(0.f, 0.f, 0.f, 120.f));
}

void UALKPressStartScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	const UALKMenuConfig* Config = GetMenuConfig();
	const bool bUseLogo = Config->HasGameLogo() && LogoImage;
	if (LogoImage)
	{
		if (bUseLogo)
		{
			LogoImage->SetBrush(Config->GameLogo);
		}
		LogoImage->SetVisibility(bUseLogo ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TitleText)
	{
		TitleText->SetText(Config->GameTitle);
		TitleText->SetVisibility(bUseLogo ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Config->GameSubtitle);
	}
	if (PromptText)
	{
		PromptText->SetText(Config->PressStartText);
	}
}

void UALKPressStartScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	if (WidgetTree)
	{
		StyleScreenBackground(Theme);
	}
	FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Title);
	FALKWidgetBuilder::StyleText(SubtitleText, Theme, EALKTextRole::Data);
	FALKWidgetBuilder::StyleText(PromptText, Theme, EALKTextRole::Button, Theme.TextPrimary);
	if (TitleAccent)
	{
		TitleAccent->SetBrush(UALKTheme::MakeSolidBrush(Theme.Accent));
	}
}

void UALKPressStartScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	bContinued = false;
	PulseTime = 0.f;
}

void UALKPressStartScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PulseTime += InDeltaTime;
	if (PromptText)
	{
		PromptText->SetRenderOpacity(0.35f + 0.65f * (0.5f + 0.5f * FMath::Cos(PulseTime * 3.f)));
	}
}

bool UALKPressStartScreen::NativeHandleMenuKey(const FKey& Key)
{
	// Ignore the first moments so a key held from the previous screen does not skip the title.
	if (PulseTime > 0.3f)
	{
		Continue();
	}
	return true;
}

FReply UALKPressStartScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (PulseTime > 0.3f)
	{
		Continue();
	}
	return FReply::Handled();
}

void UALKPressStartScreen::Continue()
{
	if (bContinued)
	{
		return;
	}
	bContinued = true;

	PlayThemeSound(EALKUISound::Click);
	UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	CloseScreen();
	if (MenuSubsystem)
	{
		MenuSubsystem->ShowMainMenu();
	}
}
