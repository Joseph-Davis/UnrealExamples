// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Settings/ALKSettingRows.h"
#include "Settings/ALKSetting.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Screens/ALKScreen.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKPointerTracker.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSettingRows)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

// ================================================================================================ Base row

UALKSettingRow::UALKSettingRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UALKSettingRow::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	USizeBox* Root = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("RowRoot"));
	Tree.RootWidget = Root;

	RowBackground = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("RowBackground"));
	RowBackground->SetPadding(FMargin(22.f, 2.f, 16.f, 2.f));
	RowBackground->SetVerticalAlignment(VAlign_Center);
	Root->AddChild(RowBackground);

	UHorizontalBox* Content = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("RowContent"));
	RowBackground->SetContent(Content);

	// Name on the left half, value on the right half.
	NameText = FALKWidgetBuilder::MakeText(Tree, TEXT("NameText"));
	FALKWidgetBuilder::AddToHorizontalBox(Content, NameText, FMargin(0.f, 0.f, 16.f, 0.f), HAlign_Left, VAlign_Center, 0.52f);

	UHorizontalBox* ValueBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ValueBox"));
	FALKWidgetBuilder::AddToHorizontalBox(Content, ValueBox, FMargin(), HAlign_Fill, VAlign_Center, 0.48f);

	NativeBuildValueArea(Tree, *ValueBox);
}

FSlateFontInfo UALKSettingRow::GetRowFont(const UALKTheme& Theme)
{
	// The menu's capitals, smaller and a little lighter.
	FSlateFontInfo Font = Theme.ButtonFont;
	Font.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.ButtonFont.Size * 0.6f));
	Font.TypefaceFontName = FName(TEXT("SemiBold"));
	Font.LetterSpacing = FMath::RoundToInt(Theme.ButtonFont.LetterSpacing * 0.8f);
	return Font;
}

void UALKSettingRow::StyleRowText(UTextBlock* Text, const UALKTheme& Theme)
{
	if (Text)
	{
		FALKWidgetBuilder::SetTextFont(Text, GetRowFont(Theme));
		Text->SetTextTransformPolicy(Theme.bUppercaseButtons ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	}
}

void UALKSettingRow::NativeApplyTheme(const UALKTheme& Theme)
{
	if (USizeBox* Root = Cast<USizeBox>(WidgetTree ? WidgetTree->RootWidget : nullptr))
	{
		Root->SetMinDesiredHeight(Theme.SettingRowHeight);
	}
	FALKWidgetBuilder::StyleText(NameText, Theme, EALKTextRole::Body);
	StyleRowText(NameText, Theme);
	ApplyHighlight();
}

void UALKSettingRow::SetSetting(UALKSetting* InSetting)
{
	Setting = InSetting;
	if (NameText && Setting)
	{
		NameText->SetText(Setting->DisplayName);
	}
	Refresh();
}

void UALKSettingRow::Refresh()
{
	if (Setting)
	{
		NativeRefresh();
		ApplyHighlight();
		OnRefreshed(Setting);
	}
}

bool UALKSettingRow::IsRowHighlighted() const
{
	return HasAnyUserFocus() || HasFocusedDescendants();
}

void UALKSettingRow::HandleGamepadAccept()
{
	if (Setting && Setting->IsEditable())
	{
		NativeOnAccept();
	}
}

UALKMenuSubsystem* UALKSettingRow::GetMenuSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UALKMenuSubsystem>() : nullptr;
}

UALKButton* UALKSettingRow::CreateRowButton(UWidgetTree& Tree, const FText& Text, EALKButtonStyle Style)
{
	TSubclassOf<UALKButton> ButtonClass = UALKDeveloperSettings::GetMenuConfig()->ButtonClass;
	if (!ButtonClass)
	{
		ButtonClass = UALKButton::StaticClass();
	}
	UALKButton* Button = Tree.ConstructWidget<UALKButton>(ButtonClass);
	Button->SetButtonStyleType(Style);
	Button->SetButtonText(Text);
	// Rows own keyboard / gamepad focus; their buttons are for the mouse only.
	Button->SetIsFocusable(false);
	Button->bFocusOnHover = false;
	// Value arrows play the Change sound when the value changes, instead of a click.
	Button->bPlayClickSound = Style != EALKButtonStyle::Arrow;
	return Button;
}

