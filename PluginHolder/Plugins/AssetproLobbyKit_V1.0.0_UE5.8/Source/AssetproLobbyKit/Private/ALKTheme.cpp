// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "ALKTheme.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "Screens/ALKScreen.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKTheme)

namespace ALKThemePrivate
{
	/** sRGB hex -> linear colour, so values match what artists pick in a colour picker. */
	static FLinearColor Hex(const TCHAR* InHex, float Alpha = 1.f)
	{
		FLinearColor Color(FColor::FromHex(InHex));
		Color.A = Alpha;
		return Color;
	}

	static FSlateFontInfo MakeFont(UObject* FontObject, const TCHAR* Typeface, int32 Size)
	{
		FSlateFontInfo Info(FontObject, Size, FName(Typeface));
		return Info;
	}

	/** Where the skins' backdrop pictures live (T_ALK_Backdrop_Default, _Light, _Arcade). */
	static const TCHAR* BackdropPictureFolder = TEXT("/AssetproLobbyKit/Backdrops/");

	/** When one action asks for several sounds in a frame, the most important one plays. */
	static int32 GetUISoundPriority(EALKUISound Sound)
	{
		switch (Sound)
		{
		case EALKUISound::MatchFound: return 7;
		case EALKUISound::Error: return 6;
		case EALKUISound::Back: return 5;
		case EALKUISound::Click: return 4;
		case EALKUISound::Ready: return 4;
		case EALKUISound::Locked: return 4;
		case EALKUISound::Tab: return 3;
		case EALKUISound::Change: return 2;
		case EALKUISound::ScreenOpen: return 1;
		default: return 0;
		}
	}

	static TOptional<EALKUISound> PendingUISound;
	static TWeakObjectPtr<UWorld> PendingUISoundWorld;
	static bool bAnyUISoundPlayed = false;
	static EALKUISound LastUISound = EALKUISound::Hover;
	static uint64 LastUISoundFrame = 0;
	static double LastUISoundTime = 0.0;
	static double LastChangeSoundTime = -1000.0;

	static bool PlayPendingUISound(float DeltaTime)
	{
		const TOptional<EALKUISound> Kind = PendingUISound;
		UWorld* World = PendingUISoundWorld.Get();
		PendingUISound.Reset();
		PendingUISoundWorld.Reset();

		const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
		USoundBase* Sound = Kind.IsSet() && Theme ? Theme->GetUISound(Kind.GetValue()) : nullptr;
		if (!World || !Sound)
		{
			return false;
		}

		// The settings' sound mix already scales sounds of the Menu Config's Interface sound class.
		float Volume = 1.f;
		const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
		const USoundClass* SoundClass = Sound->GetSoundClass();
		if (!Config || !SoundClass || FSoftObjectPath(SoundClass) != Config->InterfaceSoundClass.ToSoftObjectPath())
		{
			const UGameInstance* GameInstance = World->GetGameInstance();
			if (const UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr)
			{
				Volume = Settings->GetVolume(EALKVolumeChannel::Interface);
			}
		}

		bAnyUISoundPlayed = true;
		LastUISound = Kind.GetValue();
		LastUISoundFrame = GFrameCounter;
		LastUISoundTime = FPlatformTime::Seconds();
		if (LastUISound == EALKUISound::Change)
		{
			LastChangeSoundTime = LastUISoundTime;
		}
		if (Volume > KINDA_SMALL_NUMBER)
		{
			UGameplayStatics::PlaySound2D(World, Sound, Volume);
		}
		UALKTheme::OnUISoundPlayed().Broadcast(LastUISound);
		return false;
	}
}

