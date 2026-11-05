// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKChatPanel.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTextField.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKChatPanel)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKChatPanelPrivate
{
	/** How often the panel looks at the settings it follows (text chat, filter, streamer mode) and the game's teams. */
	static constexpr float PollInterval = 0.5f;
	/** A whisper is asked for from a dialog that is still closing: the message box takes the keyboard this much later. */
	static constexpr float WhisperFocusDelay = 0.25f;

	static const EALKChatChannel TabOrder[] = { EALKChatChannel::All, EALKChatChannel::Team, EALKChatChannel::Party, EALKChatChannel::Whisper };
	static constexpr int32 TabCount = UE_ARRAY_COUNT(TabOrder);

	/** "21:04" in the player's own time zone. */
	static FText FormatTime(const FDateTime& UtcTime)
	{
		const FDateTime Local = UtcTime + (FDateTime::Now() - FDateTime::UtcNow());
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Local.GetHour(), Local.GetMinute()));
	}

	static FText GetChannelTag(EALKChatChannel InChannel)
	{
		switch (InChannel)
		{
		case EALKChatChannel::Team: return LOCTEXT("ChatTagTeam", "Team");
		case EALKChatChannel::Party: return LOCTEXT("ChatTagParty", "Party");
		case EALKChatChannel::Whisper: return LOCTEXT("ChatTagWhisper", "Whisper");
		default: return LOCTEXT("ChatTagAll", "All");
		}
	}
}

FALKOnWhisperRequested& UALKChatPanel::OnWhisperRequested()
{
	static FALKOnWhisperRequested Delegate;
	return Delegate;
}

UALKButton* UALKChatPanel::MakeTab(UWidgetTree& Tree, FName Name, const FText& Text)
{
	TSubclassOf<UALKButton> ButtonClass = GetMenuConfig()->ButtonClass;
	if (!ButtonClass)
	{
		ButtonClass = UALKButton::StaticClass();
	}
	UALKButton* Tab = Tree.ConstructWidget<UALKButton>(ButtonClass, Name);
	Tab->SetButtonStyleType(EALKButtonStyle::Tab);
	Tab->SetButtonText(Text);
	// A tab plays the Tab sound when the channel changes.
	Tab->bPlayClickSound = false;
	return Tab;
}

void UALKChatPanel::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ChatColumn"));
	Tree.RootWidget = Column;

	// The channel tabs on the left, the filter note on the right.
	UHorizontalBox* Header = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ChatHeader"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Header);
	TabRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabRow"));
	FALKWidgetBuilder::AddToHorizontalBox(Header, TabRow, FMargin(), HAlign_Left, VAlign_Center, 1.f);

	AllTab = MakeTab(Tree, TEXT("AllTab"), LOCTEXT("ChatTabAll", "All"));
	TeamTab = MakeTab(Tree, TEXT("TeamTab"), LOCTEXT("ChatTabTeam", "Team"));
	PartyTab = MakeTab(Tree, TEXT("PartyTab"), LOCTEXT("ChatTabParty", "Party"));
	WhisperTab = MakeTab(Tree, TEXT("WhisperTab"), LOCTEXT("ChatTabWhisper", "Whisper"));
	for (UALKButton* Tab : { AllTab.Get(), TeamTab.Get(), PartyTab.Get(), WhisperTab.Get() })
	{
		FALKWidgetBuilder::AddToHorizontalBox(TabRow, Tab, FMargin(0.f, 0.f, 6.f, 0.f), HAlign_Left, VAlign_Center);
	}

	FilterNoteText = FALKWidgetBuilder::MakeText(Tree, TEXT("FilterNoteText"), LOCTEXT("ChatFilterOn", "Chat filter on"));
	FALKWidgetBuilder::AddToHorizontalBox(Header, FilterNoteText, FMargin(12.f, 0.f, 4.f, 0.f), HAlign_Right, VAlign_Center);

	HeaderRule = FALKWidgetBuilder::Make<UALKRule>(Tree, TEXT("HeaderRule"));
	FALKWidgetBuilder::AddToVerticalBox(Column, HeaderRule, FMargin(0.f, 12.f, 0.f, 0.f));

	// The lines, with one muted line in their place while there are none.
	UOverlay* Body = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("ChatBody"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Body, FMargin(0.f, 12.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	MessageList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("MessageList"));
	MessageList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToOverlay(Body, MessageList);
	EmptyText = FALKWidgetBuilder::MakeText(Tree, TEXT("EmptyText"));
	EmptyText->SetJustification(ETextJustify::Center);
	EmptyText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToOverlay(Body, FALKWidgetBuilder::MakeEmptyState(Tree, TEXT("EmptyState"), EALKIcon::More, EmptyText, 44.f), HAlign_Fill, VAlign_Center, FMargin(24.f, 0.f));

	// The message box, with the key that sends at its end.
	InputRow = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("InputRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, InputRow, FMargin(0.f, 10.f, 0.f, 0.f));
	InputField = Tree.ConstructWidget<UALKTextField>(UALKTextField::StaticClass(), TEXT("InputField"));
	FALKWidgetBuilder::AddToOverlay(InputRow, InputField);
	SendKeyChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("SendKeyChip"), LOCTEXT("KeyEnter", "Enter"), EALKIcon::None);
	SendKeyChip->SetVisibility(ESlateVisibility::HitTestInvisible);
	FALKWidgetBuilder::AddToOverlay(InputRow, SendKeyChip, HAlign_Right, VAlign_Center, FMargin(0.f, 0.f, 10.f, 0.f));
}

