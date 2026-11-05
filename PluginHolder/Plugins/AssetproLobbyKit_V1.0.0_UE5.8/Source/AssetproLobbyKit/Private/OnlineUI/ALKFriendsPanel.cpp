// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKFriendsPanel.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKFriendsPanel)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKFriendsPanelPrivate
{
	static constexpr int32 RowHeight = 60;
	static constexpr float AvatarSize = 40.f;
	/** Widest a line's button gets in a narrow column (Set Compact): a longer label shrinks the button. */
	static constexpr float CompactActionWidth = 150.f;

	/** A colour on a focused line: on a solid focus fill everything turns to the contrast colour. */
	static FLinearColor OnFocus(const UALKTheme& Theme, const FLinearColor& Rest, float Highlight)
	{
		return Theme.FocusStyle == EALKFocusStyle::Solid ? FMath::Lerp(Rest, Theme.AccentContrast, Highlight) : Rest;
	}

	static UALKButton* MakeButton(UWidgetTree& Tree, FName Name, const FText& Text, EALKButtonStyle ButtonStyle)
	{
		TSubclassOf<UALKButton> ButtonClass = UALKDeveloperSettings::GetMenuConfig()->ButtonClass;
		if (!ButtonClass)
		{
			ButtonClass = UALKButton::StaticClass();
		}
		UALKButton* Button = Tree.ConstructWidget<UALKButton>(ButtonClass, Name);
		Button->SetButtonStyleType(ButtonStyle);
		Button->SetButtonText(Text);
		return Button;
	}

	static bool IsFocusedWidget(const UWidget* Widget)
	{
		return Widget && (Widget->HasAnyUserFocus() || Widget->HasFocusedDescendants());
	}

	static FLinearColor GetPresenceColor(const UALKTheme& Theme, EALKPresence Presence)
	{
		switch (Presence)
		{
		case EALKPresence::Online: return Theme.Success;
		case EALKPresence::InLobby: return Theme.Accent;
		case EALKPresence::InGame: return Theme.AccentAlt;
		default: return Theme.TextMuted;
		}
	}

	/** The status line of a friend whose backend gave none. */
	static FText GetPresenceText(EALKPresence Presence)
	{
		switch (Presence)
		{
		case EALKPresence::Online: return LOCTEXT("PresenceOnline", "Online");
		case EALKPresence::InLobby: return LOCTEXT("PresenceInLobby", "In a lobby");
		case EALKPresence::InGame: return LOCTEXT("PresenceInGame", "In a match");
		default: return LOCTEXT("PresenceOffline", "Offline");
		}
	}
}

// ================================================================================================ One line

UALKFriendRow::UALKFriendRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A list line: the item frame when focused.
	ButtonStyleType = EALKButtonStyle::Primary;
}

void UALKFriendRow::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKFriendsPanelPrivate;

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	// Little padding at the right: the texts are cut off where their column ends, and a status that just fits the line
	// (a line without a button) keeps the room it had.
	Frame->SetPadding(FMargin(10.f, 4.f, 4.f, 6.f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Center);
	Background = Frame;
	Tree.RootWidget = Frame;

	UHorizontalBox* RowBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("RowBox"));
	Frame->SetContent(RowBox);

	FriendAvatar = FALKWidgetBuilder::Make<UALKAvatar>(Tree, TEXT("FriendAvatar"));
	FriendAvatar->Diameter = AvatarSize;
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, FriendAvatar, FMargin(2.f, 0.f, 14.f, 0.f), HAlign_Left, VAlign_Center);

	// Long names and status lines end in an ellipsis instead of running under the button beside the line. Slate puts
	// the ellipsis where a text is clipped, so the column clips to its bounds; the room over the name (taken from the
	// frame's padding) keeps accents on capitals inside them.
	UVerticalBox* Texts = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("RowTexts"));
	Texts->SetClipping(EWidgetClipping::ClipToBounds);
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	UALKTitleText* NameText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("Label"));
	NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	Label = NameText;
	FALKWidgetBuilder::AddToVerticalBox(Texts, Label, FMargin(0.f, 2.f, 0.f, 0.f));
	StatusText = FALKWidgetBuilder::MakeText(Tree, TEXT("StatusText"));
	StatusText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToVerticalBox(Texts, StatusText, FMargin(0.f, 1.f, 0.f, 0.f));
}