UALKTheme::UALKTheme()
{
	// Two families, both under the SIL Open Font License (see Content/Fonts/Inter-OFL.txt and Oswald-OFL.txt): Oswald, a
	// condensed display face, for titles, headings and buttons, and Inter for everything that is read. Roboto stands in
	// for a font asset that is missing; without the display face the text face takes its place.
	static ConstructorHelpers::FObjectFinderOptional<UFont> InterFinder(TEXT("/AssetproLobbyKit/Fonts/F_ALK_Inter.F_ALK_Inter"));
	static ConstructorHelpers::FObjectFinderOptional<UFont> DisplayFinder(TEXT("/AssetproLobbyKit/Fonts/F_ALK_Display.F_ALK_Display"));
	static ConstructorHelpers::FObjectFinderOptional<UFont> RobotoFinder(TEXT("/Engine/EngineFonts/Roboto"));
	const bool bHaveInter = InterFinder.Succeeded();
	const bool bHaveDisplay = DisplayFinder.Succeeded();
	UObject* Font = bHaveInter ? static_cast<UObject*>(InterFinder.Get()) : static_cast<UObject*>(RobotoFinder.Get());
	UObject* Display = bHaveDisplay ? static_cast<UObject*>(DisplayFinder.Get()) : Font;
	const TCHAR* Medium = bHaveInter ? TEXT("Medium") : TEXT("Regular");
	const TCHAR* SemiBold = bHaveInter ? TEXT("SemiBold") : TEXT("Bold");
	const TCHAR* DisplayMedium = bHaveDisplay ? TEXT("Medium") : SemiBold;

	using namespace ALKThemePrivate;
	TitleFont = MakeFont(Display, TEXT("Bold"), bHaveDisplay ? 78 : 72);
	HeadingFont = MakeFont(Display, bHaveDisplay ? TEXT("SemiBold") : TEXT("Bold"), bHaveDisplay ? 44 : 40);
	SubheadingFont = MakeFont(Display, DisplayMedium, 22);
	BodyFont = MakeFont(Font, TEXT("Regular"), 18);
	// The display face is tall for its size: a little smaller. Its line box is taller still (room over the capitals and
	// under the baseline); FALKWidgetBuilder::SetTextFont trims that wherever a font goes on a text block, so rows, buttons
	// and titles keep the heights the layouts were made with.
	ButtonFont = MakeFont(Display, DisplayMedium, bHaveDisplay ? 26 : 28);
	CaptionFont = MakeFont(Font, Medium, 15);
	DataFont = MakeFont(Font, SemiBold, 14);
	SubtitleFont = MakeFont(Font, Medium, 26);

	// Roomy lists and rounded panels.
	ScreenPadding = FMargin(80.f, 56.f);
	MenuButtonWidth = 480.f;
	MenuButtonHeight = 56.f;
	MenuButtonSpacing = 8.f;
	ButtonContentPadding = FMargin(26.f, 6.f);
	SecondaryButtonHeight = 46.f;
	PauseMenuWidth = 500.f;
	DialogWidth = 680.f;
	SettingRowHeight = 56.f;
	SettingsDescriptionWidth = 520.f;
	ScreenFadeInTime = 0.22f;
	ScreenSlideOffset = 14.f;
	HoverTransitionSpeed = 18.f;
	CreditsScrollSpeed = 45.f;
	BackdropLayers.Reset();

	LockIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/AssetproLobbyKit/Icons/T_ALK_Icon_Lock.T_ALK_Icon_Lock")));

	// The interface sounds that ship with the plugin.
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> HoverFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Hover.S_ALK_Hover"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> SelectFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Select.S_ALK_Select"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> BackFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Back.S_ALK_Back"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> OpenFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Open.S_ALK_Open"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> TabFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Tab.S_ALK_Tab"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> ChangeFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Change.S_ALK_Change"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> LockedFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Locked.S_ALK_Locked"));
	HoverSound = HoverFinder.Get();
	ClickSound = SelectFinder.Get();
	BackSound = BackFinder.Get();
	ScreenOpenSound = OpenFinder.Get();
	TabSound = TabFinder.Get();
	ChangeSound = ChangeFinder.Get();
	LockedSound = LockedFinder.Get();
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> ReadyFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Ready.S_ALK_Ready"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> MatchFoundFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_MatchFound.S_ALK_MatchFound"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> NotifyFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Notify.S_ALK_Notify"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> CountdownFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Countdown.S_ALK_Countdown"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> ErrorFinder(TEXT("/AssetproLobbyKit/Sounds/S_ALK_Error.S_ALK_Error"));
	ReadySound = ReadyFinder.Get();
	MatchFoundSound = MatchFoundFinder.Get();
	NotifySound = NotifyFinder.Get();
	CountdownSound = CountdownFinder.Get();
	ErrorSound = ErrorFinder.Get();

	ApplySkin(EALKThemeSkin::Default, EALKThemePreset::Mint);
}

UALKTheme_Light::UALKTheme_Light()
{
	ApplySkin(EALKThemeSkin::Light, EALKThemePreset::Mint);
}