void UALKSettingRow::ApplyHighlight()
{
	const UALKTheme* Theme = GetTheme();
	if (!Theme || !bApplyThemeStyles)
	{
		return;
	}

	// The focused row lights up in a rounded frame with the small accent bar; its name brightens.
	const bool bEditable = !Setting || Setting->IsEditable();
	if (UALKFrame* Frame = Cast<UALKFrame>(RowBackground))
	{
		FALKFrameStyle FrameStyle = Theme->MakeItemFrame(Highlight, true);
		FrameStyle.Fill = Theme->GetFocusFill(FMath::Lerp(Theme->RowNormal, Theme->RowFocused, Highlight));
		Frame->SetFrameStyle(FrameStyle);
	}
	else if (RowBackground)
	{
		RowBackground->SetBrush(Theme->MakeRoundedBrush(FMath::Lerp(Theme->RowNormal, Theme->RowFocused, Highlight)));
	}
	if (NameText)
	{
		const FLinearColor TextColor = bEditable ? FMath::Lerp(Theme->TextSecondary, Theme->GetFocusTextColor(), Highlight) : Theme->TextMuted;
		NameText->SetColorAndOpacity(FSlateColor(TextColor));
	}
	NativeApplyHighlight(*Theme, Highlight, bEditable);
}

void UALKSettingRow::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TargetHighlight = IsRowHighlighted() ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Highlight, TargetHighlight, 0.001f))
	{
		const UALKTheme* Theme = GetTheme();
		const float Speed = Theme ? Theme->HoverTransitionSpeed : 0.f;
		Highlight = Speed > 0.f ? FMath::FInterpTo(Highlight, TargetHighlight, FMath::Min(InDeltaTime, 0.05f), Speed) : TargetHighlight;
		if (FMath::IsNearlyEqual(Highlight, TargetHighlight, 0.01f))
		{
			Highlight = TargetHighlight;
		}
		ApplyHighlight();
	}
}

bool UALKSettingRow::AdjustValue(int32 Direction)
{
	const FText Before = Setting ? Setting->GetValueText() : FText::GetEmpty();
	const bool bHandled = NativeOnAdjust(Direction);
	if (bHandled)
	{
		PlayChangeSoundIfChanged(Before);
	}
	return bHandled;
}

void UALKSettingRow::PlayChangeSoundIfChanged(const FText& Before) const
{
	if (Setting && !Setting->GetValueText().EqualTo(Before))
	{
		UALKTheme::PlayUISound(this, EALKUISound::Change);
	}
}

void UALKSettingRow::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	UALKTheme::PlayUISound(this, EALKUISound::Hover);
	OnRowFocused.Broadcast(this, InFocusEvent.GetCause() == EFocusCause::Navigation);
}

void UALKSettingRow::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
}

void UALKSettingRow::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bMouseOver = true;
}

FReply UALKSettingRow::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FReply Reply = Super::NativeOnMouseMove(InGeometry, InMouseEvent);

	// Real mouse movement only (a pixel or more): Slate sends synthetic moves when rows scroll under a resting cursor, and CommonUI moves
	// the cursor itself for gamepads and when a menu opens.
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bUsingMouse = !InputSubsystem || InputSubsystem->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
	if (bUsingMouse && !UALKScreen::IsInActivationGracePeriod() && FALKPointerTracker::IsMouseMovingThisFrame() && FVector2D(InMouseEvent.GetCursorDelta()).SizeSquared() >= 1.0
		&& !HasAnyUserFocus() && !HasFocusedDescendants())
	{
		SetFocus();
	}
	return Reply;
}

void UALKSettingRow::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bMouseOver = false;
}

void UALKSettingRow::NativeConstruct()
{
	Super::NativeConstruct();

	if (!InputMethodChangedHandle.IsValid())
	{
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
		{
			InputMethodChangedHandle = InputSubsystem->OnInputMethodChangedNative.AddWeakLambda(this, [this](ECommonInputType)
			{
				UpdateGamepadHitTesting();
			});
		}
	}
	UpdateGamepadHitTesting();
}

