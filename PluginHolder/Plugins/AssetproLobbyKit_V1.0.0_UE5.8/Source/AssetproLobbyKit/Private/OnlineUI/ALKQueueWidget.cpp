// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKQueueWidget.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKQueueWidget)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKQueueWidgetPrivate
{
	/** The bar's smallest and largest width: long playlist names and translations widen it up to the largest. */
	static constexpr float MinWidth = 528.f;
	static constexpr float MaxWidth = 760.f;
	static constexpr float BarHeight = 62.f;
}

FText UALKQueueWidget::FormatMinutesSeconds(float Seconds, bool bPadMinutes)
{
	const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
	return FText::FromString(bPadMinutes ? FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60) : FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}

void UALKQueueWidget::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKQueueWidgetPrivate;

	USizeBox* Root = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("QueueRoot"));
	Root->SetMinDesiredWidth(MinWidth);
	Root->SetMaxDesiredWidth(MaxWidth);
	Root->SetMinDesiredHeight(BarHeight);
	Tree.RootWidget = Root;

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Frame->SetPadding(FMargin(18.f, 8.f, 10.f, 8.f));
	Frame->SetVerticalAlignment(VAlign_Center);
	Panel = Frame;
	Root->AddChild(Frame);

	UHorizontalBox* Row = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("QueueRow"));
	Frame->SetContent(Row);

	// The spinner while searching and joining, a tick once a match is found.
	UOverlay* StateBox = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("StateBox"));
	Spinner = FALKWidgetBuilder::Make<UALKSpinner>(Tree, TEXT("Spinner"));
	Spinner->Diameter = 28.f;
	FALKWidgetBuilder::AddToOverlay(StateBox, Spinner, HAlign_Center, VAlign_Center);
	DoneIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("DoneIcon"), EALKIcon::Check, 24.f);
	DoneIcon->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(StateBox, DoneIcon, HAlign_Center, VAlign_Center);
	FALKWidgetBuilder::AddToHorizontalBox(Row, FALKWidgetBuilder::WrapInSizeBox(Tree, StateBox, 30.f, 30.f), FMargin(0.f, 0.f, 14.f, 0.f), HAlign_Left, VAlign_Center);

	UVerticalBox* Texts = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("QueueTexts"));
	FALKWidgetBuilder::AddToHorizontalBox(Row, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	TitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("TitleText"));
	FALKWidgetBuilder::AddToVerticalBox(Texts, TitleText, FMargin(), HAlign_Left);
	DetailText = FALKWidgetBuilder::MakeText(Tree, TEXT("DetailText"));
	FALKWidgetBuilder::AddToVerticalBox(Texts, DetailText, FMargin(0.f, 1.f, 0.f, 0.f), HAlign_Left);

	// A fixed box, so the bar does not change width as the digits do.
	ElapsedText = FALKWidgetBuilder::MakeText(Tree, TEXT("ElapsedText"));
	ElapsedText->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToHorizontalBox(Row, FALKWidgetBuilder::WrapInSizeBox(Tree, ElapsedText, 84.f, 0.f, true), FMargin(14.f, 0.f, 14.f, 0.f), HAlign_Right, VAlign_Center);

	TSubclassOf<UALKButton> ButtonClass = UALKDeveloperSettings::GetMenuConfig()->ButtonClass;
	if (!ButtonClass)
	{
		ButtonClass = UALKButton::StaticClass();
	}
	CancelButton = Tree.ConstructWidget<UALKButton>(ButtonClass, TEXT("CancelButton"));
	CancelButton->SetButtonStyleType(EALKButtonStyle::Secondary);
	CancelButton->SetButtonText(LOCTEXT("QueueCancel", "Cancel"));
	// The bar comes and goes by itself, so it never holds the focus: screens give Cancel a key of its own.
	CancelButton->SetIsFocusable(false);
	CancelButton->bFocusOnHover = false;
	FALKWidgetBuilder::AddToHorizontalBox(Row, CancelButton, FMargin(), HAlign_Right, VAlign_Center);
}

void UALKQueueWidget::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &ThisClass::CancelSearch);
	}
}

