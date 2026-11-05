// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKProfileDialog.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKScoreboard.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKProfileDialog)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKProfileDialogPrivate
{
	static constexpr float PanelWidth = 840.f;
	static constexpr int32 TilesPerLine = 3;
	/** Seconds the dialog keeps asking the provider for a profile before it says there is none. */
	static constexpr float LoadTimeout = 2.5f;

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

UALKProfileDialog::UALKProfileDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsModal = true;
}

void UALKProfileDialog::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKProfileDialogPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	DimImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("DimImage"));
	FALKWidgetBuilder::AddToOverlay(Root, DimImage);

	Panel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Panel->SetPadding(FMargin(40.f, 36.f, 40.f, 32.f));
	FALKWidgetBuilder::AddToOverlay(Root, FALKWidgetBuilder::WrapInSizeBox(Tree, Panel, PanelWidth, 0.f), HAlign_Center, VAlign_Center);
	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Panel->SetContent(Column);

	// Header: picture | name over presence | level
	UHorizontalBox* HeaderRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("HeaderRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, HeaderRow);
	Avatar = FALKWidgetBuilder::Make<UALKAvatar>(Tree, TEXT("Avatar"));
	Avatar->Diameter = 96.f;
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, Avatar, FMargin(0.f, 0.f, 24.f, 0.f), HAlign_Left, VAlign_Center);

	UVerticalBox* Names = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("NameColumn"));
	Names->SetClipping(EWidgetClipping::ClipToBounds);
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, Names, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	NameText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("NameText"));
	FALKWidgetBuilder::AddToVerticalBox(Names, NameText, FMargin(), HAlign_Left);
	UHorizontalBox* PresenceRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("PresenceRow"));
	FALKWidgetBuilder::AddToVerticalBox(Names, PresenceRow, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Left);
	PlatformIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("PlatformIcon"), EALKIcon::Platform, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(PresenceRow, PlatformIcon, FMargin(0.f, 0.f, 8.f, 0.f), HAlign_Left, VAlign_Center);
	PresenceText = FALKWidgetBuilder::MakeText(Tree, TEXT("PresenceText"));
	FALKWidgetBuilder::AddToHorizontalBox(PresenceRow, PresenceText, FMargin(), HAlign_Left, VAlign_Center);

	LevelChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("LevelChip"), FText::GetEmpty(), EALKIcon::Star);
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, LevelChip, FMargin(16.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Top);

	// Level progress
	UHorizontalBox* LevelRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("LevelRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, LevelRow, FMargin(0.f, 22.f, 0.f, 0.f));
	LevelBar = FALKWidgetBuilder::Make<UALKValueBar>(Tree, TEXT("LevelBar"));
	LevelBar->BarSize = FVector2D(300.0, 10.0);
	LevelBar->TrackHeight = 6.f;
	FALKWidgetBuilder::AddToHorizontalBox(LevelRow, LevelBar, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	LevelText = FALKWidgetBuilder::MakeText(Tree, TEXT("LevelText"));
	FALKWidgetBuilder::AddToHorizontalBox(LevelRow, LevelText, FMargin(16.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

	// The stat tiles, or the spinner / "no profile" line in their place.
	UOverlay* StatOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("StatOverlay"));
	FALKWidgetBuilder::AddToVerticalBox(Column, FALKWidgetBuilder::WrapInSizeBox(Tree, StatOverlay, 0.f, 150.f, true), FMargin(0.f, 24.f, 0.f, 0.f));
	StatGrid = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("StatGrid"));
	FALKWidgetBuilder::AddToOverlay(StatOverlay, StatGrid, HAlign_Fill, VAlign_Top);
	UVerticalBox* StatusBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("StatusBox"));
	FALKWidgetBuilder::AddToOverlay(StatOverlay, StatusBox, HAlign_Center, VAlign_Center);
	Spinner = FALKWidgetBuilder::Make<UALKSpinner>(Tree, TEXT("Spinner"));
	Spinner->Diameter = 34.f;
	FALKWidgetBuilder::AddToVerticalBox(StatusBox, Spinner, FMargin(), HAlign_Center);
	StatusText = FALKWidgetBuilder::MakeText(Tree, TEXT("StatusText"), LOCTEXT("ProfileNone", "No profile available"));
	StatusText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(StatusBox, StatusText, FMargin(), HAlign_Center);

	// Buttons: what you can do with the player on the left, Close on the right.
	UHorizontalBox* ButtonRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ButtonRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, ButtonRow, FMargin(0.f, 30.f, 0.f, 0.f));
	InviteButton = CreateMenuButton(LOCTEXT("ProfileInvite", "Invite to party"), EALKButtonStyle::Secondary);
	InviteButton->SetLeadingIcon(EALKIcon::Users);
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, InviteButton);
	MuteButton = CreateMenuButton(LOCTEXT("ProfileMute", "Mute"), EALKButtonStyle::Secondary);
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, MuteButton, FMargin(12.f, 0.f, 0.f, 0.f));
	ReportButton = CreateMenuButton(LOCTEXT("ProfileReport", "Report"), EALKButtonStyle::Secondary);
	ReportButton->SetLeadingIcon(EALKIcon::Flag);
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, ReportButton, FMargin(12.f, 0.f, 0.f, 0.f));
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, FALKWidgetBuilder::Make<USpacer>(Tree, TEXT("ButtonSpacer")), FMargin(), HAlign_Fill, VAlign_Fill, 1.f);
	CloseButton = CreateMenuButton(LOCTEXT("Close", "Close"), EALKButtonStyle::Secondary);
	CloseButton->SetInputHint(FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), FText::GetEmpty()));
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, CloseButton, FMargin(12.f, 0.f, 0.f, 0.f));
}

