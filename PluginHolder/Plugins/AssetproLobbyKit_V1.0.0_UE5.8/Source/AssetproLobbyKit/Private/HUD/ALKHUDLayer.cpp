// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "HUD/ALKHUDLayer.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKPaint.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKHUDLayer)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKHUDPrivate
{
	/** Offset from the prompt's anchor to the middle of its left edge; the leader line runs between them. */
	static const FVector2D PromptOffset(58.0, -32.0);

	static UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetAutoSize(true);
		return Slot;
	}

	/**
	 * A thin shadow in the opposite brightness of the theme's text, for HUD text that stands straight on the game: light
	 * text gets a dark edge and dark text (the Light skin) a light one, so both read over bright and dark scenes.
	 */
	static void AddSceneShadow(UTextBlock* Text, const UALKTheme& Theme, float Offset = 1.f)
	{
		if (Text)
		{
			const bool bLightText = Theme.TextPrimary.GetLuminance() > 0.5f;
			Text->SetShadowColorAndOpacity(bLightText ? FLinearColor(0.f, 0.f, 0.f, 0.8f) : FLinearColor(1.f, 1.f, 1.f, 0.75f));
			Text->SetShadowOffset(FVector2D(bLightText ? Offset : 0.0, Offset));
		}
	}

	static void SetShown(UWidget* Widget, bool bShown)
	{
		if (Widget)
		{
			const ESlateVisibility Wanted = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
			if (Widget->GetVisibility() != Wanted)
			{
				Widget->SetVisibility(Wanted);
			}
		}
	}

	/** Fades a timed item in over FadeIn seconds and out over its last FadeOut seconds. */
	static float TimedOpacity(float Age, float Duration, float FadeIn, float FadeOut)
	{
		return FMath::Clamp(Age / FadeIn, 0.f, 1.f) * FMath::Clamp((Duration - Age) / FadeOut, 0.f, 1.f);
	}
}

