// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKSaveSlotScreen.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Save/ALKSaveSubsystem.h"
#include "Screens/ALKConfirmDialog.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKSaveSlotCard.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformInput.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSaveSlotScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKSaveSlotScreenPrivate
{
	static constexpr int32 DetailRowCount = 4;
	/** The preview never takes more than this share of the screen's height (ultrawide screens). */
	static constexpr float PreviewMaxHeightShare = 0.34f;
}

UALKSaveSlotScreen::UALKSaveSlotScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

void UALKSaveSlotScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKSaveSlotScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	FALKWidgetBuilder::AddToOverlay(Root, Main);

	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"));
	FALKWidgetBuilder::AddToVerticalBox(Main, SystemLabelText, FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Left);

	TitleText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleText, FMargin(), HAlign_Left);

	// Body: the cards on a dark panel | the focused save's screenshot and details
	UHorizontalBox* Body = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Body"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Body, FMargin(0.f, 28.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	UALKFrame* List = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ListPanel"));
	List->SetPadding(FMargin(16.f, 18.f, 18.f, 18.f));
	ListPanel = List;
	FALKWidgetBuilder::AddToHorizontalBox(Body, List, FMargin(), HAlign_Fill, VAlign_Fill, 0.58f);

	UOverlay* ListOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("ListOverlay"));
	List->SetContent(ListOverlay);
	SlotList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("SlotList"));
	SlotList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToOverlay(ListOverlay, SlotList);
	EmptyText = FALKWidgetBuilder::MakeText(Tree, TEXT("EmptyText"), LOCTEXT("NoSaves", "No saved games"));
	EmptyText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToOverlay(ListOverlay, FALKWidgetBuilder::MakeEmptyState(Tree, TEXT("EmptyState"), EALKIcon::Save, EmptyText), HAlign_Center, VAlign_Center);

	UALKFrame* Detail = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("DetailPanel"));
	Detail->SetPadding(FMargin(26.f, 12.f, 26.f, 24.f));
	DetailPanel = Detail;
	FALKWidgetBuilder::AddToHorizontalBox(Body, Detail, FMargin(32.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 0.42f);

	UVerticalBox* DetailColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("DetailColumn"));
	Detail->SetContent(DetailColumn);

	DetailHeader = FALKWidgetBuilder::MakeText(Tree, TEXT("DetailHeader"));
	FALKWidgetBuilder::AddToVerticalBox(DetailColumn, DetailHeader, FMargin(), HAlign_Left);

	UOverlay* Preview = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Preview"));
	UImage* PreviewBack = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("PreviewBack"));
	FALKWidgetBuilder::AddToOverlay(Preview, PreviewBack);
	UScaleBox* PreviewScale = FALKWidgetBuilder::Make<UScaleBox>(Tree, TEXT("PreviewScale"));
	PreviewScale->SetStretch(EStretch::ScaleToFill);
	PreviewImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("PreviewImage"));
	PreviewScale->AddChild(PreviewImage);
	FALKWidgetBuilder::AddToOverlay(Preview, PreviewScale);
	UALKFrame* PreviewFrame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("PreviewFrame"));
	PreviewFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	FALKWidgetBuilder::AddToOverlay(Preview, PreviewFrame);
	PreviewSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("PreviewSize"));
	PreviewSize->SetWidthOverride(480.f);
	PreviewSize->SetHeightOverride(270.f);
	PreviewSize->SetContent(Preview);
	FALKWidgetBuilder::AddToVerticalBox(DetailColumn, PreviewSize, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Center);

	ChapterLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("ChapterLabelText"));
	FALKWidgetBuilder::AddToVerticalBox(DetailColumn, ChapterLabelText, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Left);
	UALKTitleText* ChapterName = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("ChapterNameText"));
	ChapterNameText = ChapterName;
	FALKWidgetBuilder::AddToVerticalBox(DetailColumn, ChapterNameText, FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Left);

	DetailRows = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("DetailRows"));
	FALKWidgetBuilder::AddToVerticalBox(DetailColumn, DetailRows, FMargin(0.f, 16.f, 0.f, 0.f));
	for (int32 Row = 0; Row < DetailRowCount; ++Row)
	{
		UHorizontalBox* RowBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);
		UTextBlock* Name = FALKWidgetBuilder::MakeText(Tree, NAME_None);
		FALKWidgetBuilder::AddToHorizontalBox(RowBox, FALKWidgetBuilder::WrapInSizeBox(Tree, Name, 150.f, 0.f), FMargin(), HAlign_Left, VAlign_Top);
		UTextBlock* Value = FALKWidgetBuilder::MakeText(Tree, NAME_None);
		Value->SetAutoWrapText(true);
		FALKWidgetBuilder::AddToHorizontalBox(RowBox, Value, FMargin(), HAlign_Fill, VAlign_Top, 1.f);
		FALKWidgetBuilder::AddToVerticalBox(DetailRows, RowBox, FMargin(0.f, 0.f, 0.f, 6.f));
	}

	// Pushes the progress row to the bottom of the panel.
	UVerticalBox* Spacer = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("DetailSpacer"));
	FALKWidgetBuilder::AddToVerticalBox(DetailColumn, Spacer, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);

	UHorizontalBox* ProgressRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ProgressRow"));
	FALKWidgetBuilder::AddToVerticalBox(DetailColumn, ProgressRow, FMargin(0.f, 12.f, 0.f, 0.f));
	UTextBlock* ProgressLabel = FALKWidgetBuilder::MakeText(Tree, TEXT("ProgressLabel"), LOCTEXT("SaveProgress", "Progress"));
	FALKWidgetBuilder::AddToHorizontalBox(ProgressRow, ProgressLabel, FMargin(0.f, 0.f, 18.f, 0.f), HAlign_Left, VAlign_Center);
	ProgressBar = FALKWidgetBuilder::Make<UALKValueBar>(Tree, TEXT("ProgressBar"));
	ProgressBar->BarSize = FVector2D(260.0, 10.0);
	FALKWidgetBuilder::AddToHorizontalBox(ProgressRow, ProgressBar, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	ProgressText = FALKWidgetBuilder::MakeText(Tree, TEXT("ProgressText"));
	FALKWidgetBuilder::AddToHorizontalBox(ProgressRow, ProgressText, FMargin(18.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

	// Footer: [Enter] Load / Save  [Del] Delete  [Esc] Back
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("HintLoad", "Load"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { ActivateFocusedCard(); })),
		FALKInputHint(LOCTEXT("KeyDel", "Del"), EKeys::Gamepad_FaceButton_Top, LOCTEXT("HintDelete", "Delete"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { DeleteFocusedSave(); })),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Left);
}

void UALKSaveSlotScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	UpdateTitles();
}

void UALKSaveSlotScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (UALKSaveSubsystem* Saves = GetSaveSubsystem())
	{
		Saves->OnSavesChanged.AddUniqueDynamic(this, &ThisClass::HandleSavesChanged);
	}
}

void UALKSaveSlotScreen::NativeDestruct()
{
	if (UALKSaveSubsystem* Saves = GetSaveSubsystem())
	{
		Saves->OnSavesChanged.RemoveDynamic(this, &ThisClass::HandleSavesChanged);
	}
	Super::NativeDestruct();
}

UALKSaveSubsystem* UALKSaveSlotScreen::GetSaveSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UALKSaveSubsystem>() : nullptr;
}

void UALKSaveSlotScreen::SetSaveMode(bool bInSaveMode)
{
	bSaveMode = bInSaveMode;
	FocusedCard.Reset();
	RefocusSlot.Reset();
	UpdateTitles();
	RefreshSlots();
}

void UALKSaveSlotScreen::UpdateTitles()
{
	const FText Title = bSaveMode ? LOCTEXT("SaveGameTitle", "Save Game") : LOCTEXT("LoadGameTitle", "Load Game");
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (SystemLabelText)
	{
		SystemLabelText->SetText(MakeSystemLabel(bSaveMode ? 14 : 13, Title));
	}
	if (HintButtons.Num() > 0 && HintButtons[0])
	{
		HintButtons[0]->SetButtonText(bSaveMode ? LOCTEXT("HintSave", "Save") : LOCTEXT("HintLoad", "Load"));
	}
}