void UALKSettingRow::NativeDestruct()
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

void UALKSettingRow::UpdateGamepadHitTesting()
{
	// With a gamepad, CommonUI presses "accept" as a mouse click at the centre of the focused row. Make the row's
	// controls ignore that click so it always reaches the row (and becomes Accept) instead of, say, jumping a slider.
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bGamepad = InputSubsystem && InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad;
	if (RowBackground)
	{
		if (UWidget* Content = RowBackground->GetContent())
		{
			Content->SetVisibility(bGamepad ? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

bool UALKSettingRow::IsGamepadSimulatedClick() const
{
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	return InputSubsystem && InputSubsystem->GetIsGamepadSimulatedClick();
}

FReply UALKSettingRow::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	if (IsGamepadSimulatedClick())
	{
		HandleGamepadAccept();
		return FReply::Handled();
	}

	if (TSharedPtr<SWidget> CachedWidget = GetCachedWidget())
	{
		return FReply::Handled().SetUserFocus(CachedWidget.ToSharedRef(), EFocusCause::Mouse);
	}
	return FReply::Handled();
}

FReply UALKSettingRow::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Setting && Setting->IsEditable())
	{
		if (!InKeyEvent.IsRepeat() && NativeOnRowKey(InKeyEvent.GetKey()))
		{
			return FReply::Handled();
		}

		if (FSlateApplication::IsInitialized() && !InKeyEvent.IsRepeat()
			&& FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
		{
			NativeOnAccept();
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FNavigationReply UALKSettingRow::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	const EUINavigation Direction = InNavigationEvent.GetNavigationType();
	if ((Direction == EUINavigation::Left || Direction == EUINavigation::Right) && Setting && Setting->IsEditable())
	{
		if (AdjustValue(Direction == EUINavigation::Left ? -1 : 1))
		{
			return FNavigationReply::Stop();
		}
	}
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}

// ================================================================================================ Options row

void UALKSettingRow_Options::NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox)
{
	// Off / On settings show a switch (collapsed for every other setting), the rest the two arrows.
	UOverlay* ToggleArea = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("ToggleArea"));
	ToggleSwitch = FALKWidgetBuilder::Make<UALKSwitch>(Tree, TEXT("ToggleSwitch"));
	FALKWidgetBuilder::AddToOverlay(ToggleArea, ToggleSwitch, HAlign_Left, VAlign_Center);
	ToggleButton = CreateRowButton(Tree, FText::GetEmpty(), EALKButtonStyle::Arrow);
	FALKWidgetBuilder::AddToOverlay(ToggleArea, ToggleButton);
	ToggleArea->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, ToggleArea, FMargin(6.f, 0.f, 6.f, 0.f), HAlign_Left, VAlign_Center);

	LeftButton = CreateRowButton(Tree, FText::FromString(TEXT("<")), EALKButtonStyle::Arrow);
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, LeftButton);

	ValueText = FALKWidgetBuilder::MakeText(Tree, TEXT("ValueText"));
	ValueText->SetJustification(ETextJustify::Center);
	// A value wider than the space between the arrows (a long option, a long translation) shrinks to fit instead of
	// running under them.
	UScaleBox* ValueFit = FALKWidgetBuilder::Make<UScaleBox>(Tree, TEXT("ValueFit"));
	ValueFit->SetStretch(EStretch::ScaleToFit);
	ValueFit->SetStretchDirection(EStretchDirection::DownOnly);
	ValueFit->AddChild(ValueText);
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, ValueFit, FMargin(8.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);

	RightButton = CreateRowButton(Tree, FText::FromString(TEXT(">")), EALKButtonStyle::Arrow);
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, RightButton);
}

void UALKSettingRow_Options::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (LeftButton)
	{
		LeftButton->OnClicked().AddWeakLambda(this, [this]() { AdjustValue(-1); SetFocus(); });
	}
	if (RightButton)
	{
		RightButton->OnClicked().AddWeakLambda(this, [this]() { AdjustValue(1); SetFocus(); });
	}
	if (ToggleButton)
	{
		ToggleButton->OnClicked().AddWeakLambda(this, [this]() { NativeOnAccept(); SetFocus(); });
	}
}

