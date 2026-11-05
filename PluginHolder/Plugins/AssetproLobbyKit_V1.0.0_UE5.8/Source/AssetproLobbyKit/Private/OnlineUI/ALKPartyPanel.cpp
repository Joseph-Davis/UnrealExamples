// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKPartyPanel.h"
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
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformProcess.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPartyPanel)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKPartyPanelPrivate
{
	static constexpr int32 RowHeight = 56;
	static constexpr float AvatarSize = 40.f;
	/** Members and free seats shown together before the free seats are cut down to one. */
	static constexpr int32 MaxLines = 4;

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

	static bool CanTakeFocus(const UWidget* Widget)
	{
		// Shown and able to take a click: your own line is hit test invisible, a button that is not needed is collapsed.
		const UUserWidget* UserWidget = Cast<UUserWidget>(Widget);
		const ESlateVisibility Shown = UserWidget ? UserWidget->GetVisibility() : ESlateVisibility::Collapsed;
		return UserWidget && UserWidget->IsFocusable() && UserWidget->GetIsEnabled()
			&& (Shown == ESlateVisibility::Visible || Shown == ESlateVisibility::SelfHitTestInvisible);
	}
}

// ================================================================================================ One line

UALKPartyMemberRow::UALKPartyMemberRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A list line: the item frame when focused.
	ButtonStyleType = EALKButtonStyle::Primary;
}

void UALKPartyMemberRow::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKPartyPanelPrivate;

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	Frame->SetPadding(FMargin(10.f, 6.f, 14.f, 6.f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Center);
	Background = Frame;
	Tree.RootWidget = Frame;

	UHorizontalBox* RowBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("RowBox"));
	Frame->SetContent(RowBox);

	// The member's picture, or a plus on a free seat.
	UOverlay* PictureBox = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("PictureBox"));
	MemberAvatar = FALKWidgetBuilder::Make<UALKAvatar>(Tree, TEXT("MemberAvatar"));
	MemberAvatar->Diameter = AvatarSize;
	FALKWidgetBuilder::AddToOverlay(PictureBox, MemberAvatar, HAlign_Center, VAlign_Center);
	PlusIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("PlusIcon"), EALKIcon::Plus, 18.f);
	PlusIcon->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(PictureBox, PlusIcon, HAlign_Center, VAlign_Center);
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, FALKWidgetBuilder::WrapInSizeBox(Tree, PictureBox, AvatarSize + 2.f, AvatarSize + 2.f), FMargin(0.f, 0.f, 14.f, 0.f), HAlign_Left, VAlign_Center);

	UVerticalBox* Texts = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("RowTexts"));
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);

	// Name and crown; a name too long for the line is cut off at its edge.
	UHorizontalBox* NameRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("NameRow"));
	NameRow->SetClipping(EWidgetClipping::ClipToBounds);
	FALKWidgetBuilder::AddToVerticalBox(Texts, NameRow);
	Label = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("Label"));
	FALKWidgetBuilder::AddToHorizontalBox(NameRow, Label, FMargin(), HAlign_Left, VAlign_Center);
	CrownIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("CrownIcon"), EALKIcon::Crown, 17.f);
	CrownIcon->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(NameRow, CrownIcon, FMargin(9.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	StatusText = FALKWidgetBuilder::MakeText(Tree, TEXT("StatusText"));
	StatusText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToVerticalBox(Texts, StatusText, FMargin(0.f, 1.f, 0.f, 0.f));

	ReadyIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("ReadyIcon"), EALKIcon::Check, 22.f);
	ReadyIcon->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, ReadyIcon, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
}

void UALKPartyMemberRow::SetMember(const FALKPartyMember& InMember, const FString& InShownName, bool bInAnonymous, bool bInAlone)
{
	Member = InMember;
	ShownName = InShownName;
	bInviteSeat = false;
	bAnonymous = bInAnonymous;
	bAlone = bInAlone;
	RefreshContent();
}

void UALKPartyMemberRow::SetInviteSeat()
{
	Member = FALKPartyMember();
	ShownName.Reset();
	bInviteSeat = true;
	bAnonymous = false;
	bAlone = false;
	RefreshContent();
}