void UALKHUDLayer::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKHUDPrivate;

	Canvas = FALKWidgetBuilder::Make<UCanvasPanel>(Tree, TEXT("HUDCanvas"));
	Tree.RootWidget = Canvas;

	// Prompt: a marker on the target, a leader line (drawn in NativePaint) and the prompt itself.
	PromptMarker = FALKWidgetBuilder::Make<UALKIconWidget>(Tree, TEXT("PromptMarker"));
	PromptMarker->Icon = EALKIcon::Checkpoint;
	PromptMarker->IconSize = 14.f;
	Place(Canvas, PromptMarker, FAnchors(0.f, 0.f), FVector2D(0.5, 0.5), FVector2D::ZeroVector);

	UVerticalBox* PromptColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PromptBox"));
	PromptBox = PromptColumn;
	PromptSubject = FALKWidgetBuilder::MakeText(Tree, TEXT("PromptSubject"));
	FALKWidgetBuilder::AddToVerticalBox(PromptColumn, PromptSubject, FMargin(4.f, 0.f, 0.f, 6.f), HAlign_Left);
	PromptFrame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("PromptFrame"));
	PromptFrame->SetPadding(FMargin(8.f, 7.f, 22.f, 7.f));
	FALKWidgetBuilder::AddToVerticalBox(PromptColumn, PromptFrame, FMargin(), HAlign_Left);
	UHorizontalBox* PromptRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("PromptRow"));
	PromptFrame->SetContent(PromptRow);
	PromptKeyCap = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("PromptKeyCap"));
	PromptKeyCap->SetPadding(FMargin(9.f, 3.f));
	PromptKeyCap->SetHorizontalAlignment(HAlign_Center);
	PromptKeyCap->SetVerticalAlignment(VAlign_Center);
	PromptKeyText = FALKWidgetBuilder::MakeText(Tree, TEXT("PromptKeyText"));
	PromptKeyText->SetJustification(ETextJustify::Center);
	PromptKeyCap->SetContent(PromptKeyText);
	USizeBox* KeySize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("PromptKeySize"));
	KeySize->SetMinDesiredWidth(36.f);
	KeySize->SetMinDesiredHeight(36.f);
	KeySize->SetContent(PromptKeyCap);
	FALKWidgetBuilder::AddToHorizontalBox(PromptRow, KeySize, FMargin(0.f, 0.f, 14.f, 0.f), HAlign_Left, VAlign_Center);
	PromptAction = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("PromptAction"));
	FALKWidgetBuilder::AddToHorizontalBox(PromptRow, PromptAction, FMargin(), HAlign_Left, VAlign_Center);
	PromptHoldBar = FALKWidgetBuilder::Make<UALKValueBar>(Tree, TEXT("PromptHoldBar"));
	PromptHoldBar->BarSize = FVector2D(120.0, 6.0);
	PromptHoldBar->TrackHeight = 4.f;
	PromptHoldBar->Value = 0.f;
	FALKWidgetBuilder::AddToVerticalBox(PromptColumn, PromptHoldBar, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Fill);
	Place(Canvas, PromptColumn, FAnchors(0.f, 0.f), FVector2D(0.0, 0.5), FVector2D::ZeroVector);

	// Subtitles: lines stack up from the bottom middle.
	SubtitleBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("SubtitleBox"));
	Place(Canvas, SubtitleBox, FAnchors(0.5f, 1.f), FVector2D(0.5, 1.0), FVector2D(0.0, -60.0));

	// Notifications: the top right corner, newest at the bottom.
	NotificationBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("NotificationBox"));
	Place(Canvas, NotificationBox, FAnchors(1.f, 0.f), FVector2D(1.0, 0.0), FVector2D(-70.0, 60.0));

	// Chapter card: the chapter's number over a rule, then its name, in the lower middle.
	UVerticalBox* Card = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ChapterCard"));
	ChapterCard = Card;
	ChapterNumber = FALKWidgetBuilder::MakeText(Tree, TEXT("ChapterNumber"));
	FALKWidgetBuilder::AddToVerticalBox(Card, ChapterNumber, FMargin(0.f, 0.f, 0.f, 10.f), HAlign_Center);
	ChapterRule = FALKWidgetBuilder::Make<UALKRule>(Tree, TEXT("ChapterRule"));
	FALKWidgetBuilder::AddToVerticalBox(Card, FALKWidgetBuilder::WrapInSizeBox(Tree, ChapterRule, 380.f, 0.f), FMargin(0.f, 0.f, 0.f, 12.f), HAlign_Center);
	UALKTitleText* Name = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("ChapterName"));
	Name->SetJustification(ETextJustify::Center);
	ChapterName = Name;
	FALKWidgetBuilder::AddToVerticalBox(Card, ChapterName, FMargin(), HAlign_Center);
	Place(Canvas, Card, FAnchors(0.5f, 0.74f), FVector2D(0.5, 0.5), FVector2D::ZeroVector);

	// Saving indicator: the bottom right corner.
	UHorizontalBox* Saving = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("SavingBox"));
	SavingBox = Saving;
	SavingText = FALKWidgetBuilder::MakeText(Tree, TEXT("SavingText"), LOCTEXT("HUDSaving", "Saving"));
	FALKWidgetBuilder::AddToHorizontalBox(Saving, SavingText, FMargin(0.f, 0.f, 14.f, 0.f), HAlign_Left, VAlign_Center);
	SavingSpinner = FALKWidgetBuilder::Make<UALKSpinner>(Tree, TEXT("SavingSpinner"));
	SavingSpinner->Diameter = 26.f;
	FALKWidgetBuilder::AddToHorizontalBox(Saving, SavingSpinner, FMargin(), HAlign_Left, VAlign_Center);
	Place(Canvas, Saving, FAnchors(1.f, 1.f), FVector2D(1.0, 1.0), FVector2D(-70.0, -60.0));
}

void UALKHUDLayer::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	using namespace ALKHUDPrivate;
	SetShown(PromptMarker, false);
	SetShown(PromptBox, false);
	SetShown(ChapterCard, false);
	SetShown(SavingBox, false);
	SetRenderOpacity(0.f);
}

void UALKHUDLayer::SetNotificationDrop(float InDrop)
{
	NotificationDrop = FMath::Max(0.f, InDrop);
	const UALKTheme* Theme = GetTheme();
	if (UCanvasPanelSlot* BoxSlot = NotificationBox && Theme ? Cast<UCanvasPanelSlot>(NotificationBox->Slot) : nullptr)
	{
		BoxSlot->SetPosition(FVector2D(-Theme->HudMargin.Right, Theme->HudMargin.Top + NotificationDrop));
	}
}