void UALKProfileDialog::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (InviteButton)
	{
		InviteButton->OnClicked().AddUObject(this, &ThisClass::InviteToParty);
	}
	if (MuteButton)
	{
		MuteButton->OnClicked().AddUObject(this, &ThisClass::ToggleMute);
	}
	if (ReportButton)
	{
		ReportButton->OnClicked().AddUObject(this, &ThisClass::ReportPlayer);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked().AddWeakLambda(this, [this]() { OnBackRequested(); });
	}
}

void UALKProfileDialog::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (DimImage)
	{
		DimImage->SetBrush(UALKTheme::MakeSolidBrush(Theme.ScreenDim));
	}
	if (UALKFrame* Frame = Cast<UALKFrame>(Panel))
	{
		Frame->SetFrameStyle(Theme.MakeDialogFrame());
	}
	if (NameText)
	{
		// The name is shown as the player wrote it, in every skin.
		FALKWidgetBuilder::StyleText(NameText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo NameFont = Theme.HeadingFont;
		NameFont.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.8f));
		FALKWidgetBuilder::SetTextFont(NameText, NameFont);
		NameText->SetTextTransformPolicy(ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(PresenceText, Theme, EALKTextRole::Caption, Theme.TextSecondary);
	if (PlatformIcon)
	{
		PlatformIcon->SetIcon(EALKIcon::Platform, Theme.TextMuted);
	}
	FALKWidgetBuilder::StyleChip(LevelChip, Theme, Theme.Accent.CopyWithNewOpacity(0.18f), FLinearColor::Transparent, Theme.GetAccentTextColor());
	if (LevelBar)
	{
		LevelBar->SetColors(Theme.Accent, Theme.SliderTrack);
	}
	FALKWidgetBuilder::StyleText(LevelText, Theme, EALKTextRole::Data, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(StatusText, Theme, EALKTextRole::Body, Theme.TextMuted);
	if (Spinner)
	{
		Spinner->SetSpinnerColor(Theme.Accent);
	}
	if (Avatar)
	{
		Avatar->SetBackColor(Theme.Panel);
	}
	RefreshCard();
}

void UALKProfileDialog::NativeOnActivated()
{
	// The dialog is pooled: read who it was opened for each time.
	const UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	UserId = Flow ? Flow->GetContextUserId() : FString();
	ContextName = Flow ? Flow->GetContextDisplayName() : FString();
	StartLoading();
	Super::NativeOnActivated();
}

void UALKProfileDialog::ShowPlayer(const FString& InUserId, const FString& InDisplayName)
{
	UserId = InUserId;
	ContextName = InDisplayName;
	StartLoading();
}

void UALKProfileDialog::StartLoading()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (UserId.IsEmpty() && Online)
	{
		// Opened for nobody: the local player's own card.
		UserId = Online->GetLocalUserId();
		ContextName = Online->GetLocalDisplayName();
	}
	Profile = FALKPlayerProfile();
	bHasProfile = false;
	LoadTime = 0.f;
	RetryTime = 0.f;
	// Most providers answer at once; one that is still reading gets a moment, with the spinner up.
	bLoading = !TryLoadProfile();
	RefreshCard();
}

bool UALKProfileDialog::TryLoadProfile()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	FALKPlayerProfile Loaded;
	if (Online && !UserId.IsEmpty() && Online->GetPlayerProfile(UserId, Loaded))
	{
		Profile = Loaded;
		bHasProfile = true;
		return true;
	}
	return false;
}

void UALKProfileDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bLoading)
	{
		return;
	}
	const float Step = FMath::Min(InDeltaTime, 0.1f);
	LoadTime += Step;
	RetryTime += Step;
	if (RetryTime >= 0.5f)
	{
		RetryTime = 0.f;
		if (TryLoadProfile() || LoadTime >= ALKProfileDialogPrivate::LoadTimeout)
		{
			bLoading = false;
			RefreshCard();
		}
	}
}

