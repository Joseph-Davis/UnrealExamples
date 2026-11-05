// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Loading/ALKLoadingScreenSubsystem.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "MoviePlayer.h"
#include "UnrealClient.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DelayedAutoRegister.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLoadingScreenSubsystem)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

/** Pure Slate loading screen: it runs on the movie player thread while the game thread is busy loading. */
class SALKLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SALKLoadingScreen) {}
		SLATE_ARGUMENT(const UALKTheme*, Theme)
		SLATE_ARGUMENT(FText, LoadingText)
		SLATE_ARGUMENT(FText, Tip)
		SLATE_ARGUMENT(UTexture2D*, GrainTexture)
		SLATE_ARGUMENT(UTexture2D*, VignetteTexture)
		/** The game UI's scale for this viewport (DPI curve and Interface Scale); 0 keeps the movie player's own. */
		SLATE_ARGUMENT(float, UIScale)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		const UALKTheme* Theme = InArgs._Theme ? InArgs._Theme : GetDefault<UALKTheme>();

		BackgroundBrush = UALKTheme::MakeSolidBrush(FMath::Lerp(Theme->BackgroundTop, Theme->BackgroundBottom, 0.65f));
		// Round pieces and a short accent bar, like the menu's titles.
		PieceBrush = UALKTheme::MakePillBrush(Theme->Accent);
		PieceBrush.ImageSize = FVector2D(7.0, 7.0);
		AccentBrush = UALKTheme::MakeSolidBrush(Theme->Accent);
		GrainBrush = FALKWidgetBuilder::MakeTextureBrush(InArgs._GrainTexture, FLinearColor(1.f, 1.f, 1.f, Theme->BackgroundGrain * 0.16f), true);
		VignetteBrush = FALKWidgetBuilder::MakeTextureBrush(InArgs._VignetteTexture, FLinearColor(1.f, 1.f, 1.f, Theme->BackgroundVignette));
		const EVisibility GrainVisibility = InArgs._GrainTexture && Theme->BackgroundGrain > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		const EVisibility VignetteVisibility = InArgs._VignetteTexture && Theme->BackgroundVignette > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;

		FSlateFontInfo LoadingFont = Theme->ButtonFont;
		FSlateFontInfo TipFont = Theme->BodyFont;
		const FMargin Padding = Theme->ScreenPadding;
		const float TipWidth = 760.f;
		TargetScale = InArgs._UIScale;

		ChildSlot
		[
			SNew(SDPIScaler)
			.DPIScale(TAttribute<float>::CreateSP(this, &SALKLoadingScreen::GetContentScale))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(&BackgroundBrush)
				]
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(&GrainBrush)
					.Visibility(GrainVisibility)
				]
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(&VignetteBrush)
					.Visibility(VignetteVisibility)
				]
				+ SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(Padding.Left, 0.f, Padding.Right, Padding.Bottom))
				[
					// The tip and the loading text share one row, so a long tip (or a long translation) wraps in the space
					// left of the loading text instead of running under it.
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Bottom)
					.Padding(0.f, 0.f, 48.f, 0.f)
					[
						// A fixed column (narrower only when the screen is): a column sized to its wrapped text would never
						// grow back after one narrow frame.
						SNew(SBox)
						.WidthOverride(TipWidth)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Left)
							[
								SNew(SBox)
								.WidthOverride(56.f)
								.HeightOverride(4.f)
								[
									SNew(SBorder)
									.BorderImage(&AccentBrush)
								]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.f, 14.f, 0.f, 0.f)
							[
								// Wraps at the space it gets, and never wider than the tip column, from the first frame on.
								SNew(STextBlock)
								.Text(InArgs._Tip)
								.Font(TipFont)
								.ColorAndOpacity(Theme->TextSecondary)
								.AutoWrapText(true)
								.WrapTextAt(TipWidth)
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.f, 0.f, 18.f, 0.f)
						[
							SNew(STextBlock)
							.Text(InArgs._LoadingText)
							.Font(LoadingFont)
							.ColorAndOpacity(Theme->TextPrimary)
							.TransformPolicy(Theme->bUppercaseButtons ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SCircularThrobber)
							.PieceImage(&PieceBrush)
							.NumPieces(8)
							.Radius(14.f)
							.Period(1.1f)
						]
						]
					]
				]
			]
		];
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		// The movie player draws with the monitor's Windows scaling, on its own thread and again through the game
		// window while it waits out the minimum display time. Undo whatever scale the parents apply, so the loading
		// screen always matches the menus around it.
		if (TargetScale > 0.f && AllottedGeometry.Scale > 0.f)
		{
			ContentScale = TargetScale / AllottedGeometry.Scale;
		}
	}

private:
	float GetContentScale() const
	{
		return ContentScale;
	}

	float TargetScale = 0.f;
	float ContentScale = 1.f;
	FSlateBrush BackgroundBrush;
	FSlateBrush PieceBrush;
	FSlateBrush AccentBrush;
	FSlateBrush GrainBrush;
	FSlateBrush VignetteBrush;
};

namespace ALKLoadingScreenPrivate
{
	static int32 ShownCount = 0;
}

int32 UALKLoadingScreenSubsystem::GetShownCount()
{
	return ALKLoadingScreenPrivate::ShownCount;
}