void UALKHUDLayer::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	// Distances from the screen edges.
	const FMargin& Margin = Theme.HudMargin;
	auto SetSlotPosition = [](UWidget* Widget, const FVector2D& Position)
	{
		if (UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			Slot->SetPosition(Position);
		}
	};
	SetSlotPosition(SubtitleBox, FVector2D(0.0, -Margin.Bottom));
	SetSlotPosition(NotificationBox, FVector2D(-Margin.Right, Margin.Top + NotificationDrop));
	SetSlotPosition(SavingBox, FVector2D(-Margin.Right, -Margin.Bottom));

	FALKWidgetBuilder::StyleText(PromptSubject, Theme, EALKTextRole::Data, Theme.TextSecondary);
	if (PromptSubject)
	{
		PromptSubject->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		// Readable over the scene behind it, bright or dark.
		ALKHUDPrivate::AddSceneShadow(PromptSubject, Theme);
	}
	if (PromptFrame)
	{
		FALKFrameStyle Frame = Theme.MakeChipFrame(Theme.Panel.CopyWithNewOpacity(Theme.Panel.A * 0.9f), Theme.PanelOutline);
		Frame.Radius = Theme.CornerRadius;
		PromptFrame->SetFrameStyle(Frame);
	}
	if (PromptKeyCap)
	{
		PromptKeyCap->SetFrameStyle(Theme.MakeChipFrame(Theme.KeyFill, Theme.KeyOutline));
	}
	FALKWidgetBuilder::StyleText(PromptKeyText, Theme, EALKTextRole::Data, Theme.KeyText);
	FALKWidgetBuilder::StyleText(PromptAction, Theme, EALKTextRole::Button, Theme.TextPrimary);
	if (PromptAction)
	{
		FSlateFontInfo Font = Theme.ButtonFont;
		Font.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.ButtonFont.Size * 0.92f));
		FALKWidgetBuilder::SetTextFont(PromptAction, Font);
	}
	if (PromptHoldBar)
	{
		PromptHoldBar->SetColors(Theme.Accent, Theme.SliderTrack);
	}
	if (PromptMarker)
	{
		PromptMarker->SetIcon(EALKIcon::Checkpoint, Theme.Accent);
	}

	for (const FTimedItem& Item : Subtitles)
	{
		StyleSubtitle(Item.Widget.Get(), Theme);
	}

	// The chapter card and the saving indicator stand straight on the game: the accent as it reads as text on this skin,
	// and an edge that keeps them readable over a bright floor or a dark sky.
	FALKWidgetBuilder::StyleText(ChapterNumber, Theme, EALKTextRole::Data, Theme.GetAccentTextColor());
	ALKHUDPrivate::AddSceneShadow(ChapterNumber, Theme);
	if (ChapterRule)
	{
		ChapterRule->SetRuleColor(Theme.GetAccentTextColor());
	}
	FALKWidgetBuilder::StyleText(ChapterName, Theme, EALKTextRole::Title, Theme.TextPrimary);
	if (ChapterName)
	{
		FSlateFontInfo Font = Theme.TitleFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.TitleFont.Size * 0.8f));
		FALKWidgetBuilder::SetTextFont(ChapterName, Font);
		ALKHUDPrivate::AddSceneShadow(ChapterName, Theme, 2.f);
	}

	FALKWidgetBuilder::StyleText(SavingText, Theme, EALKTextRole::Data, Theme.TextSecondary);
	if (SavingText)
	{
		SavingText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		ALKHUDPrivate::AddSceneShadow(SavingText, Theme);
	}
	if (SavingSpinner)
	{
		SavingSpinner->SetSpinnerColor(Theme.Accent);
	}
}

void UALKHUDLayer::NativeConstruct()
{
	Super::NativeConstruct();
	if (!InputMethodChangedHandle.IsValid())
	{
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
		{
			InputMethodChangedHandle = InputSubsystem->OnInputMethodChangedNative.AddWeakLambda(this, [this](ECommonInputType) { RefreshPromptKey(); });
		}
	}
}

