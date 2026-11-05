// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKHostGameScreen.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKTextEntryDialog.h"
#include "Rules/ALKGameRules.h"
#include "Settings/ALKSettingRows.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKTextField.h"
#include "Widgets/ALKThumbnail.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformInput.h"
#include "Misc/EngineVersionComparison.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKHostGameScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKHostGameScreenPrivate
{
	/** The screen is one column of this width at most, centred. */
	static constexpr float ColumnWidth = 1680.f;
	static constexpr float CardWidth = 520.f;
	static constexpr float PictureHeight = 190.f;
	static constexpr float ConfirmHeight = 62.f;
	/** Longest server name and password the backend keeps. */
	static constexpr int32 ServerNameLength = 32;
	static constexpr int32 PasswordLength = 32;

	static const FName ServerNameRow(TEXT("ServerName"));
	static const FName ModeRow(TEXT("Mode"));
	static const FName MapRow(TEXT("Map"));
	static const FName MaxPlayersRow(TEXT("MaxPlayers"));
	static const FName LanRow(TEXT("LAN"));
	static const FName PrivateRow(TEXT("Private"));
	static const FName PasswordRow(TEXT("Password"));
	static const FName RandomizeRow(TEXT("RandomizeRules"));
	static const FName PresetRow(TEXT("Preset"));
	static const FName SavePresetRow(TEXT("SavePreset"));
	static const FName DeletePresetRow(TEXT("DeletePreset"));

	/** The font of names in setting rows: the button font, smaller (the same as the setting rows use). */
	static FSlateFontInfo GetRowFont(const UALKTheme& Theme)
	{
		FSlateFontInfo Font = Theme.ButtonFont;
		Font.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.ButtonFont.Size * 0.6f));
		Font.TypefaceFontName = FName(TEXT("SemiBold"));
		Font.LetterSpacing = FMath::RoundToInt(Theme.ButtonFont.LetterSpacing * 0.8f);
		return Font;
	}

	static TSubclassOf<UALKSettingRow> GetRowClass(EALKSettingType Type)
	{
		const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
		TSubclassOf<UALKSettingRow> RowClass;
		switch (Type)
		{
		case EALKSettingType::Slider: RowClass = Config->SliderRowClass; break;
		case EALKSettingType::Action: RowClass = Config->ActionRowClass; break;
		default: RowClass = Config->OptionRowClass; break;
		}
		if (RowClass)
		{
			return RowClass;
		}
		switch (Type)
		{
		case EALKSettingType::Slider: return UALKSettingRow_Slider::StaticClass();
		case EALKSettingType::Action: return UALKSettingRow_Action::StaticClass();
		default: return UALKSettingRow_Options::StaticClass();
		}
	}

	static bool IsFocusedWidget(const UWidget* Widget)
	{
		return Widget && (Widget->HasAnyUserFocus() || Widget->HasFocusedDescendants());
	}
}

UALKHostGameScreen::UALKHostGameScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

// ================================================================================================ Layout

void UALKHostGameScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKHostGameScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	USizeBox* ColumnSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainColumn"));
	ColumnSize->SetWidthOverride(ColumnWidth);
	FALKWidgetBuilder::AddToOverlay(Root, ColumnSize, HAlign_Center, VAlign_Fill);
	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainBox"));
	ColumnSize->AddChild(Main);

	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"));
	FALKWidgetBuilder::AddToVerticalBox(Main, SystemLabelText, FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Left);
	TitleText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleText, FMargin(), HAlign_Left);

	// Body: the settings on a panel | the match card with the buttons under it.
	UHorizontalBox* Body = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Body"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Body, FMargin(0.f, 28.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	UALKFrame* List = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ListPanel"));
	List->SetPadding(FMargin(16.f, 22.f, 18.f, 22.f));
	ListPanel = List;
	FALKWidgetBuilder::AddToHorizontalBox(Body, List, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);
	SettingsList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("SettingsList"));
	SettingsList->SetAnimateWheelScrolling(true);
	List->SetContent(SettingsList);

	ActionColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ActionColumn"));
	FALKWidgetBuilder::AddToHorizontalBox(Body, FALKWidgetBuilder::WrapInSizeBox(Tree, ActionColumn, CardWidth, 0.f), FMargin(32.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Fill);

	UALKFrame* Card = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("CardPanel"));
	Card->SetPadding(FMargin(14.f, 14.f, 14.f, 22.f));
	CardPanel = Card;
	FALKWidgetBuilder::AddToVerticalBox(ActionColumn, Card, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);

	// A long description is cut off at the card's edge instead of running over the buttons.
	UVerticalBox* CardColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("CardColumn"));
	CardColumn->SetClipping(EWidgetClipping::ClipToBounds);
	Card->SetContent(CardColumn);

	MapPicture = FALKWidgetBuilder::Make<UALKThumbnail>(Tree, TEXT("MapPicture"));
	MapPicture->bThemeVeil = true;
	FALKWidgetBuilder::AddToVerticalBox(CardColumn, FALKWidgetBuilder::WrapInSizeBox(Tree, MapPicture, 0.f, PictureHeight));

	UVerticalBox* CardTexts = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("CardTexts"));
	FALKWidgetBuilder::AddToVerticalBox(CardColumn, CardTexts, FMargin(10.f, 20.f, 10.f, 0.f));
	ServerText = FALKWidgetBuilder::MakeText(Tree, TEXT("ServerText"));
	ServerText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToVerticalBox(CardTexts, ServerText);
	UALKTitleText* Mode = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("ModeText"));
	Mode->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	ModeText = Mode;
	FALKWidgetBuilder::AddToVerticalBox(CardTexts, ModeText, FMargin(0.f, 4.f, 0.f, 0.f));
	MapText = FALKWidgetBuilder::MakeText(Tree, TEXT("MapText"));
	MapText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToVerticalBox(CardTexts, MapText, FMargin(0.f, 4.f, 0.f, 0.f));

	ChipBox = FALKWidgetBuilder::Make<UWrapBox>(Tree, TEXT("ChipBox"));
	ChipBox->SetInnerSlotPadding(FVector2D(8.0, 8.0));
	FALKWidgetBuilder::AddToVerticalBox(CardTexts, ChipBox, FMargin(0.f, 16.f, 0.f, 0.f));

	UALKRule* Line = FALKWidgetBuilder::Make<UALKRule>(Tree, TEXT("CardRule"));
	FALKWidgetBuilder::AddToVerticalBox(CardTexts, Line, FMargin(0.f, 20.f, 0.f, 16.f));

	DescriptionTitle = FALKWidgetBuilder::MakeText(Tree, TEXT("DescriptionTitle"));
	DescriptionTitle->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(CardTexts, DescriptionTitle);
	DescriptionText = FALKWidgetBuilder::MakeText(Tree, TEXT("DescriptionText"));
	DescriptionText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(CardTexts, DescriptionText, FMargin(0.f, 8.f, 0.f, 0.f));

	// The buttons: the main action large, then Reset and Back side by side.
	ConfirmButton = CreateMenuButton(LOCTEXT("HostCreate", "Create lobby"), EALKButtonStyle::Solid);
	ConfirmButton->SetLeadingIcon(EALKIcon::Play);
	FALKWidgetBuilder::AddToVerticalBox(ActionColumn, FALKWidgetBuilder::WrapInSizeBox(Tree, ConfirmButton, 0.f, ConfirmHeight), FMargin(0.f, 16.f, 0.f, 0.f));
	UHorizontalBox* ButtonRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ButtonRow"));
	FALKWidgetBuilder::AddToVerticalBox(ActionColumn, ButtonRow, FMargin(0.f, 12.f, 0.f, 0.f));
	ResetButton = CreateMenuButton(LOCTEXT("HostReset", "Reset to defaults"), EALKButtonStyle::Secondary);
	// A good half of the row, and wider when its label needs it (a long translation, in capitals on some skins): the
	// label never runs out of the button. Back takes what is left.
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, FALKWidgetBuilder::WrapInSizeBox(Tree, ResetButton, FMath::RoundToFloat(CardWidth * 0.56f), 0.f, true),
		FMargin(0.f, 0.f, 6.f, 0.f), HAlign_Fill, VAlign_Center);
	BackButton = CreateMenuButton(LOCTEXT("HintBack", "Back"), EALKButtonStyle::Ghost);
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, BackButton, FMargin(6.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);

	// Footer: [Enter] Select  [< >] Change  (gamepad: [Y] Create lobby  [X] Reset to defaults)  [Esc] Back
	UHorizontalBox* Footer = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Footer"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Footer, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Left);
	UHorizontalBox* MainHints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("HintSelect", "Select")),
		FALKInputHint(LOCTEXT("KeyBrowse", "< >"), LOCTEXT("PadBrowse", "D-Pad"), LOCTEXT("HintChange", "Change"))
	});
	FALKWidgetBuilder::AddToHorizontalBox(Footer, MainHints);
	TArray<UALKButton*> PadButtons;
	PadHints = BuildInputHints(Tree, {
		FALKInputHint(FText::GetEmpty(), EKeys::Gamepad_FaceButton_Top, LOCTEXT("HostCreate", "Create lobby"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ConfirmSettings)),
		FALKInputHint(FText::GetEmpty(), EKeys::Gamepad_FaceButton_Left, LOCTEXT("HostReset", "Reset to defaults"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ResetToDefaults))
	}, &PadButtons);
	PadHints->SetVisibility(ESlateVisibility::Collapsed);
	ConfirmHint = PadButtons.Num() > 0 ? PadButtons[0] : nullptr;
	FALKWidgetBuilder::AddToHorizontalBox(Footer, PadHints, FMargin(20.f, 0.f, 0.f, 0.f));
	UHorizontalBox* BackHints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBackClicked))
	});
	FALKWidgetBuilder::AddToHorizontalBox(Footer, BackHints, FMargin(20.f, 0.f, 0.f, 0.f));
}

void UALKHostGameScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &ThisClass::ConfirmSettings);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked().AddUObject(this, &ThisClass::ResetToDefaults);
	}
	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &ThisClass::HandleBackClicked);
	}

	// Rows keep Left and Right for their values, so the buttons are reached with Down past the last row, and Left or
	// Up from the buttons comes back to the row that had the focus.
	// UE 5.8: a scroll box that carries navigation rules culls its rows with a stale rectangle (SScrollPanel::OnPaint
	// uses the box's hit-test bounds), so everything past the first part of each row stops being drawn. The panel around
	// the list takes the rule instead; leaving the list downwards leaves the panel the same way.
	if (SettingsList)
	{
		FCustomWidgetNavigationDelegate ToActions;
		ToActions.BindDynamic(this, &ThisClass::HandleNavigateToActions);
#if UE_VERSION_OLDER_THAN(5, 8, 0)
		SettingsList->SetNavigationRuleCustomBoundary(EUINavigation::Down, ToActions);
#else
		if (ListPanel)
		{
			ListPanel->SetNavigationRuleCustomBoundary(EUINavigation::Down, ToActions);
		}
#endif
	}
	if (ActionColumn)
	{
		FCustomWidgetNavigationDelegate ToRows;
		ToRows.BindDynamic(this, &ThisClass::HandleNavigateToRows);
		ActionColumn->SetNavigationRuleCustomBoundary(EUINavigation::Left, ToRows);
		ActionColumn->SetNavigationRuleCustomBoundary(EUINavigation::Up, ToRows);
	}
#if !UE_VERSION_OLDER_THAN(5, 8, 0)
	// Down from Create lobby has always landed on Back (and Left from Back reaches Reset to defaults). From 5.8 the engine
	// picks the nearer Reset to defaults for the same layout, so the route of the older versions is spelled out.
	if (ConfirmButton && BackButton)
	{
		ConfirmButton->SetNavigationRuleExplicit(EUINavigation::Down, BackButton);
	}
#endif
	UpdateTitles();
}

void UALKHostGameScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (WidgetTree)
	{
		StyleScreenBackground(Theme);
		if (UWidget* Main = WidgetTree->FindWidget(TEXT("MainColumn")))
		{
			if (UOverlaySlot* MainSlot = Cast<UOverlaySlot>(Main->Slot))
			{
				MainSlot->SetPadding(Theme.ScreenPadding);
			}
		}
		if (UALKRule* Line = Cast<UALKRule>(WidgetTree->FindWidget(TEXT("CardRule"))))
		{
			Line->SetRuleColor(Theme.Separator);
		}
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading);
		FALKWidgetBuilder::SetTextFont(TitleText, Theme.HeadingFont);
	}
	FALKWidgetBuilder::StyleText(SystemLabelText, Theme, EALKTextRole::Data, Theme.GetAccentTextColor());
	if (SystemLabelText)
	{
		SystemLabelText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		SystemLabelText->SetVisibility(SystemLabelText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (UALKFrame* List = Cast<UALKFrame>(ListPanel))
	{
		List->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (UALKFrame* Card = Cast<UALKFrame>(CardPanel))
	{
		Card->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (MapPicture)
	{
		MapPicture->SetCornerRadius(FMath::Max(0.f, Theme.CornerRadius - 4.f));
	}
	FALKWidgetBuilder::StyleText(ServerText, Theme, EALKTextRole::Data, Theme.GetAccentTextColor());
	if (ServerText)
	{
		ServerText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	if (ModeText)
	{
		FALKWidgetBuilder::StyleText(ModeText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.66f));
		FALKWidgetBuilder::SetTextFont(ModeText, Font);
	}
	FALKWidgetBuilder::StyleText(MapText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(DescriptionTitle, Theme, EALKTextRole::Heading, Theme.TextPrimary);
	if (DescriptionTitle)
	{
		// A small heading, as on the settings pages.
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.56f));
		FALKWidgetBuilder::SetTextFont(DescriptionTitle, Font);
	}
	FALKWidgetBuilder::StyleText(DescriptionText, Theme, EALKTextRole::Body);
	if (DescriptionText)
	{
		FSlateFontInfo Font = Theme.BodyFont;
		Font.Size = FMath::Max(10.f, Font.Size - 2.f);
		FALKWidgetBuilder::SetTextFont(DescriptionText, Font);
		DescriptionText->SetLineHeightPercentage(1.25f);
	}

	if (SettingsList)
	{
		// A thin accent thumb and a faint track, beside the list.
		FScrollBarStyle BarStyle = SettingsList->GetWidgetBarStyle();
		const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
		BarStyle.SetNormalThumbImage(Thumb);
		BarStyle.SetHoveredThumbImage(Thumb);
		BarStyle.SetDraggedThumbImage(Thumb);
		BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
		BarStyle.SetVerticalTopSlotImage(FSlateNoResource());
		BarStyle.SetVerticalBottomSlotImage(FSlateNoResource());
		SettingsList->SetWidgetBarStyle(BarStyle);
		SettingsList->SetScrollbarThickness(FVector2D(3.0, 3.0));
		SettingsList->SetScrollbarPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}

	for (UTextBlock* Header : SectionHeaders)
	{
		StyleSectionHeader(Header, Theme);
	}
	for (int32 Index = 0; Index < TextRows.Num(); ++Index)
	{
		StyleTextRow(TextRows[Index], Theme, Index == TypingRow);
	}
	UpdateCard();
}

void UALKHostGameScreen::StyleSectionHeader(UTextBlock* Header, const UALKTheme& Theme) const
{
	// Small capitals in the muted text colour, as on the settings pages.
	FALKWidgetBuilder::StyleText(Header, Theme, EALKTextRole::Data, Theme.TextMuted);
	if (Header)
	{
		Header->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
}

void UALKHostGameScreen::StyleTextRow(const FTextRow& TextRow, const UALKTheme& Theme, bool bTyping) const
{
	// The name reads like the name of a setting row, and brightens while its box is typed in.
	if (UTextBlock* NameText = TextRow.NameText.Get())
	{
		FALKWidgetBuilder::StyleText(NameText, Theme, EALKTextRole::Body, bTyping ? Theme.TextPrimary : Theme.TextSecondary);
		FALKWidgetBuilder::SetTextFont(NameText, ALKHostGameScreenPrivate::GetRowFont(Theme));
		NameText->SetTextTransformPolicy(Theme.bUppercaseButtons ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	}
}

void UALKHostGameScreen::UpdateTitles()
{
	const FText Title = bEditMode ? LOCTEXT("HostTitleEdit", "Match Settings") : LOCTEXT("HostTitle", "Host Game");
	const FText Confirm = bEditMode ? LOCTEXT("HostApply", "Apply") : LOCTEXT("HostCreate", "Create lobby");
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (SystemLabelText)
	{
		SystemLabelText->SetText(MakeSystemLabel(22, Title));
		SystemLabelText->SetVisibility(SystemLabelText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetButtonText(Confirm);
		ConfirmButton->SetLeadingIcon(bEditMode ? EALKIcon::Check : EALKIcon::Play);
	}
	if (ConfirmHint)
	{
		ConfirmHint->SetButtonText(Confirm);
	}
}

void UALKHostGameScreen::NativeOnInputMethodChanged(bool bUsingGamepad)
{
	Super::NativeOnInputMethodChanged(bUsingGamepad);
	if (PadHints)
	{
		PadHints->SetVisibility(bUsingGamepad && GetMenuConfig()->bShowInputHints ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// ================================================================================================ Opening and closing

void UALKHostGameScreen::NativeConstruct()
{
	// Shown anew (screens are reused): the next activation starts from the saved settings again.
	bFreshOpen = true;
	Super::NativeConstruct();
}

FString UALKHostGameScreen::GetLocalPlayerName() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	return Online ? Online->GetLocalDisplayName() : FString();
}

FString UALKHostGameScreen::GetFallbackServerName() const
{
	// What the Online Config makes of an empty name: "<player name>'s Game".
	return UALKDeveloperSettings::GetOnlineConfig()->SanitizeHostSettings(FALKHostSettings(), GetLocalPlayerName()).ServerName;
}

void UALKHostGameScreen::LoadStartSettings()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	bEditMode = Flow && Flow->IsEditingCurrentGame() && Online && Online->IsInGame();
	if (bEditMode)
	{
		HostSettings = Online->GetCurrentHostSettings();
	}
	else
	{
		HostSettings = Online ? Online->GetLastHostSettings() : UALKDeveloperSettings::GetOnlineConfig()->MakeDefaultHostSettings(FString());
	}
	SelectedPreset.Reset();
}

void UALKHostGameScreen::NativeOnActivated()
{
	using namespace ALKHostGameScreenPrivate;

	if (bFreshOpen)
	{
		// Opened, not uncovered by a dialog that closed: what the player was editing is only reset here.
		bFreshOpen = false;
		LoadStartSettings();
		RebuildRows();
		if (SettingsList)
		{
			SettingsList->SetScrollOffset(0.f);
		}
		// Not the server name: landing in a text box would start typing (and bring up a console's keyboard).
		RefocusId = ModeRow;
		LastRowId = ModeRow;
		bFocusPending = true;
	}

	if (!SettingChangedHandle.IsValid())
	{
		SettingChangedHandle = UALKSetting::OnAnySettingChanged().AddUObject(this, &ThisClass::HandleAnySettingChanged);
	}
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!ListsChangedHandle.IsValid())
		{
			ListsChangedHandle = Online->OnListsChangedNative.AddUObject(this, &ThisClass::HandleListsChanged);
		}
	}
	UpdateTitles();
	RefreshRows();
	UpdateCard();

	Super::NativeOnActivated();
}

void UALKHostGameScreen::NativeOnDeactivated()
{
	UALKSetting::OnAnySettingChanged().Remove(SettingChangedHandle);
	SettingChangedHandle.Reset();
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnListsChangedNative.Remove(ListsChangedHandle);
	}
	ListsChangedHandle.Reset();
	Super::NativeOnDeactivated();
}

UWidget* UALKHostGameScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	if (Rows.Num() > 0 && Rows[0])
	{
		return Rows[0].Get();
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UALKHostGameScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bRebuildPending && IsActivated())
	{
		RebuildRows();
		RefreshRows();
		UpdateCard();
	}

	// Text boxes do not report focus the way rows do: watch which one is being typed in.
	int32 NowTyping = INDEX_NONE;
	for (int32 Index = 0; Index < TextRows.Num(); ++Index)
	{
		const UALKTextField* Field = TextRows[Index].Field.Get();
		if (Field && Field->IsTyping())
		{
			NowTyping = Index;
		}
	}
	if (NowTyping != TypingRow)
	{
		const UALKTheme& Theme = *GetTheme();
		if (TextRows.IsValidIndex(TypingRow))
		{
			StyleTextRow(TextRows[TypingRow], Theme, false);
		}
		TypingRow = NowTyping;
		if (TextRows.IsValidIndex(TypingRow))
		{
			StyleTextRow(TextRows[TypingRow], Theme, true);
			LastRowId = TextRows[TypingRow].Id;
			UpdateDescription(TextRows[TypingRow].DisplayName, TextRows[TypingRow].Description);
		}
	}

	// A row that was just made takes the focus once it has a place on screen.
	if (bFocusPending && IsActivated())
	{
		UWidget* Target = FindRowWidget(RefocusId);
		if (!Target && Rows.Num() > 0)
		{
			Target = Rows[0].Get();
		}
		if (!Target)
		{
			bFocusPending = false;
			RefocusId = NAME_None;
		}
		else if (Target->GetCachedGeometry().GetLocalSize().X > 1.0)
		{
			bFocusPending = false;
			RefocusId = NAME_None;
			Target->SetFocus();
			if (SettingsList)
			{
				SettingsList->ScrollWidgetIntoView(Target, false, EDescendantScrollDestination::IntoView, 40.f);
			}
		}
	}
}

// ================================================================================================ Rows

void UALKHostGameScreen::RequestRebuild(FName FocusAfter)
{
	if (!FocusAfter.IsNone())
	{
		RefocusId = FocusAfter;
	}
	else if (RefocusId.IsNone())
	{
		// The focused row is about to be replaced: its successor gets the focus back.
		RefocusId = GetFocusedRowId();
	}
	bRebuildPending = true;
}

FName UALKHostGameScreen::GetFocusedRowId() const
{
	for (const UALKSettingRow* Row : Rows)
	{
		if (Row && Row->GetSetting() && ALKHostGameScreenPrivate::IsFocusedWidget(Row))
		{
			return Row->GetSetting()->Id;
		}
	}
	for (const FTextRow& TextRow : TextRows)
	{
		if (ALKHostGameScreenPrivate::IsFocusedWidget(TextRow.Field.Get()))
		{
			return TextRow.Id;
		}
	}
	return NAME_None;
}

UWidget* UALKHostGameScreen::FindRowWidget(FName Id) const
{
	if (Id.IsNone())
	{
		return nullptr;
	}
	for (UALKSettingRow* Row : Rows)
	{
		if (Row && Row->GetSetting() && Row->GetSetting()->Id == Id)
		{
			return Row;
		}
	}
	for (const FTextRow& TextRow : TextRows)
	{
		if (TextRow.Id == Id && TextRow.Field.IsValid() && TextRow.Field->IsVisible())
		{
			return TextRow.Field.Get();
		}
	}
	return nullptr;
}

FText UALKHostGameScreen::GetSettingDescription(const UALKSetting* Setting) const
{
	if (!Setting)
	{
		return FText::GetEmpty();
	}
	// The game mode row explains the mode that is picked.
	if (Setting->Id == ALKHostGameScreenPrivate::ModeRow)
	{
		const FALKModeDefinition* Mode = UALKDeveloperSettings::GetOnlineConfig()->FindMode(HostSettings.ModeId);
		if (Mode && !Mode->Description.IsEmpty())
		{
			return Mode->Description;
		}
	}
	return Setting->Description;
}

void UALKHostGameScreen::ChangeMode(FName NewModeId)
{
	if (HostSettings.ModeId == NewModeId)
	{
		return;
	}
	// The new mode's maps, player limits and rules; the name is left as it was typed.
	const FString TypedName = HostSettings.ServerName;
	HostSettings.ModeId = NewModeId;
	HostSettings = UALKDeveloperSettings::GetOnlineConfig()->SanitizeHostSettings(HostSettings, FString());
	HostSettings.ServerName = TypedName;
	SelectedPreset.Reset();
	RequestRebuild(ALKHostGameScreenPrivate::ModeRow);
}

void UALKHostGameScreen::RebuildRows()
{
	using namespace ALKHostGameScreenPrivate;
	bRebuildPending = false;
	if (!SettingsList || !WidgetTree)
	{
		return;
	}

	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKTheme& Theme = *GetTheme();
	const float ScrollOffset = SettingsList->GetScrollOffset();
	const EALKSettingsCategory Category = EALKSettingsCategory::Multiplayer;

	SettingsList->ClearChildren();
	AllSettings.Reset();
	Rows.Reset();
	SectionHeaders.Reset();
	TextRows.Reset();
	TypingRow = INDEX_NONE;

	auto AddHeader = [this, &Theme](const FText& HeaderText)
	{
		UTextBlock* Header = FALKWidgetBuilder::MakeText(*WidgetTree, NAME_None, HeaderText);
		StyleSectionHeader(Header, Theme);
		SettingsList->AddChild(Header);
		if (UScrollBoxSlot* HeaderSlot = Cast<UScrollBoxSlot>(Header->Slot))
		{
			HeaderSlot->SetPadding(FMargin(20.f, 24.f, 0.f, 8.f));
		}
		SectionHeaders.Add(Header);
	};

	auto AddRow = [this](UALKSetting* Setting)
	{
		AllSettings.Add(Setting);
		UALKSettingRow* Row = WidgetTree->ConstructWidget<UALKSettingRow>(GetRowClass(Setting->GetSettingType()));
		if (!Row)
		{
			return;
		}
		Row->SetSetting(Setting);
		Row->OnRowFocused.AddUObject(this, &ThisClass::HandleRowFocused);
		SettingsList->AddChild(Row);
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		}
		Rows.Add(Row);
	};

	auto AddOptions = [this, &AddRow, Category](FName Id, const FText& RowName, const FText& RowDescription, const TArray<FText>& Choices,
		TFunction<int32()> IndexGetter, TFunction<void(int32)> IndexSetter) -> UALKSetting_Options*
	{
		UALKSetting_Options* Setting = NewObject<UALKSetting_Options>(this);
		Setting->Describe(Id, RowName, RowDescription, Category, FText::GetEmpty());
		Setting->Options = Choices;
		Setting->Getter = MoveTemp(IndexGetter);
		Setting->Setter = MoveTemp(IndexSetter);
		AddRow(Setting);
		return Setting;
	};

	auto AddToggle = [&AddOptions](FName Id, const FText& RowName, const FText& RowDescription, TFunction<bool()> ValueGetter, TFunction<void(bool)> ValueSetter) -> UALKSetting_Options*
	{
		UALKSetting_Options* Toggle = AddOptions(Id, RowName, RowDescription, { LOCTEXT("Off", "Off"), LOCTEXT("On", "On") },
			[ValueGetter]() { return ValueGetter() ? 1 : 0; },
			[ValueSetter](int32 Index) { ValueSetter(Index == 1); });
		Toggle->bIsToggle = true;
		return Toggle;
	};

	auto AddAction = [this, &AddRow, Category](FName Id, const FText& RowName, const FText& RowDescription, const FText& ButtonLabel, TFunction<void()> Run) -> UALKSetting_Action*
	{
		UALKSetting_Action* Setting = NewObject<UALKSetting_Action>(this);
		Setting->Describe(Id, RowName, RowDescription, Category, FText::GetEmpty());
		Setting->ActionText = ButtonLabel;
		Setting->Action = MoveTemp(Run);
		AddRow(Setting);
		return Setting;
	};

	// A name on the left and a text box on the right, laid out like a setting row.
	auto AddText = [this, &Theme](FName Id, const FText& RowName, const FText& RowDescription, const FText& HintText, bool bPassword, int32 MaxLength,
		TFunction<FString()> TextGetter, TFunction<void(const FString&)> TextSetter, TFunction<bool()> ShownCheck)
	{
		USizeBox* RowRoot = FALKWidgetBuilder::Make<USizeBox>(*WidgetTree);
		RowRoot->SetMinDesiredHeight(Theme.SettingRowHeight);
		UHorizontalBox* RowBox = FALKWidgetBuilder::Make<UHorizontalBox>(*WidgetTree);
		RowRoot->AddChild(RowBox);
		if (USizeBoxSlot* BoxSlot = Cast<USizeBoxSlot>(RowBox->Slot))
		{
			BoxSlot->SetPadding(FMargin(22.f, 2.f, 16.f, 2.f));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* NameText = FALKWidgetBuilder::MakeText(*WidgetTree, NAME_None, RowName);
		FALKWidgetBuilder::AddToHorizontalBox(RowBox, NameText, FMargin(0.f, 0.f, 16.f, 0.f), HAlign_Left, VAlign_Center, 0.52f);
		UALKTextField* Field = WidgetTree->ConstructWidget<UALKTextField>(UALKTextField::StaticClass());
		Field->SetHintText(HintText);
		Field->SetIsPassword(bPassword);
		Field->SetMaxLength(MaxLength);
		Field->SetText(FText::FromString(TextGetter()));
		FALKWidgetBuilder::AddToHorizontalBox(RowBox, Field, FMargin(), HAlign_Fill, VAlign_Center, 0.48f);
		SettingsList->AddChild(RowRoot);
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(RowRoot->Slot))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
		}

		FTextRow TextRow;
		TextRow.Id = Id;
		TextRow.DisplayName = RowName;
		TextRow.Description = RowDescription;
		TextRow.RowRoot = RowRoot;
		TextRow.NameText = NameText;
		TextRow.Field = Field;
		TextRow.Getter = MoveTemp(TextGetter);
		TextRow.Setter = MoveTemp(TextSetter);
		TextRow.IsShown = MoveTemp(ShownCheck);
		Field->OnTextChangedNative.AddUObject(this, &ThisClass::HandleTextRowChanged, TextRows.Num());
		StyleTextRow(TextRow, Theme, false);
		TextRows.Add(MoveTemp(TextRow));
	};

	// ---- The game

	AddText(ServerNameRow, LOCTEXT("HostServerName", "Server Name"),
		LOCTEXT("HostServerNameDesc", "The name players see in the server browser and in the lobby. Leave it empty to use your player name."),
		FText::FromString(GetFallbackServerName()), false, ServerNameLength,
		[this]() { return HostSettings.ServerName; },
		[this](const FString& Value) { HostSettings.ServerName = Value; }, nullptr);

	TArray<FName> ModeIds;
	TArray<FText> ModeNames;
	for (const FALKModeDefinition& Mode : Config->Modes)
	{
		ModeIds.Add(Mode.Id);
		ModeNames.Add(Mode.Name.IsEmpty() ? FText::FromName(Mode.Id) : Mode.Name);
	}
	if (ModeIds.Num() == 0)
	{
		// No modes in the Online Config: the game's own.
		ModeIds.Add(HostSettings.ModeId);
		ModeNames.Add(LOCTEXT("HostDefault", "Default"));
	}
	AddOptions(ModeRow, LOCTEXT("HostMode", "Game Mode"), LOCTEXT("HostModeDesc", "What kind of match is played."), ModeNames,
		[this, ModeIds]() { return FMath::Max(0, ModeIds.IndexOfByKey(HostSettings.ModeId)); },
		[this, ModeIds](int32 Index)
		{
			if (ModeIds.IsValidIndex(Index))
			{
				ChangeMode(ModeIds[Index]);
			}
		})->bWrap = true;

	TArray<FName> MapIds;
	TArray<FText> MapNames;
	for (const FALKMapDefinition& Map : Config->GetMapsForMode(HostSettings.ModeId))
	{
		MapIds.Add(Map.Id);
		MapNames.Add(Config->GetMapName(Map.Id));
	}
	if (MapIds.Num() == 0)
	{
		// No maps in the Online Config: the match is played wherever the game decides.
		MapIds.Add(HostSettings.MapId);
		MapNames.Add(LOCTEXT("HostDefault", "Default"));
	}
	AddOptions(MapRow, LOCTEXT("HostMap", "Map"), LOCTEXT("HostMapDesc", "Where the match is played. In the lobby, players can vote for another map when the game allows it."), MapNames,
		[this, MapIds]() { return FMath::Max(0, MapIds.IndexOfByKey(HostSettings.MapId)); },
		[this, MapIds](int32 Index)
		{
			if (MapIds.IsValidIndex(Index))
			{
				HostSettings.MapId = MapIds[Index];
			}
		})->bWrap = true;

	const TArray<int32> PlayerLimits = Config->GetMaxPlayersOptions(HostSettings.ModeId);
	TArray<FText> LimitNames;
	for (const int32 Limit : PlayerLimits)
	{
		LimitNames.Add(FText::AsNumber(Limit));
	}
	AddOptions(MaxPlayersRow, LOCTEXT("HostMaxPlayers", "Max Players"), LOCTEXT("HostMaxPlayersDesc", "How many players the game holds."), LimitNames,
		[this, PlayerLimits]() { return FMath::Max(0, PlayerLimits.IndexOfByKey(HostSettings.MaxPlayers)); },
		[this, PlayerLimits](int32 Index)
		{
			if (PlayerLimits.IsValidIndex(Index))
			{
				HostSettings.MaxPlayers = PlayerLimits[Index];
			}
		});

	AddToggle(LanRow, LOCTEXT("HostLan", "LAN Game"), LOCTEXT("HostLanDesc", "Only players on your local network find and join the game. It cannot be changed once the lobby is open."),
		[this]() { return HostSettings.bLAN; },
		[this](bool bOn) { HostSettings.bLAN = bOn; })->EditCondition = [this]() { return !bEditMode; };

	AddToggle(PrivateRow, LOCTEXT("HostPrivate", "Private"), LOCTEXT("HostPrivateDesc", "Hidden from the server browser: players join with the lobby code, the address or an invite. A private game can also ask for a password."),
		[this]() { return HostSettings.bPrivate; },
		[this](bool bOn) { HostSettings.bPrivate = bOn; });

	AddText(PasswordRow, LOCTEXT("HostPassword", "Password"),
		LOCTEXT("HostPasswordDesc", "Players have to give this password to join. Leave it empty for no password."),
		LOCTEXT("HostPasswordHint", "No password"), true, PasswordLength,
		[this]() { return HostSettings.Password; },
		[this](const FString& Value) { HostSettings.Password = Value; },
		[this]() { return HostSettings.bPrivate; });

	// ---- The mode's rules

	const TArray<FALKRuleDefinition> RuleDefinitions = Config->GetRulesForMode(HostSettings.ModeId);
	if (RuleDefinitions.Num() > 0)
	{
		AddHeader(LOCTEXT("HostRules", "Rules"));
	}
	for (const FALKRuleDefinition& Definition : RuleDefinitions)
	{
		const FName RuleId = Definition.Id;
		const FName RowId(*FString::Printf(TEXT("Rule.%s"), *RuleId.ToString()));
		const FText RuleName = Definition.DisplayName.IsEmpty() ? FText::FromName(RuleId) : Definition.DisplayName;
		switch (Definition.Type)
		{
		case EALKRuleType::Toggle:
			AddToggle(RowId, RuleName, Definition.Description,
				[this, RuleId]() { return UALKLobbyStatics::GetRuleBool(HostSettings.Rules, RuleId); },
				[this, RuleId](bool bOn)
				{
					UALKLobbyStatics::SetRuleValue(HostSettings.Rules, RuleId, bOn ? TEXT("1") : TEXT("0"));
					SelectedPreset.Reset();
				});
			break;

		case EALKRuleType::Options:
			AddOptions(RowId, RuleName, Definition.Description, Definition.Options,
				[this, RuleId]() { return UALKLobbyStatics::GetRuleInt(HostSettings.Rules, RuleId); },
				[this, RuleId](int32 Index)
				{
					UALKLobbyStatics::SetRuleValue(HostSettings.Rules, RuleId, FString::FromInt(Index));
					SelectedPreset.Reset();
				});
			break;

		case EALKRuleType::Slider:
		{
			UALKSetting_RuleSlider* Slider = NewObject<UALKSetting_RuleSlider>(this);
			Slider->Describe(RowId, RuleName, Definition.Description, Category, FText::GetEmpty());
			Slider->MinValue = Definition.MinValue;
			Slider->MaxValue = Definition.MaxValue;
			Slider->StepSize = Definition.StepSize;
			Slider->Format = Definition.Format;
			Slider->Getter = [this, RuleId, Lowest = Definition.MinValue]() { return UALKLobbyStatics::GetRuleFloat(HostSettings.Rules, RuleId, Lowest); };
			Slider->Setter = [this, RuleId](float Value)
			{
				UALKLobbyStatics::SetRuleValue(HostSettings.Rules, RuleId, FALKGameRules::NumberToString(Value));
				SelectedPreset.Reset();
			};
			// "10 min", "Unlimited": the rule's own way of printing its value.
			Slider->ValueText = [this, Definition]()
			{
				return UALKLobbyStatics::GetRuleValueText(Definition, UALKLobbyStatics::GetRuleString(HostSettings.Rules, Definition.Id, Definition.DefaultValue));
			};
			AddRow(Slider);
			break;
		}

		case EALKRuleType::Text:
			AddText(RowId, RuleName, Definition.Description, FText::GetEmpty(), false, 64,
				[this, RuleId]() { return UALKLobbyStatics::GetRuleString(HostSettings.Rules, RuleId); },
				[this, RuleId](const FString& Value)
				{
					UALKLobbyStatics::SetRuleValue(HostSettings.Rules, RuleId, Value);
					SelectedPreset.Reset();
				}, nullptr);
			break;
		}
	}
	if (RuleDefinitions.Num() > 0)
	{
		AddAction(RandomizeRow, LOCTEXT("HostRandomize", "Randomise Rules"), LOCTEXT("HostRandomizeDesc", "Gives every rule a random value, for a match nobody planned."),
			LOCTEXT("HostRandomizeButton", "Randomise"), [this]() { RandomizeRules(); });
	}

	// ---- Presets

	AddHeader(LOCTEXT("HostPresets", "Presets"));
	TArray<FString> PresetKeys;
	TArray<FText> PresetNames;
	PresetKeys.Add(FString());
	PresetNames.Add(LOCTEXT("HostPresetNone", "None"));
	for (const FALKRulePreset& Preset : Online ? Online->GetRulePresets() : TArray<FALKRulePreset>())
	{
		PresetKeys.Add(Preset.Name);
		PresetNames.Add(FText::FromString(Preset.Name));
	}
	if (!PresetKeys.Contains(SelectedPreset))
	{
		SelectedPreset.Reset();
	}
	UALKSetting_Options* PresetSetting = AddOptions(PresetRow, LOCTEXT("HostPreset", "Rule Preset"),
		LOCTEXT("HostPresetDesc", "Applies a set of rules you saved earlier. Changing a rule by hand takes you back to None."), PresetNames,
		[this, PresetKeys]() { return FMath::Max(0, PresetKeys.IndexOfByKey(SelectedPreset)); },
		[this, PresetKeys](int32 Index)
		{
			if (Index > 0 && PresetKeys.IsValidIndex(Index))
			{
				ApplyRulePreset(PresetKeys[Index]);
			}
			else
			{
				SelectedPreset.Reset();
			}
		});
	PresetSetting->bWrap = true;
	PresetSetting->EditCondition = [Count = PresetKeys.Num()]() { return Count > 1; };

	AddAction(SavePresetRow, LOCTEXT("HostSavePreset", "Save as Preset"), LOCTEXT("HostSavePresetDesc", "Keeps the rules on screen under a name, to use them again another day."),
		LOCTEXT("HostSavePresetButton", "Save"), [this]() { SaveRulesAsPreset(); });
	AddAction(DeletePresetRow, LOCTEXT("HostDeletePreset", "Delete Preset"), LOCTEXT("HostDeletePresetDesc", "Removes the preset picked in the row above. The rules on screen stay as they are."),
		LOCTEXT("HintDelete", "Delete"), [this]() { DeleteSelectedPreset(); })->EditCondition = [this]() { return !SelectedPreset.IsEmpty(); };

	SettingsList->SetScrollOffset(ScrollOffset);
	if (!RefocusId.IsNone())
	{
		bFocusPending = true;
	}
}