UALKTheme_Arcade::UALKTheme_Arcade()
{
	ApplySkin(EALKThemeSkin::Arcade, EALKThemePreset::Gold);
}

USoundBase* UALKTheme::GetUISound(EALKUISound Sound) const
{
	switch (Sound)
	{
	case EALKUISound::Hover: return HoverSound;
	case EALKUISound::Click: return ClickSound;
	case EALKUISound::Back: return BackSound;
	case EALKUISound::ScreenOpen: return ScreenOpenSound;
	case EALKUISound::Tab: return TabSound;
	case EALKUISound::Change: return ChangeSound;
	case EALKUISound::Locked: return LockedSound;
	case EALKUISound::Ready: return ReadySound;
	case EALKUISound::MatchFound: return MatchFoundSound;
	case EALKUISound::Notify: return NotifySound;
	case EALKUISound::Countdown: return CountdownSound;
	case EALKUISound::Error: return ErrorSound;
	default: return nullptr;
	}
}

void UALKTheme::PlayUISound(const UObject* WorldContextObject, EALKUISound Sound)
{
	using namespace ALKThemePrivate;
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
	if (!World || !World->IsGameWorld() || !Theme || !Theme->GetUISound(Sound))
	{
		return;
	}

	// Focus moves by itself while a screen opens and right after a click, a tab switch or a back (to the new screen, the
	// page's first setting, the item you came from): no Hover then. Nor an Open when a click or a back opened it.
	const double Now = FPlatformTime::Seconds();
	const bool bRightAfterAction = bAnyUISoundPlayed && LastUISound != EALKUISound::Hover
		&& (GFrameCounter - LastUISoundFrame <= 2 || Now - LastUISoundTime < 0.1);
	if (Sound == EALKUISound::Hover && (bRightAfterAction || UALKScreen::IsInActivationGracePeriod()))
	{
		return;
	}
	if (Sound == EALKUISound::ScreenOpen && bRightAfterAction)
	{
		return;
	}
	// Dragging a slider changes the value every frame: tick at most every 50 ms.
	if (Sound == EALKUISound::Change && Now - LastChangeSoundTime < 0.05)
	{
		return;
	}

	// The sounds one action asks for arrive in the same frame: play the most important one when the frame ends.
	if (!PendingUISound.IsSet())
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&PlayPendingUISound));
	}
	else if (GetUISoundPriority(Sound) <= GetUISoundPriority(PendingUISound.GetValue()))
	{
		return;
	}
	PendingUISound = Sound;
	PendingUISoundWorld = World;
}

TMulticastDelegate<void(EALKUISound)>& UALKTheme::OnUISoundPlayed()
{
	static TMulticastDelegate<void(EALKUISound)> Delegate;
	return Delegate;
}

void UALKTheme::ApplySelectedPreset()
{
	Modify();
	ApplySkin(SkinToApply, PresetToApply);
	OnAnyThemeChanged().Broadcast();
}

FLinearColor UALKTheme::GetPresetAccent(EALKThemePreset Preset)
{
	using namespace ALKThemePrivate;
	switch (Preset)
	{
	case EALKThemePreset::Azure: return Hex(TEXT("4DA3FF"));
	case EALKThemePreset::Ember: return Hex(TEXT("FF7A45"));
	case EALKThemePreset::Orchid: return Hex(TEXT("B18CFF"));
	case EALKThemePreset::Gold: return Hex(TEXT("F2C14E"));
	case EALKThemePreset::Mint:
	default: return Hex(TEXT("3DDC97"));
	}
}

void UALKTheme::ApplyPreset(EALKThemePreset Preset)
{
	ApplySkin(CurrentSkin, Preset);
}

