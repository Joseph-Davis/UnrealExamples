// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKPointerTracker.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Framework/Application/SlateApplication.h"
#include "Screens/ALKScreen.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKButton)

UALKButtonStyle_Invisible::UALKButtonStyle_Invisible()
{
	const FSlateBrush None = FSlateNoResource();
	bSingleMaterial = false;
	NormalBase = None;
	NormalHovered = None;
	NormalPressed = None;
	SelectedBase = None;
	SelectedHovered = None;
	SelectedPressed = None;
	Disabled = None;
	ButtonPadding = FMargin(0.f);
	CustomPadding = FMargin(0.f);
	MinWidth = 0;
	MinHeight = 0;
}

UALKButton::UALKButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Style = UALKButtonStyle_Invisible::StaticClass();
}

void UALKButton::NativeOnInitialized()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		NativeBuildDefaultLayout(*WidgetTree);
	}

	Super::NativeOnInitialized();

	OnFocusReceived().AddUObject(this, &ThisClass::HandleFocusChanged);
	OnFocusLost().AddUObject(this, &ThisClass::HandleFocusChanged);

	if (Label)
	{
		Label->SetText(ButtonText);
	}
}

void UALKButton::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	Background = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	Background->SetPadding(FMargin(0.f));
	Background->SetHorizontalAlignment(HAlign_Fill);
	Background->SetVerticalAlignment(VAlign_Fill);
	Tree.RootWidget = Background;

	UOverlay* Overlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("ContentOverlay"));
	Background->SetContent(Overlay);

	// [KeyCap] [Icon] Label: the key cap stays collapsed until Set Input Hint gives the button a key, the icon until Set
	// Leading Icon gives it one.
	UHorizontalBox* LabelRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("LabelRow"));
	FALKWidgetBuilder::AddToOverlay(Overlay, LabelRow, HAlign_Left, VAlign_Center);

	KeyCap = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("KeyCap"));
	KeyCap->SetPadding(FMargin(8.f, 2.f));
	KeyCap->SetHorizontalAlignment(HAlign_Center);
	KeyCap->SetVerticalAlignment(VAlign_Center);
	KeyText = FALKWidgetBuilder::MakeText(Tree, TEXT("KeyText"));
	KeyText->SetJustification(ETextJustify::Center);
	KeyCap->SetContent(KeyText);
	USizeBox* KeySize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("KeySize"));
	KeySize->SetMinDesiredWidth(28.f);
	KeySize->SetMinDesiredHeight(28.f);
	KeySize->SetContent(KeyCap);
	KeySize->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(LabelRow, KeySize, FMargin(0.f, 0.f, 8.f, 0.f), HAlign_Left, VAlign_Center);

	LeadingIcon = FALKWidgetBuilder::Make<UALKIconWidget>(Tree, TEXT("LeadingIcon"));
	LeadingIcon->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(LabelRow, LeadingIcon, FMargin(0.f, 0.f, 10.f, 0.f), HAlign_Left, VAlign_Center);

	UALKTitleText* LabelText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("Label"));
	LabelText->SetText(ButtonText);
	Label = LabelText;
	FALKWidgetBuilder::AddToHorizontalBox(LabelRow, Label, FMargin(), HAlign_Left, VAlign_Center);

	// A small arrow at the far end of a focused menu entry.
	Chevron = FALKWidgetBuilder::Make<UALKIconWidget>(Tree, TEXT("Chevron"));
	Chevron->Icon = EALKIcon::ChevronRight;
	Chevron->IconSize = 18.f;
	Chevron->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(Overlay, Chevron, HAlign_Right, VAlign_Center, FMargin(0.f, 0.f, 18.f, 0.f));

	AccentBar = FALKWidgetBuilder::Make<UBorder>(Tree, TEXT("AccentBar"));
	USizeBox* AccentSize = FALKWidgetBuilder::WrapInSizeBox(Tree, AccentBar, 4.f, 0.f);
	FALKWidgetBuilder::AddToOverlay(Overlay, AccentSize, HAlign_Left, VAlign_Fill, FMargin(0.f, 10.f));
}