void UALKHUDLayer::NativeDestruct()
{
	if (InputMethodChangedHandle.IsValid())
	{
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
		{
			InputSubsystem->OnInputMethodChangedNative.Remove(InputMethodChangedHandle);
		}
		InputMethodChangedHandle.Reset();
	}
	Super::NativeDestruct();
}

// ---------------------------------------------------------------- Prompt

void UALKHUDLayer::ShowPrompt(const FALKInteractionPrompt& InPrompt)
{
	using namespace ALKHUDPrivate;
	Prompt = InPrompt;
	PromptTarget = InPrompt.Target;
	Prompt.Target = nullptr;
	const bool bWasShown = bPromptShown;
	bPromptShown = true;
	PromptAge = bWasShown ? PromptAge : 0.f;

	if (PromptSubject)
	{
		PromptSubject->SetText(Prompt.SubjectText);
		SetShown(PromptSubject, !Prompt.SubjectText.IsEmpty());
	}
	if (PromptAction)
	{
		PromptAction->SetText(Prompt.ActionText);
	}
	if (PromptHoldBar)
	{
		PromptHoldBar->SetValue(0.f);
		SetShown(PromptHoldBar, Prompt.bHold);
	}
	RefreshPromptKey();
	SetShown(PromptBox, true);
	if (!bWasShown)
	{
		if (PromptFrame)
		{
			PromptFrame->PlayIntro(0.2f);
		}
	}
}

void UALKHUDLayer::HidePrompt()
{
	bPromptShown = false;
	bPromptAnchored = false;
	PromptTarget.Reset();
	ALKHUDPrivate::SetShown(PromptBox, false);
	ALKHUDPrivate::SetShown(PromptMarker, false);
}

void UALKHUDLayer::SetPromptProgress(float Progress)
{
	if (PromptHoldBar)
	{
		PromptHoldBar->SetValue(Progress);
	}
}

bool UALKHUDLayer::IsUsingGamepad() const
{
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	return InputSubsystem && InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad;
}

void UALKHUDLayer::RefreshPromptKey()
{
	if (!PromptKeyText || !bPromptShown)
	{
		return;
	}
	// The key the action is bound to now (the player may have rebound it), for the input method in use.
	const bool bGamepad = IsUsingGamepad();
	FKey Key = bGamepad ? Prompt.GamepadKey : Prompt.KeyboardKey;
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (Prompt.Action && LocalPlayer)
	{
		if (const UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			for (const FKey& Mapped : EnhancedInput->QueryKeysMappedToAction(Prompt.Action))
			{
				if (Mapped.IsValid() && Mapped.IsGamepadKey() == bGamepad)
				{
					Key = Mapped;
					break;
				}
			}
		}
	}
	const FText Label = Key.IsValid() ? FALKInputLabels::GetKeyLabel(Key, LocalPlayer) : FText::GetEmpty();
	if (!PromptKeyText->GetText().EqualTo(Label))
	{
		PromptKeyText->SetText(Label);
	}
}

FText UALKHUDLayer::GetPromptKeyText() const
{
	return PromptKeyText ? PromptKeyText->GetText() : FText::GetEmpty();
}

void UALKHUDLayer::UpdatePrompt(const FGeometry& MyGeometry)
{
	using namespace ALKHUDPrivate;
	if (!bPromptShown || !PromptBox)
	{
		return;
	}
	const FVector2D Size = MyGeometry.GetLocalSize();
	FVector2D Position(Size.X * 0.5, Size.Y * 0.66);
	FVector2D Alignment(0.5, 0.5);
	bool bVisible = true;
	bPromptAnchored = false;
	if (PromptTarget.IsValid())
	{
		// Beside the target, joined to it by a leader line; hidden while the target is behind the camera or off screen.
		FVector2D Screen;
		bVisible = false;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), PromptTarget->GetActorLocation() + Prompt.WorldOffset, Screen, true)
			&& Screen.X >= 0.0 && Screen.Y >= 0.0 && Screen.X <= Size.X && Screen.Y <= Size.Y)
		{
			bVisible = true;
			bPromptAnchored = true;
			PromptAnchor = Screen;
			Position = Screen + PromptOffset;
			Alignment = FVector2D(0.0, 0.5);
		}
	}
	else if (PromptTarget.IsStale())
	{
		// The target was destroyed: the prompt goes with it.
		HidePrompt();
		return;
	}
	if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(PromptBox->Slot))
	{
		BoxSlot->SetPosition(Position);
		BoxSlot->SetAlignment(Alignment);
	}
	if (PromptMarker)
	{
		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(PromptMarker->Slot))
		{
			MarkerSlot->SetPosition(PromptAnchor);
		}
		SetShown(PromptMarker, bPromptAnchored);
	}
	const float FadeIn = FMath::Clamp(PromptAge / 0.12f, 0.f, 1.f);
	PromptBox->SetRenderOpacity(bVisible ? FadeIn : 0.f);
	if (PromptMarker)
	{
		PromptMarker->SetRenderOpacity(FadeIn);
	}
}

