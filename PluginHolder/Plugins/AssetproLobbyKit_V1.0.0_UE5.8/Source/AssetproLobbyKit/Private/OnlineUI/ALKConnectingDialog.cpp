// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKConnectingDialog.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKThumbnail.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKConnectingDialog)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKConnectingDialogPrivate
{
	/** Narrower than a question dialog: it only says one thing. */
	static constexpr float WidthShare = 0.82f;
	/** How tall the map's picture lies across the top of the panel. */
	static constexpr float BandHeight = 150.f;

	/**
	 * Where the texts start under the panel's top edge. Without a picture: the panel's padding. With one: under its clear
	 * upper part where the picture melts into the panel (the theme's Picture Veil), and right under the picture on the
	 * skins that show it as a strip with a hard lower edge, so the title never stands on the picture itself.
	 */
	static float GetTextTop(const UALKTheme& Theme, bool bPicture)
	{
		if (!bPicture)
		{
			return 40.f;
		}
		return Theme.PictureVeil > 0.f ? 96.f : BandHeight + 24.f;
	}

	static bool IsBusy(EALKOnlineState OnlineState)
	{
		return OnlineState == EALKOnlineState::Hosting || OnlineState == EALKOnlineState::Searching
			|| OnlineState == EALKOnlineState::Joining || OnlineState == EALKOnlineState::Leaving;
	}
}

UALKConnectingDialog::UALKConnectingDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsModal = true;
}

void UALKConnectingDialog::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	DimImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("DimImage"));
	FALKWidgetBuilder::AddToOverlay(Root, DimImage);

	// A panel over the dimmed screen: spinner | what is happening, then Cancel.
	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Panel = Frame;
	FALKWidgetBuilder::AddToOverlay(Root, FALKWidgetBuilder::WrapInSizeBox(Tree, Panel, 560.f, 0.f), HAlign_Center, VAlign_Center);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	// The picture of the map being joined lies across the top of the panel; the texts start under its clear part.
	MapBand = FALKWidgetBuilder::AddPanelBand(Tree, Frame, Column, FMargin(40.f, 40.f, 40.f, 32.f), ALKConnectingDialogPrivate::BandHeight, TEXT("MapBand"));

	UHorizontalBox* Top = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TopRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Top);

	Spinner = FALKWidgetBuilder::Make<UALKSpinner>(Tree, TEXT("Spinner"));
	Spinner->Diameter = 44.f;
	FALKWidgetBuilder::AddToHorizontalBox(Top, Spinner, FMargin(0.f, 0.f, 24.f, 0.f), HAlign_Left, VAlign_Center);

	UVerticalBox* Texts = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("TextColumn"));
	FALKWidgetBuilder::AddToHorizontalBox(Top, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);

	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetAutoWrapText(true);
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Texts, TitleText, FMargin(), HAlign_Fill);

	DetailText = FALKWidgetBuilder::MakeText(Tree, TEXT("DetailText"));
	DetailText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(Texts, DetailText, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Fill);

	CancelButton = CreateMenuButton(LOCTEXT("Cancel", "Cancel"), EALKButtonStyle::Secondary);
	CancelButton->SetInputHint(FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), FText::GetEmpty()));
	FALKWidgetBuilder::AddToVerticalBox(Column, CancelButton, FMargin(0.f, 30.f, 0.f, 0.f), HAlign_Right);
}

void UALKConnectingDialog::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &ThisClass::CancelConnecting);
	}
}

void UALKConnectingDialog::NativeApplyTheme(const UALKTheme& Theme)
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
	if (USizeBox* PanelSize = Panel ? Cast<USizeBox>(Panel->GetParent()) : nullptr)
	{
		PanelSize->SetWidthOverride(FMath::RoundToFloat(Theme.DialogWidth * ALKConnectingDialogPrivate::WidthShare));
	}
	FALKWidgetBuilder::StylePanelBand(MapBand, Theme);
	// Another skin may show the picture as a strip instead of melting it in: the texts move with that.
	if (UWidget* Column = WidgetTree ? WidgetTree->FindWidget(TEXT("PanelColumn")) : nullptr)
	{
		if (UOverlaySlot* ColumnSlot = Cast<UOverlaySlot>(Column->Slot))
		{
			const bool bPicture = MapBand && MapBand->GetVisibility() != ESlateVisibility::Collapsed;
			ColumnSlot->SetPadding(FMargin(40.f, ALKConnectingDialogPrivate::GetTextTop(Theme, bPicture), 40.f, 32.f));
		}
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Font.Size * 0.6f));
		FALKWidgetBuilder::SetTextFont(TitleText, Font);
	}
	FALKWidgetBuilder::StyleText(DetailText, Theme, EALKTextRole::Body, Theme.TextMuted);
	if (Spinner)
	{
		Spinner->SetSpinnerColor(Theme.GetAccentTextColor());
	}
}