void UALKFriendRow::SetFriend(const FALKFriend& InFriend, const FString& InShownName, bool bInAnonymous)
{
	Friend = InFriend;
	ShownName = InShownName;
	bAnonymous = bInAnonymous;
	RefreshContent();
}

void UALKFriendRow::RefreshContent()
{
	SetButtonText(FText::FromString(ShownName));
	if (FriendAvatar)
	{
		FriendAvatar->SetAnonymous(bAnonymous);
		FriendAvatar->SetPlayer(ShownName, bAnonymous ? nullptr : Friend.Avatar.LoadSynchronous());
	}
	if (StatusText)
	{
		StatusText->SetText(Friend.StatusText.IsEmpty() ? ALKFriendsPanelPrivate::GetPresenceText(Friend.Presence) : Friend.StatusText);
	}
	if (const UALKTheme* Theme = UALKDeveloperSettings::GetTheme())
	{
		NativeApplyVisualState(*Theme, GetHighlight(), IsUsable());
	}
}

void UALKFriendRow::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	// As tall as a list line, as wide as the list.
	SetMinDimensions(0, ALKFriendsPanelPrivate::RowHeight);
	if (Label)
	{
		FSlateFontInfo NameFont = Theme.SubheadingFont;
		NameFont.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.SubheadingFont.Size * 0.82f));
		FALKWidgetBuilder::SetTextFont(Label, NameFont);
		Label->SetTextTransformPolicy(ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(StatusText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	RefreshContent();
}

void UALKFriendRow::NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled)
{
	using namespace ALKFriendsPanelPrivate;
	Super::NativeApplyVisualState(Theme, Highlight, bEnabled);

	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		Frame->SetFrameStyle(Theme.MakeItemFrame(Highlight, false));
	}
	if (Label)
	{
		// Friends who are offline step back.
		const FLinearColor Rest = Friend.Presence == EALKPresence::Offline ? Theme.TextMuted : Theme.TextPrimary;
		Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(Rest, Theme.GetFocusTextColor(), Highlight)));
	}
	if (StatusText)
	{
		StatusText->SetColorAndOpacity(FSlateColor(OnFocus(Theme, Theme.TextMuted, Highlight)));
	}
	if (FriendAvatar)
	{
		// The presence dot is cut out of the picture in the colour behind it.
		FriendAvatar->SetBackColor(Theme.FocusStyle == EALKFocusStyle::Solid ? FMath::Lerp(Theme.Panel, Theme.Accent, Highlight) : Theme.Panel);
		// "In a lobby" is the accent colour, which a solid focus fill would swallow.
		const FLinearColor Presence = GetPresenceColor(Theme, Friend.Presence);
		FriendAvatar->SetStatusColor(Friend.Presence == EALKPresence::InLobby ? OnFocus(Theme, Presence, Highlight) : Presence);
	}
}

// ================================================================================================ The panel

