// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKChapterStill.h"
#include "Widgets/ALKFrame.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKChapterStill)

UALKChapterStill::UALKChapterStill(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ButtonStyleType = EALKButtonStyle::Secondary;
	// The picture itself is the button: clicking it starts the chapter, the screen plays the sound.
	bPlayClickSound = false;
}

void UALKChapterStill::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	Background = FALKWidgetBuilder::Make<UBorder>(Tree, TEXT("Background"));
	Background->SetPadding(FMargin(0.f));
	Background->SetHorizontalAlignment(HAlign_Fill);
	Background->SetVerticalAlignment(VAlign_Fill);
	Tree.RootWidget = Background;

	UOverlay* Overlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("StillOverlay"));
	Background->SetContent(Overlay);

	UScaleBox* StillScale = FALKWidgetBuilder::Make<UScaleBox>(Tree, TEXT("StillScale"));
	StillScale->SetStretch(EStretch::ScaleToFill);
	StillImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("StillImage"));
	StillScale->AddChild(StillImage);
	FALKWidgetBuilder::AddToOverlay(Overlay, StillScale);

	LockedDim = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("LockedDim"));
	FALKWidgetBuilder::AddToOverlay(Overlay, LockedDim);

	LockImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("LockImage"));
	FALKWidgetBuilder::AddToOverlay(Overlay, FALKWidgetBuilder::WrapInSizeBox(Tree, LockImage, 56.f, 56.f), HAlign_Center, VAlign_Center);

	Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Frame"));
	Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	FALKWidgetBuilder::AddToOverlay(Overlay, Frame);
}

void UALKChapterStill::SetStill(UTexture2D* InStill, bool bInLocked)
{
	Still = InStill;
	bLocked = bInLocked;
	RefreshStill();
}

void UALKChapterStill::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	LockTexture = Theme.LockIcon.LoadSynchronous();
	RefreshStill();
}

void UALKChapterStill::RefreshStill()
{
	const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
	if (StillImage)
	{
		if (Still)
		{
			StillImage->SetBrush(FALKWidgetBuilder::MakeTextureBrush(Still));
		}
		else if (Theme)
		{
			// No picture: a quiet panel in the theme's colours.
			StillImage->SetBrush(UALKTheme::MakeSolidBrush(FMath::Lerp(Theme->BackgroundTop, Theme->BackgroundBottom, 0.6f)));
		}
	}
	if (LockedDim && Theme)
	{
		LockedDim->SetBrush(UALKTheme::MakeSolidBrush(Theme->LockedChapterDim));
		LockedDim->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LockImage && Theme)
	{
		if (LockTexture)
		{
			LockImage->SetBrush(FALKWidgetBuilder::MakeTextureBrush(LockTexture, Theme->TextPrimary));
		}
		LockImage->SetVisibility(bLocked && LockTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UALKChapterStill::NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled)
{
	if (UALKFrame* StillBox = Cast<UALKFrame>(Frame))
	{
		// A thin outline round the picture, and the focus ring when it has focus.
		FALKFrameStyle StillFrame;
		StillFrame.Outline = Theme.PanelOutline;
		StillFrame.OutlineWidth = Theme.OutlineWidth;
		StillFrame.Radius = 0.f;
		StillFrame.Ring = Theme.Accent.CopyWithNewOpacity(Theme.Accent.A * Highlight);
		StillFrame.RingWidth = Theme.FocusRingWidth;
		StillBox->SetFrameStyle(StillFrame);
	}
	else if (Frame)
	{
		const FLinearColor Outline = Theme.TextPrimary.CopyWithNewOpacity(Theme.TextPrimary.A * 0.85f * Highlight);
		Frame->SetBrush(Theme.MakeRoundedBrush(FLinearColor::Transparent, Outline, Highlight > 0.01f ? 2.f : 0.f, 0.f));
	}
}