void UALKSettingRow_Options::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	FALKWidgetBuilder::StyleText(ValueText, Theme, EALKTextRole::Body, Theme.TextSecondary);
	if (ValueText)
	{
		StyleRowText(ValueText, Theme);
	}
	NativeRefresh();
}

void UALKSettingRow_Options::NativeRefresh()
{
	const UALKSetting_Options* Options = Cast<UALKSetting_Options>(Setting);
	if (!Options)
	{
		return;
	}

	const bool bEditable = Options->IsEditable();
	const int32 Index = Options->GetIndex();
	const int32 Count = Options->GetSelectableCount();
	const bool bToggle = Options->bIsToggle && ToggleSwitch;

	if (ValueText)
	{
		ValueText->SetText(Options->GetValueText());
		// Beside a switch the value reads from the left ("On"); between the arrows it is centred.
		ValueText->SetJustification(bToggle ? ETextJustify::Left : ETextJustify::Center);
		if (UHorizontalBoxSlot* FitSlot = ValueText->GetParent() ? Cast<UHorizontalBoxSlot>(ValueText->GetParent()->Slot) : nullptr)
		{
			FitSlot->SetHorizontalAlignment(bToggle ? HAlign_Left : HAlign_Fill);
		}
	}
	if (UWidget* ToggleArea = ToggleSwitch ? ToggleSwitch->GetParent() : nullptr)
	{
		ToggleArea->SetVisibility(bToggle ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ToggleSwitch)
	{
		ToggleSwitch->SetOn(Index == 1);
	}
	if (ToggleButton)
	{
		ToggleButton->SetIsEnabled(bEditable);
	}
	if (LeftButton)
	{
		LeftButton->SetIsEnabled(bEditable && (Options->bWrap || Index > 0));
	}
	if (RightButton)
	{
		RightButton->SetIsEnabled(bEditable && (Options->bWrap || Index < Count - 1));
	}

	RebuildPips();
	if (const UALKTheme* Theme = GetTheme())
	{
		NativeApplyHighlight(*Theme, GetHighlight(), bEditable);
	}
}

void UALKSettingRow_Options::NativeApplyHighlight(const UALKTheme& Theme, float InHighlight, bool bEditable)
{
	if (ValueText)
	{
		const FLinearColor Color = bEditable ? FMath::Lerp(Theme.TextSecondary, Theme.GetFocusTextColor(), InHighlight) : Theme.TextMuted;
		ValueText->SetColorAndOpacity(FSlateColor(Color));
	}
	// The arrows stay faintly visible and light up with focus (the mouse focuses a row it moves over). Off / On settings
	// show the switch instead.
	const UALKSetting_Options* Options = Cast<UALKSetting_Options>(Setting);
	const bool bToggle = Options && Options->bIsToggle && ToggleSwitch;
	for (UALKButton* Arrow : { LeftButton.Get(), RightButton.Get() })
	{
		if (Arrow)
		{
			Arrow->SetRenderOpacity(0.3f + 0.7f * InHighlight);
			Arrow->SetVisibility(bToggle ? ESlateVisibility::Collapsed : (bEditable ? ESlateVisibility::Visible : ESlateVisibility::Hidden));
		}
	}
	if (ToggleSwitch)
	{
		// On a row filled with the accent colour the switch is drawn in the contrast colour.
		const bool bSolidFocus = Theme.FocusStyle == EALKFocusStyle::Solid;
		const FLinearColor On = FMath::Lerp(Theme.Accent, bSolidFocus ? Theme.AccentContrast : Theme.Accent, InHighlight);
		const FLinearColor Off = FMath::Lerp(Theme.SliderTrack, bSolidFocus ? Theme.AccentContrast.CopyWithNewOpacity(0.35f) : Theme.SliderTrack, InHighlight);
		const FLinearColor KnobOn = FMath::Lerp(Theme.AccentContrast, bSolidFocus ? Theme.Accent : Theme.AccentContrast, InHighlight);
		ToggleSwitch->CornerRadius = Theme.CornerRadius < 6.f ? Theme.CornerRadius : -1.f;
		ToggleSwitch->SetColors(On, Off, KnobOn, FMath::Lerp(Theme.TextMuted, bSolidFocus ? Theme.Accent : Theme.TextMuted, InHighlight));
		ToggleSwitch->SetRenderOpacity(bEditable ? 1.f : 0.4f);
	}
}

void UALKSettingRow_Options::RebuildPips()
{
	const UALKSetting_Options* Options = Cast<UALKSetting_Options>(Setting);
	if (!PipBox || !Options || !WidgetTree)
	{
		return;
	}

	const int32 Count = Options->GetSelectableCount();
	const int32 WantedPips = (Count >= 2 && Count <= 10) ? Count : 0;
	if (Pips.Num() != WantedPips)
	{
		PipBox->ClearChildren();
		Pips.Reset();
		for (int32 Index = 0; Index < WantedPips; ++Index)
		{
			UBorder* Pip = FALKWidgetBuilder::Make<UBorder>(*WidgetTree);
			USizeBox* PipSize = FALKWidgetBuilder::WrapInSizeBox(*WidgetTree, Pip, 14.f, 3.f);
			FALKWidgetBuilder::AddToHorizontalBox(PipBox, PipSize, FMargin(Index == 0 ? 0.f : 4.f, 0.f, 0.f, 0.f));
			Pips.Add(Pip);
		}
	}

	const UALKTheme* Theme = GetTheme();
	const int32 Current = Options->GetIndex();
	const bool bEditable = Options->IsEditable();
	for (int32 Index = 0; Index < Pips.Num(); ++Index)
	{
		const FLinearColor Color = (Index == Current && bEditable) ? Theme->Accent : Theme->SliderTrack;
		Pips[Index]->SetBrush(UALKTheme::MakeSolidBrush(Color));
	}
}

bool UALKSettingRow_Options::NativeOnAdjust(int32 Direction)
{
	if (UALKSetting_Options* Options = Cast<UALKSetting_Options>(Setting))
	{
		Options->Cycle(Direction);
		return true;
	}
	return false;
}

void UALKSettingRow_Options::NativeOnAccept()
{
	if (UALKSetting_Options* Options = Cast<UALKSetting_Options>(Setting))
	{
		const int32 Count = Options->GetSelectableCount();
		if (Count > 0)
		{
			const FText Before = Options->GetValueText();
			Options->SetIndex((FMath::Min(Options->GetIndex(), Count - 1) + 1) % Count);
			PlayChangeSoundIfChanged(Before);
		}
	}
}

// ================================================================================================ Slider row

void UALKSettingRow_Slider::NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox)
{
	// Segments | value. The slider on top of the segments takes the mouse; the progress bar stays for designer layouts.
	UOverlay* Track = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("SliderTrack"));
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, Track, FMargin(0.f, 0.f, 14.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);

	SliderFill = FALKWidgetBuilder::Make<UProgressBar>(Tree, TEXT("SliderFill"));
	USizeBox* FillSize = FALKWidgetBuilder::WrapInSizeBox(Tree, SliderFill, 0.f, 2.f);
	FALKWidgetBuilder::AddToOverlay(Track, FillSize, HAlign_Fill, VAlign_Center, FMargin(2.f, 0.f));

	ValueBar = FALKWidgetBuilder::Make<UALKValueBar>(Tree, TEXT("ValueBar"));
	ValueBar->BarSize = FVector2D(200.0, 20.0);
	ValueBar->bShowKnob = true;
	FALKWidgetBuilder::AddToOverlay(Track, ValueBar, HAlign_Fill, VAlign_Center);

	ValueText = FALKWidgetBuilder::MakeText(Tree, TEXT("ValueText"));
	ValueText->SetJustification(ETextJustify::Right);
	USizeBox* ValueSize = FALKWidgetBuilder::WrapInSizeBox(Tree, ValueText, 70.f, 0.f);
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, ValueSize, FMargin(0.f), HAlign_Right, VAlign_Center);

	Slider = FALKWidgetBuilder::Make<USlider>(Tree, TEXT("Slider"));
	Slider->IsFocusable = false;
	Slider->SetMinValue(0.f);
	Slider->SetMaxValue(1.f);
	Slider->SetStepSize(0.01f);
	FALKWidgetBuilder::AddToOverlay(Track, Slider, HAlign_Fill, VAlign_Center);
}