bool UALKLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UALKLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The movie player starts itself on PreLoadMap and asks for a loading screen through this event when none is
	// prepared, so there is no delegate ordering to worry about.
	if (!GIsEditor && IsMoviePlayerEnabled())
	{
		if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
		{
			PrepareLoadingScreenHandle = MoviePlayer->OnPrepareLoadingScreen().AddUObject(this, &ThisClass::HandlePrepareLoadingScreen);
		}
#if WITH_EDITOR
		// A game started from the editor executable scans all content once the first level has loaded (the asset
		// registry does it on OnFEngineLoopInitComplete), long after the loading screen has closed. Delegates run
		// newest first, so this handler runs before that scan and can bring the loading screen back for it.
		EngineLoopInitCompleteHandle = FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &ThisClass::HandleEngineLoopInitComplete);
#endif
	}
}

void UALKLoadingScreenSubsystem::Deinitialize()
{
	if (PrepareLoadingScreenHandle.IsValid())
	{
		if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
		{
			MoviePlayer->OnPrepareLoadingScreen().Remove(PrepareLoadingScreenHandle);
		}
		PrepareLoadingScreenHandle.Reset();
	}
	FCoreDelegates::OnFEngineLoopInitComplete.Remove(EngineLoopInitCompleteHandle);
	EngineLoopInitCompleteHandle.Reset();
	Super::Deinitialize();
}

void UALKLoadingScreenSubsystem::HandlePrepareLoadingScreen()
{
	SetupLoadingScreen(false, GetDefault<UALKDeveloperSettings>()->MinimumLoadingScreenDisplayTime);
}

bool UALKLoadingScreenSubsystem::SetupLoadingScreen(bool bReuseTip, float MinimumDisplayTime)
{
	if (!GetDefault<UALKDeveloperSettings>()->bEnableLoadingScreen)
	{
		return false;
	}

	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	if (!bReuseTip)
	{
		CurrentTip = Config->LoadingTips.Num() > 0 ? Config->LoadingTips[FMath::RandRange(0, Config->LoadingTips.Num() - 1)] : FText::GetEmpty();
	}

	// Kept here while the screen shows: loading a level collects garbage.
	OverlayTextures = { FALKWidgetBuilder::GetOverlayTexture(FALKWidgetBuilder::EOverlayTexture::Grain), FALKWidgetBuilder::GetOverlayTexture(FALKWidgetBuilder::EOverlayTexture::Vignette) };

	// The game UI's scale for this viewport (DPI curve and Interface Scale): the loading screen draws at it, so it matches
	// the menus instead of the monitor's Windows scaling.
	FIntPoint ViewportSize(0, 0);
	if (GEngine && GEngine->GameViewport)
	{
		if (GEngine->GameViewport->Viewport)
		{
			ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		}
		// Before the first level the viewport has no size yet; the game window has.
		const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
		if ((ViewportSize.X <= 0 || ViewportSize.Y <= 0) && Window.IsValid())
		{
			const FVector2D ClientSize = Window->GetClientSizeInScreen();
			ViewportSize = FIntPoint(FMath::RoundToInt(ClientSize.X), FMath::RoundToInt(ClientSize.Y));
		}
	}
	const float UIScale = ViewportSize.X > 0 && ViewportSize.Y > 0 ? GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(ViewportSize) : 0.f;
	UE_LOG(LogALK, Log, TEXT("Loading screen drawn at the game UI scale %.3f (%d x %d)."), UIScale, ViewportSize.X, ViewportSize.Y);

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.MinimumLoadingScreenDisplayTime = MinimumDisplayTime;
	Attributes.WidgetLoadingScreen = SNew(SALKLoadingScreen)
		.Theme(UALKDeveloperSettings::GetTheme())
		.LoadingText(Config->LoadingText)
		.Tip(CurrentTip)
		.GrainTexture(OverlayTextures[0])
		.VignetteTexture(OverlayTextures[1])
		.UIScale(UIScale);

	GetMoviePlayer()->SetupLoadingScreen(Attributes);
	++ALKLoadingScreenPrivate::ShownCount;
	UE_LOG(LogALK, Log, TEXT("Loading screen prepared (%d this session)."), ALKLoadingScreenPrivate::ShownCount);
	return true;
}

void UALKLoadingScreenSubsystem::HandleEngineLoopInitComplete()
{
	FCoreDelegates::OnFEngineLoopInitComplete.Remove(EngineLoopInitCompleteHandle);
	EngineLoopInitCompleteHandle.Reset();

	// The same screen and tip as the first level's, with no minimum time: that one has been met already.
	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
	if (!MoviePlayer || MoviePlayer->IsMovieCurrentlyPlaying() || !SetupLoadingScreen(true, -1.f) || !MoviePlayer->PlayMovie())
	{
		return;
	}

	// The engine must not start a frame while a loading screen plays, so close it at the end of engine
	// initialization, which comes after the scan, and give the window back to the game.
	FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::EndOfEngineInit, TFunction<void()>([]()
	{
		GetMoviePlayer()->WaitForMovieToFinish();
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			GameEngine->SwitchGameWindowToUseGameViewport();
		}
	}));
	UE_LOG(LogALK, Log, TEXT("Loading screen kept up while the editor build scans content."));
}

#undef LOCTEXT_NAMESPACE