void UALKSaveSlotScreen::NativeApplyTheme(const UALKTheme& Theme)
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
		if (UImage* PreviewBack = Cast<UImage>(WidgetTree->FindWidget(TEXT("PreviewBack"))))
		{
			PreviewBack->SetBrush(UALKTheme::MakeSolidBrush(FMath::Lerp(Theme.BackgroundTop, Theme.BackgroundBottom, 0.6f)));
		}
		if (UALKFrame* PreviewFrame = Cast<UALKFrame>(WidgetTree->FindWidget(TEXT("PreviewFrame"))))
		{
			FALKFrameStyle Frame;
			Frame.Outline = Theme.PanelOutline;
			Frame.OutlineWidth = Theme.OutlineWidth;
			Frame.Radius = 0.f;
			PreviewFrame->SetFrameStyle(Frame);
		}
		for (const FName Name : { FName(TEXT("ProgressLabel")) })
		{
			if (UTextBlock* Small = Cast<UTextBlock>(WidgetTree->FindWidget(Name)))
			{
				FALKWidgetBuilder::StyleText(Small, Theme, EALKTextRole::Data, Theme.TextMuted);
				Small->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
			}
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
	if (UALKFrame* Detail = Cast<UALKFrame>(DetailPanel))
	{
		Detail->SetFrameStyle(Theme.MakePanelFrame());
	}
	FALKWidgetBuilder::StyleText(DetailHeader, Theme, EALKTextRole::Data, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(ChapterLabelText, Theme, EALKTextRole::Data, Theme.GetAccentTextColor());
	FALKWidgetBuilder::StyleText(ProgressText, Theme, EALKTextRole::Data, Theme.TextSecondary);
	for (UTextBlock* Upper : { DetailHeader.Get(), ChapterLabelText.Get() })
	{
		if (Upper)
		{
			Upper->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		}
	}
	if (ChapterNameText)
	{
		FALKWidgetBuilder::StyleText(ChapterNameText, Theme, EALKTextRole::Heading);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.62f));
		FALKWidgetBuilder::SetTextFont(ChapterNameText, Font);
	}
	FALKWidgetBuilder::StyleText(EmptyText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleEmptyState(EmptyText, Theme);
	if (ProgressBar)
	{
		ProgressBar->SetColors(Theme.Accent, Theme.TextMuted.CopyWithNewOpacity(0.5f));
	}
	StyleDetailRows(Theme);

	if (SlotList)
	{
		FScrollBarStyle BarStyle = SlotList->GetWidgetBarStyle();
		FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
		BarStyle.SetNormalThumbImage(Thumb);
		BarStyle.SetHoveredThumbImage(Thumb);
		BarStyle.SetDraggedThumbImage(Thumb);
		BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
		SlotList->SetWidgetBarStyle(BarStyle);
		SlotList->SetScrollbarThickness(FVector2D(3.0, 3.0));
		SlotList->SetScrollbarPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}
	ShowDetails(FocusedCard.Get());
}

void UALKSaveSlotScreen::StyleDetailRows(const UALKTheme& Theme)
{
	if (!DetailRows)
	{
		return;
	}
	for (UWidget* Child : DetailRows->GetAllChildren())
	{
		UHorizontalBox* Row = Cast<UHorizontalBox>(Child);
		if (!Row || Row->GetChildrenCount() < 2)
		{
			continue;
		}
		const USizeBox* NameSize = Cast<USizeBox>(Row->GetChildAt(0));
		if (UTextBlock* Name = NameSize ? Cast<UTextBlock>(NameSize->GetContent()) : nullptr)
		{
			FALKWidgetBuilder::StyleText(Name, Theme, EALKTextRole::Data, Theme.TextMuted);
			Name->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		}
		if (UTextBlock* Value = Cast<UTextBlock>(Row->GetChildAt(1)))
		{
			FALKWidgetBuilder::StyleText(Value, Theme, EALKTextRole::Data, Theme.TextPrimary);
			Value->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		}
	}
}