// ---------------------------------------------------------------- Subtitles

void UALKHUDLayer::AddSubtitle(const FText& Speaker, const FText& Line, float Duration)
{
	const UALKTheme* Theme = GetTheme();
	if (!SubtitleBox || !WidgetTree || !Theme || Line.IsEmpty())
	{
		return;
	}
	USizeBox* Holder = WidgetTree->ConstructWidget<USizeBox>();
	UALKFrame* Frame = WidgetTree->ConstructWidget<UALKFrame>();
	Frame->SetPadding(FMargin(26.f, 9.f, 26.f, 11.f));
	Holder->SetContent(Frame);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Frame->SetContent(Row);
	UTextBlock* SpeakerText = FALKWidgetBuilder::MakeText(*WidgetTree, NAME_None, Speaker);
	UTextBlock* LineText = FALKWidgetBuilder::MakeText(*WidgetTree, NAME_None, Line);
	FALKWidgetBuilder::AddToHorizontalBox(Row, SpeakerText, FMargin(0.f, 0.f, 16.f, 0.f), HAlign_Left, VAlign_Top);
	FALKWidgetBuilder::AddToHorizontalBox(Row, LineText, FMargin(), HAlign_Fill, VAlign_Top, 1.f);
	SpeakerText->SetVisibility(Speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* LineSlot = SubtitleBox->AddChildToVerticalBox(Holder))
	{
		LineSlot->SetHorizontalAlignment(HAlign_Center);
		LineSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	StyleSubtitle(Holder, *Theme);
	Holder->SetRenderOpacity(0.f);

	FTimedItem& Item = Subtitles.AddDefaulted_GetRef();
	Item.Widget = Holder;
	Item.Duration = Duration;

	// The oldest line leaves when there are too many.
	const int32 MaxLines = FMath::Max(1, GetMenuConfig()->MaxSubtitleLines);
	while (Subtitles.Num() > MaxLines)
	{
		if (UWidget* Old = Subtitles[0].Widget.Get())
		{
			Old->RemoveFromParent();
		}
		Subtitles.RemoveAt(0);
	}
}

void UALKHUDLayer::StyleSubtitle(UWidget* Line, const UALKTheme& Theme) const
{
	USizeBox* Holder = Cast<USizeBox>(Line);
	UALKFrame* Frame = Holder ? Cast<UALKFrame>(Holder->GetContent()) : nullptr;
	UHorizontalBox* Row = Frame ? Cast<UHorizontalBox>(Frame->GetContent()) : nullptr;
	if (!Row || Row->GetChildrenCount() < 2)
	{
		return;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	const UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr;
	const float Scale = Settings ? Settings->GetSubtitleScale() : 1.f;
	const float Background = Settings ? Settings->GetSubtitleBackgroundOpacity() : 0.5f;

	Holder->SetMaxDesiredWidth(Theme.SubtitleMaxWidth * Scale);
	Frame->SetFrameStyle(Theme.MakeChipFrame(FLinearColor(0.f, 0.f, 0.f, 0.9f * Background), FLinearColor::Transparent));

	FSlateFontInfo Font = Theme.SubtitleFont;
	Font.Size = FMath::Max(8.f, FMath::RoundToFloat(Theme.SubtitleFont.Size * Scale));
	if (UTextBlock* SpeakerText = Cast<UTextBlock>(Row->GetChildAt(0)))
	{
		FALKWidgetBuilder::SetTextFont(SpeakerText, Font);
		SpeakerText->SetColorAndOpacity(FSlateColor(Theme.Accent));
		SpeakerText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		SpeakerText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		SpeakerText->SetShadowOffset(FVector2D(1.0, 1.0));
	}
	if (UTextBlock* LineText = Cast<UTextBlock>(Row->GetChildAt(1)))
	{
		FALKWidgetBuilder::SetTextFont(LineText, Font);
		// A fixed wrap width (the line's share of Subtitle Max Width) rather than auto wrap: auto wrap inside the HUD's
		// auto-sized layout can keep a stale, narrow width and break short lines ("Go." / "Now.") on wide screens.
		float SpeakerWidth = 0.f;
		const UTextBlock* SpeakerText = Cast<UTextBlock>(Row->GetChildAt(0));
		if (SpeakerText && SpeakerText->GetVisibility() != ESlateVisibility::Collapsed && FSlateApplication::IsInitialized())
		{
			SpeakerWidth = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(SpeakerText->GetText().ToUpper(), Font).X + 16.f;
		}
		LineText->SetAutoWrapText(false);
		LineText->SetWrapTextAt(FMath::Max(120.f, Theme.SubtitleMaxWidth * Scale - 52.f - SpeakerWidth));
		// The box behind the line is black on every skin, so the line is light on every skin: the Light skin's own text
		// colour is dark, its panel colour is the light one.
		LineText->SetColorAndOpacity(FSlateColor(Theme.TextPrimary.GetLuminance() > 0.5f ? Theme.TextPrimary : Theme.Panel.CopyWithNewOpacity(1.f)));
		// A soft shadow keeps lines readable when the box is turned off.
		LineText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		LineText->SetShadowOffset(FVector2D(1.0, 1.0));
	}
}

float UALKHUDLayer::GetNewestSubtitleFontSize() const
{
	const USizeBox* Holder = Subtitles.Num() > 0 ? Cast<USizeBox>(Subtitles.Last().Widget.Get()) : nullptr;
	const UALKFrame* Frame = Holder ? Cast<UALKFrame>(Holder->GetContent()) : nullptr;
	const UHorizontalBox* Row = Frame ? Cast<UHorizontalBox>(Frame->GetContent()) : nullptr;
	const UTextBlock* LineText = Row && Row->GetChildrenCount() > 1 ? Cast<UTextBlock>(Row->GetChildAt(1)) : nullptr;
	return LineText ? static_cast<float>(LineText->GetFont().Size) : 0.f;
}

void UALKHUDLayer::ClearSubtitles()
{
	for (const FTimedItem& Item : Subtitles)
	{
		if (UWidget* Widget = Item.Widget.Get())
		{
			Widget->RemoveFromParent();
		}
	}
	Subtitles.Reset();
}

// ---------------------------------------------------------------- Notifications

void UALKHUDLayer::AddNotification(const FText& Title, const FText& Detail, EALKIcon Icon, float Duration)
{
	const UALKTheme* Theme = GetTheme();
	if (!NotificationBox || !WidgetTree || !Theme)
	{
		return;
	}
	USizeBox* Holder = WidgetTree->ConstructWidget<USizeBox>();
	// Notification Width is the smallest width: a longer title (or its translation) widens the notification up to half
	// as much again, so it is never cut off; the detail wraps inside.
	Holder->SetMinDesiredWidth(Theme->NotificationWidth);
	Holder->SetMaxDesiredWidth(Theme->NotificationWidth * 1.5f);
	UALKFrame* Frame = WidgetTree->ConstructWidget<UALKFrame>();
	Frame->SetPadding(FMargin(26.f, 14.f, 26.f, 15.f));
	Holder->SetContent(Frame);

	FALKFrameStyle Style = Theme->MakePanelFrame();
	Style.Fill = Theme->Panel.CopyWithNewOpacity(Theme->Panel.A * 0.94f);
	Style.Tick = Theme->Accent;
	Style.TickWidth = 4.f;
	Frame->SetFrameStyle(Style);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Frame->SetContent(Row);
	UALKIconWidget* IconWidget = WidgetTree->ConstructWidget<UALKIconWidget>();
	IconWidget->IconSize = 28.f;
	IconWidget->SetIcon(Icon, Theme->GetAccentTextColor());
	FALKWidgetBuilder::AddToHorizontalBox(Row, IconWidget, FMargin(0.f, 0.f, 18.f, 0.f), HAlign_Left, VAlign_Center);
	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
	FALKWidgetBuilder::AddToHorizontalBox(Row, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);

	UALKTitleText* TitleText = WidgetTree->ConstructWidget<UALKTitleText>();
	TitleText->SetText(Title);
	FALKWidgetBuilder::StyleText(TitleText, *Theme, EALKTextRole::Heading, Theme->TextPrimary);
	FSlateFontInfo TitleFont = Theme->HeadingFont;
	TitleFont.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme->HeadingFont.Size * 0.6f));
	FALKWidgetBuilder::SetTextFont(TitleText, TitleFont);
	FALKWidgetBuilder::AddToVerticalBox(Texts, TitleText, FMargin(), HAlign_Left);
	UTextBlock* DetailText = FALKWidgetBuilder::MakeText(*WidgetTree, NAME_None, Detail);
	FALKWidgetBuilder::StyleText(DetailText, *Theme, EALKTextRole::Caption, Theme->TextMuted);
	DetailText->SetAutoWrapText(true);
	DetailText->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	FALKWidgetBuilder::AddToVerticalBox(Texts, DetailText, FMargin(0.f, 4.f, 0.f, 0.f), HAlign_Left);

	if (UVerticalBoxSlot* ItemSlot = NotificationBox->AddChildToVerticalBox(Holder))
	{
		ItemSlot->SetHorizontalAlignment(HAlign_Right);
		ItemSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}
	Frame->PlayIntro(0.26f);
	TitleText->PlayIntro();

	FTimedItem& Item = Notifications.AddDefaulted_GetRef();
	Item.Widget = Holder;
	Item.Duration = FMath::Max(1.f, Duration);

	// Too many: the oldest leaves early (it fades out now).
	const int32 MaxShown = FMath::Max(1, GetMenuConfig()->MaxNotifications);
	for (int32 Index = 0; Index < Notifications.Num() - MaxShown; ++Index)
	{
		Notifications[Index].Duration = FMath::Min(Notifications[Index].Duration, Notifications[Index].Age + 0.3f);
	}
}