void UALKSettingRow_Slider::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (Slider)
	{
		Slider->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderValueChanged);
	}
}

void UALKSettingRow_Slider::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	FALKWidgetBuilder::StyleText(ValueText, Theme, EALKTextRole::Data, Theme.TextSecondary);
	// White brushes, tinted per highlight in NativeApplyHighlight.
	if (SliderFill)
	{
		FProgressBarStyle BarStyle;
		BarStyle.BackgroundImage = UALKTheme::MakeSolidBrush(Theme.SliderTrack);
		BarStyle.FillImage = UALKTheme::MakeSolidBrush(FLinearColor::White);
		BarStyle.MarqueeImage = FSlateNoResource();
		SliderFill->SetWidgetStyle(BarStyle);
	}

	if (Slider)
	{
		// The value bar shows the value and its knob: the slider only takes the mouse.
		FSlateBrush Notch = ValueBar ? FSlateBrush(FSlateNoResource()) : UALKTheme::MakeSolidBrush(FLinearColor::White);
		Notch.ImageSize = FVector2D(4.0, 16.0);
		FSliderStyle Style;
		Style.NormalBarImage = FSlateNoResource();
		Style.HoveredBarImage = FSlateNoResource();
		Style.DisabledBarImage = FSlateNoResource();
		Style.NormalThumbImage = Notch;
		Style.HoveredThumbImage = Notch;
		Notch.TintColor = FSlateColor(Theme.TextMuted);
		Style.DisabledThumbImage = Notch;
		Style.BarThickness = 2.f;
		Slider->SetWidgetStyle(Style);
		Slider->SetSliderBarColor(FLinearColor::Transparent);
	}

	NativeRefresh();
}