void UALKButton::NativeConstruct()
{
	Super::NativeConstruct();

	// Focusing the user widget itself would not let Enter / gamepad accept click the button; forward focus to the
	// internal Slate button CommonUI wraps our content in.
	if (GetIsFocusable())
	{
		SetDesiredFocusWidget(FName(TEXT("InternalRootButtonBase")));
	}

	if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		bGamepadInput = InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad;
		if (!InputMethodChangedHandle.IsValid())
		{
			InputMethodChangedHandle = InputSubsystem->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
		}
	}

	HandleThemeChanged();
	if (!ThemeChangedHandle.IsValid())
	{
		ThemeChangedHandle = UALKTheme::OnAnyThemeChanged().AddUObject(this, &ThisClass::HandleThemeChanged);
	}
	RefreshInputHint();
}

void UALKButton::NativeDestruct()
{
	UALKTheme::OnAnyThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	if (InputMethodChangedHandle.IsValid())
	{
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
		{
			InputSubsystem->OnInputMethodChangedNative.Remove(InputMethodChangedHandle);
		}
		InputMethodChangedHandle.Reset();
	}
	bHoveredVisual = false;
	bPressedVisual = false;
	bEnterPressed = false;
	bWasFocused = false;
	Super::NativeDestruct();
}

void UALKButton::HandleThemeChanged()
{
	if (bApplyThemeStyles)
	{
		ApplyTheme(UALKDeveloperSettings::GetTheme());
	}
}

void UALKButton::ApplyTheme_Implementation(const UALKTheme* Theme)
{
	if (Theme)
	{
		NativeApplyTheme(*Theme);
	}
}