bool UALKProfileDialog::IsLocalUser() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	return Online && !UserId.IsEmpty() && UserId == Online->GetLocalUserId();
}

FString UALKProfileDialog::ResolveShownName() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online)
	{
		return ContextName;
	}
	// The local player's own name is never hidden.
	if (IsLocalUser())
	{
		return Online->GetLocalDisplayName();
	}
	// A player of the current game keeps the name (or the "Player 3" of streamer mode) they have everywhere else in it.
	const UWorld* World = GetWorld();
	const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
	if (const AALKLobbyPlayerState* Player = GameState ? GameState->FindPlayerByUserId(UserId) : nullptr)
	{
		return UALKScoreboard::GetShownPlayerName(Player);
	}
	const FString& RealName = !Profile.DisplayName.IsEmpty() ? Profile.DisplayName : ContextName;
	return Online->GetDisplayName(RealName, INDEX_NONE);
}

void UALKProfileDialog::RefreshCard()
{
	using namespace ALKProfileDialogPrivate;
	const UALKTheme* Theme = GetTheme();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const bool bLocalUser = IsLocalUser();
	const FString ShownName = ResolveShownName();

	if (NameText)
	{
		NameText->SetText(ShownName.IsEmpty() ? LOCTEXT("ProfileUnknownPlayer", "Player") : FText::FromString(ShownName));
	}

	// ---- What the player is doing: from the friends list, or "in your game" for someone met in a lobby
	FText Presence;
	FLinearColor PresenceColor = FLinearColor::Transparent;
	if (Theme && Online)
	{
		const UWorld* World = GetWorld();
		const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
		const TArray<FALKFriend> Friends = Online->GetFriends();
		const FALKFriend* Friend = Friends.FindByPredicate([this](const FALKFriend& Each) { return Each.UserId == UserId; });
		if (bLocalUser)
		{
			Presence = LOCTEXT("ProfileYou", "This is you");
			PresenceColor = Theme->Success;
		}
		else if (GameState && GameState->FindPlayerByUserId(UserId))
		{
			Presence = LOCTEXT("ProfileInYourGame", "In your game");
			PresenceColor = Theme->Success;
		}
		else if (Friend)
		{
			switch (Friend->Presence)
			{
			case EALKPresence::Online:
				Presence = LOCTEXT("ProfileOnline", "Online");
				PresenceColor = Theme->Success;
				break;
			case EALKPresence::InLobby:
				Presence = LOCTEXT("ProfileInLobby", "In a lobby");
				PresenceColor = Theme->AccentAlt;
				break;
			case EALKPresence::InGame:
				Presence = LOCTEXT("ProfileInGame", "In a game");
				PresenceColor = Theme->AccentAlt;
				break;
			default:
				Presence = LOCTEXT("ProfileOffline", "Offline");
				PresenceColor = Theme->TextMuted;
				break;
			}
			// The friend's own status line says more ("Team Deathmatch on Gridlock").
			if (!Friend->StatusText.IsEmpty() && Friend->Presence != EALKPresence::Offline)
			{
				Presence = Friend->StatusText;
			}
		}
	}
	if (PresenceText)
	{
		PresenceText->SetText(Presence);
		SetShown(PresenceText->GetParent(), !Presence.IsEmpty());
	}
	if (Avatar)
	{
		UTexture2D* Picture = bHasProfile ? Profile.Avatar.LoadSynchronous() : nullptr;
		Avatar->SetPlayer(ShownName, Picture);
		Avatar->SetAnonymous(Online && Online->IsStreamerMode() && !bLocalUser);
		Avatar->SetStatusColor(PresenceColor);
	}

	// ---- Level and progress, when the profile has them
	const bool bHasLevel = bHasProfile && Profile.Level > 0;
	if (LevelChip)
	{
		FALKWidgetBuilder::SetChipText(LevelChip, FText::Format(LOCTEXT("ProfileLevel", "Level {0}"), FText::AsNumber(Profile.Level)));
		SetShown(LevelChip, bHasLevel);
	}
	if (LevelBar)
	{
		LevelBar->SetValue(FMath::Clamp(Profile.XPProgress, 0.f, 1.f));
		SetShown(LevelBar->GetParent(), bHasLevel);
	}
	if (LevelText)
	{
		LevelText->SetText(FText::Format(LOCTEXT("ProfileProgress", "{0} to level {1}"), FText::AsPercent(FMath::Clamp(Profile.XPProgress, 0.f, 1.f)), FText::AsNumber(Profile.Level + 1)));
	}

	// ---- Stats, or why there are none
	SetShown(Spinner, bLoading);
	SetShown(StatusText, !bLoading && !bHasProfile);
	SetShown(StatGrid, bHasProfile);
	if (Theme)
	{
		RebuildStats(*Theme);
	}
	RefreshButtons();
}

