// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKLoadoutScreen.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKCardButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKThumbnail.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformInput.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLoadoutScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKLoadoutScreenPrivate
{
	/** The content never gets wider than this (ultrawide screens). */
	static constexpr float MaxContentWidth = 1728.f;
	static constexpr float CardWidth = 300.f;
	static constexpr float CardHeight = 250.f;
	static constexpr float SwatchSize = 46.f;

	static void SetShown(UWidget* Widget, bool bShown, ESlateVisibility ShownAs = ESlateVisibility::HitTestInvisible)
	{
		if (Widget)
		{
			const ESlateVisibility Wanted = bShown ? ShownAs : ESlateVisibility::Collapsed;
			if (Widget->GetVisibility() != Wanted)
			{
				Widget->SetVisibility(Wanted);
			}
		}
	}
}

UALKLoadoutScreen::UALKLoadoutScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

FLinearColor UALKLoadoutScreen::GetPlayerColor(int32 ColorIndex, int32 ColorCount)
{
	// An even step round the colour wheel per index, at a saturation and brightness that read on dark and light panels.
	const int32 Count = FMath::Max(1, ColorCount);
	const int32 Index = ((ColorIndex % Count) + Count) % Count;
	const uint8 Hue = static_cast<uint8>((Index * 256 / Count) & 0xff);
	return FLinearColor::MakeFromHSV8(Hue, 165, 235);
}

void UALKLoadoutScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKLoadoutScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	USizeBox* MainSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainSize"));
	MainSize->SetWidthOverride(MaxContentWidth);
	FALKWidgetBuilder::AddToOverlay(Root, MainSize, HAlign_Center, VAlign_Fill);
	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	MainSize->SetContent(Main);

	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("LoadoutTitle", "Loadout"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleText, FMargin(), HAlign_Left);
	NoticeText = FALKWidgetBuilder::MakeText(Tree, TEXT("NoticeText"), LOCTEXT("LoadoutJoinFirst", "Join a lobby to equip a loadout"));
	FALKWidgetBuilder::AddToVerticalBox(Main, NoticeText, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Left);

	// Body: the cards | the focused loadout and the colours
	UOverlay* BodyOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("BodyOverlay"));
	FALKWidgetBuilder::AddToVerticalBox(Main, BodyOverlay, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	UHorizontalBox* Body = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Body"));
	FALKWidgetBuilder::AddToOverlay(BodyOverlay, Body);

	CardScroll = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("CardScroll"));
	CardScroll->SetAnimateWheelScrolling(true);
	// Moved out by the grid's padding (see below), so the cards still line up with the title.
	FALKWidgetBuilder::AddToHorizontalBox(Body, CardScroll, FMargin(-6.f, -6.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 0.6f);
	CardGrid = FALKWidgetBuilder::Make<UWrapBox>(Tree, TEXT("CardGrid"));
	CardGrid->SetInnerSlotPadding(FVector2D(16.0, 16.0));
	CardScroll->AddChild(CardGrid);
	if (UScrollBoxSlot* GridSlot = Cast<UScrollBoxSlot>(CardGrid->Slot))
	{
		// Room for the focus ring, which is drawn just outside a card.
		GridSlot->SetPadding(FMargin(6.f));
	}

	UALKFrame* Summary = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("SummaryPanel"));
	Summary->SetPadding(FMargin(22.f, 22.f, 22.f, 24.f));
	SummaryPanel = Summary;
	FALKWidgetBuilder::AddToHorizontalBox(Body, Summary, FMargin(32.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 0.4f);
	UVerticalBox* SummaryColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("SummaryColumn"));
	Summary->SetContent(SummaryColumn);

	SummaryPicture = FALKWidgetBuilder::Make<UALKThumbnail>(Tree, TEXT("SummaryPicture"));
	SummaryPicture->bThemeVeil = true;
	FALKWidgetBuilder::AddToVerticalBox(SummaryColumn, FALKWidgetBuilder::WrapInSizeBox(Tree, SummaryPicture, 0.f, 260.f), FMargin(), HAlign_Fill);
	SummaryName = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("SummaryName"));
	FALKWidgetBuilder::AddToVerticalBox(SummaryColumn, SummaryName, FMargin(0.f, 18.f, 0.f, 0.f), HAlign_Left);
	SummaryLock = FALKWidgetBuilder::MakeText(Tree, TEXT("SummaryLock"));
	FALKWidgetBuilder::AddToVerticalBox(SummaryColumn, SummaryLock, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Left);
	SummaryDescription = FALKWidgetBuilder::MakeText(Tree, TEXT("SummaryDescription"));
	SummaryDescription->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(SummaryColumn, SummaryDescription, FMargin(0.f, 10.f, 0.f, 0.f), HAlign_Fill);

	// Pushes the colours and the equip button to the bottom of the panel.
	FALKWidgetBuilder::AddToVerticalBox(SummaryColumn, FALKWidgetBuilder::Make<USpacer>(Tree, TEXT("SummarySpacer")), FMargin(), HAlign_Fill, VAlign_Fill, 1.f);

	ColorLabel = FALKWidgetBuilder::MakeText(Tree, TEXT("ColorLabel"), LOCTEXT("LoadoutColour", "Colour"));
	FALKWidgetBuilder::AddToVerticalBox(SummaryColumn, ColorLabel, FMargin(0.f, 12.f, 0.f, 8.f), HAlign_Left);
	ColorGrid = FALKWidgetBuilder::Make<UWrapBox>(Tree, TEXT("ColorGrid"));
	ColorGrid->SetInnerSlotPadding(FVector2D(8.0, 8.0));
	FALKWidgetBuilder::AddToVerticalBox(SummaryColumn, ColorGrid, FMargin(), HAlign_Fill);

	EquipButton = CreateMenuButton(LOCTEXT("LoadoutEquip", "Equip"), EALKButtonStyle::Solid);
	FALKWidgetBuilder::AddToVerticalBox(SummaryColumn, EquipButton, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Fill);

	EmptyText = FALKWidgetBuilder::MakeText(Tree, TEXT("EmptyText"),
		LOCTEXT("LoadoutNone", "No loadouts yet.\nAdd them to the Loadouts list of your Online Config (Online Config > Loadouts) and they show up here."));
	EmptyText->SetJustification(ETextJustify::Center);
	EmptyText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToOverlay(BodyOverlay, FALKWidgetBuilder::WrapInSizeBox(Tree, FALKWidgetBuilder::MakeEmptyState(Tree, TEXT("EmptyState"), EALKIcon::User, EmptyText), 760.f, 0.f),
		HAlign_Center, VAlign_Center);

	// Footer: [Enter] Equip  [Esc] Back
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("LoadoutEquip", "Equip")),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Left);
}

void UALKLoadoutScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (EquipButton)
	{
		EquipButton->OnClicked().AddUObject(this, &ThisClass::EquipFocused);
	}

	// The colours and Equip sit lower than the cards, out of the way of a straight step to the right, so Right from the
	// cards' edge and Left from the colours and from Equip are told where to go.
	FCustomWidgetNavigationDelegate ToColors;
	ToColors.BindDynamic(this, &ThisClass::HandleNavigateToColors);
	FCustomWidgetNavigationDelegate ToCards;
	ToCards.BindDynamic(this, &ThisClass::HandleNavigateToCards);
	if (CardGrid)
	{
		CardGrid->SetNavigationRuleCustomBoundary(EUINavigation::Right, ToColors);
	}
	if (ColorGrid)
	{
		ColorGrid->SetNavigationRuleCustomBoundary(EUINavigation::Left, ToCards);
	}
	if (EquipButton)
	{
		EquipButton->SetNavigationRuleCustomBoundary(EUINavigation::Left, ToCards);
	}
}

void UALKLoadoutScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (WidgetTree)
	{
		StyleScreenBackground(Theme);
		if (UWidget* MainSize = WidgetTree->FindWidget(TEXT("MainSize")))
		{
			if (UOverlaySlot* MainSlot = Cast<UOverlaySlot>(MainSize->Slot))
			{
				MainSlot->SetPadding(Theme.ScreenPadding);
			}
		}
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading);
		FALKWidgetBuilder::SetTextFont(TitleText, Theme.HeadingFont);
	}
	FALKWidgetBuilder::StyleText(NoticeText, Theme, EALKTextRole::Caption, Theme.Warning);
	if (UALKFrame* Summary = Cast<UALKFrame>(SummaryPanel))
	{
		Summary->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (SummaryPicture)
	{
		SummaryPicture->SetCornerRadius(FMath::Max(0.f, Theme.CornerRadius - 4.f));
	}
	if (SummaryName)
	{
		FALKWidgetBuilder::StyleText(SummaryName, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo NameFont = Theme.HeadingFont;
		NameFont.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.75f));
		FALKWidgetBuilder::SetTextFont(SummaryName, NameFont);
	}
	FALKWidgetBuilder::StyleText(SummaryLock, Theme, EALKTextRole::Data, Theme.Warning);
	FALKWidgetBuilder::StyleText(SummaryDescription, Theme, EALKTextRole::Body, Theme.TextSecondary);
	if (SummaryDescription)
	{
		SummaryDescription->SetLineHeightPercentage(1.25f);
	}
	FALKWidgetBuilder::StyleText(ColorLabel, Theme, EALKTextRole::Data, Theme.TextMuted);
	if (ColorLabel)
	{
		ColorLabel->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	FALKWidgetBuilder::StyleText(EmptyText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleEmptyState(EmptyText, Theme);
	if (EmptyText)
	{
		EmptyText->SetLineHeightPercentage(1.3f);
	}
	if (CardScroll)
	{
		FScrollBarStyle BarStyle = CardScroll->GetWidgetBarStyle();
		const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
		BarStyle.SetNormalThumbImage(Thumb);
		BarStyle.SetHoveredThumbImage(Thumb);
		BarStyle.SetDraggedThumbImage(Thumb);
		BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
		CardScroll->SetWidgetBarStyle(BarStyle);
		CardScroll->SetScrollbarThickness(FVector2D(3.0, 3.0));
		CardScroll->SetScrollbarPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}
	// The swatches are made again in the new look (the cards restyle themselves).
	if (SwatchButtons.Num() > 0)
	{
		RebuildSwatches();
		RefreshState();
	}
}

void UALKLoadoutScreen::NativeOnActivated()
{
	// Screens are pooled: start from the config and the player as they are now.
	FocusedIndex = 0;
	RefreshLoadouts();
	Super::NativeOnActivated();
}

void UALKLoadoutScreen::NativeOnDeactivated()
{
	UnbindGameState();
	Super::NativeOnDeactivated();
}

AALKLobbyGameState* UALKLoadoutScreen::ResolveGameState()
{
	const UWorld* World = GetWorld();
	// The game state can be missing for a moment after a level change: look again until it is there.
	AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
	if (GameState != BoundGameState.Get())
	{
		UnbindGameState();
		BoundGameState = GameState;
		if (GameState)
		{
			// The equipped loadout and the colour come back from the server with the player state.
			GameState->OnPlayersChangedNative.AddUObject(this, &ThisClass::MarkDirty);
		}
		bDirty = true;
	}
	return GameState;
}

void UALKLoadoutScreen::UnbindGameState()
{
	if (AALKLobbyGameState* Old = BoundGameState.Get())
	{
		Old->OnPlayersChangedNative.RemoveAll(this);
	}
	BoundGameState.Reset();
}

void UALKLoadoutScreen::MarkDirty()
{
	bDirty = true;
}

UALKPlayerNetComponent* UALKLoadoutScreen::GetNetComponent() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	return Online ? Online->GetLocalNetComponent() : nullptr;
}