void UALKButton::NativeApplyTheme(const UALKTheme& Theme)
{
	int32 MinW = 0;
	int32 MinH = 0;
	float FontScale = 1.f;
	EHorizontalAlignment LabelAlign = HAlign_Center;
	FMargin LabelPadding = Theme.ButtonContentPadding;

	switch (ButtonStyleType)
	{
	case EALKButtonStyle::Primary:
		MinW = FMath::RoundToInt(Theme.MenuButtonWidth);
		MinH = FMath::RoundToInt(Theme.MenuButtonHeight);
		LabelAlign = HAlign_Left;
		break;
	case EALKButtonStyle::Secondary:
	case EALKButtonStyle::Danger:
	case EALKButtonStyle::Solid:
	case EALKButtonStyle::Ghost:
		MinH = FMath::RoundToInt(Theme.SecondaryButtonHeight);
		FontScale = 0.68f;
		LabelPadding = FMargin(22.f, 4.f);
		break;
	case EALKButtonStyle::Tab:
		MinH = 44;
		FontScale = 0.64f;
		LabelPadding = FMargin(18.f, 6.f);
		break;
	case EALKButtonStyle::Arrow:
		MinW = 34;
		MinH = 34;
		FontScale = 0.8f;
		LabelPadding = FMargin(0.f);
		break;
	case EALKButtonStyle::Hint:
		// Sized by LayoutLabel: boxed while the mouse is in use, bare for gamepads.
		break;
	}

	if (ButtonStyleType != EALKButtonStyle::Hint)
	{
		SetMinDimensions(MinW, MinH);
	}

	if (Label)
	{
		if (ButtonStyleType == EALKButtonStyle::Hint)
		{
			FALKWidgetBuilder::StyleText(Label, Theme, EALKTextRole::Caption);
		}
		else
		{
			FALKWidgetBuilder::StyleText(Label, Theme, EALKTextRole::Button);
			if (!FMath::IsNearlyEqual(FontScale, 1.f))
			{
				FSlateFontInfo Font = Theme.ButtonFont;
				Font.Size = FMath::Max(8.f, FMath::RoundToFloat(Font.Size * FontScale));
				FALKWidgetBuilder::SetTextFont(Label, Font);
			}
		}
	}
	LabelAlignment = LabelAlign;
	LabelPaddingValue = LabelPadding;
	LayoutLabel(Theme);

	if (KeyText)
	{
		FSlateFontInfo KeyFont = Theme.DataFont;
		KeyFont.Size = FMath::Max(8.f, KeyFont.Size - 1.f);
		KeyFont.LetterSpacing = 0;
		FALKWidgetBuilder::SetTextFont(KeyText, KeyFont);
		KeyText->SetShadowColorAndOpacity(FLinearColor::Transparent);
	}

	UpdateLeadingIcon();

	if (Background && !Cast<UALKFrame>(Background))
	{
		Background->SetBrush(Theme.MakeRoundedBrush(FLinearColor::White, FLinearColor::White, Theme.OutlineWidth));
	}

	if (AccentBar)
	{
		// A frame draws its own accent bar; plain border layouts get a bar beside menu entries.
		const bool bFramed = Cast<UALKFrame>(Background) != nullptr;
		const bool bHasBar = !bFramed && ButtonStyleType == EALKButtonStyle::Primary && Theme.AccentBarWidth > 0.f;
		AccentBar->SetVisibility(bHasBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		AccentBar->SetBrush(Theme.MakeRoundedBrush(FLinearColor::White, FLinearColor::Transparent, 0.f, 0.5f));
		if (USizeBox* AccentSize = Cast<USizeBox>(AccentBar->GetParent()))
		{
			UOverlaySlot* AccentSlot = Cast<UOverlaySlot>(AccentSize->Slot);
			if (ButtonStyleType == EALKButtonStyle::Tab)
			{
				AccentSize->ClearWidthOverride();
				AccentSize->SetHeightOverride(2.f);
				if (AccentSlot)
				{
					AccentSlot->SetHorizontalAlignment(HAlign_Fill);
					AccentSlot->SetVerticalAlignment(VAlign_Bottom);
					AccentSlot->SetPadding(FMargin(16.f, 0.f, 16.f, 4.f));
				}
			}
			else
			{
				AccentSize->ClearHeightOverride();
				AccentSize->SetWidthOverride(Theme.AccentBarWidth);
				if (AccentSlot)
				{
					// A short dash to the left of the text.
					AccentSlot->SetHorizontalAlignment(HAlign_Left);
					AccentSlot->SetVerticalAlignment(VAlign_Center);
					AccentSlot->SetPadding(FMargin(-Theme.AccentBarWidth - 18.f, 0.f, 0.f, 0.f));
				}
				AccentSize->SetHeightOverride(ButtonStyleType == EALKButtonStyle::Primary ? 2.f : 0.f);
			}
		}
	}

	UpdateVisualTargets(true);
}

void UALKButton::LayoutLabel(const UALKTheme& Theme)
{
	if (!Label)
	{
		return;
	}

	EHorizontalAlignment Align = LabelAlignment;
	FMargin LabelPad = LabelPaddingValue;
	if (ButtonStyleType == EALKButtonStyle::Hint)
	{
		const bool bBoxed = IsHintBoxed();
		Align = HAlign_Left;
		// A key on its own sits in a tighter box than a key with a label.
		LabelPad = bBoxed ? (ButtonText.IsEmpty() ? FMargin(8.f, 4.f) : FMargin(12.f, 4.f)) : FMargin(0.f);
		SetMinDimensions(0, bBoxed ? 32 : 0);
	}

	// The label sits in the LabelRow next to the key cap; a designer layout may put it in the overlay directly.
	UWidget* Holder = Label;
	if (UHorizontalBox* Row = Cast<UHorizontalBox>(Label->GetParent()))
	{
		Holder = Row;
	}
	if (UOverlaySlot* HolderSlot = Cast<UOverlaySlot>(Holder->Slot))
	{
		HolderSlot->SetHorizontalAlignment(Align);
		HolderSlot->SetPadding(LabelPad);
	}
}

bool UALKButton::IsHintBoxed() const
{
	// Clickable hints get a box while the mouse is in use, so they read as buttons; gamepad hints and bare keys (tab
	// caps) stay as they are.
	return ButtonStyleType == EALKButtonStyle::Hint && bHasInputHint && InputHint.Action.IsBound() && !bGamepadInput && (!ButtonText.IsEmpty() || bBoxKeyOnlyHint);
}

void UALKButton::SetInputHint(const FALKInputHint& InHint)
{
	InputHint = InHint;
	bHasInputHint = true;
	RefreshInputHint();
}

void UALKButton::ClearInputHint()
{
	InputHint = FALKInputHint();
	bHasInputHint = false;
	RefreshInputHint();
}

void UALKButton::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	bGamepadInput = NewInputType == ECommonInputType::Gamepad;
	RefreshInputHint();
}