// ---------------------------------------------------------------- Chapter card and saving

void UALKHUDLayer::ShowChapterCard(int32 InChapterNumber, const FText& InChapterName, float Delay)
{
	if (ChapterNumber)
	{
		FNumberFormattingOptions TwoDigits;
		TwoDigits.MinimumIntegralDigits = 2;
		ChapterNumber->SetText(FText::Format(LOCTEXT("HUDChapterNumber", "Chapter {0}"), FText::AsNumber(InChapterNumber, &TwoDigits)));
		ChapterNumber->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	if (ChapterName)
	{
		ChapterName->SetText(InChapterName);
	}
	CardTime = 0.f;
	CardDelay = FMath::Max(0.f, Delay);
	bCardVisible = false;
	++ChapterCardCount;
	if (ChapterCard)
	{
		ChapterCard->SetRenderOpacity(0.f);
	}
}

void UALKHUDLayer::ShowSaving(float MinimumSeconds, bool bUntilHidden)
{
	SavingMinimum = FMath::Max(SavingMinimum, MinimumSeconds);
	bSavingRequested = bUntilHidden;
	ALKHUDPrivate::SetShown(SavingBox, true);
}

void UALKHUDLayer::HideSaving()
{
	bSavingRequested = false;
}

bool UALKHUDLayer::IsSavingShown() const
{
	return SavingBox && SavingBox->GetVisibility() != ESlateVisibility::Collapsed;
}

float UALKHUDLayer::GetSettingOpacity() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr;
	return Settings ? Settings->GetHudOpacity() : 1.f;
}