void UALKTheme::ApplySkin(EALKThemeSkin Skin, EALKThemePreset Preset)
{
	using namespace ALKThemePrivate;
	CurrentSkin = Skin;
	CurrentPreset = Preset;
	SkinToApply = Skin;
	PresetToApply = Preset;

	const FLinearColor PresetAccent = GetPresetAccent(Preset);
	FLinearColor Line;

	// The skin's own picture for the backdrop, when the plugin's content has it (the kit works without: the glows and the
	// dot pattern then carry the backdrop as before).
	const TCHAR* PictureName = Skin == EALKThemeSkin::Light ? TEXT("T_ALK_Backdrop_Light") : (Skin == EALKThemeSkin::Arcade ? TEXT("T_ALK_Backdrop_Arcade") : TEXT("T_ALK_Backdrop_Default"));
	const FString PicturePackage = FString(BackdropPictureFolder) + PictureName;
	const bool bHasPicture = FPackageName::DoesPackageExist(PicturePackage);
	// The picture's tint (lower values dim it), how much the menu's side of it is covered, and how far it steps back
	// behind screens full of panels.
	FLinearColor PictureColor = FLinearColor::White;
	float PictureScrim = 0.f;
	float PageDimStrength = 0.5f;
	float PageBlurStrength = 10.f;

	switch (Skin)
	{
	case EALKThemeSkin::Light:
		// White panels on light grey; the accent is a touch darker so it holds against white.
		BackgroundTop = Hex(TEXT("ECEEF2"));
		BackgroundBottom = Hex(TEXT("DDE1E8"));
		Panel = Hex(TEXT("FFFFFF"));
		PanelRaised = Hex(TEXT("F3F4F7"));
		Line = Hex(TEXT("CDD2DB"));
		TextPrimary = Hex(TEXT("14171C"));
		TextSecondary = Hex(TEXT("4B5565"));
		TextMuted = Hex(TEXT("667085"));
		Accent = FMath::Lerp(PresetAccent, FLinearColor::Black, 0.10f).CopyWithNewOpacity(1.f);
		AccentContrast = Hex(TEXT("08130E"));
		Danger = Hex(TEXT("D93A40"));
		Success = Hex(TEXT("1F9D66"));
		Warning = Hex(TEXT("C98510"));
		Shadow = Hex(TEXT("4A5568"), 0.20f);
		PanelStyle = EALKPanelStyle::Raised;
		ShadowOffset = FVector2D(0.0, 8.0);
		ShadowBlur = 16.f;
		CornerRadius = 10.f;
		OutlineWidth = 1.f;
		FocusStyle = EALKFocusStyle::Tinted;
		AccentBarWidth = 4.f;
		FocusRingWidth = 2.f;
		bUppercaseTitles = false;
		bUppercaseButtons = false;
		BackgroundGlow = Accent.CopyWithNewOpacity(bHasPicture ? 0.07f : 0.10f);
		PatternColor = TextPrimary.CopyWithNewOpacity(bHasPicture ? 0.03f : 0.05f);
		PatternSpacing = 38.f;
		PatternDotSize = 2.4f;
		BackgroundVignette = 0.f;
		ScreenDim = Hex(TEXT("14171C"), 0.35f);
		// Frosted white glass: nearly opaque, a faint shade towards the bottom, a soft glow round the focus.
		PanelOpacity = 0.84f;
		PanelGradient = 0.05f;
		PanelTopEdge = FLinearColor(1.f, 1.f, 1.f, 0.9f);
		FocusGlow = 0.3f;
		FocusGradient = 1.f;
		CardFocusScale = 1.02f;
		PictureVeil = 0.75f;
		PictureScrim = 0.8f;
		PageDimStrength = 0.42f;
		break;
	case EALKThemeSkin::Arcade:
		// Deep violet, thick dark outlines, hard shadows and a solid highlight.
		BackgroundTop = Hex(TEXT("1A1236"));
		BackgroundBottom = Hex(TEXT("2B1B57"));
		Panel = Hex(TEXT("251A4A"));
		PanelRaised = Hex(TEXT("34266C"));
		Line = Hex(TEXT("0C0820"));
		TextPrimary = Hex(TEXT("FFFFFF"));
		TextSecondary = Hex(TEXT("D2C8F0"));
		TextMuted = Hex(TEXT("B7ABDD"));
		Accent = PresetAccent;
		AccentContrast = Hex(TEXT("140C2A"));
		Danger = Hex(TEXT("FF5A5F"));
		Success = Hex(TEXT("3DDC97"));
		Warning = Hex(TEXT("FFB02E"));
		Shadow = Hex(TEXT("0C0820"), 1.f);
		PanelStyle = EALKPanelStyle::Chunky;
		ShadowOffset = FVector2D(5.0, 5.0);
		ShadowBlur = 0.f;
		CornerRadius = 3.f;
		OutlineWidth = 3.f;
		FocusStyle = EALKFocusStyle::Solid;
		AccentBarWidth = 0.f;
		FocusRingWidth = 3.f;
		bUppercaseTitles = true;
		bUppercaseButtons = true;
		BackgroundGlow = Accent.CopyWithNewOpacity(bHasPicture ? 0.12f : 0.24f);
		PatternColor = TextPrimary.CopyWithNewOpacity(bHasPicture ? 0.05f : 0.07f);
		PatternSpacing = 44.f;
		PatternDotSize = 4.f;
		BackgroundVignette = 0.3f;
		ScreenDim = BackgroundTop.CopyWithNewOpacity(0.7f);
		// Flat and chunky on purpose: opaque fills, no gradients, no glow. Its richness is the picture behind and the art
		// in the cards.
		PanelOpacity = 1.f;
		PanelGradient = 0.f;
		PanelTopEdge = FLinearColor::Transparent;
		FocusGlow = 0.f;
		FocusGradient = 0.f;
		CardFocusScale = 1.f;
		PictureVeil = 0.f;
		PictureScrim = 0.7f;
		// The flat shapes of its picture stay crisp behind the panels: dimmed, not blurred.
		PageDimStrength = 0.55f;
		PageBlurStrength = 0.f;
		break;
	default:
		// Neutral dark grey with thin outlines and soft shadows.
		BackgroundTop = Hex(TEXT("13151A"));
		BackgroundBottom = Hex(TEXT("1B1E24"));
		Panel = Hex(TEXT("1B1E24"));
		PanelRaised = Hex(TEXT("262A33"));
		Line = Hex(TEXT("353A45"));
		TextPrimary = Hex(TEXT("ECEEF2"));
		TextSecondary = Hex(TEXT("B4BAC6"));
		TextMuted = Hex(TEXT("8B93A1"));
		Accent = PresetAccent;
		AccentContrast = Hex(TEXT("0B1410"));
		Danger = Hex(TEXT("E5484D"));
		Success = Hex(TEXT("3DDC97"));
		Warning = Hex(TEXT("F5A524"));
		Shadow = Hex(TEXT("000000"), 0.40f);
		PanelStyle = EALKPanelStyle::Raised;
		ShadowOffset = FVector2D(0.0, 8.0);
		ShadowBlur = 16.f;
		CornerRadius = 10.f;
		OutlineWidth = 1.f;
		FocusStyle = EALKFocusStyle::Tinted;
		AccentBarWidth = 4.f;
		FocusRingWidth = 2.f;
		bUppercaseTitles = false;
		bUppercaseButtons = false;
		BackgroundGlow = Accent.CopyWithNewOpacity(bHasPicture ? 0.10f : 0.15f);
		PatternColor = TextPrimary.CopyWithNewOpacity(bHasPicture ? 0.03f : 0.05f);
		PatternSpacing = 38.f;
		PatternDotSize = 2.4f;
		BackgroundVignette = 0.3f;
		ScreenDim = BackgroundTop.CopyWithNewOpacity(0.62f);
		// Dark glass: the blurred backdrop shows faintly through panels, which are lit from above.
		PanelOpacity = 0.86f;
		PanelGradient = 0.07f;
		PanelTopEdge = FLinearColor(1.f, 1.f, 1.f, 0.11f);
		FocusGlow = 0.34f;
		FocusGradient = 1.f;
		CardFocusScale = 1.02f;
		PictureVeil = 0.85f;
		PictureScrim = 0.8f;
		PageDimStrength = 0.36f;
		break;
	}
	FocusGlowSize = 16.f;

	// One layer of the backdrop is the skin's picture: swap it for this skin's and keep every layer that is not ours.
	BackdropLayers.RemoveAll([](const FALKBackdropLayer& Layer)
	{
		return Layer.Texture.ToSoftObjectPath().GetLongPackageName().StartsWith(BackdropPictureFolder);
	});
	if (bHasPicture)
	{
		FALKBackdropLayer Picture(TSoftObjectPtr<UTexture2D>(FSoftObjectPath(PicturePackage + TEXT(".") + PictureName)), PictureColor, 0.6f);
		Picture.bBehindGlows = true;
		BackdropLayers.Insert(Picture, 0);
	}

	// A calm second colour for information; never the same as the accent.
	AccentAlt = Preset == EALKThemePreset::Azure ? Hex(TEXT("B18CFF")) : Hex(TEXT("4DA3FF"));
	PanelOutline = Line;
	TitleShadowColor = BackgroundTop.CopyWithNewOpacity(0.f);
	// Over the paused game: dark for the skins with light text, a white veil for the skin with dark text, so titles and
	// hints that sit straight on the game (the results screen) stay readable.
	GameDim = Skin == EALKThemeSkin::Light ? BackgroundTop.CopyWithNewOpacity(0.72f) : Hex(TEXT("0A0C10"), 0.45f);
	// Behind screens full of panels the picture steps back further than the plain gradient needs to.
	PageDim = BackgroundTop.CopyWithNewOpacity(bHasPicture ? PageDimStrength : 0.5f);
	PageBlur = bHasPicture ? PageBlurStrength : 6.f;
	LockedChapterDim = BackgroundTop.CopyWithNewOpacity(0.65f);
	// Behind the menu list the picture is covered in the background colour, so the entries read over its brightest part.
	MenuScrim = BackgroundTop.CopyWithNewOpacity(bHasPicture ? PictureScrim : 0.f);
	BackgroundGrain = 0.f;
	GlowDriftSpeed = 1.f;

	// Focus: a tint of the accent colour, or the accent itself (Solid).
	const bool bSolid = FocusStyle == EALKFocusStyle::Solid;
	ButtonNormal = Accent.CopyWithNewOpacity(0.f);
	ButtonHovered = bSolid ? Accent : Accent.CopyWithNewOpacity(0.15f);
	ButtonPressed = bSolid ? FMath::Lerp(Accent, FLinearColor::Black, 0.12f).CopyWithNewOpacity(1.f) : Accent.CopyWithNewOpacity(0.26f);
	ButtonDisabled = Accent.CopyWithNewOpacity(0.f);
	RowNormal = Accent.CopyWithNewOpacity(0.f);
	RowFocused = bSolid ? Accent : Accent.CopyWithNewOpacity(0.15f);
	SliderTrack = Skin == EALKThemeSkin::Arcade ? Hex(TEXT("4A3A8A")) : Line;
	Separator = Skin == EALKThemeSkin::Arcade ? Hex(TEXT("4A3A8A")) : Line;
	KeyFill = PanelRaised;
	KeyOutline = Line;
	KeyText = TextPrimary;
}