void UALKFriendsPanel::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKFriendsPanelPrivate;

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Frame->SetPadding(FMargin(20.f, 14.f, 20.f, 18.f));
	Panel = Frame;
	Tree.RootWidget = Frame;

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Frame->SetContent(Column);

	// "Friends" with the two tabs at its right.
	UHorizontalBox* Header = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Header"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Header, FMargin(4.f, 0.f, 0.f, 10.f));
	TitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("TitleText"), LOCTEXT("FriendsTitle", "Friends"));
	// As tall as the tabs, so the header keeps its height when they are hidden.
	FALKWidgetBuilder::AddToHorizontalBox(Header, FALKWidgetBuilder::WrapInSizeBox(Tree, TitleText, 0.f, 44.f, true), FMargin(), HAlign_Left, VAlign_Center, 1.f);
	if (USizeBoxSlot* TitleSlot = Cast<USizeBoxSlot>(TitleText->Slot))
	{
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	TabBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabBox"));
	FALKWidgetBuilder::AddToHorizontalBox(Header, TabBox, FMargin(), HAlign_Right, VAlign_Center);
	OnlineTab = MakeButton(Tree, TEXT("OnlineTab"), FText::GetEmpty(), EALKButtonStyle::Tab);
	RecentTab = MakeButton(Tree, TEXT("RecentTab"), LOCTEXT("FriendsTabRecent", "Recent"), EALKButtonStyle::Tab);
	for (UALKButton* Tab : { OnlineTab.Get(), RecentTab.Get() })
	{
		// Tabs are for the mouse; screens give them keys (Show Recent Players).
		Tab->SetIsFocusable(false);
		Tab->bFocusOnHover = false;
		Tab->bPlayClickSound = false;
	}
	FALKWidgetBuilder::AddToHorizontalBox(TabBox, OnlineTab);
	FALKWidgetBuilder::AddToHorizontalBox(TabBox, RecentTab, FMargin(6.f, 0.f, 0.f, 0.f));

	// The list, with a friendly line over it while it is empty.
	UOverlay* ListArea = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("ListArea"));
	FALKWidgetBuilder::AddToVerticalBox(Column, ListArea, FMargin(-6.f, 0.f, -12.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	FriendList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("FriendList"));
	FriendList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToOverlay(ListArea, FriendList);
	EmptyText = FALKWidgetBuilder::MakeText(Tree, TEXT("EmptyText"));
	EmptyText->SetAutoWrapText(true);
	EmptyText->SetJustification(ETextJustify::Center);
	EmptyText->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(ListArea, FALKWidgetBuilder::MakeEmptyState(Tree, TEXT("EmptyState"), EALKIcon::Users, EmptyText), HAlign_Fill, VAlign_Center, FMargin(24.f, 18.f, 30.f, 18.f));

	AllFriendsButton = MakeButton(Tree, TEXT("AllFriendsButton"), FText::GetEmpty(), EALKButtonStyle::Ghost);
	AllFriendsButton->SetLeadingIcon(EALKIcon::Users);
	AllFriendsButton->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, AllFriendsButton, FMargin(0.f, 6.f, 0.f, 0.f));
}

void UALKFriendsPanel::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (OnlineTab)
	{
		OnlineTab->OnClicked().AddUObject(this, &ThisClass::ShowRecentPlayers, false);
	}
	if (RecentTab)
	{
		RecentTab->OnClicked().AddUObject(this, &ThisClass::ShowRecentPlayers, true);
	}
	if (AllFriendsButton)
	{
		AllFriendsButton->OnClicked().AddUObject(this, &ThisClass::HandleAllFriendsClicked);
	}
}

void UALKFriendsPanel::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (UALKFrame* Frame = Cast<UALKFrame>(Panel))
	{
		Frame->SetFrameStyle(Theme.MakePanelFrame());
	}
	FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
	if (TitleText)
	{
		TitleText->SetTextTransformPolicy(Theme.bUppercaseTitles ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(EmptyText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleEmptyState(EmptyText, Theme);
	if (EmptyText)
	{
		EmptyText->SetLineHeightPercentage(1.25f);
	}
	StyleList(Theme);
}

void UALKFriendsPanel::StyleList(const UALKTheme& Theme)
{
	if (!FriendList)
	{
		return;
	}
	// A thin accent thumb, as on every list of the kit.
	FScrollBarStyle BarStyle = FriendList->GetWidgetBarStyle();
	const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
	BarStyle.SetNormalThumbImage(Thumb);
	BarStyle.SetHoveredThumbImage(Thumb);
	BarStyle.SetDraggedThumbImage(Thumb);
	BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
	BarStyle.SetVerticalTopSlotImage(FSlateNoResource());
	BarStyle.SetVerticalBottomSlotImage(FSlateNoResource());
	FriendList->SetWidgetBarStyle(BarStyle);
	FriendList->SetScrollbarThickness(FVector2D(3.0, 3.0));
	FriendList->SetScrollbarPadding(FMargin(2.f, 0.f, 0.f, 0.f));
}

void UALKFriendsPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!FriendsHandle.IsValid())
		{
			FriendsHandle = Online->OnFriendsChangedNative.AddUObject(this, &ThisClass::RefreshFriends);
			// Who can still be invited depends on the party and on being in a game.
			PartyHandle = Online->OnPartyChangedNative.AddUObject(this, &ThisClass::RefreshFriends);
			StateHandle = Online->OnStateChangedNative.AddWeakLambda(this, [this](EALKOnlineState, EALKOnlineState) { RefreshFriends(); });
		}
		// Shown: ask for a fresh list. The one read last is listed until it arrives.
		Online->QueryFriends();
	}
	RefreshFriends();
}

