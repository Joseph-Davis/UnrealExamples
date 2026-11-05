// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKScreen.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKPaint.h"
#include "Widgets/ALKTitleText.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKPointerTracker.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Settings/ALKSettingRows.h"
#include "Slate/SObjectWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformInput.h"
#include "Input/UIActionBindingHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKScreen)

UALKScreen::UALKScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	bIsBackHandler = false;
	// Screens remember their own focus (GetLastFocusedWidget): CommonUI's restore target is only updated when the
	// input method changes away from gamepad, so it can send focus back to an old button.
	bAutoRestoreFocus = false;
}

void UALKScreen::NativeOnInitialized()
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

void UALKScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// A pooled screen (dialogs are reused) starts fresh.
	LastFocusedWidget.Reset();
	bOpenSoundPlayed = false;

	HandleThemeChanged();
	if (!ThemeChangedHandle.IsValid())
	{
		ThemeChangedHandle = UALKTheme::OnAnyThemeChanged().AddUObject(this, &ThisClass::HandleThemeChanged);
	}

	if (!InputMethodChangedHandle.IsValid())
	{
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
		{
			InputMethodChangedHandle = InputSubsystem->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
		}
	}
	NativeOnInputMethodChanged(IsUsingGamepad());
}

void UALKScreen::NativeDestruct()
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

	Super::NativeDestruct();
}

void UALKScreen::HandleThemeChanged()
{
	if (bApplyThemeStyles)
	{
		ApplyTheme(GetTheme());
	}
}

void UALKScreen::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	NativeOnInputMethodChanged(NewInputType == ECommonInputType::Gamepad);
}

void UALKScreen::ApplyTheme_Implementation(const UALKTheme* Theme)
{
	if (Theme)
	{
		NativeApplyTheme(*Theme);
	}
}

const UALKTheme* UALKScreen::GetTheme() const
{
	return UALKDeveloperSettings::GetTheme();
}

const UALKMenuConfig* UALKScreen::GetMenuConfig() const
{
	return UALKDeveloperSettings::GetMenuConfig();
}

UALKMenuSubsystem* UALKScreen::GetMenuSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UALKMenuSubsystem>() : nullptr;
}

void UALKScreen::CloseScreen()
{
	DeactivateWidget();
}

bool UALKScreen::OnBackRequested_Implementation()
{
	if (bCloseOnBack)
	{
		PlayThemeSound(EALKUISound::Back);
		CloseScreen();
		return true;
	}
	return false;
}

UALKButton* UALKScreen::CreateMenuButton(const FText& Text, EALKButtonStyle Style)
{
	TSubclassOf<UALKButton> ButtonClass = GetMenuConfig()->ButtonClass;
	if (!ButtonClass)
	{
		ButtonClass = UALKButton::StaticClass();
	}

	UALKButton* Button = WidgetTree ? WidgetTree->ConstructWidget<UALKButton>(ButtonClass) : CreateWidget<UALKButton>(this, ButtonClass);
	if (Button)
	{
		Button->SetButtonStyleType(Style);
		Button->SetButtonText(Text);
	}
	return Button;
}

bool UALKScreen::IsInActivationGracePeriod()
{
	// Grace time counts at most a tenth of a second per frame: after a hitch right as a screen opens (a level load,
	// shader compiles), the mouse move from CommonUI re-centring the cursor is only handled once the hitch is over.
	return FALKPointerTracker::GetGraceTime() < 0.35f;
}

void UALKScreen::StartActivationGracePeriod()
{
	FALKPointerTracker::RestartGraceTime();
}

bool UALKScreen::IsGamepadSimulatedClick() const
{
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	return InputSubsystem && InputSubsystem->GetIsGamepadSimulatedClick();
}

bool UALKScreen::IsUsingMouseAndKeyboard() const
{
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	return !InputSubsystem || InputSubsystem->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
}

