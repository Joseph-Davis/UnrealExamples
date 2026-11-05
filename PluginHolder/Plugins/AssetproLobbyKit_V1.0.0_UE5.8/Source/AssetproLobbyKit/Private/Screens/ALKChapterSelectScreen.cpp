// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKChapterSelectScreen.h"
#include "Widgets/ALKTitleText.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKChapterStill.h"
#include "Widgets/ALKChapterStrip.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKChapterSelectScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKChapterSelectPrivate
{
	/** Seconds the still takes to slide in after browsing. */
	static constexpr float SlideTime = 0.3f;
	/** Pixels it slides. */
	static constexpr float SlideDistance = 26.f;
}

UALKChapterSelectScreen::UALKChapterSelectScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

void UALKChapterSelectScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	// The scene steps back (Page Dim) so the still stands out.
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root, TEXT("BackgroundImage"), true);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ChapterColumn"));
	FALKWidgetBuilder::AddToOverlay(Root, Column, HAlign_Center, VAlign_Center);

	TitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("TitleText"), MakeSystemLabel(12, LOCTEXT("ChapterSelectTitle", "Select Chapter")));
	TitleText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleText, FMargin(0.f, 0.f, 0.f, 22.f), HAlign_Center);

	Strip = FALKWidgetBuilder::Make<UALKChapterStrip>(Tree, TEXT("Strip"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Strip, FMargin(0.f, 0.f, 0.f, 22.f), HAlign_Center);

	UHorizontalBox* StillRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("StillRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, StillRow, FMargin(), HAlign_Center);

	PreviousButton = CreateMenuButton(LOCTEXT("PreviousChapter", "<"), EALKButtonStyle::Arrow);
	PreviousButton->SetIsFocusable(false);
	PreviousButton->bPlayClickSound = false;
	FALKWidgetBuilder::AddToHorizontalBox(StillRow, PreviousButton, FMargin(0.f, 0.f, 26.f, 0.f), HAlign_Center, VAlign_Center);

	Still = Tree.ConstructWidget<UALKChapterStill>(UALKChapterStill::StaticClass(), TEXT("Still"));
	StillSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("StillSize"));
	StillSize->SetWidthOverride(880.f);
	StillSize->SetHeightOverride(495.f);
	StillSize->SetContent(Still);
	FALKWidgetBuilder::AddToHorizontalBox(StillRow, StillSize, FMargin(), HAlign_Center, VAlign_Center);

	NextButton = CreateMenuButton(LOCTEXT("NextChapter", ">"), EALKButtonStyle::Arrow);
	NextButton->SetIsFocusable(false);
	NextButton->bPlayClickSound = false;
	FALKWidgetBuilder::AddToHorizontalBox(StillRow, NextButton, FMargin(26.f, 0.f, 0.f, 0.f), HAlign_Center, VAlign_Center);

	NumberText = FALKWidgetBuilder::MakeText(Tree, TEXT("NumberText"));
	NumberText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, NumberText, FMargin(0.f, 26.f, 0.f, 0.f), HAlign_Center);

	NameText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("NameText"));
	NameText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, NameText, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Center);

	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyBrowse", "< >"), LOCTEXT("PadBrowse", "D-Pad"), LOCTEXT("HintBrowse", "Browse")),
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("HintPlay", "Play")),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
	FALKWidgetBuilder::AddToOverlay(Root, Hints, HAlign_Left, VAlign_Bottom);
}

void UALKChapterSelectScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	if (Strip)
	{
		Strip->OnSegmentClicked.AddUObject(this, &ThisClass::HandleSegmentClicked);
	}
	if (Still)
	{
		Still->OnClicked().AddUObject(this, &ThisClass::PlayShownChapter);
		FCustomWidgetNavigationDelegate Navigate;
		Navigate.BindDynamic(this, &ThisClass::HandleStillNavigation);
		Still->SetNavigationRuleCustom(EUINavigation::Left, Navigate);
		Still->SetNavigationRuleCustom(EUINavigation::Right, Navigate);
		Still->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
		Still->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
		DefaultFocusWidget = Still;
	}
	if (PreviousButton)
	{
		PreviousButton->OnClicked().AddWeakLambda(this, [this]() { BrowseChapters(-1); });
	}
	if (NextButton)
	{
		NextButton->OnClicked().AddWeakLambda(this, [this]() { BrowseChapters(1); });
	}
	RefreshLocks();
}

void UALKChapterSelectScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// Listens while on screen: pooled screens are removed and added again.
	UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	if (MenuSubsystem && !ProgressChangedHandle.IsValid())
	{
		ProgressChangedHandle = MenuSubsystem->OnChapterProgressChanged.AddUObject(this, &ThisClass::HandleProgressChanged);
	}
	RefreshLocks();
}

void UALKChapterSelectScreen::NativeDestruct()
{
	if (UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem())
	{
		MenuSubsystem->OnChapterProgressChanged.Remove(ProgressChangedHandle);
	}
	ProgressChangedHandle.Reset();
	Super::NativeDestruct();
}

void UALKChapterSelectScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	StyleScreenBackground(Theme);

	if (WidgetTree)
	{
		if (UWidget* Hints = HintButtons.Num() > 0 && HintButtons[0] ? HintButtons[0]->GetParent() : nullptr)
		{
			if (UOverlaySlot* HintSlot = Cast<UOverlaySlot>(Hints->Slot))
			{
				HintSlot->SetPadding(FMargin(Theme.ScreenPadding.Left, 0.f, 0.f, FMath::Max(24.f, Theme.ScreenPadding.Bottom * 0.55f)));
			}
		}
	}

	if (TitleText)
	{
		// The screen's optional small label (see System Label).
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Data, Theme.Accent);
		TitleText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	FALKWidgetBuilder::StyleText(NumberText, Theme, EALKTextRole::Data, Theme.AccentAlt);
	if (NumberText)
	{
		NumberText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	if (NameText)
	{
		FALKWidgetBuilder::StyleText(NameText, Theme, EALKTextRole::Heading);
		FSlateFontInfo NameFont = Theme.HeadingFont;
		NameFont.Size = FMath::Max(12.f, FMath::RoundToFloat(NameFont.Size * 0.72f));
		FALKWidgetBuilder::SetTextFont(NameText, NameFont);
	}
	if (Strip)
	{
		Strip->ApplyTheme(Theme);
	}
	UpdateShownChapter(false, 0);
}

void UALKChapterSelectScreen::NativeOnActivated()
{
	RefreshLocks();
	const UALKMenuConfig* Config = GetMenuConfig();
	ShownIndex = Config->Chapters.IsValidIndex(RememberedIndex) ? RememberedIndex : GetDefaultIndex();
	UpdateShownChapter(false, 0);
	Super::NativeOnActivated();
	bFocusPending = true;
}

int32 UALKChapterSelectScreen::GetDefaultIndex() const
{
	// The furthest chapter the player has reached.
	const UALKMenuConfig* Config = GetMenuConfig();
	const UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	for (int32 Index = Config->Chapters.Num() - 1; Index >= 0; --Index)
	{
		if (MenuSubsystem && MenuSubsystem->IsChapterUnlocked(Config->Chapters[Index].Id))
		{
			return Index;
		}
	}
	return Config->Chapters.Num() > 0 ? 0 : INDEX_NONE;
}

UWidget* UALKChapterSelectScreen::NativeGetDesiredFocusTarget() const
{
	return Still ? static_cast<UWidget*>(Still) : Super::NativeGetDesiredFocusTarget();
}