void UALKConnectingDialog::NativeOnActivated()
{
	DefaultFocusWidget = CancelButton.Get();
	RefreshState(true);
	Super::NativeOnActivated();
}

FText UALKConnectingDialog::GetStateText(EALKOnlineState OnlineState) const
{
	switch (OnlineState)
	{
	case EALKOnlineState::Hosting:
		return LOCTEXT("ConnectingHosting", "Creating the game...");
	case EALKOnlineState::Searching:
		return LOCTEXT("ConnectingSearching", "Looking for games...");
	case EALKOnlineState::Joining:
	{
		const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
		const FString ServerName = Online ? Online->GetCurrentSession().ServerName : FString();
		return ServerName.IsEmpty() ? LOCTEXT("ConnectingJoining", "Joining the game...")
			: FText::Format(LOCTEXT("ConnectingJoiningNamed", "Joining {0}..."), FText::FromString(ServerName));
	}
	case EALKOnlineState::Leaving:
		return LOCTEXT("ConnectingLeaving", "Leaving...");
	default:
		return FText::GetEmpty();
	}
}

void UALKConnectingDialog::RefreshState(bool bForce)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const EALKOnlineState Now = Online ? Online->GetState() : EALKOnlineState::Idle;
	if (!ALKConnectingDialogPrivate::IsBusy(Now))
	{
		// Idle, in a lobby or in a match: nothing left to wait for.
		if (IsActivated())
		{
			CloseScreen();
		}
		return;
	}
	if (!bForce && Now == ShownState)
	{
		return;
	}
	ShownState = Now;

	// Joining or creating a game on a map that has a picture: the picture shows across the top of the panel.
	{
		const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
		const bool bHasGame = Now == EALKOnlineState::Hosting || Now == EALKOnlineState::Joining;
		const FALKMapDefinition* Map = bHasGame && Config ? Config->FindMap(Online->GetCurrentSession().MapId) : nullptr;
		UTexture2D* Picture = Map ? Map->Thumbnail.LoadSynchronous() : nullptr;
		FALKWidgetBuilder::SetPanelBandPicture(MapBand, Picture);
		// With the picture the texts move down, out of its clear upper part.
		if (UWidget* Column = WidgetTree ? WidgetTree->FindWidget(TEXT("PanelColumn")) : nullptr)
		{
			if (UOverlaySlot* ColumnSlot = Cast<UOverlaySlot>(Column->Slot))
			{
				ColumnSlot->SetPadding(FMargin(40.f, ALKConnectingDialogPrivate::GetTextTop(*GetTheme(), Picture != nullptr), 40.f, 32.f));
			}
		}
	}

	if (TitleText)
	{
		TitleText->SetText(GetStateText(Now));
	}
	if (DetailText)
	{
		FText Detail;
		switch (Now)
		{
		case EALKOnlineState::Hosting: Detail = LOCTEXT("ConnectingHostingDetail", "Your lobby opens in a moment."); break;
		case EALKOnlineState::Searching: Detail = LOCTEXT("ConnectingSearchingDetail", "Asking for the list of games."); break;
		case EALKOnlineState::Joining: Detail = LOCTEXT("ConnectingJoiningDetail", "Connecting to the host."); break;
		default: Detail = LOCTEXT("ConnectingLeavingDetail", "Going back to the main menu."); break;
		}
		DetailText->SetText(Detail);
	}
	if (CancelButton)
	{
		// Leaving cannot be stopped half way.
		const bool bCanCancel = Now != EALKOnlineState::Leaving;
		const bool bHadFocus = CancelButton->HasFocusedDescendants() || CancelButton->HasAnyUserFocus();
		CancelButton->SetVisibility(bCanCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		DefaultFocusWidget = bCanCancel ? static_cast<UWidget*>(CancelButton.Get()) : nullptr;
		if (!bCanCancel && bHadFocus)
		{
			SetFocus();
		}
	}
}

void UALKConnectingDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (IsActivated())
	{
		RefreshState(false);
	}
}

void UALKConnectingDialog::CancelConnecting()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online)
	{
		CloseScreen();
		return;
	}
	switch (Online->GetState())
	{
	case EALKOnlineState::Searching:
		Online->CancelFindGames();
		break;
	case EALKOnlineState::Hosting:
	case EALKOnlineState::Joining:
		Online->LeaveGame();
		break;
	default:
		break;
	}
	// The dialog closes by itself once the subsystem is idle again.
}

bool UALKConnectingDialog::OnBackRequested_Implementation()
{
	// Back is Cancel. It never just closes the dialog: the game would still be busy behind it.
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (Online && Online->GetState() != EALKOnlineState::Leaving)
	{
		PlayThemeSound(EALKUISound::Back);
		CancelConnecting();
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