void UALKPartyMemberRow::RefreshContent()
{
	SetButtonText(bInviteSeat ? LOCTEXT("PartyInviteSeat", "Invite a friend") : FText::FromString(ShownName));

	if (MemberAvatar)
	{
		MemberAvatar->SetVisibility(bInviteSeat ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (!bInviteSeat)
		{
			MemberAvatar->SetAnonymous(bAnonymous);
			MemberAvatar->SetPlayer(ShownName, bAnonymous ? nullptr : Member.Avatar.LoadSynchronous());
		}
	}
	if (PlusIcon)
	{
		PlusIcon->SetVisibility(bInviteSeat ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CrownIcon)
	{
		CrownIcon->SetVisibility(!bInviteSeat && !bAlone && Member.bLeader ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ReadyIcon)
	{
		ReadyIcon->SetVisibility(!bInviteSeat && !bAlone && Member.bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (StatusText)
	{
		FText Status;
		if (!bInviteSeat && !bAlone)
		{
			Status = Member.bLeader ? LOCTEXT("PartyLeader", "Party leader") : (Member.bReady ? LOCTEXT("PartyReady", "Ready") : LOCTEXT("PartyNotReady", "Not ready"));
		}
		if (!bInviteSeat && Member.bLocal)
		{
			Status = Status.IsEmpty() ? LOCTEXT("PartyYou", "You") : FText::Format(LOCTEXT("PartyStatusYou", "{0}  ·  You"), Status);
		}
		StatusText->SetText(Status);
		StatusText->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (const UALKTheme* Theme = UALKDeveloperSettings::GetTheme())
	{
		NativeApplyVisualState(*Theme, GetHighlight(), IsUsable());
	}
}

void UALKPartyMemberRow::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	// As tall as a list line, as wide as the list.
	SetMinDimensions(0, ALKPartyPanelPrivate::RowHeight);
	if (Label)
	{
		FSlateFontInfo NameFont = Theme.SubheadingFont;
		NameFont.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.SubheadingFont.Size * 0.86f));
		FALKWidgetBuilder::SetTextFont(Label, NameFont);
		Label->SetTextTransformPolicy(ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(StatusText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	RefreshContent();
}

void UALKPartyMemberRow::NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled)
{
	using namespace ALKPartyPanelPrivate;
	Super::NativeApplyVisualState(Theme, Highlight, bEnabled);

	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		FALKFrameStyle FrameStyle = Theme.MakeItemFrame(Highlight, false);
		if (bInviteSeat)
		{
			// A free seat is an outlined box at rest. Its see-through fill takes the outline's colour, so the thin line
			// is not tinted by the accent colour where the two blend.
			FrameStyle.Outline = FMath::Lerp(Theme.PanelOutline, FrameStyle.Outline, Highlight);
			FrameStyle.Fill = FMath::Lerp(Theme.PanelOutline.CopyWithNewOpacity(0.f), FrameStyle.Fill, Highlight);
		}
		Frame->SetFrameStyle(FrameStyle);
	}
	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(bInviteSeat ? Theme.TextMuted : Theme.TextPrimary, Theme.GetFocusTextColor(), Highlight)));
	}
	if (StatusText)
	{
		StatusText->SetColorAndOpacity(FSlateColor(OnFocus(Theme, Theme.TextMuted, Highlight)));
	}
	if (PlusIcon)
	{
		PlusIcon->SetIcon(EALKIcon::Plus, FMath::Lerp(Theme.TextMuted, Theme.GetFocusTextColor(), Highlight));
	}
	if (CrownIcon)
	{
		CrownIcon->SetIcon(EALKIcon::Crown, OnFocus(Theme, Theme.Warning, Highlight));
	}
	if (ReadyIcon)
	{
		ReadyIcon->SetIcon(EALKIcon::Check, OnFocus(Theme, Theme.GetAccentTextColor(), Highlight));
	}
	if (MemberAvatar)
	{
		// The presence dot is cut out of the picture in the colour behind it.
		MemberAvatar->SetBackColor(Theme.FocusStyle == EALKFocusStyle::Solid ? FMath::Lerp(Theme.Panel, Theme.Accent, Highlight) : Theme.Panel);
		MemberAvatar->SetStatusColor(Theme.Success);
	}
}

// ================================================================================================ The panel

void UALKPartyPanel::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKPartyPanelPrivate;

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Frame->SetPadding(FMargin(20.f, 16.f, 20.f, 20.f));
	Panel = Frame;
	Tree.RootWidget = Frame;

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Frame->SetContent(Column);

	UHorizontalBox* Header = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Header"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Header, FMargin(4.f, 0.f, 0.f, 10.f));
	TitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("TitleText"), LOCTEXT("PartyTitle", "Party"));
	FALKWidgetBuilder::AddToHorizontalBox(Header, TitleText, FMargin(), HAlign_Left, VAlign_Center, 1.f);
	CountChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("CountChip"), FText::GetEmpty(), EALKIcon::None);
	FALKWidgetBuilder::AddToHorizontalBox(Header, CountChip, FMargin(), HAlign_Right, VAlign_Center);

	// The lines reach a little past the panel's padding, with room at the right for the focused line's shadow.
	MemberList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("MemberList"));
	MemberList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToVerticalBox(Column, MemberList, FMargin(-6.f, 0.f, -12.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	ButtonBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ButtonBox"));
	FALKWidgetBuilder::AddToVerticalBox(Column, ButtonBox, FMargin(0.f, 8.f, 0.f, 0.f));

	// Ready on a line of its own (only in a party), then Invite and Leave side by side.
	ReadyButton = MakeButton(Tree, TEXT("ReadyButton"), LOCTEXT("PartyReady", "Ready"), EALKButtonStyle::Solid);
	FALKWidgetBuilder::AddToVerticalBox(ButtonBox, ReadyButton, FMargin(0.f, 0.f, 0.f, 8.f));
	UHorizontalBox* ActionRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ActionRow"));
	FALKWidgetBuilder::AddToVerticalBox(ButtonBox, ActionRow);
	InviteButton = MakeButton(Tree, TEXT("InviteButton"), LOCTEXT("PartyInvite", "Invite"), EALKButtonStyle::Secondary);
	InviteButton->SetLeadingIcon(EALKIcon::Plus);
	FALKWidgetBuilder::AddToHorizontalBox(ActionRow, InviteButton, FMargin(0.f, 0.f, 6.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
	LeaveButton = MakeButton(Tree, TEXT("LeaveButton"), FText::GetEmpty(), EALKButtonStyle::Ghost);
	FALKWidgetBuilder::AddToHorizontalBox(ActionRow, LeaveButton, FMargin(6.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
	UpdateLeaveLabel();
}

void UALKPartyPanel::UpdateLeaveLabel()
{
	if (LeaveButton)
	{
		// A narrow panel has no room for the long label beside Invite.
		LeaveButton->SetButtonText(bCompact ? LOCTEXT("PartyLeaveShort", "Leave") : LOCTEXT("PartyLeave", "Leave party"));
	}
}

void UALKPartyPanel::SetCompact(bool bInCompact)
{
	if (bCompact != bInCompact)
	{
		bCompact = bInCompact;
		UpdateLeaveLabel();
	}
}

void UALKPartyPanel::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (ReadyButton)
	{
		ReadyButton->OnClicked().AddUObject(this, &ThisClass::HandleReadyClicked);
	}
	if (InviteButton)
	{
		InviteButton->OnClicked().AddUObject(this, &ThisClass::HandleInviteClicked);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked().AddUObject(this, &ThisClass::HandleLeaveClicked);
	}
}

void UALKPartyPanel::NativeApplyTheme(const UALKTheme& Theme)
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
	FALKWidgetBuilder::StyleChip(CountChip, Theme, Theme.PanelRaised, FLinearColor::Transparent, Theme.TextMuted);
	StyleList(Theme);
}

void UALKPartyPanel::StyleList(const UALKTheme& Theme)
{
	if (!MemberList)
	{
		return;
	}
	// A thin accent thumb, as on every list of the kit.
	FScrollBarStyle BarStyle = MemberList->GetWidgetBarStyle();
	const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
	BarStyle.SetNormalThumbImage(Thumb);
	BarStyle.SetHoveredThumbImage(Thumb);
	BarStyle.SetDraggedThumbImage(Thumb);
	BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
	BarStyle.SetVerticalTopSlotImage(FSlateNoResource());
	BarStyle.SetVerticalBottomSlotImage(FSlateNoResource());
	MemberList->SetWidgetBarStyle(BarStyle);
	MemberList->SetScrollbarThickness(FVector2D(3.0, 3.0));
	MemberList->SetScrollbarPadding(FMargin(2.f, 0.f, 0.f, 0.f));
}

void UALKPartyPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!PartyHandle.IsValid())
		{
			PartyHandle = Online->OnPartyChangedNative.AddUObject(this, &ThisClass::RefreshParty);
		}
	}
	RefreshParty();
}