FALKFrameStyle UALKTheme::MakePanelFrame(bool bWithShadow) const
{
	FALKFrameStyle Frame;
	Frame.Fill = GetPanelFill();
	Frame.Outline = PanelStyle == EALKPanelStyle::Flat ? FLinearColor::Transparent : PanelOutline;
	Frame.OutlineWidth = OutlineWidth;
	Frame.Radius = CornerRadius;
	Frame.Gradient = PanelGradient;
	Frame.TopEdge = PanelTopEdge;
	if (bWithShadow && (PanelStyle == EALKPanelStyle::Raised || PanelStyle == EALKPanelStyle::Chunky))
	{
		Frame.Shadow = Shadow;
		Frame.ShadowOffset = ShadowOffset;
		Frame.ShadowBlur = PanelStyle == EALKPanelStyle::Chunky ? 0.f : ShadowBlur;
	}
	return Frame;
}

FALKFrameStyle UALKTheme::MakeDialogFrame() const
{
	// The Panel colour itself, without Panel Opacity: even a few percent of see-through leaves a ghost of the dark text
	// of the screen behind on a light panel.
	FALKFrameStyle Frame = MakePanelFrame();
	Frame.Fill = Panel;
	return Frame;
}

FALKFrameStyle UALKTheme::MakeCardFrame(float Highlight) const
{
	const float H = FMath::Clamp(Highlight, 0.f, 1.f);
	FALKFrameStyle Frame;
	Frame.Fill = PanelRaised.CopyWithNewOpacity(PanelRaised.A * FMath::Clamp(PanelOpacity, 0.f, 1.f));
	Frame.Outline = PanelStyle == EALKPanelStyle::Flat ? FLinearColor::Transparent : PanelOutline;
	Frame.OutlineWidth = OutlineWidth;
	Frame.Radius = CornerRadius;
	Frame.Gradient = PanelGradient;
	Frame.TopEdge = PanelTopEdge;
	Frame.Ring = Accent.CopyWithNewOpacity(Accent.A * H);
	Frame.RingWidth = FocusRingWidth;
	Frame.Glow = Accent.CopyWithNewOpacity(Accent.A * FocusGlow * H);
	Frame.GlowSize = FocusGlowSize;
	if (PanelStyle == EALKPanelStyle::Chunky)
	{
		Frame.Shadow = Shadow;
		Frame.ShadowOffset = ShadowOffset * 0.8;
		Frame.ShadowBlur = 0.f;
	}
	else if (PanelStyle == EALKPanelStyle::Raised)
	{
		// A card sits on a panel: a small soft shadow of its own, deeper while it has focus.
		Frame.Shadow = Shadow.CopyWithNewOpacity(Shadow.A * (0.45f + 0.35f * H));
		Frame.ShadowOffset = ShadowOffset * (0.3 + 0.3 * H);
		Frame.ShadowBlur = ShadowBlur * 0.5f;
	}
	return Frame;
}

