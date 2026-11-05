// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKReportDialog.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTextField.h"
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
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKReportDialog)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKReportDialogPrivate
{
	static constexpr float PanelWidth = 900.f;
	static constexpr int32 ReasonsPerLine = 2;

	static void SetShown(UWidget* Widget, bool bShown, ESlateVisibility ShownAs = ESlateVisibility::SelfHitTestInvisible)
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

UALKReportDialog::UALKReportDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsModal = true;
}

void UALKReportDialog::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKReportDialogPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	DimImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("DimImage"));
	FALKWidgetBuilder::AddToOverlay(Root, DimImage);

	Panel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Panel->SetPadding(FMargin(44.f, 40.f, 44.f, 34.f));
	FALKWidgetBuilder::AddToOverlay(Root, FALKWidgetBuilder::WrapInSizeBox(Tree, Panel, PanelWidth, 0.f), HAlign_Center, VAlign_Center);
	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Panel->SetContent(Column);

	TitleText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleText, FMargin(), HAlign_Left);
	NoteText = FALKWidgetBuilder::MakeText(Tree, TEXT("NoteText"));
	NoteText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(Column, NoteText, FMargin(0.f, 10.f, 0.f, 0.f), HAlign_Fill);

	// ---- The form: reasons, details, the mute switch, the buttons
	FormBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("FormBox"));
	FALKWidgetBuilder::AddToVerticalBox(Column, FormBox, FMargin(0.f, 24.f, 0.f, 0.f));

	ReasonLabel = FALKWidgetBuilder::MakeText(Tree, TEXT("ReasonLabel"), LOCTEXT("ReportReasonLabel", "Reason"));
	FALKWidgetBuilder::AddToVerticalBox(FormBox, ReasonLabel, FMargin(0.f, 0.f, 0.f, 8.f), HAlign_Left);
	ReasonGrid = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ReasonGrid"));
	FALKWidgetBuilder::AddToVerticalBox(FormBox, ReasonGrid);

	DetailsField = Tree.ConstructWidget<UALKTextField>(UALKTextField::StaticClass(), TEXT("DetailsField"));
	FALKWidgetBuilder::AddToVerticalBox(FormBox, DetailsField, FMargin(0.f, 14.f, 0.f, 0.f), HAlign_Fill);

	UHorizontalBox* ButtonRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ButtonRow"));
	FALKWidgetBuilder::AddToVerticalBox(FormBox, ButtonRow, FMargin(0.f, 28.f, 0.f, 0.f));
	MuteButton = CreateMenuButton(LOCTEXT("ReportAlsoMute", "Also mute this player"), EALKButtonStyle::Secondary);
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, MuteButton);
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, FALKWidgetBuilder::Make<USpacer>(Tree, TEXT("ButtonSpacer")), FMargin(), HAlign_Fill, VAlign_Fill, 1.f);
	SubmitButton = CreateMenuButton(LOCTEXT("ReportSubmit", "Submit report"), EALKButtonStyle::Solid);
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, SubmitButton, FMargin(16.f, 0.f, 0.f, 0.f));
	CancelButton = CreateMenuButton(LOCTEXT("Cancel", "Cancel"), EALKButtonStyle::Secondary);
	CancelButton->SetInputHint(FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), FText::GetEmpty()));
	FALKWidgetBuilder::AddToHorizontalBox(ButtonRow, CancelButton, FMargin(16.f, 0.f, 0.f, 0.f));

	// ---- After sending: a tick, a thank you and Close
	SentBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("SentBox"));
	SentBox->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, SentBox, FMargin(0.f, 30.f, 0.f, 0.f));
	SentIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("SentIcon"), EALKIcon::Check, 48.f);
	FALKWidgetBuilder::AddToVerticalBox(SentBox, SentIcon, FMargin(), HAlign_Center);
	SentText = FALKWidgetBuilder::MakeText(Tree, TEXT("SentText"), LOCTEXT("ReportSent", "Thanks, report sent"));
	SentText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(SentBox, SentText, FMargin(0.f, 14.f, 0.f, 0.f), HAlign_Center);
	SentDetailText = FALKWidgetBuilder::MakeText(Tree, TEXT("SentDetailText"));
	SentDetailText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(SentBox, SentDetailText, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Center);
	DoneButton = CreateMenuButton(LOCTEXT("Close", "Close"), EALKButtonStyle::Solid);
	FALKWidgetBuilder::AddToVerticalBox(SentBox, DoneButton, FMargin(0.f, 30.f, 0.f, 0.f), HAlign_Center);
}