void UALKSettingRow_Slider::NativeRefresh()
{
	const UALKSetting_Slider* SliderSetting = Cast<UALKSetting_Slider>(Setting);
	if (!SliderSetting)
	{
		return;
	}

	const float Normalized = SliderSetting->GetNormalizedValue();
	const bool bEditable = SliderSetting->IsEditable();

	TGuardValue<bool> Guard(bUpdatingSlider, true);
	if (Slider)
	{
		Slider->SetValue(Normalized);
		Slider->SetLocked(!bEditable);
	}
	if (SliderFill)
	{
		SliderFill->SetPercent(Normalized);
		SliderFill->SetRenderOpacity(ValueBar ? 0.f : (bEditable ? 1.f : 0.4f));
	}
	if (ValueBar)
	{
		ValueBar->SetValue(Normalized);
		ValueBar->SetRenderOpacity(bEditable ? 1.f : 0.4f);
	}
	if (ValueText)
	{
		ValueText->SetText(SliderSetting->GetValueText());
	}
	if (const UALKTheme* Theme = GetTheme())
	{
		NativeApplyHighlight(*Theme, GetHighlight(), bEditable);
	}
}

void UALKSettingRow_Slider::NativeApplyHighlight(const UALKTheme& Theme, float InHighlight, bool bEditable)
{
	const FLinearColor Active = bEditable ? FMath::Lerp(Theme.TextPrimary, Theme.Accent, InHighlight) : Theme.TextMuted;
	if (ValueText)
	{
		ValueText->SetColorAndOpacity(FSlateColor(bEditable ? FMath::Lerp(Theme.TextSecondary, Theme.GetFocusTextColor(), InHighlight) : Theme.TextMuted));
	}
	if (ValueBar)
	{
		const bool bSolidFocus = Theme.FocusStyle == EALKFocusStyle::Solid;
		// On a row filled with the accent colour the bar is drawn in the contrast colour instead.
		ValueBar->SetColors(FMath::Lerp(Theme.Accent, bSolidFocus ? Theme.AccentContrast : Theme.Accent, InHighlight),
			FMath::Lerp(Theme.SliderTrack, bSolidFocus ? Theme.AccentContrast.CopyWithNewOpacity(0.3f) : Theme.SliderTrack, InHighlight));
		ValueBar->SetKnobColor(FMath::Lerp(Theme.TextPrimary, bSolidFocus ? Theme.AccentContrast : Theme.TextPrimary, InHighlight));
	}
	if (SliderFill)
	{
		SliderFill->SetFillColorAndOpacity(Active);
	}
	if (Slider)
	{
		Slider->SetSliderHandleColor(Active);
	}
}