FALKFrameStyle UALKTheme::MakeItemFrame(float Highlight, bool bWithTick) const
{
	const float H = FMath::Clamp(Highlight, 0.f, 1.f);
	const bool bSolid = FocusStyle == EALKFocusStyle::Solid;
	FALKFrameStyle Frame;
	Frame.Fill = GetFocusFill(FMath::Lerp(ButtonNormal, ButtonHovered, H));
	Frame.Outline = bSolid ? PanelOutline.CopyWithNewOpacity(PanelOutline.A * H) : Accent.CopyWithNewOpacity(Accent.A * 0.55f * H);
	Frame.OutlineWidth = OutlineWidth;
	Frame.Radius = CornerRadius;
	Frame.Tick = bWithTick && !bSolid ? Accent.CopyWithNewOpacity(Accent.A * H) : FLinearColor::Transparent;
	Frame.TickWidth = AccentBarWidth;
	if (!bSolid)
	{
		// The tint gathers at the leading edge and the item glows a little, instead of one flat tint.
		Frame.Wash = Accent.CopyWithNewOpacity(Accent.A * 0.24f * FMath::Clamp(FocusGradient, 0.f, 1.f) * H);
		Frame.Glow = Accent.CopyWithNewOpacity(Accent.A * FocusGlow * 0.5f * H);
		Frame.GlowSize = FocusGlowSize * 0.75f;
	}
	if (PanelStyle == EALKPanelStyle::Chunky)
	{
		Frame.Shadow = Shadow.CopyWithNewOpacity(Shadow.A * H);
		Frame.ShadowOffset = ShadowOffset;
		Frame.ShadowBlur = 0.f;
	}
	return Frame;
}