FReply UALKChapterSelectScreen::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	// The picture is the only thing here that takes focus, so a click on the strip, an arrow or empty space (which
	// hands focus to the screen) leaves it on the picture.
	if (InFocusEvent.GetCause() == EFocusCause::Mouse && Still)
	{
		if (TSharedPtr<SWidget> StillWidget = Still->GetCachedWidget())
		{
			return FReply::Handled().SetUserFocus(StillWidget.ToSharedRef(), EFocusCause::SetDirectly);
		}
	}
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void UALKChapterSelectScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The still takes a share of the screen's width, 16:9, but never more than 60% of its height (on super-ultrawide
	// screens, 32:9, a share of the width alone would push the chapter name off the bottom); the strip matches it.
	const FVector2D Size = MyGeometry.GetLocalSize();
	const float Width = Size.X;
	if (Width > 1.f && Size.Y > 1.f && (!FMath::IsNearlyEqual(Width, LastLayoutWidth) || !FMath::IsNearlyEqual(Size.Y, LastLayoutHeight)) && StillSize)
	{
		LastLayoutWidth = Width;
		LastLayoutHeight = Size.Y;
		const UALKTheme* Theme = GetTheme();
		const float StillWidth = FMath::RoundToFloat(FMath::Min(Width * (Theme ? Theme->ChapterStillWidth : 0.46f), Size.Y * 0.6f * 16.f / 9.f));
		StillSize->SetWidthOverride(StillWidth);
		StillSize->SetHeightOverride(FMath::RoundToFloat(StillWidth * 9.f / 16.f));
		if (Strip)
		{
			Strip->MaxWidth = StillWidth;
			Strip->SetCurrent(ShownIndex);
		}
	}

	// Focus the still once it has been laid out: CommonUI puts the gamepad cursor at the focused widget's centre.
	if (bFocusPending && Still)
	{
		const FVector2D StillExtent = Still->GetCachedGeometry().GetLocalSize();
		if (StillExtent.X > 1.0 && StillExtent.Y > 1.0)
		{
			bFocusPending = false;
			Still->SetFocus();
		}
	}

	// After browsing, the new still slides in from the side it came from.
	if (SlideElapsed >= 0.f && Still)
	{
		SlideElapsed += InDeltaTime;
		const float Alpha = FMath::Clamp(SlideElapsed / ALKChapterSelectPrivate::SlideTime, 0.f, 1.f);
		const float Ease = 1.f - FMath::Square(1.f - Alpha);
		Still->SetRenderTranslation(FVector2D(SlideDirection * ALKChapterSelectPrivate::SlideDistance * (1.f - Ease), 0.0));
		Still->SetRenderOpacity(FMath::Lerp(0.35f, 1.f, Ease));
		if (Alpha >= 1.f)
		{
			SlideElapsed = -1.f;
		}
	}
}