AALKLobbyPlayerState* UALKLoadoutScreen::GetLocalLobbyPlayer() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return Controller ? Controller->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
}

int32 UALKLoadoutScreen::GetLocalLevel() const
{
	// In a game the player state knows the level; in the menus the profile does.
	if (const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer())
	{
		return Local->PlayerLevel;
	}
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	FALKPlayerProfile Profile;
	if (Online && Online->GetPlayerProfile(Online->GetLocalUserId(), Profile))
	{
		return Profile.Level;
	}
	return 1;
}

void UALKLoadoutScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsActivated())
	{
		return;
	}
	ResolveGameState();
	if (bDirty)
	{
		RefreshState();
	}
}

void UALKLoadoutScreen::RefreshLoadouts()
{
	Loadouts = UALKDeveloperSettings::GetOnlineConfig()->Loadouts;
	FocusedIndex = FMath::Clamp(FocusedIndex, 0, FMath::Max(0, Loadouts.Num() - 1));
	RebuildCards();
	RebuildSwatches();
	RefreshState();
}

void UALKLoadoutScreen::RebuildCards()
{
	using namespace ALKLoadoutScreenPrivate;
	Cards.Reset();
	if (!CardGrid || !WidgetTree)
	{
		return;
	}
	CardGrid->ClearChildren();
	for (int32 Index = 0; Index < Loadouts.Num(); ++Index)
	{
		const FALKLoadoutDefinition& Loadout = Loadouts[Index];
		UALKCardButton* Card = CreateWidget<UALKCardButton>(this, UALKCardButton::StaticClass());
		if (!Card)
		{
			continue;
		}
		Card->SetCard(Loadout.Name.IsEmpty() ? FText::FromName(Loadout.Id) : Loadout.Name, FText::GetEmpty(), EALKIcon::None);
		// The loadout's own picture, or generated art that is always the same for the same Id.
		Card->SetPicture(Loadout.Thumbnail.LoadSynchronous(), Loadout.Id);
		Card->OnCardFocused.AddUObject(this, &ThisClass::HandleCardFocused);
		Card->OnClicked().AddUObject(this, &ThisClass::EquipLoadout, Index);

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetWidthOverride(CardWidth);
		CardSize->SetHeightOverride(CardHeight);
		CardSize->SetContent(Card);
		CardGrid->AddChildToWrapBox(CardSize);
		Cards.Add(Card);
	}
}

void UALKLoadoutScreen::RebuildSwatches()
{
	using namespace ALKLoadoutScreenPrivate;
	SwatchButtons.Reset();
	SwatchTicks.Reset();
	const UALKTheme* Theme = GetTheme();
	if (!ColorGrid || !WidgetTree || !Theme)
	{
		return;
	}
	ColorGrid->ClearChildren();
	const int32 ColorCount = FMath::Clamp(UALKDeveloperSettings::GetOnlineConfig()->PlayerColorCount, 1, 32);
	for (int32 Index = 0; Index < ColorCount; ++Index)
	{
		const FLinearColor Color = GetPlayerColor(Index, ColorCount);

		// A button with the colour laid over it; the tick marks the colour the player has.
		UOverlay* Swatch = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UALKButton* SwatchButton = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Secondary);
		SwatchButton->OnClicked().AddUObject(this, &ThisClass::PickColor, Index);
		FALKWidgetBuilder::AddToOverlay(Swatch, SwatchButton);

		UALKFrame* Chip = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
		Chip->SetVisibility(ESlateVisibility::HitTestInvisible);
		FALKFrameStyle ChipFrame = Theme->MakeChipFrame(Color, FLinearColor::Transparent);
		ChipFrame.Radius = FMath::Max(0.f, Theme->CornerRadius - 4.f);
		Chip->SetFrameStyle(ChipFrame);
		FALKWidgetBuilder::AddToOverlay(Swatch, Chip, HAlign_Fill, VAlign_Fill, FMargin(7.f));

		UALKIconWidget* TickIcon = WidgetTree->ConstructWidget<UALKIconWidget>(UALKIconWidget::StaticClass());
		TickIcon->IconSize = 20.f;
		// Dark on a light colour, light on a dark one.
		TickIcon->SetIcon(EALKIcon::Check, Color.GetLuminance() > 0.3f ? Theme->AccentContrast : FLinearColor::White);
		TickIcon->SetVisibility(ESlateVisibility::Collapsed);
		FALKWidgetBuilder::AddToOverlay(Swatch, TickIcon, HAlign_Center, VAlign_Center);

		USizeBox* SwatchBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SwatchBox->SetWidthOverride(SwatchSize);
		SwatchBox->SetHeightOverride(SwatchSize);
		SwatchBox->SetContent(Swatch);
		ColorGrid->AddChildToWrapBox(SwatchBox);
		SwatchButtons.Add(SwatchButton);
		SwatchTicks.Add(TickIcon);
	}
}