FALKFrameStyle UALKTheme::MakeChipFrame(const FLinearColor& Fill, const FLinearColor& Outline) const
{
	FALKFrameStyle Frame;
	Frame.Fill = Fill;
	Frame.Outline = Outline;
	Frame.OutlineWidth = FMath::Min(OutlineWidth, 2.f);
	Frame.Radius = FMath::Min(CornerRadius, 8.f);
	return Frame;
}

FALKFrameStyle UALKTheme::MakeSolidButtonFrame(float Highlight, bool bDanger) const
{
	const float H = FMath::Clamp(Highlight, 0.f, 1.f);
	const FLinearColor Color = bDanger ? Danger : Accent;
	FALKFrameStyle Frame;
	Frame.Fill = Color;
	Frame.Outline = PanelStyle == EALKPanelStyle::Chunky ? PanelOutline : FLinearColor::Transparent;
	Frame.OutlineWidth = OutlineWidth;
	Frame.Radius = CornerRadius;
	Frame.Ring = Color.CopyWithNewOpacity(Color.A * H);
	Frame.RingWidth = FocusRingWidth;
	// The thing you are about to press: a stronger gradient than panels, a bright top edge, and a glow with focus.
	Frame.Gradient = FMath::Min(PanelGradient * 3.f, 1.f);
	Frame.TopEdge = PanelTopEdge.A > 0.f ? FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(PanelTopEdge.A * 3.f, 0.25f, 0.5f)) : FLinearColor::Transparent;
	Frame.Glow = Color.CopyWithNewOpacity(Color.A * FMath::Min(FocusGlow * 1.3f, 1.f) * H);
	Frame.GlowSize = FocusGlowSize;
	if (PanelStyle == EALKPanelStyle::Chunky)
	{
		Frame.Shadow = Shadow;
		Frame.ShadowOffset = ShadowOffset * 0.8;
		Frame.ShadowBlur = 0.f;
	}
	return Frame;
}