void UALKProfileDialog::RebuildStats(const UALKTheme& Theme)
{
	using namespace ALKProfileDialogPrivate;
	if (!StatGrid || !WidgetTree)
	{
		return;
	}
	StatGrid->ClearChildren();
	if (!bHasProfile)
	{
		return;
	}

	UHorizontalBox* Line = nullptr;
	int32 InLine = 0;
	for (const FALKProfileStat& Stat : Profile.Stats)
	{
		if (!Line || InLine >= TilesPerLine)
		{
			Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			FALKWidgetBuilder::AddToVerticalBox(StatGrid, Line, FMargin(0.f, 0.f, 0.f, 12.f));
			InLine = 0;
		}
		// A tile: the stat's name over its value.
		UALKFrame* Tile = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
		Tile->SetPadding(FMargin(18.f, 12.f, 18.f, 14.f));
		FALKFrameStyle TileFrame = Theme.MakeCardFrame(0.f);
		TileFrame.Shadow = FLinearColor::Transparent;
		Tile->SetFrameStyle(TileFrame);
		UVerticalBox* TileColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Tile->SetContent(TileColumn);
		UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelText->SetText(Stat.Label);
		FALKWidgetBuilder::StyleText(LabelText, Theme, EALKTextRole::Data, Theme.TextMuted);
		LabelText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		FALKWidgetBuilder::AddToVerticalBox(TileColumn, LabelText, FMargin(), HAlign_Left);
		UTextBlock* ValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ValueText->SetText(Stat.Value);
		FALKWidgetBuilder::StyleText(ValueText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
		FSlateFontInfo ValueFont = Theme.SubheadingFont;
		ValueFont.Size = FMath::RoundToFloat(Theme.SubheadingFont.Size * 1.15f);
		FALKWidgetBuilder::SetTextFont(ValueText, ValueFont);
		FALKWidgetBuilder::AddToVerticalBox(TileColumn, ValueText, FMargin(0.f, 4.f, 0.f, 0.f), HAlign_Left);

		FALKWidgetBuilder::AddToHorizontalBox(Line, Tile, FMargin(InLine > 0 ? 12.f : 0.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
		++InLine;
	}
	// The last line is filled up, so its tiles are as wide as the others.
	for (; Line && InLine < TilesPerLine; ++InLine)
	{
		FALKWidgetBuilder::AddToHorizontalBox(Line, WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()), FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	}
}

void UALKProfileDialog::RefreshButtons()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	// Nothing to do to yourself: the local player's own card only closes.
	const bool bOther = Online && !UserId.IsEmpty() && !IsLocalUser();
	const ESlateVisibility ActionVisibility = bOther ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (InviteButton)
	{
		InviteButton->SetVisibility(ActionVisibility);
		InviteButton->SetIsEnabled(bOther && Online->IsOnlineEnabled());
	}
	if (MuteButton)
	{
		const bool bMuted = bOther && Online->IsPlayerMuted(UserId);
		MuteButton->SetVisibility(ActionVisibility);
		MuteButton->SetButtonText(bMuted ? LOCTEXT("ProfileUnmute", "Unmute") : LOCTEXT("ProfileMute", "Mute"));
		MuteButton->SetLeadingIcon(bMuted ? EALKIcon::Speaker : EALKIcon::SpeakerMuted);
	}
	if (ReportButton)
	{
		ReportButton->SetVisibility(ActionVisibility);
	}
}

UWidget* UALKProfileDialog::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	// Close is the safe place to start: a stray Enter does nothing to the player.
	return CloseButton ? static_cast<UWidget*>(CloseButton.Get()) : Super::NativeGetDesiredFocusTarget();
}

void UALKProfileDialog::InviteToParty()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || UserId.IsEmpty() || IsLocalUser())
	{
		return;
	}
	Online->InviteToParty(UserId);
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->ShowNotice(LOCTEXT("ProfileInviteSent", "Party invite sent"), FText::FromString(ResolveShownName()));
	}
}

void UALKProfileDialog::ToggleMute()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || UserId.IsEmpty() || IsLocalUser())
	{
		return;
	}
	Online->SetPlayerMuted(UserId, !Online->IsPlayerMuted(UserId));
	RefreshButtons();
}

void UALKProfileDialog::ReportPlayer()
{
	if (UserId.IsEmpty() || IsLocalUser())
	{
		return;
	}
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->ShowReport(UserId, ResolveShownName());
	}
}

#undef LOCTEXT_NAMESPACE