void UALKQueueWidget::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (UALKFrame* Frame = Cast<UALKFrame>(Panel))
	{
		Frame->SetFrameStyle(Theme.MakeCardFrame());
	}
	FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
	if (TitleText)
	{
		FSlateFontInfo Font = Theme.SubheadingFont;
		Font.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.SubheadingFont.Size * 0.82f));
		FALKWidgetBuilder::SetTextFont(TitleText, Font);
	}
	FALKWidgetBuilder::StyleText(DetailText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(ElapsedText, Theme, EALKTextRole::Subheading, Theme.GetAccentTextColor());
	if (ElapsedText)
	{
		FSlateFontInfo Font = Theme.SubheadingFont;
		Font.Size = FMath::RoundToFloat(Theme.SubheadingFont.Size * 1.1f);
		FALKWidgetBuilder::SetTextFont(ElapsedText, Font);
	}
	if (Spinner)
	{
		Spinner->SetSpinnerColor(Theme.GetAccentTextColor());
	}
	if (DoneIcon)
	{
		DoneIcon->SetIcon(EALKIcon::Check, Theme.Success);
	}
}

void UALKQueueWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!MatchmakingHandle.IsValid())
		{
			MatchmakingHandle = Online->OnMatchmakingChangedNative.AddUObject(this, &ThisClass::HandleMatchmakingChanged);
		}
	}
	RefreshQueue();
}

void UALKQueueWidget::NativeDestruct()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnMatchmakingChangedNative.Remove(MatchmakingHandle);
	}
	MatchmakingHandle.Reset();
	Super::NativeDestruct();
}

void UALKQueueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bQueueShown && bSearching)
	{
		UpdateElapsed(false);
	}
}

void UALKQueueWidget::HandleMatchmakingChanged(EALKMatchmakingState NewState)
{
	RefreshQueue();
}

void UALKQueueWidget::RefreshQueue()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const EALKMatchmakingState State = Online ? Online->GetMatchmakingState() : EALKMatchmakingState::Idle;
	const bool bFound = State == EALKMatchmakingState::MatchFound || State == EALKMatchmakingState::Accepted;
	bSearching = State == EALKMatchmakingState::Searching;
	bQueueShown = bSearching || bFound || State == EALKMatchmakingState::Joining;

	// The root is collapsed, not this widget: the screen that placed the bar keeps the say over the widget itself.
	if (UWidget* Root = GetRootWidget())
	{
		Root->SetVisibility(bQueueShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bQueueShown)
	{
		return;
	}

	if (TitleText)
	{
		TitleText->SetText(bSearching ? LOCTEXT("QueueSearching", "Searching for a match")
			: (bFound ? LOCTEXT("QueueFound", "Match found") : LOCTEXT("QueueJoining", "Joining...")));
	}
	if (DetailText)
	{
		const FName PlaylistId = Online->GetQueuePlaylist();
		const FALKPlaylist* Playlist = UALKDeveloperSettings::GetOnlineConfig()->FindPlaylist(PlaylistId);
		const FText PlaylistName = Playlist && !Playlist->Name.IsEmpty() ? Playlist->Name : (PlaylistId.IsNone() ? FText::GetEmpty() : FText::FromName(PlaylistId));
		const float Estimate = bSearching ? Online->GetEstimatedWaitSeconds(PlaylistId) : -1.f;
		DetailText->SetText(Estimate >= 0.f
			? FText::Format(LOCTEXT("QueueDetailEstimate", "{0}  ·  est. {1}"), PlaylistName, FormatMinutesSeconds(Estimate, false))
			: PlaylistName);
		DetailText->SetVisibility(DetailText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Spinner)
	{
		Spinner->SetVisibility(bFound ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (DoneIcon)
	{
		DoneIcon->SetVisibility(bFound ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* ElapsedHolder = ElapsedText ? ElapsedText->GetParent() : nullptr)
	{
		ElapsedHolder->SetVisibility(bSearching ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CancelButton)
	{
		CancelButton->SetVisibility(bSearching ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	UpdateElapsed(true);
}

void UALKQueueWidget::UpdateElapsed(bool bForce)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const int32 Seconds = Online ? FMath::FloorToInt(Online->GetQueueElapsedSeconds()) : 0;
	if (ElapsedText && (bForce || Seconds != ShownSeconds))
	{
		ShownSeconds = Seconds;
		ElapsedText->SetText(FormatMinutesSeconds(static_cast<float>(Seconds), true));
	}
}

void UALKQueueWidget::CancelSearch()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->CancelQuickMatch();
	}
}

#undef LOCTEXT_NAMESPACE