FALKFrameStyle UALKTheme::MakeBandFrame(const FLinearColor& Color) const
{
	FALKFrameStyle Frame;
	Frame.Radius = CornerRadius;
	Frame.OutlineWidth = OutlineWidth;
	if (FocusGradient > 0.f && FocusStyle != EALKFocusStyle::Solid)
	{
		Frame.Fill = Color.CopyWithNewOpacity(Color.A * 0.08f);
		Frame.Wash = Color.CopyWithNewOpacity(Color.A * 0.5f * FMath::Clamp(FocusGradient, 0.f, 1.f));
		Frame.Tick = Color;
		Frame.TickWidth = AccentBarWidth;
	}
	else
	{
		Frame.Fill = FMath::Lerp(Panel, Color, 0.45f).CopyWithNewOpacity(Color.A);
		Frame.Outline = PanelStyle == EALKPanelStyle::Flat ? FLinearColor::Transparent : PanelOutline;
	}
	return Frame;
}

FLinearColor UALKTheme::GetFocusFill(const FLinearColor& FlatFill) const
{
	if (FocusStyle == EALKFocusStyle::Solid)
	{
		return FlatFill;
	}
	return FlatFill.CopyWithNewOpacity(FlatFill.A * (1.f - 0.45f * FMath::Clamp(FocusGradient, 0.f, 1.f)));
}

FLinearColor UALKTheme::GetPanelFill() const
{
	return Panel.CopyWithNewOpacity(Panel.A * FMath::Clamp(PanelOpacity, 0.f, 1.f));
}

FLinearColor UALKTheme::GetFocusTextColor() const
{
	return FocusStyle == EALKFocusStyle::Solid ? AccentContrast : TextPrimary;
}

FLinearColor UALKTheme::GetAccentTextColor() const
{
	// On light panels the accent itself is too pale to read as text.
	const bool bLightPanel = Panel.GetLuminance() > 0.5f;
	return bLightPanel ? FMath::Lerp(Accent, FLinearColor::Black, 0.42f).CopyWithNewOpacity(1.f) : Accent;
}

FLinearColor UALKTheme::GetPingColor(int32 PingMs) const
{
	return PingMs < 60 ? Success : (PingMs < 120 ? Warning : Danger);
}

FSlateFontInfo UALKTheme::GetFont(EALKTextRole Role) const
{
	switch (Role)
	{
	case EALKTextRole::Title: return TitleFont;
	case EALKTextRole::Heading: return HeadingFont;
	case EALKTextRole::Subheading: return SubheadingFont;
	case EALKTextRole::Button: return ButtonFont;
	case EALKTextRole::Caption: return CaptionFont;
	case EALKTextRole::Data: return DataFont;
	case EALKTextRole::Body:
	default: return BodyFont;
	}
}

FLinearColor UALKTheme::GetTextColor(EALKTextRole Role) const
{
	switch (Role)
	{
	case EALKTextRole::Subheading:
	case EALKTextRole::Body:
		return TextSecondary;
	case EALKTextRole::Caption:
	case EALKTextRole::Data:
		return TextMuted;
	default:
		return TextPrimary;
	}
}

FSlateBrush UALKTheme::MakeRoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline, float InOutlineWidth, float RadiusScale) const
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Fill);
	const float Radius = FMath::Max(0.f, CornerRadius * RadiusScale);
	Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.Color = FSlateColor(Outline);
	Brush.OutlineSettings.Width = InOutlineWidth;
	return Brush;
}

FSlateBrush UALKTheme::MakeSolidBrush(const FLinearColor& Fill)
{
	return FSlateColorBrush(Fill);
}

FSlateBrush UALKTheme::MakePillBrush(const FLinearColor& Fill, const FLinearColor& Outline, float InOutlineWidth)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(Fill);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	Brush.OutlineSettings.Color = FSlateColor(Outline);
	Brush.OutlineSettings.Width = InOutlineWidth;
	return Brush;
}

FSimpleMulticastDelegate& UALKTheme::OnAnyThemeChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

#if WITH_EDITOR
void UALKTheme::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnAnyThemeChanged().Broadcast();
}
#endif