void UALKChatPanel::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	for (const EALKChatChannel Each : ALKChatPanelPrivate::TabOrder)
	{
		if (UALKButton* Tab = GetTab(Each))
		{
			Tab->OnClicked().AddUObject(this, &ThisClass::HandleTabClicked, Each);
		}
	}
	if (InputField)
	{
		InputField->SetMaxLength(UALKDeveloperSettings::GetOnlineConfig()->ChatMaxLength);
		InputField->OnTextCommittedNative.AddUObject(this, &ThisClass::HandleInputCommitted);
	}
}

void UALKChatPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!ChatHandle.IsValid())
		{
			ChatHandle = Online->OnChatMessageNative.AddUObject(this, &ThisClass::HandleChatMessage);
		}
	}
	if (!WhisperHandle.IsValid())
	{
		WhisperHandle = OnWhisperRequested().AddUObject(this, &ThisClass::HandleWhisperRequested);
	}
	// The theme was applied before the events were bound: show what arrived until now.
	RefreshState(true);
}

void UALKChatPanel::NativeDestruct()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnChatMessageNative.Remove(ChatHandle);
	}
	ChatHandle.Reset();
	OnWhisperRequested().Remove(WhisperHandle);
	WhisperHandle.Reset();
	FocusBeforeTyping.Reset();
	FocusDelay = -1.f;
	Super::NativeDestruct();
}