void UALKScreen::NativeOnActivated()
{
	if (bStyledOverGame != IsShownOverGame())
	{
		HandleThemeChanged();
	}
	StartActivationGracePeriod();
	Super::NativeOnActivated();

	const UALKTheme* Theme = GetTheme();
	if (bAnimateIn && Theme->ScreenFadeInTime > 0.f)
	{
		AnimInElapsed = 0.f;
		SetRenderOpacity(0.f);
		SetRenderTranslation(FVector2D(0.0, Theme->ScreenSlideOffset));
	}
	if (bAnimateIn)
	{
		PlayIntroEffects(!bOpenSoundPlayed);
	}
	if (!bOpenSoundPlayed)
	{
		bOpenSoundPlayed = true;
		PlayThemeSound(EALKUISound::ScreenOpen);
	}
}

void UALKScreen::NativeOnDeactivated()
{
	AnimInElapsed = -1.f;
	SetRenderOpacity(1.f);
	SetRenderTranslation(FVector2D::ZeroVector);
	Super::NativeOnDeactivated();
}

void UALKScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (AnimInElapsed >= 0.f)
	{
		const UALKTheme* Theme = GetTheme();
		const float Duration = FMath::Max(0.01f, Theme->ScreenFadeInTime);
		// Menus are usually open while the game is paused, so use real time rather than world time.
		AnimInElapsed += FMath::Min(InDeltaTime, 0.05f);
		const float Alpha = FMath::Clamp(AnimInElapsed / Duration, 0.f, 1.f);
		const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f);
		SetRenderOpacity(Eased);
		// The screen rises into place while it fades in; with Reduce Effects on it only fades.
		SetRenderTranslation(FVector2D(0.0, FALKEffects::AreReduced() ? 0.0 : (1.f - Eased) * Theme->ScreenSlideOffset));
		if (Alpha >= 1.f)
		{
			AnimInElapsed = -1.f;
		}
	}
}

void UALKScreen::CapContentWidth(const FGeometry& ScreenGeometry, float MaxWidth)
{
	const UWidget* Main = WidgetTree ? WidgetTree->FindWidget(TEXT("MainColumn")) : nullptr;
	UOverlaySlot* MainSlot = Main ? Cast<UOverlaySlot>(Main->Slot) : nullptr;
	const UALKTheme* Theme = GetTheme();
	const float ScreenWidth = ScreenGeometry.GetLocalSize().X;
	if (!MainSlot || !Theme || ScreenWidth <= 1.f)
	{
		return;
	}
	const float Extra = FMath::RoundToFloat(FMath::Max(0.f, (ScreenWidth - MaxWidth) * 0.5f));
	const FMargin Wanted(Theme->ScreenPadding.Left + Extra, Theme->ScreenPadding.Top, Theme->ScreenPadding.Right + Extra, Theme->ScreenPadding.Bottom);
	const FMargin Current = MainSlot->GetPadding();
	if (!FMath::IsNearlyEqual(Current.Left, Wanted.Left, 0.5) || !FMath::IsNearlyEqual(Current.Right, Wanted.Right, 0.5)
		|| !FMath::IsNearlyEqual(Current.Top, Wanted.Top, 0.5) || !FMath::IsNearlyEqual(Current.Bottom, Wanted.Bottom, 0.5))
	{
		MainSlot->SetPadding(Wanted);
	}
}

void UALKScreen::PlayIntroEffects(bool bFirstTime)
{
	// The whole screen already fades and rises in (NativeTick); screens with something more to show override this.
}

FText UALKScreen::MakeSystemLabel(int32 Code, const FText& Name) const
{
	// Screens have no label over their title unless you give them one.
	return SystemLabel;
}