void UALKFriendsPanel::NativeDestruct()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnFriendsChangedNative.Remove(FriendsHandle);
		Online->OnPartyChangedNative.Remove(PartyHandle);
		Online->OnStateChangedNative.Remove(StateHandle);
	}
	FriendsHandle.Reset();
	PartyHandle.Reset();
	StateHandle.Reset();
	PendingFocus.Reset();
	Super::NativeDestruct();
}

void UALKFriendsPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// A line that was just made takes the focus once it has a place on screen.
	if (UWidget* Target = PendingFocus.Get())
	{
		if (!Target->IsVisible())
		{
			PendingFocus.Reset();
		}
		else if (Target->GetCachedGeometry().GetLocalSize().X > 1.0)
		{
			PendingFocus.Reset();
			Target->SetFocus();
		}
	}
}

void UALKFriendsPanel::SetCompact(bool bInCompact)
{
	if (bCompact != bInCompact)
	{
		bCompact = bInCompact;
		RefreshFriends();
	}
}

void UALKFriendsPanel::ShowRecentPlayers(bool bInShowRecent)
{
	if (bShowRecent != bInShowRecent && !bCompact)
	{
		bShowRecent = bInShowRecent;
		UALKTheme::PlayUISound(this, EALKUISound::Tab);
		if (FriendList)
		{
			FriendList->SetScrollOffset(0.f);
		}
		RefreshFriends();
	}
}