void UALKSettingRow_Slider::HandleSliderValueChanged(float Value)
{
	if (bUpdatingSlider)
	{
		return;
	}
	if (UALKSetting_Slider* SliderSetting = Cast<UALKSetting_Slider>(Setting))
	{
		const FText Before = SliderSetting->GetValueText();
		SliderSetting->SetNormalizedValue(Value);
		PlayChangeSoundIfChanged(Before);
		// Keep the fill in sync while dragging even if the snapped value did not change.
		if (SliderFill)
		{
			SliderFill->SetPercent(SliderSetting->GetNormalizedValue());
		}
		if (ValueBar)
		{
			ValueBar->SetValue(SliderSetting->GetNormalizedValue());
		}
	}
	if (!HasAnyUserFocus())
	{
		SetFocus();
	}
}

bool UALKSettingRow_Slider::NativeOnAdjust(int32 Direction)
{
	if (UALKSetting_Slider* SliderSetting = Cast<UALKSetting_Slider>(Setting))
	{
		SliderSetting->StepValue(Direction);
		return true;
	}
	return false;
}

// ================================================================================================ Key binding row

void UALKSettingRow_KeyBinding::NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox)
{
	PrimaryKeyButton = CreateRowButton(Tree, FText::GetEmpty(), EALKButtonStyle::Secondary);
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, PrimaryKeyButton, FMargin(0.f, 0.f, 8.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);

	SecondaryKeyButton = CreateRowButton(Tree, FText::GetEmpty(), EALKButtonStyle::Secondary);
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, SecondaryKeyButton, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
}

void UALKSettingRow_KeyBinding::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (PrimaryKeyButton)
	{
		PrimaryKeyButton->OnClicked().AddWeakLambda(this, [this]() { SelectedSlot = 0; SetFocus(); BeginCapture(0); });
	}
	if (SecondaryKeyButton)
	{
		SecondaryKeyButton->OnClicked().AddWeakLambda(this, [this]() { SelectedSlot = 1; SetFocus(); BeginCapture(1); });
	}
}

void UALKSettingRow_KeyBinding::NativeRefresh()
{
	const UALKSetting_KeyBinding* Binding = Cast<UALKSetting_KeyBinding>(Setting);
	if (!Binding)
	{
		return;
	}
	SelectedSlot = FMath::Clamp(SelectedSlot, 0, FMath::Max(0, Binding->GetNumSlots() - 1));
	if (PrimaryKeyButton)
	{
		PrimaryKeyButton->SetButtonText(Binding->GetKeyText(0));
	}
	if (SecondaryKeyButton)
	{
		SecondaryKeyButton->SetButtonText(Binding->GetKeyText(1));
		// Gamepad rows have a single slot; keep the space so both kinds of row line up.
		SecondaryKeyButton->SetVisibility(Binding->GetNumSlots() > 1 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	UpdateSlotSelection();
}

void UALKSettingRow_KeyBinding::NativeApplyHighlight(const UALKTheme& Theme, float InHighlight, bool bEditable)
{
	// Focus reaches the row before it reports focus, so the selected slot lights up with the row's highlight.
	UpdateSlotSelection();
}

void UALKSettingRow_KeyBinding::UpdateSlotSelection()
{
	const bool bRowFocused = IsRowHighlighted();
	if (PrimaryKeyButton)
	{
		PrimaryKeyButton->SetActiveVisual(bRowFocused && SelectedSlot == 0);
	}
	if (SecondaryKeyButton)
	{
		SecondaryKeyButton->SetActiveVisual(bRowFocused && SelectedSlot == 1);
	}
}

void UALKSettingRow_KeyBinding::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	UpdateSlotSelection();
}

void UALKSettingRow_KeyBinding::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	if (PrimaryKeyButton)
	{
		PrimaryKeyButton->SetActiveVisual(false);
	}
	if (SecondaryKeyButton)
	{
		SecondaryKeyButton->SetActiveVisual(false);
	}
}