void UALKButton::RefreshInputHint()
{
	const FText Key = bHasInputHint ? InputHint.GetKeyText(bGamepadInput, GetOwningLocalPlayer()) : FText::GetEmpty();
	if (KeyText)
	{
		KeyText->SetText(Key);
	}
	if (UWidget* KeyHolder = KeyCap ? KeyCap->GetParent() : nullptr)
	{
		if (UHorizontalBoxSlot* KeySlot = Cast<UHorizontalBoxSlot>(KeyHolder->Slot))
		{
			KeySlot->SetPadding(FMargin(0.f, 0.f, ButtonText.IsEmpty() ? 0.f : 8.f, 0.f));
		}
	}
	if (Label)
	{
		// A hint without a label (tab caps) is just its key.
		Label->SetVisibility(ButtonText.IsEmpty() && bHasInputHint ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	UpdateKeyCapVisibility();

	if (ButtonStyleType == EALKButtonStyle::Hint && bHasInputHint)
	{
		// A hint the gamepad has no button for (a mouse only "Apply") disappears while a gamepad is in use.
		const bool bHidden = bGamepadInput && Key.IsEmpty();
		const ESlateVisibility Shown = InputHint.Action.IsBound() ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible;
		SetVisibility(bHidden ? ESlateVisibility::Collapsed : Shown);
	}

	if (const UALKTheme* Theme = UALKDeveloperSettings::GetTheme())
	{
		LayoutLabel(*Theme);
	}
	UpdateVisualTargets(true);
}

void UALKButton::UpdateKeyCapVisibility()
{
	UWidget* KeyHolder = KeyCap ? KeyCap->GetParent() : nullptr;
	if (!KeyHolder)
	{
		return;
	}
	bool bShow = bHasInputHint && KeyText && !KeyText->GetText().IsEmpty();
	if (bShow && InputHint.bOnlyWhenFocused)
	{
		bShow = HasAnyUserFocus() || HasFocusedDescendants();
	}
	const ESlateVisibility Wanted = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (KeyHolder->GetVisibility() != Wanted)
	{
		KeyHolder->SetVisibility(Wanted);
	}
}

void UALKButton::SetLeadingIcon(EALKIcon InIcon)
{
	LeadingIconType = InIcon;
	UpdateLeadingIcon();
	ApplyCurrentVisuals();
}

void UALKButton::UpdateLeadingIcon()
{
	if (!LeadingIcon)
	{
		return;
	}
	const bool bShow = LeadingIconType != EALKIcon::None;
	LeadingIcon->IconSize = ButtonStyleType == EALKButtonStyle::Primary ? 24.f : 18.f;
	LeadingIcon->SetIcon(LeadingIconType, LeadingIcon->Color);
	LeadingIcon->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UALKButton::SetButtonText(const FText& InText)
{
	ButtonText = InText;
	if (Label)
	{
		Label->SetText(ButtonText);
	}
	if (bHasInputHint)
	{
		RefreshInputHint();
	}
}

void UALKButton::SetActiveVisual(bool bInActive)
{
	if (bActiveVisual != bInActive)
	{
		bActiveVisual = bInActive;
		UpdateVisualTargets(false);
	}
}

void UALKButton::SetButtonStyleType(EALKButtonStyle InStyle)
{
	if (ButtonStyleType != InStyle)
	{
		ButtonStyleType = InStyle;
		HandleThemeChanged();
	}
}

bool UALKButton::IsHighlighted() const
{
	const bool bFocused = HasAnyUserFocus() || HasFocusedDescendants();
	// Focusable buttons follow focus only (the mouse moves focus in NativeOnMouseMove), so exactly one is highlighted.
	if (bFocusOnHover && GetIsFocusable())
	{
		return bFocused;
	}
	return bHoveredVisual || bFocused;
}

FReply UALKButton::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = Super::NativeOnMouseMove(InGeometry, InMouseEvent);

	// Real mouse movement only (a pixel or more): Slate sends synthetic moves when a widget appears under a resting cursor, and CommonUI
	// moves the cursor itself for gamepads and when a menu opens.
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bUsingMouse = !InputSubsystem || InputSubsystem->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
	if (bFocusOnHover && bUsingMouse && !UALKScreen::IsInActivationGracePeriod() && FALKPointerTracker::IsMouseMovingThisFrame() && FVector2D(InMouseEvent.GetCursorDelta()).SizeSquared() >= 1.0
		&& GetIsFocusable() && IsInteractionEnabled() && !HasAnyUserFocus() && !HasFocusedDescendants())
	{
		SetFocus();
	}
	return Reply;
}

void UALKButton::SimulateClick()
{
	if (GetIsEnabled() && IsInteractionEnabled())
	{
		HandleButtonPressed();
		HandleButtonReleased();
		HandleButtonClicked();
	}
}

void UALKButton::UpdateVisualTargets(bool bInstant)
{
	const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
	if (!Theme || !bApplyThemeStyles)
	{
		return;
	}

	const bool bEnabled = GetIsEnabled() && IsInteractionEnabled();
	const bool bHighlighted = bEnabled && IsHighlighted();
	const bool bShowSelected = GetSelected() || bActiveVisual;

	FLinearColor Fill = Theme->ButtonNormal;
	FLinearColor Outline = FLinearColor::Transparent;
	FLinearColor Text = Theme->TextPrimary;
	float Accent = 0.f;

	switch (ButtonStyleType)
	{
	case EALKButtonStyle::Primary:
		Fill = bHighlighted ? Theme->ButtonHovered : Theme->ButtonNormal;
		Text = bHighlighted ? Theme->GetFocusTextColor() : Theme->TextSecondary;
		Accent = (bHighlighted || bShowSelected) ? 1.f : 0.f;
		break;

	case EALKButtonStyle::Secondary:
		// A raised box with a thin outline; focus turns the outline to the accent colour.
		Fill = Theme->PanelRaised;
		Outline = (bHighlighted || bShowSelected) ? Theme->Accent : Theme->PanelOutline;
		// Marked as the chosen one (the key slot of a focused binding row, a vote given): where the focused row is filled
		// with the accent colour, an accent outline alone vanishes into that row, so the box darkens to the contrast colour.
		if (bShowSelected && Theme->FocusStyle == EALKFocusStyle::Solid)
		{
			Fill = Theme->AccentContrast;
		}
		Text = Theme->TextPrimary;
		Accent = bHighlighted ? 1.f : 0.f;
		break;

	case EALKButtonStyle::Danger:
		// Tinted with the danger colour, stronger with focus.
		Fill = FMath::Lerp(Theme->Panel, Theme->Danger, bHighlighted ? 0.26f : 0.14f).CopyWithNewOpacity(1.f);
		Outline = Theme->Danger;
		Text = FMath::Lerp(Theme->Danger, Theme->TextPrimary, bHighlighted ? 0.5f : 0.15f).CopyWithNewOpacity(1.f);
		Accent = bHighlighted ? 1.f : 0.f;
		break;

	case EALKButtonStyle::Solid:
		// Filled with the accent colour; focus adds the ring.
		Fill = Theme->Accent;
		Text = Theme->AccentContrast;
		Accent = bHighlighted ? 1.f : 0.f;
		break;

	case EALKButtonStyle::Ghost:
		Fill = bHighlighted ? Theme->ButtonHovered : FLinearColor::Transparent;
		Outline = bHighlighted && Theme->FocusStyle == EALKFocusStyle::Solid ? Theme->PanelOutline : FLinearColor::Transparent;
		Text = bHighlighted ? Theme->GetFocusTextColor() : Theme->TextMuted;
		Accent = bHighlighted ? 1.f : 0.f;
		break;

	case EALKButtonStyle::Tab:
		// The shown tab is a solid accent chip; the others are outlined chips, and focus turns their outline to the accent colour.
		Fill = bShowSelected ? Theme->Accent : Theme->Panel;
		Outline = bShowSelected ? (Theme->PanelStyle == EALKPanelStyle::Chunky ? Theme->PanelOutline : FLinearColor::Transparent)
			: (bHighlighted ? Theme->Accent : Theme->PanelOutline);
		Text = bShowSelected ? Theme->AccentContrast : (bHighlighted ? Theme->TextPrimary : Theme->TextMuted);
		Accent = bShowSelected ? 1.f : 0.f;
		break;

	case EALKButtonStyle::Arrow:
		Fill = FLinearColor::Transparent;
		Text = bHighlighted ? Theme->GetAccentTextColor() : Theme->TextMuted;
		break;

	case EALKButtonStyle::Hint:
	{
		// Clickable hints keep a (theme coloured) box while the mouse is in use; the text lights up on hover.
		const bool bBoxed = IsHintBoxed();
		Fill = bBoxed && bHighlighted ? Theme->ButtonHovered : FLinearColor::Transparent;
		Outline = bBoxed ? Theme->PanelOutline : FLinearColor::Transparent;
		Text = bHighlighted ? Theme->TextPrimary : Theme->TextMuted;
		break;
	}
	}

	UpdateKeyCapVisibility();

	const bool bDrawsOwnFill = ButtonStyleType == EALKButtonStyle::Hint && !IsHintBoxed();
	const bool bKeepsFill = ButtonStyleType == EALKButtonStyle::Solid || ButtonStyleType == EALKButtonStyle::Danger || ButtonStyleType == EALKButtonStyle::Secondary
		|| (ButtonStyleType == EALKButtonStyle::Tab && bShowSelected);
	if (bPressedVisual && bEnabled && !bDrawsOwnFill)
	{
		// Filled buttons darken a little; the others take the pressed tint.
		Fill = bKeepsFill ? FMath::Lerp(Fill, FLinearColor::Black, 0.12f).CopyWithNewOpacity(Fill.A) : Theme->ButtonPressed;
	}

	if (!bEnabled)
	{
		Fill = bDrawsOwnFill ? FLinearColor::Transparent : (bKeepsFill ? Fill.CopyWithNewOpacity(Fill.A * 0.35f) : Theme->ButtonDisabled);
		Outline = Outline.CopyWithNewOpacity(Outline.A * 0.4f);
		Text = Theme->TextMuted.CopyWithNewOpacity(Theme->TextMuted.A * 0.7f);
		Accent = 0.f;
	}

	const bool bTargetsChanged = !TargetFill.Equals(Fill) || !TargetOutline.Equals(Outline) || !TargetText.Equals(Text) || !FMath::IsNearlyEqual(TargetAccent, Accent);
	if (!bTargetsChanged && !bInstant)
	{
		// Nothing new; NativeTick keeps blending towards the current targets.
		return;
	}

	TargetFill = Fill;
	TargetOutline = Outline;
	TargetText = Text;
	TargetAccent = Accent;

	if (bInstant || Theme->HoverTransitionSpeed <= 0.f)
	{
		CurrentFill = TargetFill;
		CurrentOutline = TargetOutline;
		CurrentText = TargetText;
		CurrentAccent = TargetAccent;
		ApplyCurrentVisuals();
	}
}

void UALKButton::ApplyCurrentVisuals()
{
	if (!bApplyThemeStyles)
	{
		return;
	}

	const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();

	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		FALKFrameStyle FrameStyle;
		switch (ButtonStyleType)
		{
		case EALKButtonStyle::Primary:
			FrameStyle = Theme->MakeItemFrame(CurrentAccent);
			FrameStyle.Fill = Theme->GetFocusFill(CurrentFill);
			break;
		case EALKButtonStyle::Secondary:
		case EALKButtonStyle::Danger:
		case EALKButtonStyle::Solid:
		{
			// A box in the theme's shape; the main action and the danger button get a ring and a glow with focus.
			FrameStyle = Theme->MakeSolidButtonFrame(ButtonStyleType == EALKButtonStyle::Secondary ? 0.f : CurrentAccent, ButtonStyleType == EALKButtonStyle::Danger);
			FrameStyle.Fill = CurrentFill;
			if (ButtonStyleType != EALKButtonStyle::Solid)
			{
				// Boxed buttons are lit like the panels they sit on; only the main action shines.
				FrameStyle.Outline = CurrentOutline;
				FrameStyle.Gradient = Theme->PanelGradient;
				FrameStyle.TopEdge = Theme->PanelTopEdge;
				if (ButtonStyleType == EALKButtonStyle::Secondary)
				{
					FrameStyle.Glow = Theme->Accent.CopyWithNewOpacity(Theme->Accent.A * Theme->FocusGlow * 0.5f * CurrentAccent);
					FrameStyle.GlowSize = Theme->FocusGlowSize * 0.75f;
				}
			}
			break;
		}
		case EALKButtonStyle::Ghost:
			FrameStyle = Theme->MakeItemFrame(CurrentAccent, false);
			FrameStyle.Fill = Theme->GetFocusFill(CurrentFill);
			break;
		default:
			FrameStyle = Theme->MakeChipFrame(CurrentFill, CurrentOutline);
			// Tabs and filter chips are lit like panels; the shown tab, filled with the accent, a little more.
			FrameStyle.Gradient = ButtonStyleType == EALKButtonStyle::Tab ? Theme->PanelGradient * (1.f + 1.5f * CurrentAccent) : 0.f;
			break;
		}
		Frame->SetFrameStyle(FrameStyle);
	}
	else if (Background)
	{
		Background->SetBrushColor(FLinearColor::White);
		FSlateBrush Brush = Theme->MakeRoundedBrush(CurrentFill, CurrentOutline, CurrentOutline.A > 0.f ? Theme->OutlineWidth : 0.f);
		Background->SetBrush(Brush);
	}
	if (AccentBar)
	{
		AccentBar->SetBrushColor(Theme->Accent.CopyWithNewOpacity(Theme->Accent.A * CurrentAccent));
	}
	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(CurrentText));
	}
	if (LeadingIcon && LeadingIconType != EALKIcon::None)
	{
		LeadingIcon->SetIcon(LeadingIconType, CurrentText);
	}
	if (Chevron)
	{
		const bool bChevron = ButtonStyleType == EALKButtonStyle::Primary && CurrentAccent > 0.01f;
		Chevron->SetVisibility(bChevron ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bChevron)
		{
			Chevron->SetIcon(EALKIcon::ChevronRight, Theme->GetFocusTextColor().CopyWithNewOpacity(CurrentAccent));
		}
	}
	if (KeyCap)
	{
		// Keys are small chips in the theme's key colours.
		const float KeyAlpha = IsUsable() ? 1.f : 0.35f;
		const FLinearColor KeyFill = Theme->KeyFill.CopyWithNewOpacity(Theme->KeyFill.A * KeyAlpha);
		const FLinearColor KeyOutline = Theme->KeyOutline.CopyWithNewOpacity(Theme->KeyOutline.A * KeyAlpha);
		if (UALKFrame* KeyFrame = Cast<UALKFrame>(KeyCap))
		{
			KeyFrame->SetFrameStyle(Theme->MakeChipFrame(KeyFill, KeyOutline));
		}
		else
		{
			KeyCap->SetBrush(bGamepadInput ? UALKTheme::MakePillBrush(KeyFill, KeyOutline, Theme->OutlineWidth)
				: Theme->MakeRoundedBrush(KeyFill, KeyOutline, Theme->OutlineWidth, 1.f));
		}
		if (KeyText)
		{
			KeyText->SetColorAndOpacity(FSlateColor(Theme->KeyText.CopyWithNewOpacity(Theme->KeyText.A * KeyAlpha)));
		}
	}
	NativeApplyVisualState(*Theme, CurrentAccent, IsUsable());
}

void UALKButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Focus can move without an event we listen to (e.g. focus set on a parent), so re-evaluate cheaply every frame.
	UpdateVisualTargets(false);

	const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
	const float Speed = Theme ? Theme->HoverTransitionSpeed : 0.f;
	if (Speed <= 0.f)
	{
		return;
	}

	const bool bSettled = CurrentFill.Equals(TargetFill, 0.002f) && CurrentOutline.Equals(TargetOutline, 0.002f)
		&& CurrentText.Equals(TargetText, 0.002f) && FMath::IsNearlyEqual(CurrentAccent, TargetAccent, 0.002f);
	if (bSettled)
	{
		return;
	}

	const float Alpha = FMath::Clamp(InDeltaTime * Speed, 0.f, 1.f);
	CurrentFill = FMath::Lerp(CurrentFill, TargetFill, Alpha);
	CurrentOutline = FMath::Lerp(CurrentOutline, TargetOutline, Alpha);
	CurrentText = FMath::Lerp(CurrentText, TargetText, Alpha);
	CurrentAccent = FMath::Lerp(CurrentAccent, TargetAccent, Alpha);

	// Snap the last step so the button stops re-painting once it is visually settled.
	if (CurrentFill.Equals(TargetFill, 0.002f) && CurrentOutline.Equals(TargetOutline, 0.002f)
		&& CurrentText.Equals(TargetText, 0.002f) && FMath::IsNearlyEqual(CurrentAccent, TargetAccent, 0.002f))
	{
		CurrentFill = TargetFill;
		CurrentOutline = TargetOutline;
		CurrentText = TargetText;
		CurrentAccent = TargetAccent;
	}
	ApplyCurrentVisuals();
}

void UALKButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	// Once each time focus enters the button, however it arrives. CommonUI's own focus events differ between engine
	// versions (from 5.6 the user widget forwards focus to its inner Slate button), so the sound does not rely on them.
	PlayThemeSound(EALKUISound::Hover);
}

void UALKButton::HandleFocusChanged()
{
	// Focus arrives twice (the user widget, then the inner Slate button it forwards to), so act on the edge only.
	const bool bFocused = HasAnyUserFocus() || HasFocusedDescendants();
	if (!bFocused && bEnterPressed)
	{
		// Focus moved away while Enter was held: cancel the press instead of clicking later.
		bEnterPressed = false;
		bPressedVisual = false;
	}
	bWasFocused = bFocused;
	UpdateVisualTargets(false);
}

namespace ALKButtonPrivate
{
	static bool IsAcceptKey(const FKeyEvent& KeyEvent)
	{
		return KeyEvent.GetKey() == EKeys::Enter
			|| (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetNavigationActionFromKey(KeyEvent) == EUINavigationAction::Accept);
	}
}

FReply UALKButton::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Accept keys only get here when CommonUI's inner Slate button did not handle them: always for Enter (the inner
	// button deliberately ignores it and expects a Default Click Action from CommonUI Input Data), and for Space /
	// gamepad accept when keyboard focus sits on this widget rather than the inner button. Click in both cases, so the
	// button works with no project setup.
	if (ALKButtonPrivate::IsAcceptKey(InKeyEvent) && GetIsEnabled() && IsInteractionEnabled())
	{
		if (!InKeyEvent.IsRepeat() && !bEnterPressed)
		{
			bEnterPressed = true;
			HandleButtonPressed();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UALKButton::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bEnterPressed && ALKButtonPrivate::IsAcceptKey(InKeyEvent))
	{
		bEnterPressed = false;
		HandleButtonReleased();
		HandleButtonClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UALKButton::NativeOnHovered()
{
	Super::NativeOnHovered();

	const bool bWasHighlighted = IsHighlighted();
	bHoveredVisual = true;
	if (!bWasHighlighted && IsHighlighted())
	{
		PlayThemeSound(EALKUISound::Hover);
	}
	UpdateVisualTargets(false);
}

void UALKButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	bHoveredVisual = false;
	bPressedVisual = false;
	UpdateVisualTargets(false);
}

void UALKButton::NativeOnPressed()
{
	Super::NativeOnPressed();
	bPressedVisual = true;
	UpdateVisualTargets(true);
}

void UALKButton::NativeOnReleased()
{
	Super::NativeOnReleased();
	bPressedVisual = false;
	UpdateVisualTargets(false);
}

void UALKButton::NativeOnClicked()
{
	if (bPlayClickSound)
	{
		PlayThemeSound(EALKUISound::Click);
	}
	Super::NativeOnClicked();
}

void UALKButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	UpdateVisualTargets(false);
}

void UALKButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	UpdateVisualTargets(false);
}

void UALKButton::NativeOnEnabled()
{
	Super::NativeOnEnabled();
	UpdateVisualTargets(true);
}

void UALKButton::NativeOnDisabled()
{
	Super::NativeOnDisabled();
	UpdateVisualTargets(true);
}

void UALKButton::PlayThemeSound(EALKUISound Sound) const
{
	UALKTheme::PlayUISound(this, Sound);
}