TOptional<FUIInputConfig> UALKScreen::GetDesiredInputConfig() const
{
	TOptional<FUIInputConfig> BlueprintConfig = Super::GetDesiredInputConfig();
	if (BlueprintConfig.IsSet())
	{
		return BlueprintConfig;
	}
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UALKScreen::IsShownOverGame() const
{
	const UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	return bAlwaysOverGame || (MenuSubsystem && MenuSubsystem->IsPauseMenuOpen());
}

void UALKScreen::StyleScreenBackground(const UALKTheme& Theme, float Opacity)
{
	if (WidgetTree)
	{
		bStyledOverGame = IsShownOverGame();
		FALKWidgetBuilder::StyleBackgroundLayers(*WidgetTree, Theme, this, BackgroundTexture, OverlayTextures, Opacity, TEXT("BackgroundImage"), bStyledOverGame, bDimScene, bShowKeyArt);
	}
}

UWidget* UALKScreen::GetLastFocusedWidget() const
{
	UWidget* Widget = LastFocusedWidget.Get();
	// Rebuilt lists remove their old rows from the screen; those are no longer targets.
	return Widget && Widget != this && Widget->GetParent() && Widget->GetIsEnabled() && Widget->IsVisible() ? Widget : nullptr;
}

void UALKScreen::NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	// Remember the innermost user widget (button, row, chapter still) that takes focus inside this screen.
	const TSharedPtr<SWidget> Self = GetCachedWidget();
	UUserWidget* Innermost = nullptr;
	for (int32 Index = NewWidgetPath.Widgets.Num() - 1; Index >= 0; --Index)
	{
		const TSharedRef<SWidget>& Widget = NewWidgetPath.Widgets[Index].Widget;
		if (Self.IsValid() && &Widget.Get() == Self.Get())
		{
			if (Innermost)
			{
				LastFocusedWidget = Innermost;
			}
			return;
		}
		if (!Innermost && Widget->GetType() == FName(TEXT("SObjectWidget")))
		{
			Innermost = StaticCastSharedRef<SObjectWidget>(Widget)->GetWidgetObject();
		}
	}
}

UWidget* UALKScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	if (UWidget* BlueprintTarget = Super::NativeGetDesiredFocusTarget())
	{
		return BlueprintTarget;
	}
	// Screens without a focusable child (credits, title screen, key prompt) take focus themselves. Letting CommonUI do it,
	// rather than calling SetFocus on activation, lets it remember what was focused underneath and restore it on close.
	return DefaultFocusWidget ? DefaultFocusWidget.Get() : const_cast<UALKScreen*>(this);
}

bool UALKScreen::NativeOnHandleBackAction()
{
	// Only reached when a CommonUI back action is configured in the project; key presses are handled in NativeOnKeyDown.
	return OnBackRequested();
}

bool UALKScreen::IsBackKey(const FKey& Key)
{
	return Key == EKeys::Escape
		|| Key == EKeys::Gamepad_FaceButton_Right
		|| Key == EKeys::Android_Back
		|| Key == FPlatformInput::GetGamepadBackKey();
}

FReply UALKScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (!InKeyEvent.IsRepeat())
	{
		if (NativeHandleMenuKey(Key))
		{
			return FReply::Handled();
		}

		if (IsBackKey(Key) && OnBackRequested())
		{
			return FReply::Handled();
		}
	}

	// The screen itself has focus (e.g. after clicking empty space): send navigation back into the menu.
	if (HasUserFocus(GetOwningPlayer()) && FSlateApplication::IsInitialized())
	{
		const EUINavigation Direction = FSlateApplication::Get().GetNavigationDirectionFromKey(InKeyEvent);
		if (Direction != EUINavigation::Invalid)
		{
			if (UWidget* Target = GetDesiredFocusTarget())
			{
				Target->SetFocus();
				return FReply::Handled();
			}
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FNavigationReply UALKScreen::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	const FNavigationReply Reply = Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
	// A screen keeps the focus: arrow keys or the D-pad past its last control stop there. Left to escape, Slate would move
	// the focus to a screen underneath that stays focusable: the screen behind a dialog, or the pause menu (stepped out,
	// so nobody sees where the focus went) behind the settings or the host's panel opened from it.
	if (Reply.GetBoundaryRule() == EUINavigationRule::Escape)
	{
		return FNavigationReply::Stop();
	}
	return Reply;
}

FReply UALKScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

	// CommonUI turns gamepad accept into a click at the centre of the focused widget. If that click missed (the widget
	// was not laid out yet when the cursor was placed), apply it to the focused widget directly and keep focus there.
	if (IsGamepadSimulatedClick() && FSlateApplication::IsInitialized())
	{
		if (TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(InMouseEvent.GetUserIndex()))
		{
			for (TSharedPtr<SWidget> Current = Focused; Current.IsValid(); Current = Current->GetParentWidget())
			{
				if (Current->GetType() != FName(TEXT("SObjectWidget")))
				{
					continue;
				}
				UUserWidget* Owner = StaticCastSharedPtr<SObjectWidget>(Current)->GetWidgetObject();
				if (Owner == this)
				{
					break;
				}
				if (UALKSettingRow* Row = Cast<UALKSettingRow>(Owner))
				{
					Row->HandleGamepadAccept();
					break;
				}
				if (UALKButton* Button = Cast<UALKButton>(Owner))
				{
					Button->SimulateClick();
					break;
				}
			}
			return FReply::Handled().SetUserFocus(Focused.ToSharedRef(), EFocusCause::SetDirectly);
		}
	}

	// Swallow clicks on empty space so they never reach the game viewport behind the menu.
	return FReply::Handled();
}