void UALKHostGameScreen::RefreshRows()
{
	for (UALKSettingRow* Row : Rows)
	{
		if (Row)
		{
			Row->Refresh();
		}
	}

	// Text rows: shown or not (the password belongs to a private game), and their text when something else changed it.
	TGuardValue<bool> Guard(bUpdatingTextRows, true);
	for (const FTextRow& TextRow : TextRows)
	{
		const bool bShown = !TextRow.IsShown || TextRow.IsShown();
		if (UWidget* RowRoot = TextRow.RowRoot.Get())
		{
			RowRoot->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		UALKTextField* Field = TextRow.Field.Get();
		if (Field && TextRow.Getter && !Field->IsTyping())
		{
			const FString Wanted = TextRow.Getter();
			if (!Field->GetText().ToString().Equals(Wanted, ESearchCase::CaseSensitive))
			{
				Field->SetText(FText::FromString(Wanted));
			}
		}
	}
}

void UALKHostGameScreen::HandleTextRowChanged(const FText& Text, int32 Index)
{
	if (bUpdatingTextRows || !TextRows.IsValidIndex(Index) || !TextRows[Index].Setter)
	{
		return;
	}
	TextRows[Index].Setter(Text.ToString());
	UpdateCard();
}

void UALKHostGameScreen::HandleAnySettingChanged(UALKSetting* Setting)
{
	// Every setting of the game reports here; only the rows of this screen matter.
	if (!Setting || !AllSettings.Contains(Setting))
	{
		return;
	}
	RefreshRows();
	UpdateCard();
	if (Setting->Id == LastRowId)
	{
		UpdateDescription(Setting->DisplayName, GetSettingDescription(Setting));
	}
}

void UALKHostGameScreen::HandleRowFocused(UALKSettingRow* Row, bool bFromNavigation)
{
	const UALKSetting* Setting = Row ? Row->GetSetting() : nullptr;
	if (!Setting)
	{
		return;
	}
	LastRowId = Setting->Id;
	UpdateDescription(Setting->DisplayName, GetSettingDescription(Setting));
	if (bFromNavigation && SettingsList)
	{
		SettingsList->ScrollWidgetIntoView(Row, true, EDescendantScrollDestination::IntoView, 40.f);
	}
}

void UALKHostGameScreen::HandleListsChanged()
{
	// A preset was saved or deleted: the Preset row lists them.
	RequestRebuild();
}

UWidget* UALKHostGameScreen::HandleNavigateToActions(EUINavigation Direction)
{
	return ConfirmButton.Get();
}

UWidget* UALKHostGameScreen::HandleNavigateToRows(EUINavigation Direction)
{
	if (UWidget* Last = FindRowWidget(LastRowId))
	{
		return Last;
	}
	return Rows.Num() > 0 ? Rows.Last().Get() : nullptr;
}

// ================================================================================================ The card

void UALKHostGameScreen::UpdateDescription(const FText& Name, const FText& Description)
{
	if (DescriptionTitle)
	{
		DescriptionTitle->SetText(Name);
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
		DescriptionText->SetVisibility(Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UALKHostGameScreen::UpdateCard()
{
	if (!WidgetTree)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKTheme& Theme = *GetTheme();

	if (ServerText)
	{
		const FString TypedName = HostSettings.ServerName.TrimStartAndEnd();
		ServerText->SetText(FText::FromString(TypedName.IsEmpty() ? GetFallbackServerName() : TypedName));
	}
	if (ModeText)
	{
		const FText ModeName = Config->GetModeName(HostSettings.ModeId);
		ModeText->SetText(ModeName.IsEmpty() ? LOCTEXT("HostCustomGame", "Custom Game") : ModeName);
	}
	if (MapText)
	{
		const FText MapName = Config->GetMapName(HostSettings.MapId);
		MapText->SetText(FText::Format(LOCTEXT("HostCardMap", "{0}  ·  {1} players"),
			MapName.IsEmpty() ? LOCTEXT("HostDefaultMap", "Default map") : MapName, FText::AsNumber(HostSettings.MaxPlayers)));
	}
	if (MapPicture)
	{
		// The map's own picture, or art made up from its name (the mode's, when there is no map).
		const FALKMapDefinition* Map = Config->FindMap(HostSettings.MapId);
		MapPicture->SetPicture(Map ? Map->Thumbnail.LoadSynchronous() : nullptr, HostSettings.MapId.IsNone() ? HostSettings.ModeId : HostSettings.MapId);
	}

	if (ChipBox)
	{
		ChipBox->ClearChildren();
		Chips.Reset();
		auto AddChip = [this, &Theme](const FText& ChipText, EALKIcon ChipIcon)
		{
			UALKFrame* Chip = FALKWidgetBuilder::MakeChip(*WidgetTree, NAME_None, ChipText, ChipIcon);
			FALKWidgetBuilder::StyleChip(Chip, Theme, Theme.PanelRaised, Theme.PanelOutline, Theme.TextPrimary);
			ChipBox->AddChildToWrapBox(Chip);
			Chips.Add(Chip);
		};
		if (HostSettings.bLAN)
		{
			AddChip(LOCTEXT("HostChipLan", "LAN game"), EALKIcon::None);
		}
		if (HostSettings.bPrivate)
		{
			AddChip(HostSettings.Password.IsEmpty() ? LOCTEXT("HostChipPrivate", "Private") : LOCTEXT("HostChipPassword", "Private, password"), EALKIcon::Lock);
		}
		int32 RuleChips = 0;
		for (const FALKRuleDefinition& Definition : Config->GetRulesForMode(HostSettings.ModeId))
		{
			if (RuleChips++ >= MaxRuleChips)
			{
				break;
			}
			const FString Value = UALKLobbyStatics::GetRuleString(HostSettings.Rules, Definition.Id, Definition.DefaultValue);
			AddChip(FText::Format(LOCTEXT("HostChipRule", "{0}: {1}"),
				Definition.DisplayName.IsEmpty() ? FText::FromName(Definition.Id) : Definition.DisplayName, UALKLobbyStatics::GetRuleValueText(Definition, Value)), EALKIcon::None);
		}
	}
}

// ================================================================================================ Actions

FALKHostSettings UALKHostGameScreen::MakeFinalSettings() const
{
	FALKHostSettings Final = HostSettings;
	if (!Final.bPrivate)
	{
		// The password row is only there for a private game.
		Final.Password.Reset();
	}
	// A known mode, a map of that mode, valid rules, and "<player name>'s Game" for an empty name.
	return UALKDeveloperSettings::GetOnlineConfig()->SanitizeHostSettings(Final, GetLocalPlayerName());
}

void UALKHostGameScreen::ConfirmSettings()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online)
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	const FALKHostSettings Final = MakeFinalSettings();
	if (bEditMode)
	{
		// The lobby is open: the server takes the new settings and tells everyone.
		if (UALKPlayerNetComponent* NetComponent = Online->GetLocalNetComponent())
		{
			NetComponent->ServerChangeMatchSettings(Final);
			CloseScreen();
		}
		else
		{
			PlayThemeSound(EALKUISound::Locked);
		}
		return;
	}
	if (!Online->HostGame(Final))
	{
		PlayThemeSound(EALKUISound::Locked);
		if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
		{
			Flow->ShowNotice(LOCTEXT("HostFailed", "Could not host a game"), LOCTEXT("HostFailedDetail", "Online play is busy or turned off. Try again in a moment."));
		}
	}
}

void UALKHostGameScreen::ResetToDefaults()
{
	const bool bWasLan = HostSettings.bLAN;
	HostSettings = UALKDeveloperSettings::GetOnlineConfig()->MakeDefaultHostSettings(GetLocalPlayerName());
	if (bEditMode)
	{
		// What a running game cannot change stays.
		HostSettings.bLAN = bWasLan;
	}
	SelectedPreset.Reset();
	PlayThemeSound(EALKUISound::Change);
	RequestRebuild();
}

void UALKHostGameScreen::RandomizeRules()
{
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	HostSettings.Rules.Randomize(Config->GetRulesForMode(HostSettings.ModeId), FMath::Rand());
	SelectedPreset.Reset();
	RefreshRows();
	UpdateCard();
}

bool UALKHostGameScreen::ApplyRulePreset(const FString& PresetName)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const TArray<FALKRulePreset> Presets = Online ? Online->GetRulePresets() : TArray<FALKRulePreset>();
	const FALKRulePreset* Preset = Presets.FindByPredicate([&PresetName](const FALKRulePreset& Each) { return Each.Name == PresetName; });
	if (!Preset)
	{
		return false;
	}

	// A preset saved for another mode brings its mode along; the rules are made valid for the mode either way.
	const bool bModeChanges = !Preset->ModeId.IsNone() && Preset->ModeId != HostSettings.ModeId && Config->FindMode(Preset->ModeId) != nullptr;
	const FString TypedName = HostSettings.ServerName;
	if (bModeChanges)
	{
		HostSettings.ModeId = Preset->ModeId;
	}
	HostSettings.Rules = Preset->Rules;
	HostSettings = Config->SanitizeHostSettings(HostSettings, FString());
	HostSettings.ServerName = TypedName;
	SelectedPreset = PresetName;
	if (bModeChanges)
	{
		RequestRebuild(ALKHostGameScreenPrivate::PresetRow);
	}
	else
	{
		RefreshRows();
		UpdateCard();
	}
	return true;
}

void UALKHostGameScreen::SaveRulesAsPreset()
{
	UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	if (!Flow)
	{
		return;
	}
	FALKTextEntryDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("HostSavePresetTitle", "Save Rule Preset");
	Descriptor.Message = LOCTEXT("HostSavePresetMessage", "Give this set of rules a name. A preset with the same name is replaced.");
	Descriptor.HintText = LOCTEXT("HostSavePresetHint", "Preset name");
	Descriptor.InitialText = SelectedPreset;
	Descriptor.ConfirmText = LOCTEXT("HostSavePresetButton", "Save");
	Descriptor.MaxLength = 32;

	TWeakObjectPtr<UALKHostGameScreen> WeakThis(this);
	Flow->ShowTextEntry(Descriptor, [WeakThis](bool bConfirmed, const FString& Text, const FString&)
	{
		UALKHostGameScreen* Self = WeakThis.Get();
		UALKOnlineSubsystem* Online = Self ? UALKOnlineSubsystem::Get(Self) : nullptr;
		if (!bConfirmed || !Online || Text.IsEmpty())
		{
			return;
		}
		Self->SelectedPreset = Text;
		Online->SaveRulePreset(Text, Self->HostSettings.ModeId, Self->HostSettings.Rules);
		Self->RequestRebuild(ALKHostGameScreenPrivate::SavePresetRow);
	});
}

void UALKHostGameScreen::DeleteSelectedPreset()
{
	UALKMenuSubsystem* Menu = GetMenuSubsystem();
	if (SelectedPreset.IsEmpty() || !Menu)
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	FALKDialogDescriptor Descriptor;
	Descriptor.Title = FText::Format(LOCTEXT("HostDeletePresetTitle", "Delete {0}?"), FText::FromString(SelectedPreset));
	Descriptor.Message = LOCTEXT("HostDeletePresetMessage", "The preset will be gone for good. The rules on screen stay as they are.");
	Descriptor.ConfirmText = LOCTEXT("HintDelete", "Delete");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = true;

	TWeakObjectPtr<UALKHostGameScreen> WeakThis(this);
	const FString PresetName = SelectedPreset;
	Menu->ShowConfirmDialog(Descriptor, [WeakThis, PresetName](EALKDialogResult Result)
	{
		UALKHostGameScreen* Self = WeakThis.Get();
		UALKOnlineSubsystem* Online = Self ? UALKOnlineSubsystem::Get(Self) : nullptr;
		if (Result != EALKDialogResult::Confirmed || !Online)
		{
			return;
		}
		Self->SelectedPreset.Reset();
		Online->DeleteRulePreset(PresetName);
		Self->RequestRebuild(ALKHostGameScreenPrivate::PresetRow);
	});
}

void UALKHostGameScreen::SetHostSettings(const FALKHostSettings& InSettings)
{
	HostSettings = InSettings;
	SelectedPreset.Reset();
	RequestRebuild();
}

void UALKHostGameScreen::HandleBackClicked()
{
	OnBackRequested();
}

bool UALKHostGameScreen::NativeHandleMenuKey(const FKey& Key)
{
	// Gamepad only: letters belong to the text boxes.
	if (Key == EKeys::Gamepad_FaceButton_Top)
	{
		ConfirmSettings();
		return true;
	}
	if (Key == EKeys::Gamepad_FaceButton_Left)
	{
		ResetToDefaults();
		return true;
	}
	return Super::NativeHandleMenuKey(Key);
}

#undef LOCTEXT_NAMESPACE