FReply UALKChapterSelectScreen::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!FMath::IsNearlyZero(InMouseEvent.GetWheelDelta()))
	{
		BrowseChapters(InMouseEvent.GetWheelDelta() > 0.f ? -1 : 1);
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

UWidget* UALKChapterSelectScreen::HandleStillNavigation(EUINavigation Direction)
{
	if (Direction == EUINavigation::Left || Direction == EUINavigation::Right)
	{
		BrowseChapters(Direction == EUINavigation::Left ? -1 : 1);
	}
	// Focus stays on the still.
	return Still;
}

bool UALKChapterSelectScreen::BrowseChapters(int32 Direction)
{
	const int32 Count = GetMenuConfig()->Chapters.Num();
	const int32 Target = FMath::Clamp(ShownIndex + (Direction < 0 ? -1 : 1), 0, FMath::Max(0, Count - 1));
	if (Count == 0 || Target == ShownIndex)
	{
		return false;
	}
	ShownIndex = Target;
	UpdateShownChapter(true, Direction);
	PlayThemeSound(EALKUISound::Tab);
	return true;
}

void UALKChapterSelectScreen::ShowChapterAt(int32 Index)
{
	if (GetMenuConfig()->Chapters.IsValidIndex(Index) && Index != ShownIndex)
	{
		const int32 Direction = Index < ShownIndex ? -1 : 1;
		ShownIndex = Index;
		UpdateShownChapter(true, Direction);
	}
}

void UALKChapterSelectScreen::ShowChapter(FName ChapterId)
{
	const TArray<FALKChapter>& Chapters = GetMenuConfig()->Chapters;
	ShowChapterAt(Chapters.IndexOfByPredicate([ChapterId](const FALKChapter& Chapter) { return Chapter.Id == ChapterId; }));
}

void UALKChapterSelectScreen::HandleSegmentClicked(int32 Index)
{
	if (Index != ShownIndex && GetMenuConfig()->Chapters.IsValidIndex(Index))
	{
		ShowChapterAt(Index);
		PlayThemeSound(EALKUISound::Tab);
	}
}

FName UALKChapterSelectScreen::GetShownChapterId() const
{
	const TArray<FALKChapter>& Chapters = GetMenuConfig()->Chapters;
	return Chapters.IsValidIndex(ShownIndex) ? Chapters[ShownIndex].Id : NAME_None;
}

bool UALKChapterSelectScreen::IsShownChapterLocked() const
{
	const UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	const FName ChapterId = GetShownChapterId();
	return ChapterId.IsNone() || !MenuSubsystem || !MenuSubsystem->IsChapterUnlocked(ChapterId);
}

void UALKChapterSelectScreen::PlayShownChapter()
{
	if (IsShownChapterLocked())
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	PlayThemeSound(EALKUISound::Click);
	if (UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem())
	{
		MenuSubsystem->StartChapter(GetShownChapterId());
	}
}

void UALKChapterSelectScreen::RefreshLocks()
{
	const UALKMenuConfig* Config = GetMenuConfig();
	const UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	TArray<bool> Unlocked;
	for (const FALKChapter& Chapter : Config->Chapters)
	{
		Unlocked.Add(MenuSubsystem && MenuSubsystem->IsChapterUnlocked(Chapter.Id));
	}
	if (Strip)
	{
		Strip->SetChapters(Unlocked);
	}
	UpdateShownChapter(false, 0);
}

void UALKChapterSelectScreen::HandleProgressChanged()
{
	RefreshLocks();
}

void UALKChapterSelectScreen::UpdateShownChapter(bool bAnimate, int32 Direction)
{
	const UALKMenuConfig* Config = GetMenuConfig();
	if (!Config->Chapters.IsValidIndex(ShownIndex))
	{
		if (NameText)
		{
			NameText->SetText(FText::GetEmpty());
		}
		return;
	}
	RememberedIndex = ShownIndex;

	const FALKChapter& Chapter = Config->Chapters[ShownIndex];
	const bool bLocked = IsShownChapterLocked();
	ShownTexture = Chapter.Image.LoadSynchronous();
	if (Still)
	{
		Still->SetStill(ShownTexture, bLocked);
	}
	if (Strip)
	{
		Strip->SetCurrent(ShownIndex);
	}
	if (NumberText)
	{
		NumberText->SetText(FText::Format(LOCTEXT("ChapterNumber", "Chapter {0}"), FText::AsNumber(ShownIndex + 1)));
		NumberText->SetVisibility(Config->bNumberChapters ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (NameText)
	{
		const FText Name = bLocked ? LOCTEXT("ChapterLocked", "Locked") : Chapter.Name;
		UALKTitleText* NameFx = Cast<UALKTitleText>(NameText);
		if (NameFx && !NameText->GetText().EqualTo(Name) && !NameText->GetText().IsEmpty())
		{
			// Browsing to another chapter fades the new name in.
			NameFx->PlayIntro(0.2f);
		}
		NameText->SetText(Name);
		if (const UALKTheme* Theme = GetTheme())
		{
			NameText->SetColorAndOpacity(FSlateColor(bLocked ? Theme->TextMuted : Theme->TextPrimary));
		}
	}
	if (PreviousButton)
	{
		PreviousButton->SetVisibility(ShownIndex > 0 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	if (NextButton)
	{
		NextButton->SetVisibility(ShownIndex < Config->Chapters.Num() - 1 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (bAnimate)
	{
		SlideElapsed = 0.f;
		SlideDirection = Direction < 0 ? -1.f : 1.f;
	}
	else if (Still)
	{
		SlideElapsed = -1.f;
		Still->SetRenderTranslation(FVector2D::ZeroVector);
		Still->SetRenderOpacity(1.f);
	}
}

#undef LOCTEXT_NAMESPACE