void UALKPartyPanel::NativeDestruct()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnPartyChangedNative.Remove(PartyHandle);
	}
	PartyHandle.Reset();
	PendingFocus.Reset();
	Super::NativeDestruct();
}

void UALKPartyPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// A line that was just made takes the focus once it has a place on screen.
	if (UWidget* Target = PendingFocus.Get())
	{
		if (!ALKPartyPanelPrivate::CanTakeFocus(Target))
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

bool UALKPartyPanel::HasFocusInside() const
{
	return HasAnyUserFocus() || HasFocusedDescendants();
}

void UALKPartyPanel::RefreshParty()
{
	using namespace ALKPartyPanelPrivate;
	if (!MemberList || !WidgetTree)
	{
		return;
	}

	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const bool bOnline = Online && Online->IsOnlineEnabled();
	TArray<FALKPartyMember> Members = Online ? Online->GetPartyMembers() : TArray<FALKPartyMember>();
	if (Members.Num() == 0)
	{
		// A backend without parties: the player alone.
		FALKPartyMember Local;
		Local.UserId = Online ? Online->GetLocalUserId() : FString();
		Local.DisplayName = Online ? Online->GetLocalDisplayName() : FString(FPlatformProcess::UserName());
		Local.bLocal = true;
		Local.bLeader = true;
		Members.Add(Local);
	}

	MemberCount = Members.Num();
	const bool bInParty = MemberCount > 1;
	const int32 MaxSize = FMath::Max(Online ? Online->GetPartyMaxSize() : 1, MemberCount);
	const int32 FreeSeats = bOnline ? MaxSize - MemberCount : 0;
	// Every free seat while the list is short; a long party keeps one.
	const int32 InviteSeats = FMath::Clamp(MaxLines - MemberCount, FMath::Min(1, FreeSeats), FreeSeats);

	// What had the focus, to give it back when its line is made again or its button goes away.
	UWidget* Focused = nullptr;
	int32 FocusedRow = INDEX_NONE;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (IsFocusedWidget(Rows[Index]))
		{
			Focused = Rows[Index];
			FocusedRow = Index;
		}
	}
	for (UALKButton* Button : { ReadyButton.Get(), InviteButton.Get(), LeaveButton.Get() })
	{
		if (IsFocusedWidget(Button))
		{
			Focused = Button;
		}
	}

	FString Key;
	for (const FALKPartyMember& Each : Members)
	{
		Key += Each.UserId + (Each.bLocal ? TEXT("*|") : TEXT("|"));
	}
	Key += FString::FromInt(InviteSeats);

	if (Key != StructureKey || Rows.Num() != MemberCount + InviteSeats)
	{
		StructureKey = Key;
		MemberList->ClearChildren();
		Rows.Reset();
		for (int32 Index = 0; Index < MemberCount + InviteSeats; ++Index)
		{
			UALKPartyMemberRow* Row = WidgetTree->ConstructWidget<UALKPartyMemberRow>(UALKPartyMemberRow::StaticClass());
			if (!Row)
			{
				continue;
			}
			if (Members.IsValidIndex(Index) && Members[Index].bLocal)
			{
				// Your own line is only shown: there is nothing to pick on it.
				Row->SetIsFocusable(false);
				Row->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			Row->OnClicked().AddUObject(this, &ThisClass::HandleRowClicked, TWeakObjectPtr<UALKPartyMemberRow>(Row));
			MemberList->AddChild(Row);
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 4.f));
				RowSlot->SetHorizontalAlignment(HAlign_Fill);
			}
			Rows.Add(Row);
		}
		if (FocusedRow != INDEX_NONE)
		{
			Focused = nullptr;
			// The nearest line that can be picked takes over.
			for (int32 Offset = 0; Offset < Rows.Num() && !Focused; ++Offset)
			{
				for (const int32 Index : { FocusedRow + Offset, FocusedRow - Offset })
				{
					if (Rows.IsValidIndex(Index) && CanTakeFocus(Rows[Index]))
					{
						Focused = Rows[Index];
						PendingFocus = Focused;
						break;
					}
				}
			}
			if (!Focused)
			{
				PendingFocus = InviteButton.Get();
			}
		}
	}

	const bool bStreamer = Online && Online->IsStreamerMode();
	bLocalReady = false;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		UALKPartyMemberRow* Row = Rows[Index];
		if (!Members.IsValidIndex(Index))
		{
			Row->SetInviteSeat();
			continue;
		}
		const FALKPartyMember& Each = Members[Index];
		if (Each.bLocal)
		{
			bLocalReady = Each.bReady;
		}
		// Your own name is never hidden; streamer mode hides everyone else's name and picture.
		const FString Shown = Each.bLocal || !Online ? Each.DisplayName : Online->GetDisplayName(Each.DisplayName, Index);
		Row->SetMember(Each, Shown, bStreamer && !Each.bLocal, !bInParty);
	}

	FALKWidgetBuilder::SetChipText(CountChip, FText::Format(LOCTEXT("PartyCount", "{0} / {1}"), FText::AsNumber(MemberCount), FText::AsNumber(MaxSize)));
	if (ReadyButton)
	{
		ReadyButton->SetButtonStyleType(bLocalReady ? EALKButtonStyle::Secondary : EALKButtonStyle::Solid);
		ReadyButton->SetButtonText(bLocalReady ? LOCTEXT("PartyNotReady", "Not ready") : LOCTEXT("PartyReady", "Ready"));
		ReadyButton->SetLeadingIcon(bLocalReady ? EALKIcon::None : EALKIcon::Check);
		ReadyButton->SetVisibility(bInParty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (LeaveButton)
	{
		LeaveButton->SetVisibility(bInParty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (InviteButton)
	{
		InviteButton->SetIsEnabled(FreeSeats > 0);
	}

	// The button that had the focus is gone or switched off: hand the focus to something that is there.
	if (Focused && FocusedRow == INDEX_NONE && !CanTakeFocus(Focused))
	{
		PendingFocus = GetFirstFocusTarget();
	}
}

UWidget* UALKPartyPanel::GetFirstFocusTarget() const
{
	using namespace ALKPartyPanelPrivate;
	for (UALKPartyMemberRow* Row : Rows)
	{
		if (CanTakeFocus(Row))
		{
			return Row;
		}
	}
	for (UALKButton* Button : { ReadyButton.Get(), InviteButton.Get(), LeaveButton.Get() })
	{
		if (CanTakeFocus(Button))
		{
			return Button;
		}
	}
	return nullptr;
}

bool UALKPartyPanel::FocusFirst()
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

void UALKPartyPanel::HandleRowClicked(TWeakObjectPtr<UALKPartyMemberRow> WeakRow)
{
	const UALKPartyMemberRow* Row = WeakRow.Get();
	if (!Row)
	{
		return;
	}
	if (Row->IsInviteSeat())
	{
		HandleInviteClicked();
	}
	else if (!Row->GetMember().bLocal)
	{
		if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
		{
			Flow->ShowPlayerOptions(Row->GetMember().UserId, Row->GetShownName());
		}
	}
}

void UALKPartyPanel::HandleInviteClicked()
{
	if (!bInviteOpensSocial)
	{
		OnInviteRequested.Broadcast();
	}
	else if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->OpenSocial();
	}
}

void UALKPartyPanel::HandleLeaveClicked()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->LeaveParty();
	}
}

void UALKPartyPanel::HandleReadyClicked()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!bLocalReady)
		{
			UALKTheme::PlayUISound(this, EALKUISound::Ready);
		}
		Online->SetPartyReady(!bLocalReady);
	}
}

#undef LOCTEXT_NAMESPACE