void UALKLoadoutScreen::RefreshState()
{
	using namespace ALKLoadoutScreenPrivate;
	ResolveGameState();
	bDirty = false;

	const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
	const bool bInGame = GetNetComponent() != nullptr && Local != nullptr;
	const int32 Level = GetLocalLevel();
	const bool bHasLoadouts = Loadouts.Num() > 0;

	if (WidgetTree)
	{
		SetShown(WidgetTree->FindWidget(TEXT("Body")), bHasLoadouts, ESlateVisibility::SelfHitTestInvisible);
	}
	FALKWidgetBuilder::SetEmptyStateShown(EmptyText, !bHasLoadouts);
	SetShown(NoticeText, bHasLoadouts && !bInGame);

	// ---- The cards: a lock tag and a dimmed card below the level, a tag on the one that is equipped
	for (int32 Index = 0; Index < Cards.Num() && Index < Loadouts.Num(); ++Index)
	{
		UALKCardButton* Card = Cards[Index];
		if (!Card)
		{
			continue;
		}
		const FALKLoadoutDefinition& Loadout = Loadouts[Index];
		const bool bLocked = Level < Loadout.RequiredLevel;
		const bool bEquipped = Local && !Loadout.Id.IsNone() && Local->LoadoutId == Loadout.Id;
		Card->SetBadge(bLocked ? FText::Format(LOCTEXT("LoadoutLevel", "Level {0}"), FText::AsNumber(Loadout.RequiredLevel)) : FText::GetEmpty());
		Card->SetCornerChip(bEquipped ? LOCTEXT("LoadoutEquipped", "Equipped") : FText::GetEmpty());
		// The card dims itself only when disabled, and a locked card must stay focusable: dim its holder instead.
		if (UWidget* Holder = Card->GetParent())
		{
			Holder->SetRenderOpacity(bLocked ? 0.55f : 1.f);
		}
	}

	// ---- The colours
	const int32 CurrentColor = Local ? Local->ColorIndex : INDEX_NONE;
	for (int32 Index = 0; Index < SwatchButtons.Num(); ++Index)
	{
		if (SwatchButtons[Index])
		{
			SwatchButtons[Index]->SetIsEnabled(bInGame);
			SwatchButtons[Index]->SetActiveVisual(Index == CurrentColor);
		}
		if (SwatchTicks.IsValidIndex(Index) && SwatchTicks[Index])
		{
			SwatchTicks[Index]->SetVisibility(Index == CurrentColor ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	UpdateSummary();
}

void UALKLoadoutScreen::UpdateSummary()
{
	if (!Loadouts.IsValidIndex(FocusedIndex))
	{
		return;
	}
	const FALKLoadoutDefinition& Loadout = Loadouts[FocusedIndex];
	const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
	const bool bInGame = GetNetComponent() != nullptr && Local != nullptr;
	const bool bLocked = GetLocalLevel() < Loadout.RequiredLevel;
	const bool bEquipped = Local && !Loadout.Id.IsNone() && Local->LoadoutId == Loadout.Id;

	if (SummaryPicture)
	{
		SummaryPicture->SetPicture(Loadout.Thumbnail.LoadSynchronous(), Loadout.Id);
	}
	if (SummaryName)
	{
		SummaryName->SetText(Loadout.Name.IsEmpty() ? FText::FromName(Loadout.Id) : Loadout.Name);
	}
	if (SummaryLock)
	{
		SummaryLock->SetText(FText::Format(LOCTEXT("LoadoutUnlocksAt", "Unlocks at level {0}"), FText::AsNumber(Loadout.RequiredLevel)));
		SummaryLock->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (SummaryDescription)
	{
		SummaryDescription->SetText(Loadout.Description);
	}
	if (EquipButton)
	{
		EquipButton->SetButtonText(bEquipped ? LOCTEXT("LoadoutEquipped", "Equipped") : (bLocked ? LOCTEXT("LoadoutLocked", "Locked") : LOCTEXT("LoadoutEquip", "Equip")));
		EquipButton->SetLeadingIcon(bEquipped ? EALKIcon::Check : (bLocked ? EALKIcon::Lock : EALKIcon::None));
		EquipButton->SetIsEnabled(bInGame && !bLocked && !bEquipped);
	}

}

UWidget* UALKLoadoutScreen::HandleNavigateToColors(EUINavigation Direction)
{
	const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
	const int32 ColorIndex = Local ? Local->ColorIndex : INDEX_NONE;
	UALKButton* Swatch = SwatchButtons.IsValidIndex(ColorIndex) ? SwatchButtons[ColorIndex].Get() : (SwatchButtons.Num() > 0 ? SwatchButtons[0].Get() : nullptr);
	if (Swatch && Swatch->GetIsEnabled())
	{
		return Swatch;
	}
	return EquipButton && EquipButton->GetIsEnabled() ? EquipButton.Get() : nullptr;
}

UWidget* UALKLoadoutScreen::HandleNavigateToCards(EUINavigation Direction)
{
	return Cards.IsValidIndex(FocusedIndex) ? Cards[FocusedIndex].Get() : nullptr;
}

void UALKLoadoutScreen::HandleCardFocused(UALKCardButton* Card)
{
	const int32 Index = Cards.IndexOfByKey(Card);
	if (Index != INDEX_NONE && Index != FocusedIndex)
	{
		FocusedIndex = Index;
		UpdateSummary();
	}
	if (CardScroll && Card)
	{
		CardScroll->ScrollWidgetIntoView(Card, true, EDescendantScrollDestination::IntoView, 24.f);
	}
}

void UALKLoadoutScreen::EquipFocused()
{
	EquipLoadout(FocusedIndex);
}

void UALKLoadoutScreen::EquipLoadout(int32 LoadoutIndex)
{
	UALKPlayerNetComponent* NetComponent = GetNetComponent();
	if (!Loadouts.IsValidIndex(LoadoutIndex) || !NetComponent || GetLocalLevel() < Loadouts[LoadoutIndex].RequiredLevel)
	{
		// Not in a game, or the level is too low.
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	// The "Equipped" tag moves when the player state comes back from the server.
	NetComponent->ServerSetLoadout(Loadouts[LoadoutIndex].Id);
}

void UALKLoadoutScreen::PickColor(int32 ColorIndex)
{
	if (UALKPlayerNetComponent* NetComponent = GetNetComponent())
	{
		NetComponent->ServerSetColor(ColorIndex);
		PlayThemeSound(EALKUISound::Change);
	}
}

UWidget* UALKLoadoutScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	// The equipped loadout's card, else the first one.
	const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
	for (int32 Index = 0; Local && Index < Cards.Num() && Index < Loadouts.Num(); ++Index)
	{
		if (Cards[Index] && Loadouts[Index].Id == Local->LoadoutId)
		{
			return Cards[Index];
		}
	}
	if (Cards.Num() > 0 && Cards[0])
	{
		return Cards[0];
	}
	return Super::NativeGetDesiredFocusTarget();
}

#undef LOCTEXT_NAMESPACE