void UALKSaveSlotScreen::SetDetailRow(int32 RowIndex, const FText& Name, const FText& Value)
{
	UHorizontalBox* Row = DetailRows ? Cast<UHorizontalBox>(DetailRows->GetChildAt(RowIndex)) : nullptr;
	if (!Row || Row->GetChildrenCount() < 2)
	{
		return;
	}
	const USizeBox* NameSize = Cast<USizeBox>(Row->GetChildAt(0));
	if (UTextBlock* NameText = NameSize ? Cast<UTextBlock>(NameSize->GetContent()) : nullptr)
	{
		NameText->SetText(Name);
	}
	if (UTextBlock* ValueText = Cast<UTextBlock>(Row->GetChildAt(1)))
	{
		ValueText->SetText(Value);
	}
	Row->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UALKSaveSlotScreen::NativeOnActivated()
{
	// Each opening starts on the first card (the newest save, or New Save). Screens are pooled, so forget the card
	// focused the last time it was open.
	FocusedCard.Reset();
	RefocusSlot.Reset();
	UpdateTitles();
	RefreshSlots();
	Super::NativeOnActivated();
}

UALKSaveSlotCard* UALKSaveSlotScreen::MakeCard()
{
	TSubclassOf<UALKSaveSlotCard> CardClass = GetMenuConfig()->SaveSlotCardClass;
	if (!CardClass)
	{
		CardClass = UALKSaveSlotCard::StaticClass();
	}
	UALKSaveSlotCard* Card = CreateWidget<UALKSaveSlotCard>(this, CardClass);
	if (Card)
	{
		Card->OnCardFocused.AddUObject(this, &ThisClass::HandleCardFocused);
		TWeakObjectPtr<UALKSaveSlotCard> WeakCard(Card);
		Card->OnClicked().AddWeakLambda(this, [this, WeakCard]() { HandleCardClicked(WeakCard.Get()); });
		Cards.Add(Card);
	}
	return Card;
}

void UALKSaveSlotScreen::RefreshSlots()
{
	UALKSaveSubsystem* Saves = GetSaveSubsystem();
	if (!SlotList || !Saves)
	{
		return;
	}
	if (const UALKSaveSlotCard* Focused = FocusedCard.Get())
	{
		if (RefocusSlot.IsEmpty())
		{
			RefocusSlot = Focused->IsNewSaveCard() ? FString() : Focused->GetSaveInfo().SlotName;
		}
	}
	SlotList->ClearChildren();
	Cards.Reset();
	FocusedCard.Reset();

	const TArray<FALKSaveSlotInfo> All = Saves->GetSaves();
	auto AddCard = [this, Saves](const FALKSaveSlotInfo* Info, bool bReadOnly)
	{
		UALKSaveSlotCard* Card = MakeCard();
		if (!Card)
		{
			return;
		}
		if (Info)
		{
			Card->SetSave(*Info, Saves->GetThumbnail(Info->SlotName), bReadOnly);
		}
		else
		{
			const UALKMenuConfig* MenuConfig = GetMenuConfig();
			Card->SetNewSave(MenuConfig->ManualSlots - Saves->GetManualSaveCount(), MenuConfig->ManualSlots);
		}
		SlotList->AddChild(Card);
		if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(Card->Slot))
		{
			CardSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			CardSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	if (bSaveMode)
	{
		// A new save first (while a slot is free), then the manual saves to overwrite, then the read-only others.
		if (Saves->GetFreeManualSlot() != INDEX_NONE)
		{
			AddCard(nullptr, false);
		}
		for (const FALKSaveSlotInfo& Info : All)
		{
			if (Info.Type == EALKSaveSlotType::Manual)
			{
				AddCard(&Info, false);
			}
		}
		for (const FALKSaveSlotInfo& Info : All)
		{
			if (Info.Type != EALKSaveSlotType::Manual)
			{
				AddCard(&Info, true);
			}
		}
	}
	else
	{
		for (const FALKSaveSlotInfo& Info : All)
		{
			AddCard(&Info, false);
		}
	}

	if (EmptyText)
	{
		FALKWidgetBuilder::SetEmptyStateShown(EmptyText, Cards.Num() == 0);
	}

	// Focus: the save focused before (or just written), else the first card that can be picked.
	UALKSaveSlotCard* Target = nullptr;
	for (UALKSaveSlotCard* Card : Cards)
	{
		if (Card && Card->GetIsEnabled() && !RefocusSlot.IsEmpty() && !Card->IsNewSaveCard() && Card->GetSaveInfo().SlotName == RefocusSlot)
		{
			Target = Card;
		}
	}
	if (!Target)
	{
		for (UALKSaveSlotCard* Card : Cards)
		{
			if (Card && Card->GetIsEnabled())
			{
				Target = Card;
				break;
			}
		}
	}
	RefocusSlot.Reset();
	DefaultFocusWidget = Target;
	FocusedCard = Target;
	ShowDetails(Target);
	// Focused once laid out (and only while this screen is the active one).
	bFocusPending = Target != nullptr;
}

TArray<UALKSaveSlotCard*> UALKSaveSlotScreen::GetCards() const
{
	TArray<UALKSaveSlotCard*> Result;
	for (UALKSaveSlotCard* Card : Cards)
	{
		Result.Add(Card);
	}
	return Result;
}

UWidget* UALKSaveSlotScreen::NativeGetDesiredFocusTarget() const
{
	if (UALKSaveSlotCard* Focused = FocusedCard.Get())
	{
		return Focused;
	}
	return DefaultFocusWidget ? DefaultFocusWidget.Get() : Super::NativeGetDesiredFocusTarget();
}

void UALKSaveSlotScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	CapContentWidth(MyGeometry);
	using namespace ALKSaveSlotScreenPrivate;

	// The preview: the detail panel's width, 16:9, but never taller than a share of the screen (ultrawide screens).
	if (DetailPanel && PreviewSize)
	{
		const float PanelWidth = DetailPanel->GetCachedGeometry().GetLocalSize().X;
		const float ScreenHeight = MyGeometry.GetLocalSize().Y;
		if (PanelWidth > 1.f && ScreenHeight > 1.f && (!FMath::IsNearlyEqual(PanelWidth, LastPanelWidth) || !FMath::IsNearlyEqual(ScreenHeight, LastScreenHeight)))
		{
			LastPanelWidth = PanelWidth;
			LastScreenHeight = ScreenHeight;
			const FMargin PanelPadding = DetailPanel->GetPadding();
			const float Width = FMath::Min(PanelWidth - PanelPadding.Left - PanelPadding.Right, ScreenHeight * PreviewMaxHeightShare * 16.f / 9.f);
			PreviewSize->SetWidthOverride(FMath::RoundToFloat(FMath::Max(64.f, Width)));
			PreviewSize->SetHeightOverride(FMath::RoundToFloat(FMath::Max(36.f, Width) * 9.f / 16.f));
		}
	}

	// Focus the chosen card once the list is laid out.
	if (bFocusPending)
	{
		UALKSaveSlotCard* Card = FocusedCard.Get();
		if (!Card)
		{
			bFocusPending = false;
		}
		else if (IsActivated() && Card->GetCachedGeometry().GetLocalSize().X > 1.0)
		{
			bFocusPending = false;
			Card->SetFocus();
			if (SlotList)
			{
				SlotList->ScrollWidgetIntoView(Card, false, EDescendantScrollDestination::IntoView, 24.f);
			}
		}
	}
}

void UALKSaveSlotScreen::HandleCardFocused(UALKSaveSlotCard* Card)
{
	FocusedCard = Card;
	ShowDetails(Card);
	if (SlotList && Card)
	{
		SlotList->ScrollWidgetIntoView(Card, true, EDescendantScrollDestination::IntoView, 24.f);
	}
}

void UALKSaveSlotScreen::ShowDetails(const UALKSaveSlotCard* Card)
{
	UALKSaveSubsystem* Saves = GetSaveSubsystem();
	const UALKMenuConfig* Config = GetMenuConfig();
	const bool bNew = Card && Card->IsNewSaveCard();
	if (!Card || !Saves)
	{
		if (DetailPanel)
		{
			DetailPanel->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}
	if (DetailPanel)
	{
		DetailPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// A new save shows the game as it is now.
	FALKSaveSlotInfo Info = Card->GetSaveInfo();
	if (bNew)
	{
		Info.ChapterId = Saves->GetCurrentChapter();
		Info.PlaytimeSeconds = Saves->GetPlaytime();
		Info.SavedAt = FDateTime::UtcNow();
	}

	if (DetailHeader)
	{
		DetailHeader->SetText(bNew ? LOCTEXT("NewSave", "New Save") : UALKSaveSlotCard::GetSlotLabel(Info));
	}
	// A new save shows the view it will keep (taken as the pause menu opened).
	PreviewTexture = bNew ? Saves->GetCurrentViewThumbnail() : Saves->GetThumbnail(Info.SlotName);
	if (PreviewImage)
	{
		if (PreviewTexture)
		{
			PreviewImage->SetBrush(FALKWidgetBuilder::MakeTextureBrush(PreviewTexture));
		}
		PreviewImage->SetVisibility(PreviewTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	const int32 ChapterIndex = Config->Chapters.IndexOfByPredicate([&Info](const FALKChapter& Chapter) { return Chapter.Id == Info.ChapterId; });
	if (ChapterLabelText)
	{
		ChapterLabelText->SetText(ChapterIndex != INDEX_NONE ? FText::Format(LOCTEXT("ChapterNumber", "Chapter {0}"), FText::AsNumber(ChapterIndex + 1)) : FText::GetEmpty());
		ChapterLabelText->SetVisibility(ChapterIndex != INDEX_NONE && Config->bNumberChapters ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ChapterNameText)
	{
		const FText Name = ChapterIndex != INDEX_NONE ? Config->Chapters[ChapterIndex].Name
			: (!Info.ChapterName.IsEmpty() ? Info.ChapterName : FText::FromString(FPackageName::GetShortName(Info.LevelName)));
		ChapterNameText->SetText(Name);
	}

	const FText Difficulties[] = { LOCTEXT("Easy", "Easy"), LOCTEXT("Normal", "Normal"), LOCTEXT("Hard", "Hard") };
	SetDetailRow(0, LOCTEXT("DetailPlaytime", "Playtime"), UALKSaveSlotCard::FormatPlaytime(Info.PlaytimeSeconds));
	SetDetailRow(1, LOCTEXT("DetailSaved", "Saved"), bNew ? LOCTEXT("DetailNow", "Now")
		: FText::Format(LOCTEXT("DetailSavedValue", "{0}  ({1})"), UALKSaveSlotCard::FormatDate(Info.SavedAt), UALKSaveSlotCard::FormatAge(Info.SavedAt)));
	SetDetailRow(2, LOCTEXT("Difficulty", "Difficulty"), bNew ? FText::GetEmpty() : Difficulties[FMath::Clamp(Info.Difficulty, 0, 2)]);
	SetDetailRow(3, LOCTEXT("DetailSummary", "Objective"), Info.Summary);

	const int32 ChapterCount = Config->Chapters.Num();
	if (UWidget* ProgressRow = WidgetTree ? WidgetTree->FindWidget(TEXT("ProgressRow")) : nullptr)
	{
		ProgressRow->SetVisibility(ChapterCount > 0 && ChapterIndex != INDEX_NONE ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ProgressBar && ChapterCount > 0)
	{
		ProgressBar->SetValue(static_cast<float>(ChapterIndex + 1) / ChapterCount);
	}
	if (ProgressText)
	{
		ProgressText->SetText(FText::FromString(FString::Printf(TEXT("%02d / %02d"), ChapterIndex + 1, ChapterCount)));
	}
}

void UALKSaveSlotScreen::ActivateFocusedCard()
{
	HandleCardClicked(FocusedCard.Get());
}

void UALKSaveSlotScreen::HandleCardClicked(UALKSaveSlotCard* Card)
{
	UALKSaveSubsystem* Saves = GetSaveSubsystem();
	UALKMenuSubsystem* Menu = GetMenuSubsystem();
	if (!Card || !Saves || !Menu || !Card->GetIsEnabled())
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	const FALKSaveSlotInfo Info = Card->GetSaveInfo();
	TWeakObjectPtr<UALKSaveSlotScreen> WeakThis(this);

	if (!bSaveMode)
	{
		// Loading while playing loses what was not saved: ask first.
		if (Saves->IsInGame() && GetMenuConfig()->bConfirmLoadInGame)
		{
			FALKDialogDescriptor Descriptor;
			Descriptor.Title = FText::Format(LOCTEXT("LoadConfirmTitle", "Load {0}?"), UALKSaveSlotCard::GetSlotLabel(Info));
			Descriptor.Message = LOCTEXT("LoadConfirmMessage", "Progress since your last save will be lost.");
			Descriptor.ConfirmText = LOCTEXT("HintLoad", "Load");
			Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
			Descriptor.bDestructive = true;
			const FString SlotName = Info.SlotName;
			Menu->ShowConfirmDialog(Descriptor, [WeakThis, SlotName](EALKDialogResult Result)
			{
				UALKSaveSlotScreen* Self = WeakThis.Get();
				UALKSaveSubsystem* SaveSubsystem = Self ? Self->GetSaveSubsystem() : nullptr;
				if (Result == EALKDialogResult::Confirmed && SaveSubsystem)
				{
					SaveSubsystem->LoadSave(SlotName);
				}
			});
			return;
		}
		if (!Saves->LoadSave(Info.SlotName))
		{
			PlayThemeSound(EALKUISound::Locked);
		}
		return;
	}

	if (Card->IsNewSaveCard())
	{
		const int32 Number = Saves->GetFreeManualSlot();
		RefocusSlot = UALKSaveSubsystem::MakeSlotName(EALKSaveSlotType::Manual, Number);
		if (Number == INDEX_NONE || !Saves->SaveToManualSlot(Number))
		{
			RefocusSlot.Reset();
			PlayThemeSound(EALKUISound::Locked);
		}
		return;
	}

	// Overwriting a save: ask first.
	FALKDialogDescriptor Descriptor;
	Descriptor.Title = FText::Format(LOCTEXT("OverwriteTitle", "Overwrite {0}?"), UALKSaveSlotCard::GetSlotLabel(Info));
	Descriptor.Message = FText::Format(LOCTEXT("OverwriteMessage", "{0}, {1} played, will be replaced by the game as it is now."),
		UALKSaveSlotCard::GetChapterLine(Info), UALKSaveSlotCard::FormatPlaytime(Info.PlaytimeSeconds));
	Descriptor.ConfirmText = LOCTEXT("Overwrite", "Overwrite");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = true;
	const int32 Number = Info.Number;
	const FString SlotName = Info.SlotName;
	Menu->ShowConfirmDialog(Descriptor, [WeakThis, Number, SlotName](EALKDialogResult Result)
	{
		UALKSaveSlotScreen* Self = WeakThis.Get();
		UALKSaveSubsystem* SaveSubsystem = Self ? Self->GetSaveSubsystem() : nullptr;
		if (Result == EALKDialogResult::Confirmed && SaveSubsystem)
		{
			Self->RefocusSlot = SlotName;
			if (!SaveSubsystem->SaveToManualSlot(Number))
			{
				Self->RefocusSlot.Reset();
				Self->PlayThemeSound(EALKUISound::Locked);
			}
		}
	});
}

void UALKSaveSlotScreen::DeleteFocusedSave()
{
	UALKSaveSlotCard* Card = FocusedCard.Get();
	UALKMenuSubsystem* Menu = GetMenuSubsystem();
	if (!Card || Card->IsNewSaveCard() || !Card->GetIsEnabled() || !Menu)
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	const FALKSaveSlotInfo Info = Card->GetSaveInfo();
	FALKDialogDescriptor Descriptor;
	Descriptor.Title = FText::Format(LOCTEXT("DeleteSaveTitle", "Delete {0}?"), UALKSaveSlotCard::GetSlotLabel(Info));
	Descriptor.Message = FText::Format(LOCTEXT("DeleteSaveMessage", "{0}, {1} played, will be gone for good."),
		UALKSaveSlotCard::GetChapterLine(Info), UALKSaveSlotCard::FormatPlaytime(Info.PlaytimeSeconds));
	Descriptor.ConfirmText = LOCTEXT("HintDelete", "Delete");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = true;
	TWeakObjectPtr<UALKSaveSlotScreen> WeakThis(this);
	const FString SlotName = Info.SlotName;
	Menu->ShowConfirmDialog(Descriptor, [WeakThis, SlotName](EALKDialogResult Result)
	{
		UALKSaveSlotScreen* Self = WeakThis.Get();
		UALKSaveSubsystem* SaveSubsystem = Self ? Self->GetSaveSubsystem() : nullptr;
		if (Result == EALKDialogResult::Confirmed && SaveSubsystem)
		{
			SaveSubsystem->DeleteSave(SlotName);
		}
	});
}

bool UALKSaveSlotScreen::NativeHandleMenuKey(const FKey& Key)
{
	if (Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Top)
	{
		DeleteFocusedSave();
		return true;
	}
	return Super::NativeHandleMenuKey(Key);
}

void UALKSaveSlotScreen::HandleSavesChanged()
{
	RefreshSlots();
}

#undef LOCTEXT_NAMESPACE