// ---------------------------------------------------------------- Tick and paint

void UALKHUDLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace ALKHUDPrivate;
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float DeltaTime = FMath::Min(InDeltaTime, 0.1f);

	// The HUD steps aside while a menu is open, and its timers wait.
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UALKMenuSubsystem* Menus = LocalPlayer ? LocalPlayer->GetSubsystem<UALKMenuSubsystem>() : nullptr;
	const bool bMenuOpen = Menus && Menus->IsAnyMenuOpen();
	const float TargetOpacity = bHidden || bMenuOpen ? 0.f : GetSettingOpacity();
	CurrentOpacity = FMath::FInterpConstantTo(CurrentOpacity, TargetOpacity, DeltaTime, 6.f);
	SetRenderOpacity(CurrentOpacity);
	if (bMenuOpen)
	{
		return;
	}

	PromptAge += DeltaTime;
	PromptKeyRefresh -= DeltaTime;
	if (PromptKeyRefresh <= 0.f)
	{
		// Also catches a rebinding made while the prompt is shown.
		PromptKeyRefresh = 0.5f;
		RefreshPromptKey();
	}
	UpdatePrompt(MyGeometry);

	auto TickItems = [DeltaTime](TArray<FTimedItem>& Items, float FadeOut)
	{
		for (int32 Index = Items.Num() - 1; Index >= 0; --Index)
		{
			FTimedItem& Item = Items[Index];
			Item.Age += DeltaTime;
			UWidget* Widget = Item.Widget.Get();
			if (!Widget || Item.Age >= Item.Duration)
			{
				if (Widget)
				{
					Widget->RemoveFromParent();
				}
				Items.RemoveAt(Index);
				continue;
			}
			Widget->SetRenderOpacity(TimedOpacity(Item.Age, Item.Duration, 0.15f, FadeOut));
		}
	};
	TickItems(Subtitles, 0.25f);
	TickItems(Notifications, 0.35f);

	if (CardTime >= 0.f && ChapterCard)
	{
		constexpr float CardDuration = 4.8f;
		CardTime += DeltaTime;
		const float Shown = CardTime - CardDelay;
		if (Shown >= 0.f && !bCardVisible)
		{
			bCardVisible = true;
			SetShown(ChapterCard, true);
			if (UALKTitleText* Name = Cast<UALKTitleText>(ChapterName))
			{
				Name->PlayIntro();
			}
		}
		if (Shown >= CardDuration)
		{
			CardTime = -1.f;
			bCardVisible = false;
			SetShown(ChapterCard, false);
		}
		else if (Shown >= 0.f)
		{
			ChapterCard->SetRenderOpacity(TimedOpacity(Shown, CardDuration, 0.3f, 0.8f));
		}
	}

	if (SavingBox && IsSavingShown())
	{
		SavingMinimum = FMath::Max(0.f, SavingMinimum - DeltaTime);
		const bool bWanted = bSavingRequested || SavingMinimum > 0.f;
		const float Opacity = FMath::FInterpConstantTo(SavingBox->GetRenderOpacity(), bWanted ? 1.f : 0.f, DeltaTime, 5.f);
		SavingBox->SetRenderOpacity(Opacity);
		if (!bWanted && Opacity <= 0.f)
		{
			SetShown(SavingBox, false);
		}
	}
}

int32 UALKHUDLayer::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const UALKTheme* Theme = GetTheme();
	if (bPromptShown && bPromptAnchored && Theme && PromptBox && PromptBox->GetRenderOpacity() > 0.01f)
	{
		// The leader line: up at 45 degrees from the marker, then across to the prompt.
		using namespace ALKHUDPrivate;
		const FVector2f Anchor = FVector2f(PromptAnchor);
		const FVector2f Rise = FVector2f(FMath::Abs(static_cast<float>(PromptOffset.Y)), static_cast<float>(PromptOffset.Y));
		const FLinearColor Color = Theme->Accent * InWidgetStyle.GetColorAndOpacityTint() * FLinearColor(1.f, 1.f, 1.f, 0.85f * PromptBox->GetRenderOpacity());
		FALKPaint::Lines(OutDrawElements, Layer + 1, AllottedGeometry,
			{ Anchor + FVector2f(Rise.X, Rise.Y).GetSafeNormal() * 9.f, Anchor + Rise, Anchor + FVector2f(static_cast<float>(PromptOffset.X) - 4.f, static_cast<float>(PromptOffset.Y)) },
			Color, 1.f);
		++Layer;
	}
	return Layer;
}

#undef LOCTEXT_NAMESPACE