void UALKChatPanel::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	FALKWidgetBuilder::StyleText(FilterNoteText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(EmptyText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleEmptyState(EmptyText, Theme);
	if (HeaderRule)
	{
		HeaderRule->SetRuleColor(Theme.Separator);
	}
	FALKWidgetBuilder::StyleChip(SendKeyChip, Theme, Theme.Panel, Theme.PanelOutline, Theme.TextMuted);
	if (MessageList)
	{
		FScrollBarStyle BarStyle = MessageList->GetWidgetBarStyle();
		const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
		BarStyle.SetNormalThumbImage(Thumb);
		BarStyle.SetHoveredThumbImage(Thumb);
		BarStyle.SetDraggedThumbImage(Thumb);
		BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
		MessageList->SetWidgetBarStyle(BarStyle);
		MessageList->SetScrollbarThickness(FVector2D(3.0, 3.0));
		MessageList->SetScrollbarPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	}
	// The lines are made at runtime in the theme's fonts and colours: make them again.
	RefreshState(true);
}

// ---------------------------------------------------------------- Channels

UALKButton* UALKChatPanel::GetTab(EALKChatChannel InChannel) const
{
	switch (InChannel)
	{
	case EALKChatChannel::All: return AllTab;
	case EALKChatChannel::Team: return TeamTab;
	case EALKChatChannel::Party: return PartyTab;
	case EALKChatChannel::Whisper: return WhisperTab;
	default: return nullptr;
	}
}

bool UALKChatPanel::IsChannelAvailable(EALKChatChannel InChannel) const
{
	switch (InChannel)
	{
	case EALKChatChannel::All:
	case EALKChatChannel::Party:
		return true;
	case EALKChatChannel::Team:
		return bHasTeams;
	case EALKChatChannel::Whisper:
		return !WhisperTargetId.IsEmpty();
	default:
		return false;
	}
}

void UALKChatPanel::SetChannel(EALKChatChannel InChannel)
{
	const EALKChatChannel NewChannel = IsChannelAvailable(InChannel) ? InChannel : EALKChatChannel::All;
	const bool bChanged = NewChannel != Channel;
	Channel = NewChannel;
	UpdateTabs();
	if (bChanged)
	{
		RefreshMessages();
	}
}

void UALKChatPanel::ShowNextChannel()
{
	const int32 Count = ALKChatPanelPrivate::TabCount;
	int32 Index = 0;
	for (int32 Each = 0; Each < Count; ++Each)
	{
		if (ALKChatPanelPrivate::TabOrder[Each] == Channel)
		{
			Index = Each;
		}
	}
	for (int32 Step = 1; Step <= Count; ++Step)
	{
		const EALKChatChannel Next = ALKChatPanelPrivate::TabOrder[(Index + Step) % Count];
		if (IsChannelAvailable(Next))
		{
			if (Next != Channel)
			{
				UALKTheme::PlayUISound(this, EALKUISound::Tab);
				SetChannel(Next);
			}
			return;
		}
	}
}

void UALKChatPanel::SetWhisperTarget(const FString& UserId, const FString& DisplayName)
{
	WhisperTargetId = UserId;
	WhisperTargetName = DisplayName;
	if (WhisperTargetId.IsEmpty())
	{
		ClearWhisperTarget();
		return;
	}
	Channel = EALKChatChannel::Whisper;
	UpdateTabs();
	RefreshMessages();
}

void UALKChatPanel::ClearWhisperTarget()
{
	WhisperTargetId.Reset();
	WhisperTargetName.Reset();
	if (Channel == EALKChatChannel::Whisper)
	{
		SetChannel(EALKChatChannel::All);
	}
	else
	{
		UpdateTabs();
	}
}

void UALKChatPanel::UpdateTabs()
{
	for (const EALKChatChannel Each : ALKChatPanelPrivate::TabOrder)
	{
		if (UALKButton* Tab = GetTab(Each))
		{
			Tab->SetVisibility(IsChannelAvailable(Each) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Tab->SetActiveVisual(Each == Channel);
		}
	}
	if (InputField)
	{
		FText Hint;
		switch (Channel)
		{
		case EALKChatChannel::Team: Hint = LOCTEXT("ChatHintTeam", "Message your team"); break;
		case EALKChatChannel::Party: Hint = LOCTEXT("ChatHintParty", "Message your party"); break;
		case EALKChatChannel::Whisper: Hint = FText::Format(LOCTEXT("ChatHintWhisper", "Whisper to {0}"), FText::FromString(WhisperTargetName)); break;
		default: Hint = LOCTEXT("ChatHintAll", "Message everyone"); break;
		}
		InputField->SetHintText(Hint);
	}
}

void UALKChatPanel::HandleTabClicked(EALKChatChannel InChannel)
{
	if (InChannel != Channel)
	{
		UALKTheme::PlayUISound(this, EALKUISound::Tab);
	}
	SetChannel(InChannel);
	// A channel is picked to write to it.
	FocusInput();
}

// ---------------------------------------------------------------- State

void UALKChatPanel::RefreshState(bool bForce)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UWorld* World = GetWorld();
	const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;

	const bool bNowEnabled = !Online || Online->IsTextChatEnabled();
	const bool bNowFilterOn = Online && Online->IsChatFilterEnabled();
	const bool bNowStreamerMode = Online && Online->IsStreamerMode();
	const bool bNowHasTeams = GameState && GameState->TeamCount > 0;
	if (!bForce && bNowEnabled == bChatEnabled && bNowFilterOn == bFilterOn && bNowStreamerMode == bStreamerMode && bNowHasTeams == bHasTeams)
	{
		return;
	}
	// Names change with streamer mode, and the whole list with the text chat switch.
	const bool bRebuild = bForce || bNowEnabled != bChatEnabled || bNowStreamerMode != bStreamerMode;
	bChatEnabled = bNowEnabled;
	bFilterOn = bNowFilterOn;
	bStreamerMode = bNowStreamerMode;
	bHasTeams = bNowHasTeams;

	const ESlateVisibility Shown = bChatEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (TabRow)
	{
		TabRow->SetVisibility(Shown);
	}
	if (InputRow)
	{
		InputRow->SetVisibility(Shown);
	}
	if (MessageList)
	{
		MessageList->SetVisibility(bChatEnabled ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (FilterNoteText)
	{
		FilterNoteText->SetVisibility(bChatEnabled && bFilterOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!IsChannelAvailable(Channel))
	{
		Channel = EALKChatChannel::All;
	}
	UpdateTabs();
	if (bRebuild)
	{
		RefreshMessages();
	}
	UpdateEmptyText();
}

void UALKChatPanel::UpdateEmptyText()
{
	if (!EmptyText)
	{
		return;
	}
	const bool bNoLines = !MessageList || MessageList->GetChildrenCount() == 0;
	EmptyText->SetText(bChatEnabled ? LOCTEXT("ChatEmpty", "No messages yet") : LOCTEXT("ChatOff", "Text chat is turned off. You can turn it on in Settings, under Multiplayer."));
	FALKWidgetBuilder::SetEmptyStateShown(EmptyText, !bChatEnabled || bNoLines);
}

// ---------------------------------------------------------------- Lines

bool UALKChatPanel::ShouldShow(const FALKChatMessage& Message) const
{
	// "All" is everything the player can read; the other tabs show their channel only.
	return Channel == EALKChatChannel::All || Message.Channel == Channel;
}

FText UALKChatPanel::GetSenderText(const FALKChatMessage& Message) const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const bool bOwn = Online && Message.SenderId == Online->GetLocalUserId();
	if (!bOwn)
	{
		return FText::FromString(Online ? Online->GetDisplayName(Message.SenderName, Message.SenderStableIndex) : Message.SenderName);
	}
	if (Message.Channel == EALKChatChannel::Whisper)
	{
		// The player's own whisper reads "To Mirela": who it went to.
		FString TargetName = Message.WhisperTargetId == WhisperTargetId ? WhisperTargetName : FString();
		const UWorld* World = GetWorld();
		const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
		if (const AALKLobbyPlayerState* Target = GameState ? GameState->FindPlayerByUserId(Message.WhisperTargetId) : nullptr)
		{
			TargetName = Online->GetDisplayName(Target->GetPlayerName(), Target->GetStableIndex());
		}
		if (!TargetName.IsEmpty())
		{
			return FText::Format(LOCTEXT("ChatWhisperTo", "To {0}"), FText::FromString(TargetName));
		}
	}
	// The player's own name is never hidden.
	return FText::FromString(Message.SenderName);
}

void UALKChatPanel::AddMessageRow(const FALKChatMessage& Message, const UALKTheme& Theme)
{
	if (!MessageList || !WidgetTree)
	{
		return;
	}
	UWidgetTree& Tree = *WidgetTree;
	const bool bSystem = Message.Channel == EALKChatChannel::System;

	UHorizontalBox* Row = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);

	UTextBlock* TimeText = FALKWidgetBuilder::MakeText(Tree, NAME_None, ALKChatPanelPrivate::FormatTime(Message.Time));
	FALKWidgetBuilder::StyleText(TimeText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::AddToHorizontalBox(Row, TimeText, FMargin(0.f, 3.f, 14.f, 0.f), HAlign_Left, VAlign_Top);

	if (!bSystem)
	{
		// The channel in its colour: the sender's team for Team, the accent for Party.
		FLinearColor ChannelColor = Theme.TextMuted;
		if (Message.Channel == EALKChatChannel::Team && Message.TeamId >= 0)
		{
			ChannelColor = UALKDeveloperSettings::GetOnlineConfig()->GetTeamColor(Message.TeamId);
		}
		else if (Message.Channel == EALKChatChannel::Party)
		{
			ChannelColor = Theme.GetAccentTextColor();
		}
		else if (Message.Channel == EALKChatChannel::Whisper)
		{
			ChannelColor = Theme.AccentAlt;
		}

		UTextBlock* TagText = FALKWidgetBuilder::MakeText(Tree, NAME_None, ALKChatPanelPrivate::GetChannelTag(Message.Channel));
		FALKWidgetBuilder::StyleText(TagText, Theme, EALKTextRole::Data, ChannelColor);
		FSlateFontInfo TagFont = Theme.DataFont;
		TagFont.Size = FMath::Max(8.f, Theme.DataFont.Size - 1.f);
		FALKWidgetBuilder::SetTextFont(TagText, TagFont);
		TagText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		FALKWidgetBuilder::AddToHorizontalBox(Row, TagText, FMargin(0.f, 5.f, 12.f, 0.f), HAlign_Left, VAlign_Top);

		UTextBlock* SenderText = FALKWidgetBuilder::MakeText(Tree, NAME_None, GetSenderText(Message));
		FSlateFontInfo SenderFont = Theme.SubheadingFont;
		SenderFont.Size = Theme.BodyFont.Size;
		FALKWidgetBuilder::SetTextFont(SenderText, SenderFont);
		SenderText->SetColorAndOpacity(FSlateColor(FMath::Lerp(ChannelColor, Theme.TextPrimary, 0.55f).CopyWithNewOpacity(1.f)));
		FALKWidgetBuilder::AddToHorizontalBox(Row, SenderText, FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Left, VAlign_Top);
	}

	UTextBlock* LineText = FALKWidgetBuilder::MakeText(Tree, NAME_None, FText::FromString(Message.Text));
	FALKWidgetBuilder::StyleText(LineText, Theme, EALKTextRole::Body, bSystem ? Theme.TextMuted : Theme.TextPrimary);
	LineText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToHorizontalBox(Row, LineText, FMargin(), HAlign_Fill, VAlign_Top, 1.f);

	// The time, the channel, the name (the display face) and the message stand on one baseline, whatever the fonts.
	TArray<UTextBlock*> LineParts;
	for (UWidget* Part : Row->GetAllChildren())
	{
		LineParts.Add(Cast<UTextBlock>(Part));
	}
	FALKWidgetBuilder::AlignBaselines(LineParts);

	MessageList->AddChild(Row);
	if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
	{
		RowSlot->SetPadding(FMargin(4.f, 0.f, 8.f, 12.f));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UALKChatPanel::RefreshMessages()
{
	const UALKTheme* Theme = GetTheme();
	if (!MessageList || !Theme)
	{
		return;
	}
	MessageList->ClearChildren();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (Online && bChatEnabled)
	{
		for (const FALKChatMessage& Message : Online->GetChatHistory())
		{
			if (ShouldShow(Message))
			{
				AddMessageRow(Message, *Theme);
			}
		}
	}
	MessageList->ScrollToEnd();
	UpdateEmptyText();
}

void UALKChatPanel::HandleChatMessage(const FALKChatMessage& Message)
{
	const UALKTheme* Theme = GetTheme();
	if (!MessageList || !Theme || !bChatEnabled || !ShouldShow(Message))
	{
		return;
	}
	AddMessageRow(Message, *Theme);
	// As many lines as the subsystem keeps.
	const int32 MaxLines = FMath::Max(1, UALKDeveloperSettings::GetOnlineConfig()->ChatHistorySize);
	while (MessageList->GetChildrenCount() > MaxLines)
	{
		MessageList->RemoveChildAt(0);
	}
	MessageList->ScrollToEnd();
	UpdateEmptyText();
}

// ---------------------------------------------------------------- Typing

bool UALKChatPanel::SendChatLine(const FString& Text)
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const FString Clean = Text.TrimStartAndEnd();
	if (!Online || Clean.IsEmpty() || !Online->IsTextChatEnabled())
	{
		return false;
	}

	bool bSent = false;
	switch (Channel)
	{
	case EALKChatChannel::Party:
		if (!Online->IsInParty())
		{
			// The reason, as a line only this player sees.
			Online->AddSystemMessage(LOCTEXT("ChatNoParty", "You are not in a party."));
			return false;
		}
		bSent = Online->SendChat(EALKChatChannel::Party, Clean, FString());
		break;

	case EALKChatChannel::Whisper:
		bSent = !WhisperTargetId.IsEmpty() && Online->SendChat(EALKChatChannel::Whisper, Clean, WhisperTargetId);
		break;

	default:
		// Everyone and the team are reached through the game.
		if (UALKPlayerNetComponent* NetComponent = Online->GetLocalNetComponent())
		{
			NetComponent->ServerSendChat(Channel, Clean, FString());
			bSent = true;
		}
		else
		{
			bSent = Online->SendChat(Channel, Clean, FString());
		}
		break;
	}
	if (!bSent)
	{
		Online->AddSystemMessage(LOCTEXT("ChatNotSent", "The message could not be sent."));
	}
	return bSent;
}

void UALKChatPanel::HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter || !InputField)
	{
		return;
	}
	if (Text.ToString().TrimStartAndEnd().IsEmpty())
	{
		// Enter on an empty line is "done typing".
		InputField->SetText(FText::GetEmpty());
		ReleaseInput();
		return;
	}
	if (SendChatLine(Text.ToString()))
	{
		InputField->SetText(FText::GetEmpty());
	}
}

void UALKChatPanel::FocusInput()
{
	if (InputField && bChatEnabled)
	{
		InputField->FocusInput();
	}
}

bool UALKChatPanel::IsTyping() const
{
	return InputField && InputField->IsTyping();
}

void UALKChatPanel::ReleaseInput()
{
	const TSharedPtr<SWidget> Previous = FocusBeforeTyping.Pin();
	FocusBeforeTyping.Reset();
	// False when that widget is gone by now (a seat that was rebuilt, a dialog that closed).
	const bool bRestored = Previous.IsValid() && FSlateApplication::IsInitialized() && FSlateApplication::Get().SetKeyboardFocus(Previous, EFocusCause::SetDirectly);
	if (!bRestored)
	{
		OnInputReleased.Broadcast();
	}
}

void UALKChatPanel::HandleWhisperRequested(const FString& UserId, const FString& DisplayName)
{
	SetWhisperTarget(UserId, DisplayName);
	FocusDelay = ALKChatPanelPrivate::WhisperFocusDelay;
}

void UALKChatPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	PollTime += InDeltaTime;
	if (PollTime >= ALKChatPanelPrivate::PollInterval)
	{
		PollTime = 0.f;
		RefreshState(false);
	}

	if (FocusDelay >= 0.f)
	{
		FocusDelay -= FMath::Min(InDeltaTime, 0.1f);
		if (FocusDelay < 0.f)
		{
			// Only on the screen the player is looking at.
			const UCommonActivatableWidget* Screen = GetTypedOuter<UCommonActivatableWidget>();
			if (!Screen || Screen->IsActivated())
			{
				FocusInput();
			}
		}
	}
}

FReply UALKChatPanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape (or the gamepad's back button) in the message box gives the keyboard back; it never closes the screen.
	const FKey Key = InKeyEvent.GetKey();
	if (IsTyping() && (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right))
	{
		ReleaseInput();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UALKChatPanel::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Tab in the message box moves to the next channel instead of to the next widget.
	if (InKeyEvent.GetKey() == EKeys::Tab && IsTyping())
	{
		ShowNextChannel();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UALKChatPanel::NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	// Remember what had the keyboard before the message box took it, so Escape can give it back.
	const TSharedPtr<SWidget> Field = InputField ? InputField->GetCachedWidget() : TSharedPtr<SWidget>();
	if (Field.IsValid() && NewWidgetPath.ContainsWidget(Field.Get()) && !PreviousFocusPath.ContainsWidget(Field.Get()))
	{
		FocusBeforeTyping = PreviousFocusPath.IsValid() ? PreviousFocusPath.GetLastWidget() : TWeakPtr<SWidget>();
	}
}

#undef LOCTEXT_NAMESPACE