FReply UALKScreen::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	FReply Reply = Super::NativeOnFocusReceived(InGeometry, InFocusEvent);

	// Focus landed on the screen itself (not a child) from code or gamepad: forward it to the desired target.
	if (InFocusEvent.GetCause() != EFocusCause::Mouse)
	{
		if (UWidget* Target = GetDesiredFocusTarget())
		{
			if (Target != this)
			{
				if (TSharedPtr<SWidget> TargetWidget = Target->GetCachedWidget())
				{
					return FReply::Handled().SetUserFocus(TargetWidget.ToSharedRef(), InFocusEvent.GetCause());
				}
			}
		}
	}
	return Reply;
}

void UALKScreen::PlayThemeSound(EALKUISound Sound) const
{
	UALKTheme::PlayUISound(this, Sound);
}

bool UALKScreen::IsUsingGamepad() const
{
	if (const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		return InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad;
	}
	return false;
}

UHorizontalBox* UALKScreen::BuildInputHints(UWidgetTree& Tree, const TArray<FALKInputHint>& Hints, TArray<UALKButton*>* OutButtons)
{
	// Appends, so a screen can build several hint groups (e.g. tab caps and a footer).
	UHorizontalBox* Row = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);
	for (int32 Index = 0; Index < Hints.Num(); ++Index)
	{
		const FALKInputHint& Hint = Hints[Index];
		UALKButton* HintButton = CreateMenuButton(Hint.Label, EALKButtonStyle::Hint);
		if (!HintButton)
		{
			continue;
		}
		// Hints never take focus: gamepad and keyboard use the keys they show, the mouse clicks them.
		HintButton->SetIsFocusable(false);
		HintButton->bFocusOnHover = false;
		// A hint plays what its action plays (Back, Tab...), the same as pressing its key.
		HintButton->bPlayClickSound = false;
		HintButton->SetInputHint(Hint);
		if (Hint.Action.IsBound())
		{
			HintButton->OnClicked().AddLambda([Action = Hint.Action]()
			{
				Action.ExecuteIfBound();
			});
		}
		else
		{
			HintButton->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		// A key without a label belongs to the hint after it ("[Q] [E] Switch tab"): the two sit close together and, as
		// buttons, share one look.
		const bool bLeadsPair = Hint.Label.IsEmpty() && Hints.IsValidIndex(Index + 1);
		const bool bEndsPair = Index > 0 && Hints[Index - 1].Label.IsEmpty();
		HintButton->bBoxKeyOnlyHint = bLeadsPair;
		FALKWidgetBuilder::AddToHorizontalBox(Row, HintButton, FMargin(Index == 0 ? 0.f : (bEndsPair ? 6.f : 20.f), 0.f, 0.f, 0.f));
		HintButtons.Add(HintButton);
		if (OutButtons)
		{
			OutButtons->Add(HintButton);
		}
	}

	Row->SetVisibility(GetMenuConfig()->bShowInputHints ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	return Row;
}