bool UALKSettingRow_KeyBinding::NativeOnAdjust(int32 Direction)
{
	const UALKSetting_KeyBinding* Binding = Cast<UALKSetting_KeyBinding>(Setting);
	const int32 SlotCount = Binding ? Binding->GetNumSlots() : UALKSetting_KeyBinding::NumSlots;
	const int32 PreviousSlot = SelectedSlot;
	SelectedSlot = FMath::Clamp(SelectedSlot + Direction, 0, FMath::Max(0, SlotCount - 1));
	UpdateSlotSelection();
	if (SelectedSlot != PreviousSlot)
	{
		UALKTheme::PlayUISound(this, EALKUISound::Hover);
	}
	return true;
}

void UALKSettingRow_KeyBinding::NativeOnAccept()
{
	BeginCapture(SelectedSlot);
}

bool UALKSettingRow_KeyBinding::NativeOnRowKey(const FKey& Key)
{
	if (Key == EKeys::Delete || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Left)
	{
		if (UALKSetting_KeyBinding* Binding = Cast<UALKSetting_KeyBinding>(Setting))
		{
			const FText Before = Binding->GetKeyText(SelectedSlot);
			Binding->ClearKey(SelectedSlot);
			if (!Binding->GetKeyText(SelectedSlot).EqualTo(Before))
			{
				UALKTheme::PlayUISound(this, EALKUISound::Change);
			}
		}
		return true;
	}
	return false;
}

void UALKSettingRow_KeyBinding::BeginCapture(int32 SlotIndex)
{
	UALKSetting_KeyBinding* Binding = Cast<UALKSetting_KeyBinding>(Setting);
	UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	if (!Binding || !MenuSubsystem)
	{
		return;
	}

	UALKTheme::PlayUISound(this, EALKUISound::Click);
	TWeakObjectPtr<UALKSetting_KeyBinding> WeakBinding(Binding);
	MenuSubsystem->ShowKeyCaptureDialog(Binding->DisplayName, Binding->bGamepad, [WeakBinding, SlotIndex](FKey Key)
	{
		if (WeakBinding.IsValid() && Key.IsValid())
		{
			WeakBinding->SetKey(SlotIndex, Key);
		}
	});
}

// ================================================================================================ Action row

void UALKSettingRow_Action::NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox)
{
	ActionButton = CreateRowButton(Tree, FText::GetEmpty(), EALKButtonStyle::Secondary);
	FALKWidgetBuilder::AddToHorizontalBox(&ValueBox, ActionButton, FMargin(), HAlign_Right, VAlign_Center, 1.f);
}

void UALKSettingRow_Action::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (ActionButton)
	{
		ActionButton->OnClicked().AddWeakLambda(this, [this]() { SetFocus(); NativeOnAccept(); });
	}
}

void UALKSettingRow_Action::NativeRefresh()
{
	if (const UALKSetting_Action* Action = Cast<UALKSetting_Action>(Setting))
	{
		if (ActionButton)
		{
			ActionButton->SetButtonText(Action->ActionText);
			ActionButton->SetIsEnabled(Action->IsEditable());
		}
	}
}

void UALKSettingRow_Action::NativeOnAccept()
{
	UALKSetting_Action* Action = Cast<UALKSetting_Action>(Setting);
	if (!Action)
	{
		return;
	}
	UALKTheme::PlayUISound(this, EALKUISound::Click);

	if (!Action->bRequiresConfirmation)
	{
		Action->Execute();
		return;
	}

	if (UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem())
	{
		FALKDialogDescriptor Descriptor;
		Descriptor.Title = Action->DisplayName;
		Descriptor.Message = Action->ConfirmMessage;
		Descriptor.ConfirmText = Action->ActionText;
		Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");

		TWeakObjectPtr<UALKSetting_Action> WeakAction(Action);
		MenuSubsystem->ShowConfirmDialog(Descriptor, [WeakAction](EALKDialogResult Result)
		{
			if (Result == EALKDialogResult::Confirmed && WeakAction.IsValid())
			{
				WeakAction->Execute();
			}
		});
	}
}

#undef LOCTEXT_NAMESPACE