void UALKReportDialog::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (MuteButton)
	{
		MuteButton->OnClicked().AddUObject(this, &ThisClass::ToggleAlsoMute);
	}
	if (SubmitButton)
	{
		SubmitButton->OnClicked().AddUObject(this, &ThisClass::SubmitReport);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddWeakLambda(this, [this]() { OnBackRequested(); });
	}
	if (DoneButton)
	{
		DoneButton->OnClicked().AddWeakLambda(this, [this]() { CloseScreen(); });
	}
}

void UALKReportDialog::NativeApplyTheme(const UALKTheme& Theme)
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
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo TitleFont = Theme.HeadingFont;
		TitleFont.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.78f));
		FALKWidgetBuilder::SetTextFont(TitleText, TitleFont);
		// The player's name is part of the title: shown as written.
		TitleText->SetTextTransformPolicy(ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(NoteText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(ReasonLabel, Theme, EALKTextRole::Data, Theme.TextMuted);
	if (ReasonLabel)
	{
		ReasonLabel->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	if (SentIcon)
	{
		SentIcon->SetIcon(EALKIcon::Check, Theme.Success);
	}
	if (SentText)
	{
		FALKWidgetBuilder::StyleText(SentText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
		FSlateFontInfo SentFont = Theme.SubheadingFont;
		SentFont.Size = FMath::RoundToFloat(Theme.SubheadingFont.Size * 1.2f);
		FALKWidgetBuilder::SetTextFont(SentText, SentFont);
	}
	FALKWidgetBuilder::StyleText(SentDetailText, Theme, EALKTextRole::Caption, Theme.TextMuted);
}

void UALKReportDialog::NativeOnActivated()
{
	// The dialog is pooled: read who it was opened for and start with an empty form each time.
	const UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	UserId = Flow ? Flow->GetContextUserId() : FString();
	ShownName = Flow ? Flow->GetContextDisplayName() : FString();
	SelectedReason = INDEX_NONE;
	bAlsoMute = true;
	bSent = false;

	Reasons = UALKDeveloperSettings::GetOnlineConfig()->ReportReasons;
	if (Reasons.Num() == 0)
	{
		Reasons = {
			FALKReportReason(TEXT("Cheating"), LOCTEXT("ReportCheating", "Cheating")),
			FALKReportReason(TEXT("AbusiveChat"), LOCTEXT("ReportAbusiveChat", "Abusive chat")),
			FALKReportReason(TEXT("AbusiveVoice"), LOCTEXT("ReportAbusiveVoice", "Abusive voice")),
			FALKReportReason(TEXT("OffensiveName"), LOCTEXT("ReportOffensiveName", "Offensive name")),
			FALKReportReason(TEXT("Leaving"), LOCTEXT("ReportLeaving", "Leaving matches")),
			FALKReportReason(TEXT("Other"), LOCTEXT("ReportOther", "Other"))
		};
	}
	RebuildReasons();

	if (DetailsField)
	{
		DetailsField->SetText(FText::GetEmpty());
		DetailsField->SetHintText(LOCTEXT("ReportDetailsHint", "Add details (optional)"));
		DetailsField->SetMaxLength(MaxDetailsLength);
	}
	RefreshState();
	Super::NativeOnActivated();
}

void UALKReportDialog::RebuildReasons()
{
	using namespace ALKReportDialogPrivate;
	ReasonButtons.Reset();
	if (!ReasonGrid || !WidgetTree)
	{
		return;
	}
	ReasonGrid->ClearChildren();

	UHorizontalBox* Line = nullptr;
	int32 InLine = 0;
	for (int32 Index = 0; Index < Reasons.Num(); ++Index)
	{
		if (!Line || InLine >= ReasonsPerLine)
		{
			Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			FALKWidgetBuilder::AddToVerticalBox(ReasonGrid, Line, FMargin(0.f, 0.f, 0.f, 6.f));
			InLine = 0;
		}
		const FALKReportReason& Reason = Reasons[Index];
		// A menu entry per reason: focus lights it up, the tick marks the one that is picked.
		UALKButton* ReasonButton = CreateMenuButton(Reason.Name.IsEmpty() ? FText::FromName(Reason.Id) : Reason.Name, EALKButtonStyle::Primary);
		if (!ReasonButton)
		{
			continue;
		}
		ReasonButton->OnClicked().AddUObject(this, &ThisClass::SelectReason, Index);
		FALKWidgetBuilder::AddToHorizontalBox(Line, ReasonButton, FMargin(InLine > 0 ? 12.f : 0.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
		ReasonButtons.Add(ReasonButton);
		++InLine;
	}
	// The last line is filled up, so its button is as wide as the others.
	for (; Line && InLine < ReasonsPerLine; ++InLine)
	{
		FALKWidgetBuilder::AddToHorizontalBox(Line, WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()), FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	}
}

void UALKReportDialog::RefreshState()
{
	using namespace ALKReportDialogPrivate;
	const bool bHasPlayer = !UserId.IsEmpty();

	if (TitleText)
	{
		TitleText->SetText(bHasPlayer && !ShownName.IsEmpty()
			? FText::Format(LOCTEXT("ReportTitle", "Report {0}"), FText::FromString(ShownName))
			: LOCTEXT("ReportTitleNobody", "Report a player"));
	}
	if (NoteText)
	{
		NoteText->SetText(bHasPlayer ? LOCTEXT("ReportNote", "Reports go to the game's own moderation tools.")
			: LOCTEXT("ReportNoteNobody", "No player is selected. Open this from a player's options to report them."));
	}
	SetShown(FormBox, !bSent);
	SetShown(SentBox, bSent);

	for (int32 Index = 0; Index < ReasonButtons.Num(); ++Index)
	{
		if (ReasonButtons[Index])
		{
			ReasonButtons[Index]->SetLeadingIcon(Index == SelectedReason ? EALKIcon::Check : EALKIcon::None);
		}
	}
	if (MuteButton)
	{
		MuteButton->SetLeadingIcon(bAlsoMute ? EALKIcon::Check : EALKIcon::Cross);
		MuteButton->SetIsEnabled(bHasPlayer);
	}
	if (SubmitButton)
	{
		// A report needs a player and a reason.
		SubmitButton->SetIsEnabled(bHasPlayer && Reasons.IsValidIndex(SelectedReason));
	}
	if (DetailsField)
	{
		// Down from the details line goes along the button row from its start, not to whichever button happens to be nearest.
		UWidget* Next = nullptr;
		for (UALKButton* Button : { MuteButton.Get(), SubmitButton.Get(), CancelButton.Get() })
		{
			if (!Next && Button && Button->GetIsEnabled())
			{
				Next = Button;
			}
		}
		if (Next)
		{
			DetailsField->SetNavigationRuleExplicit(EUINavigation::Down, Next);
		}
	}
	if (SentDetailText)
	{
		SentDetailText->SetText(bAlsoMute && bHasPlayer ? FText::Format(LOCTEXT("ReportSentMuted", "{0} is muted from now on."), FText::FromString(ShownName)) : FText::GetEmpty());
		SentDetailText->SetVisibility(bAlsoMute && bHasPlayer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UALKReportDialog::SelectReason(int32 ReasonIndex)
{
	if (bSent || !Reasons.IsValidIndex(ReasonIndex))
	{
		return;
	}
	SelectedReason = ReasonIndex;
	RefreshState();
}

void UALKReportDialog::ToggleAlsoMute()
{
	if (!bSent)
	{
		bAlsoMute = !bAlsoMute;
		PlayThemeSound(EALKUISound::Change);
		RefreshState();
	}
}

void UALKReportDialog::SubmitReport()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (bSent || !Online || UserId.IsEmpty() || !Reasons.IsValidIndex(SelectedReason))
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	const FString Details = DetailsField ? DetailsField->GetText().ToString().TrimStartAndEnd() : FString();
	// The kit raises On Player Reported and tells the provider; the game's own moderation tools take it from there.
	Online->ReportPlayer(UserId, Reasons[SelectedReason].Id, Details);
	if (bAlsoMute)
	{
		Online->SetPlayerMuted(UserId, true);
	}
	bSent = true;
	RefreshState();
	if (DoneButton)
	{
		DoneButton->SetFocus();
	}
}

UWidget* UALKReportDialog::NativeGetDesiredFocusTarget() const
{
	if (bSent)
	{
		return DoneButton ? static_cast<UWidget*>(DoneButton.Get()) : Super::NativeGetDesiredFocusTarget();
	}
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	if (ReasonButtons.Num() > 0 && ReasonButtons[0])
	{
		return ReasonButtons[0];
	}
	return CancelButton ? static_cast<UWidget*>(CancelButton.Get()) : Super::NativeGetDesiredFocusTarget();
}

#undef LOCTEXT_NAMESPACE