void UALKFriendsPanel::EnsureLines(int32 Count)
{
	using namespace ALKFriendsPanelPrivate;
	while (Rows.Num() < Count && FriendList && WidgetTree)
	{
		const int32 Index = Rows.Num();
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UALKFriendRow* Row = WidgetTree->ConstructWidget<UALKFriendRow>(UALKFriendRow::StaticClass());
		UALKButton* Action = MakeButton(*WidgetTree, NAME_None, FText::GetEmpty(), EALKButtonStyle::Secondary);
		Row->OnClicked().AddUObject(this, &ThisClass::HandleRowClicked, Index);
		Action->OnClicked().AddUObject(this, &ThisClass::HandleActionClicked, Index);
		FALKWidgetBuilder::AddToHorizontalBox(Line, Row, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);
		// The button sits in a box that can cap its width (Refresh Friends, for a narrow column): a long label, such as a
		// translation of "Join", then shrinks the button instead of squeezing the friend's name out of the line.
		UScaleBox* ActionFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		ActionFit->SetStretch(EStretch::ScaleToFit);
		ActionFit->SetStretchDirection(EStretchDirection::DownOnly);
		ActionFit->AddChild(Action);
		USizeBox* ActionSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ActionSize->AddChild(ActionFit);
		FALKWidgetBuilder::AddToHorizontalBox(Line, ActionSize, FMargin(8.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
		FriendList->AddChild(Line);
		if (UScrollBoxSlot* LineSlot = Cast<UScrollBoxSlot>(Line->Slot))
		{
			// Room at the right and under the line for a focused line's shadow.
			LineSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 4.f));
			LineSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Rows.Add(Row);
		ActionButtons.Add(Action);
		Lines.Add(Line);
	}
}

void UALKFriendsPanel::RefreshFriends()
{
	using namespace ALKFriendsPanelPrivate;
	if (!FriendList)
	{
		return;
	}

	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const bool bOnline = Online && Online->IsOnlineEnabled();
	const TArray<FALKFriend> Friends = bOnline ? Online->GetFriends() : TArray<FALKFriend>();
	OnlineCount = 0;
	for (const FALKFriend& Each : Friends)
	{
		OnlineCount += Each.Presence != EALKPresence::Offline ? 1 : 0;
	}

	const bool bRecent = bShowRecent && !bCompact;
	TArray<FALKFriend> Shown = bRecent ? (bOnline ? Online->GetRecentPlayers() : TArray<FALKFriend>()) : Friends;
	if (bCompact)
	{
		// A side column lists who is around, and only a few of them.
		Shown.RemoveAll([](const FALKFriend& Each) { return Each.Presence == EALKPresence::Offline; });
		Shown.SetNum(FMath::Min(Shown.Num(), FMath::Max(1, CompactRows)));
	}

	// Who cannot be invited to the party any more: its members, and everyone once it is full.
	TArray<FString> PartyIds;
	bool bPartyFull = false;
	if (bOnline)
	{
		const TArray<FALKPartyMember> Members = Online->GetPartyMembers();
		for (const FALKPartyMember& Member : Members)
		{
			PartyIds.Add(Member.UserId);
		}
		bPartyFull = Members.Num() >= Online->GetPartyMaxSize();
	}
	const bool bInGame = bOnline && Online->IsInGame();
	const bool bStreamer = bOnline && Online->IsStreamerMode();

	// The line and button that have the focus, in case they go away.
	int32 FocusedLine = INDEX_NONE;
	bool bFocusOnAction = false;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (IsFocusedWidget(ActionButtons[Index]))
		{
			FocusedLine = Index;
			bFocusOnAction = true;
		}
		else if (IsFocusedWidget(Rows[Index]))
		{
			FocusedLine = Index;
		}
	}
	const bool bFocusOnAllFriends = IsFocusedWidget(AllFriendsButton);

	EnsureLines(Shown.Num());
	ShownCount = FMath::Min(Shown.Num(), Rows.Num());
	Actions.SetNum(Rows.Num());
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const bool bUsed = Index < ShownCount;
		Lines[Index]->SetVisibility(bUsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		Actions[Index] = EFriendAction::None;
		if (!bUsed)
		{
			continue;
		}
		const FALKFriend& Each = Shown[Index];
		Rows[Index]->SetFriend(Each, Online->GetDisplayName(Each.DisplayName, Index), bStreamer);

		// One button: join their game when that is possible, else invite them.
		if (Each.bJoinable)
		{
			Actions[Index] = EFriendAction::Join;
		}
		else if (Each.bInvitable && bInGame)
		{
			Actions[Index] = EFriendAction::InviteToGame;
		}
		else if (Each.bInvitable && !bPartyFull && !PartyIds.Contains(Each.UserId))
		{
			Actions[Index] = EFriendAction::InviteToParty;
		}
		UALKButton* Action = ActionButtons[Index];
		Action->SetButtonText(Actions[Index] == EFriendAction::Join ? LOCTEXT("FriendJoin", "Join") : LOCTEXT("FriendInvite", "Invite"));
		Action->SetVisibility(Actions[Index] == EFriendAction::None ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		// The box round the button goes with it (an empty box would still take the gap beside the line), and caps the
		// button's width in a narrow column.
		UWidget* ActionFit = Action->GetParent();
		if (USizeBox* ActionSize = ActionFit ? Cast<USizeBox>(ActionFit->GetParent()) : nullptr)
		{
			ActionSize->SetVisibility(Actions[Index] == EFriendAction::None ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
			if (bCompact)
			{
				ActionSize->SetMaxDesiredWidth(CompactActionWidth);
			}
			else
			{
				ActionSize->ClearMaxDesiredWidth();
			}
		}
	}

	if (TabBox)
	{
		TabBox->SetVisibility(bCompact ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (OnlineTab)
	{
		OnlineTab->SetButtonText(FText::Format(LOCTEXT("FriendsTabOnline", "Online {0}"), FText::AsNumber(OnlineCount)));
		OnlineTab->SetActiveVisual(!bRecent);
	}
	if (RecentTab)
	{
		RecentTab->SetActiveVisual(bRecent);
	}
	if (AllFriendsButton)
	{
		AllFriendsButton->SetButtonText(FText::Format(LOCTEXT("FriendsAll", "All friends ({0})"), FText::AsNumber(Friends.Num())));
		AllFriendsButton->SetVisibility(bCompact && bOnline ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (EmptyText)
	{
		FText Empty;
		if (!bOnline)
		{
			Empty = LOCTEXT("FriendsOffline", "Online play is turned off.");
		}
		else if (bRecent)
		{
			Empty = LOCTEXT("FriendsRecentEmpty", "Players you meet in lobbies and matches show up here.");
		}
		else if (bCompact)
		{
			Empty = LOCTEXT("FriendsNoneOnline", "None of your friends are online right now.");
		}
		else
		{
			Empty = LOCTEXT("FriendsEmpty", "No friends here yet. Friends from your platform show up in this list.");
		}
		EmptyText->SetText(Empty);
		FALKWidgetBuilder::SetEmptyStateShown(EmptyText, ShownCount == 0);
	}

	// Keep the focus on something that is still there.
	if (FocusedLine != INDEX_NONE)
	{
		if (FocusedLine >= ShownCount)
		{
			PendingFocus = ShownCount > 0 ? static_cast<UWidget*>(Rows[ShownCount - 1].Get()) : GetFirstFocusTarget();
		}
		else if (bFocusOnAction && Actions[FocusedLine] == EFriendAction::None)
		{
			PendingFocus = Rows[FocusedLine].Get();
		}
	}
	else if (bFocusOnAllFriends && AllFriendsButton && !AllFriendsButton->IsVisible())
	{
		PendingFocus = GetFirstFocusTarget();
	}
}

UWidget* UALKFriendsPanel::GetFirstFocusTarget() const
{
	if (ShownCount > 0 && Rows.Num() > 0)
	{
		return Rows[0].Get();
	}
	return AllFriendsButton && AllFriendsButton->GetVisibility() == ESlateVisibility::Visible ? AllFriendsButton.Get() : nullptr;
}

bool UALKFriendsPanel::FocusFirst()
{
	UWidget* Target = GetFirstFocusTarget();
	if (!Target)
	{
		return false;
	}
	if (Target->GetCachedGeometry().GetLocalSize().X > 1.0)
	{
		Target->SetFocus();
	}
	else
	{
		PendingFocus = Target;
	}
	return true;
}

void UALKFriendsPanel::HandleRowClicked(int32 Index)
{
	if (Index < ShownCount && Rows.IsValidIndex(Index))
	{
		if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
		{
			Flow->ShowPlayerOptions(Rows[Index]->GetFriend().UserId, Rows[Index]->GetShownName());
		}
	}
}

void UALKFriendsPanel::HandleActionClicked(int32 Index)
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || Index >= ShownCount || !Rows.IsValidIndex(Index) || !Actions.IsValidIndex(Index))
	{
		return;
	}
	const FString UserId = Rows[Index]->GetFriend().UserId;
	const FText ShownName = FText::FromString(Rows[Index]->GetShownName());
	UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	switch (Actions[Index])
	{
	case EFriendAction::Join:
		Online->JoinFriend(UserId);
		break;
	case EFriendAction::InviteToGame:
		Online->InviteFriendToGame(UserId);
		if (Flow)
		{
			Flow->ShowNotice(FText::Format(LOCTEXT("FriendInvitedGame", "Game invite sent to {0}"), ShownName), FText::GetEmpty());
		}
		break;
	case EFriendAction::InviteToParty:
		Online->InviteToParty(UserId);
		if (Flow)
		{
			Flow->ShowNotice(FText::Format(LOCTEXT("FriendInvitedParty", "Party invite sent to {0}"), ShownName), FText::GetEmpty());
		}
		break;
	default:
		break;
	}
}

void UALKFriendsPanel::HandleAllFriendsClicked()
{
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->OpenSocial();
	}
}

#undef LOCTEXT_NAMESPACE
