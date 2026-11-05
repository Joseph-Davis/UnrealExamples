// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Settings/ALKSettingsSubsystem.h"
#include "ALKMenuSubsystem.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKTextEntryDialog.h"
#include "Online/ALKOnlineTypes.h"
#include "Widgets/ALKInputLabels.h"
#include "AudioDevice.h"
#include "Containers/Ticker.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericApplication.h"
#include "Widgets/ALKPaint.h"
#include "Settings/ALKSetting.h"
#include "Settings/ALKPlayerMappableKeyProfile.h"
#include "Settings/ALKUserSettings.h"
#include "Game/ALKCameraModifier_FieldOfView.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "EnhancedInputDeveloperSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Rendering/SlateRenderer.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "UObject/UObjectGlobals.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSettingsSubsystem)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKSettingsPrivate
{
	template <typename TSetting>
	TSetting* NewSetting(UObject* Outer, FName Id, const FText& Name, const FText& Description, EALKSettingsCategory Category, const FText& Section)
	{
		TSetting* Setting = NewObject<TSetting>(Outer);
		Setting->Describe(Id, Name, Description, Category, Section);
		return Setting;
	}

	/**
	 * The engine picks the saved language before plugin translations are registered, so unless the project has its own
	 * translations for that language it falls back to its default. Apply the saved choice again now that they are.
	 * A language given on the command line still wins.
	 */
	static void RestoreSavedLanguage()
	{
		FString Saved;
		FString CommandLineCulture;
		if (GIsEditor || !GConfig
			|| FParse::Value(FCommandLine::Get(), TEXT("CULTURE="), CommandLineCulture) || FParse::Value(FCommandLine::Get(), TEXT("LANGUAGE="), CommandLineCulture)
			|| !GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), Saved, GGameUserSettingsIni) || Saved.IsEmpty())
		{
			return;
		}

		FInternationalization& I18N = FInternationalization::Get();
		const FString Current = I18N.GetCurrentLanguage()->GetName();
		const TArray<FString> Translated = FTextLocalizationManager::Get().GetLocalizedCultureNames(ELocalizationLoadFlags::Game | ELocalizationLoadFlags::Additional);
		if (!Saved.Equals(Current, ESearchCase::IgnoreCase) && Translated.Contains(Saved))
		{
			UE_LOG(LogALK, Log, TEXT("Restoring the saved language '%s' (the engine started in '%s')."), *Saved, *Current);
			I18N.SetCurrentCulture(Saved);
		}
	}

	static TArray<FText> OnOff()
	{
		return { LOCTEXT("Off", "Off"), LOCTEXT("On", "On") };
	}

	static TArray<FText> QualityLevels()
	{
		return { LOCTEXT("QLow", "Low"), LOCTEXT("QMedium", "Medium"), LOCTEXT("QHigh", "High"), LOCTEXT("QEpic", "Epic"), LOCTEXT("QCinematic", "Cinematic") };
	}

	static const int32 FrameRateValues[] = { 30, 60, 90, 120, 144, 165, 240, 0 };
	static const int32 AntiAliasingValues[] = { 0, 1, 2, 4 };
	static const float ResolutionScaleValues[] = { 0.f, 50.f, 58.328f, 66.662f, 75.f, 85.f, 100.f };

	/** Game instances overriding the focus rectangle rule, and the project value to restore when the last one ends. */
	static int32 FocusRuleOverrideCount = 0;
	static ERenderFocusRule OriginalFocusRule = ERenderFocusRule::NavigationOnly;

	static FAutoConsoleCommand ListSettingsCommand(
		TEXT("ALK.ListSettings"),
		TEXT("Prints the Id of every built-in Assetpro Lobby Kit setting (use them in Hidden Settings)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			for (const FName& Id : UALKSettingsSubsystem::GetBuiltInSettingIds())
			{
				UE_LOG(LogALK, Display, TEXT("%s"), *Id.ToString());
			}
		}));
}

void UALKSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UALKOnlineSubsystem>();

	UserSettings = NewObject<UALKUserSettings>(this);
	UserSettings->LoadConfig();
	InitializeCustomDefaults();

	// Before any menu or loading screen text is created.
	ALKSettingsPrivate::RestoreSavedLanguage();

	// Keep cleared key bindings unbound in game (see UALKPlayerMappableKeyProfile), unless the project picked its own
	// key profile class. Runs before the local players, and so their input user settings, are created.
	UEnhancedInputDeveloperSettings* InputSettings = GetMutableDefault<UEnhancedInputDeveloperSettings>();
	const FSoftObjectPath ProfileClass = InputSettings->DefaultPlayerMappableKeyProfileClass.ToSoftObjectPath();
	if (ProfileClass.IsNull() || ProfileClass == FSoftObjectPath(UEnhancedPlayerMappableKeyProfile::StaticClass()))
	{
		InputSettings->DefaultPlayerMappableKeyProfileClass = UALKPlayerMappableKeyProfile::StaticClass();
	}

	SettingChangedHandle = UALKSetting::OnAnySettingChanged().AddUObject(this, &ThisClass::HandleSettingChanged);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

	if (GetDefault<UALKDeveloperSettings>()->bHideEngineFocusRectangle)
	{
		bOverridesFocusRule = true;
		UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>();
		if (ALKSettingsPrivate::FocusRuleOverrideCount++ == 0)
		{
			ALKSettingsPrivate::OriginalFocusRule = UISettings->RenderFocusRule;
		}
		UISettings->RenderFocusRule = ERenderFocusRule::Never;
	}

	if (IConsoleVariable* AntiAliasing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
	{
		ProjectAntiAliasingMethod = AntiAliasing->GetInt();
	}

	if (GetDefault<UALKDeveloperSettings>()->bApplySavedSettingsOnStartup)
	{
		ApplyUserSettings();
		// The saved monitor: once the game window exists (not in Play In Editor, which uses the editor's window).
		if (!GIsEditor && !UserSettings->Monitor.IsEmpty())
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::RestoreMonitorOnStartup), 0.1f);
		}
	}
	CaptureAppliedState();
}

void UALKSettingsSubsystem::CaptureAppliedState()
{
	if (!AppliedUserSettings)
	{
		AppliedUserSettings = NewObject<UALKUserSettings>(this);
	}
	for (TFieldIterator<FProperty> It(UALKUserSettings::StaticClass()); It; ++It)
	{
		It->CopyCompleteValue_InContainer(AppliedUserSettings, UserSettings);
	}

	if (const UGameUserSettings* GUS = GetGameUserSettings())
	{
		AppliedVideo.WindowMode = static_cast<int32>(GUS->GetFullscreenMode());
		AppliedVideo.Resolution = GUS->GetScreenResolution();
		AppliedVideo.bVSync = GUS->IsVSyncEnabled();
		AppliedVideo.FrameRateLimit = GUS->GetFrameRateLimit();
		float Normalized = 0.f, Min = 0.f, Max = 100.f;
		GUS->GetResolutionScaleInformationEx(Normalized, AppliedVideo.ResolutionScale, Min, Max);
		const int32 Quality[] = {
			GUS->GetViewDistanceQuality(), GUS->GetAntiAliasingQuality(), GUS->GetShadowQuality(), GUS->GetGlobalIlluminationQuality(),
			GUS->GetReflectionQuality(), GUS->GetPostProcessingQuality(), GUS->GetTextureQuality(), GUS->GetVisualEffectQuality(),
			GUS->GetFoliageQuality(), GUS->GetShadingQuality()
		};
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Quality)); ++Index)
		{
			AppliedVideo.Quality[Index] = Quality[Index];
		}
	}
}

void UALKSettingsSubsystem::Deinitialize()
{
	UALKSetting::OnAnySettingChanged().Remove(SettingChangedHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	if (bOverridesFocusRule)
	{
		bOverridesFocusRule = false;
		if (--ALKSettingsPrivate::FocusRuleOverrideCount == 0)
		{
			GetMutableDefault<UUserInterfaceSettings>()->RenderFocusRule = ALKSettingsPrivate::OriginalFocusRule;
		}
	}

	Super::Deinitialize();
}

void UALKSettingsSubsystem::InitializeCustomDefaults()
{
	for (const FALKCustomSettingDefinition& Definition : UALKDeveloperSettings::GetMenuConfig()->CustomSettings)
	{
		if (!Definition.Id.IsNone() && !UserSettings->CustomValues.Contains(Definition.Id))
		{
			UserSettings->CustomValues.Add(Definition.Id, Definition.DefaultValue);
		}
	}
}

UGameUserSettings* UALKSettingsSubsystem::GetGameUserSettings() const
{
	return GEngine ? GEngine->GetGameUserSettings() : nullptr;
}

TArray<FName> UALKSettingsSubsystem::GetBuiltInSettingIds()
{
	return {
		TEXT("WindowMode"), TEXT("Monitor"), TEXT("Resolution"), TEXT("VSync"), TEXT("FrameRateLimit"), TEXT("ResolutionScale"), TEXT("Brightness"),
		TEXT("QualityPreset"), TEXT("AutoDetect"), TEXT("ViewDistance"), TEXT("AntiAliasingQuality"), TEXT("Shadows"), TEXT("GlobalIllumination"),
		TEXT("Reflections"), TEXT("PostProcessing"), TEXT("Textures"), TEXT("Effects"), TEXT("Foliage"), TEXT("Shading"),
		TEXT("AntiAliasingMethod"), TEXT("MotionBlur"),
		TEXT("MasterVolume"), TEXT("MusicVolume"), TEXT("EffectsVolume"), TEXT("VoiceVolume"), TEXT("InterfaceVolume"), TEXT("MuteWhenUnfocused"),
		TEXT("MouseSensitivity"), TEXT("InvertMouseY"), TEXT("GamepadSensitivity"), TEXT("InvertGamepadY"), TEXT("Vibration"),
		TEXT("KeyBindings"), TEXT("GamepadBindings"), TEXT("ResetKeyBindings"),
		TEXT("FieldOfView"), TEXT("CameraShake"), TEXT("Difficulty"), TEXT("Language"),
		TEXT("Subtitles"), TEXT("SubtitleSize"), TEXT("SubtitleBackground"), TEXT("ColorBlindMode"), TEXT("ColorBlindStrength"), TEXT("UIScale"),
		TEXT("ReduceEffects"), TEXT("HudOpacity"),
		TEXT("PlayerName"), TEXT("Region"), TEXT("Crossplay"), TEXT("ShowPing"), TEXT("NetStats"), TEXT("TextChat"), TEXT("ChatFilter"), TEXT("StreamerMode"),
		TEXT("VoiceMode"), TEXT("PushToTalkKey"), TEXT("VoiceInputDevice"), TEXT("VoiceOutputDevice"), TEXT("MicVolume"), TEXT("VoiceChatVolume")
	};
}

TArray<UALKSetting*> UALKSettingsSubsystem::CreateSettings(UObject* Outer, ULocalPlayer* LocalPlayer)
{
	using namespace ALKSettingsPrivate;

	// The Video settings show the window the game really has.
	SyncDisplayWithWindow();

	TArray<UALKSetting*> Settings;
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	UGameUserSettings* GUS = GetGameUserSettings();
	UALKUserSettings* US = UserSettings;
	UALKSettingsSubsystem* Self = this;

	auto AddOptions = [&](FName Id, const FText& Name, const FText& Desc, EALKSettingsCategory Category, const FText& Section,
		const TArray<FText>& Options, int32 DefaultIndex, TFunction<int32()> Getter, TFunction<void(int32)> Setter) -> UALKSetting_Options*
	{
		UALKSetting_Options* Setting = NewSetting<UALKSetting_Options>(Outer, Id, Name, Desc, Category, Section);
		Setting->Options = Options;
		Setting->DefaultIndex = DefaultIndex;
		Setting->Getter = MoveTemp(Getter);
		Setting->Setter = MoveTemp(Setter);
		Settings.Add(Setting);
		return Setting;
	};

	auto AddSlider = [&](FName Id, const FText& Name, const FText& Desc, EALKSettingsCategory Category, const FText& Section,
		float Min, float Max, float Step, float Default, EALKValueFormat Format, TFunction<float()> Getter, TFunction<void(float)> Setter) -> UALKSetting_Slider*
	{
		UALKSetting_Slider* Setting = NewSetting<UALKSetting_Slider>(Outer, Id, Name, Desc, Category, Section);
		Setting->MinValue = Min;
		Setting->MaxValue = Max;
		Setting->StepSize = Step;
		Setting->DefaultValue = Default;
		Setting->Format = Format;
		Setting->Getter = MoveTemp(Getter);
		Setting->Setter = MoveTemp(Setter);
		Settings.Add(Setting);
		return Setting;
	};

	auto AddToggle = [&](FName Id, const FText& Name, const FText& Desc, EALKSettingsCategory Category, const FText& Section,
		bool bDefault, TFunction<bool()> Getter, TFunction<void(bool)> Setter) -> UALKSetting_Options*
	{
		UALKSetting_Options* Toggle = AddOptions(Id, Name, Desc, Category, Section, OnOff(), bDefault ? 1 : 0,
			[Getter]() { return Getter() ? 1 : 0; },
			[Setter](int32 Index) { Setter(Index == 1); });
		Toggle->bIsToggle = true;
		return Toggle;
	};

	auto AddAction = [&](FName Id, const FText& Name, const FText& Desc, EALKSettingsCategory Category, const FText& Section,
		const FText& ActionText, TFunction<void()> Action) -> UALKSetting_Action*
	{
		UALKSetting_Action* Setting = NewSetting<UALKSetting_Action>(Outer, Id, Name, Desc, Category, Section);
		Setting->ActionText = ActionText;
		Setting->Action = MoveTemp(Action);
		Settings.Add(Setting);
		return Setting;
	};

	// ============================================================ Video
	if (GUS)
	{
		const FText Display = LOCTEXT("SectionDisplay", "Display");

		AddOptions(TEXT("WindowMode"), LOCTEXT("WindowMode", "Window Mode"),
			LOCTEXT("WindowModeDesc", "Fullscreen gives the best performance. Windowed Fullscreen makes switching to other applications instant."),
			EALKSettingsCategory::Video, Display,
			{ LOCTEXT("Fullscreen", "Fullscreen"), LOCTEXT("WindowedFullscreen", "Windowed Fullscreen"), LOCTEXT("Windowed", "Windowed") },
			static_cast<int32>(EWindowMode::WindowedFullscreen),
			[GUS]() { return static_cast<int32>(GUS->GetFullscreenMode()); },
			[GUS](int32 Index) { GUS->SetFullscreenMode(static_cast<EWindowMode::Type>(Index)); });

		// Which monitor, when there is more than one. Like the window mode it waits for Apply and asks to keep it.
		const TArray<FMonitorInfo> Monitors = GetMonitors();
		if (Monitors.Num() > 1)
		{
			TArray<FText> MonitorNames;
			int32 PrimaryIndex = 0;
			for (int32 Index = 0; Index < Monitors.Num(); ++Index)
			{
				const FMonitorInfo& Monitor = Monitors[Index];
				const FText Size = FText::Format(LOCTEXT("ResolutionFormat", "{0} x {1}"), FText::AsNumber(Monitor.NativeWidth, &FNumberFormattingOptions::DefaultNoGrouping()),
					FText::AsNumber(Monitor.NativeHeight, &FNumberFormattingOptions::DefaultNoGrouping()));
				MonitorNames.Add(Monitor.bIsPrimary
					? FText::Format(LOCTEXT("MonitorPrimaryFormat", "Monitor {0} - {1} (Primary)"), FText::AsNumber(Index + 1), Size)
					: FText::Format(LOCTEXT("MonitorFormat", "Monitor {0} - {1}"), FText::AsNumber(Index + 1), Size));
				if (Monitor.bIsPrimary)
				{
					PrimaryIndex = Index;
				}
			}
			AddOptions(TEXT("Monitor"), LOCTEXT("Monitor", "Monitor"),
				LOCTEXT("MonitorDesc", "The screen the game runs on. Applied with Apply, and asks to keep it."),
				EALKSettingsCategory::Video, Display, MonitorNames, PrimaryIndex,
				[US, Monitors]()
				{
					const int32 Saved = Monitors.IndexOfByPredicate([US](const FMonitorInfo& Monitor) { return !US->Monitor.IsEmpty() && Monitor.ID == US->Monitor; });
					if (Saved != INDEX_NONE)
					{
						return Saved;
					}
					const int32 Current = GetWindowMonitorIndex();
					return Current != INDEX_NONE ? Current : FMath::Max(0, Monitors.IndexOfByPredicate([](const FMonitorInfo& Monitor) { return Monitor.bIsPrimary; }));
				},
				[US, Self, Monitors](int32 Index)
				{
					if (Monitors.IsValidIndex(Index))
					{
						// Choosing the monitor the game is on already keeps the saved value as it was (empty: wherever the window opens).
						const FString Applied = Self->AppliedUserSettings ? Self->AppliedUserSettings->Monitor : FString();
						US->Monitor = Monitors[Index].ID == ResolveMonitorId(Applied) ? Applied : Monitors[Index].ID;
					}
				});
		}

		TArray<FIntPoint> Resolutions;
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		if (Resolutions.Num() == 0)
		{
			UKismetSystemLibrary::GetConvenientWindowedResolutions(Resolutions);
		}
		Resolutions.AddUnique(GUS->GetScreenResolution());
		Resolutions.AddUnique(GUS->GetDesktopResolution());
		Resolutions.RemoveAll([](const FIntPoint& Res) { return Res.X < 640 || Res.Y < 480; });
		Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });

		TArray<FText> ResolutionNames;
		for (const FIntPoint& Res : Resolutions)
		{
			ResolutionNames.Add(FText::Format(LOCTEXT("ResolutionFormat", "{0} x {1}"), FText::AsNumber(Res.X, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Res.Y, &FNumberFormattingOptions::DefaultNoGrouping())));
		}
		const int32 DesktopIndex = FMath::Max(0, Resolutions.IndexOfByKey(GUS->GetDesktopResolution()));

		UALKSetting_Options* Resolution = AddOptions(TEXT("Resolution"), LOCTEXT("Resolution", "Resolution"),
			LOCTEXT("ResolutionDesc", "The number of pixels rendered. Windowed Fullscreen always uses your desktop resolution."),
			EALKSettingsCategory::Video, Display, ResolutionNames, DesktopIndex,
			[GUS, Resolutions]()
			{
				// Windowed Fullscreen always runs at the desktop resolution, so the locked row shows that one; the
				// saved resolution comes back when Windowed or Fullscreen is picked.
				const bool bDesktop = GUS->GetFullscreenMode() == EWindowMode::WindowedFullscreen;
				return FMath::Max(0, Resolutions.IndexOfByKey(bDesktop ? GUS->GetDesktopResolution() : GUS->GetScreenResolution()));
			},
			[GUS, Resolutions](int32 Index) { if (Resolutions.IsValidIndex(Index)) { GUS->SetScreenResolution(Resolutions[Index]); } });
		Resolution->EditCondition = [GUS]() { return GUS->GetFullscreenMode() != EWindowMode::WindowedFullscreen; };

		AddToggle(TEXT("VSync"), LOCTEXT("VSync", "Vertical Sync"),
			LOCTEXT("VSyncDesc", "Synchronises frames with your monitor's refresh rate to remove tearing. Can add input latency."),
			EALKSettingsCategory::Video, Display, false,
			[GUS]() { return GUS->IsVSyncEnabled(); },
			[GUS](bool bValue) { GUS->SetVSyncEnabled(bValue); });

		TArray<FText> FrameRateNames;
		for (int32 Value : FrameRateValues)
		{
			FrameRateNames.Add(Value > 0 ? FText::Format(LOCTEXT("FpsFormat", "{0} FPS"), FText::AsNumber(Value)) : LOCTEXT("Unlimited", "Unlimited"));
		}
		AddOptions(TEXT("FrameRateLimit"), LOCTEXT("FrameRateLimit", "Frame Rate Limit"),
			LOCTEXT("FrameRateLimitDesc", "Caps the frame rate to reduce heat, noise and power use."),
			EALKSettingsCategory::Video, Display, FrameRateNames, UE_ARRAY_COUNT(FrameRateValues) - 1,
			[GUS]()
			{
				const float Limit = GUS->GetFrameRateLimit();
				if (Limit <= 0.f)
				{
					return static_cast<int32>(UE_ARRAY_COUNT(FrameRateValues) - 1);
				}
				int32 Best = 0;
				for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(FrameRateValues)) - 1; ++Index)
				{
					if (FMath::Abs(FrameRateValues[Index] - Limit) < FMath::Abs(FrameRateValues[Best] - Limit))
					{
						Best = Index;
					}
				}
				return Best;
			},
			[GUS](int32 Index) { GUS->SetFrameRateLimit(static_cast<float>(FrameRateValues[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(FrameRateValues) - 1))])); });

		// 0 = the engine's automatic screen percentage for the display; the others match the engine's upscaling presets.
		TArray<FText> ScaleNames = { LOCTEXT("ScaleAuto", "Auto") };
		for (int32 Index = 1; Index < static_cast<int32>(UE_ARRAY_COUNT(ResolutionScaleValues)); ++Index)
		{
			ScaleNames.Add(ResolutionScaleValues[Index] >= 100.f ? LOCTEXT("ScaleNative", "Native (100%)")
				: FText::Format(LOCTEXT("ScalePercent", "{0}%"), FText::AsNumber(FMath::RoundToInt(ResolutionScaleValues[Index]))));
		}
		AddOptions(TEXT("ResolutionScale"), LOCTEXT("ResolutionScale", "Render Resolution"),
			LOCTEXT("ResolutionScaleDesc", "Renders the 3D scene at a lower resolution and upscales it. Lower values greatly improve performance. Auto picks a value for your display."),
			EALKSettingsCategory::Video, Display, ScaleNames, 0,
			[GUS]()
			{
				float Normalized = 0.f, Value = 0.f, Min = 0.f, Max = 100.f;
				GUS->GetResolutionScaleInformationEx(Normalized, Value, Min, Max);
				if (Value <= 0.f)
				{
					return 0;
				}
				int32 Best = 1;
				for (int32 Index = 1; Index < static_cast<int32>(UE_ARRAY_COUNT(ResolutionScaleValues)); ++Index)
				{
					if (FMath::Abs(ResolutionScaleValues[Index] - Value) < FMath::Abs(ResolutionScaleValues[Best] - Value))
					{
						Best = Index;
					}
				}
				return Best;
			},
			[GUS](int32 Index) { GUS->SetResolutionScaleValueEx(ResolutionScaleValues[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(ResolutionScaleValues)) - 1)]); });

		AddSlider(TEXT("Brightness"), LOCTEXT("Brightness", "Brightness"),
			LOCTEXT("BrightnessDesc", "Adjusts display gamma. Increase it if dark areas are hard to see."),
			EALKSettingsCategory::Video, Display, 0.f, 1.f, 0.05f, 0.5f, EALKValueFormat::Percent,
			[US]() { return US->Brightness; },
			[US, Self](float Value) { US->Brightness = Value; Self->ApplyBrightness(); });
	}

	// ============================================================ Graphics
	if (GUS)
	{
		const FText Quality = LOCTEXT("SectionQuality", "Quality");
		const FText Details = LOCTEXT("SectionDetails", "Details");
		const FText Effects = LOCTEXT("SectionEffects", "Post Effects");

		TArray<FText> PresetNames = QualityLevels();
		PresetNames.Add(LOCTEXT("Custom", "Custom"));
		UALKSetting_Options* Preset = AddOptions(TEXT("QualityPreset"), LOCTEXT("QualityPreset", "Quality Preset"),
			LOCTEXT("QualityPresetDesc", "Sets every graphics option at once. Changing an individual option switches this to Custom."),
			EALKSettingsCategory::Graphics, Quality, PresetNames, 3,
			[GUS]()
			{
				// Render Resolution is its own setting, so only the quality groups decide the preset.
				const int32 Level = GUS->GetViewDistanceQuality();
				const bool bUniform = Level == GUS->GetAntiAliasingQuality() && Level == GUS->GetShadowQuality()
					&& Level == GUS->GetGlobalIlluminationQuality() && Level == GUS->GetReflectionQuality()
					&& Level == GUS->GetPostProcessingQuality() && Level == GUS->GetTextureQuality()
					&& Level == GUS->GetVisualEffectQuality() && Level == GUS->GetFoliageQuality() && Level == GUS->GetShadingQuality();
				return (bUniform && Level >= 0 && Level <= 4) ? Level : 5;
			},
			[GUS](int32 Index)
			{
				if (Index >= 0 && Index <= 4)
				{
					float Normalized = 0.f, Resolution = 0.f, Min = 0.f, Max = 100.f;
					GUS->GetResolutionScaleInformationEx(Normalized, Resolution, Min, Max);
					GUS->SetOverallScalabilityLevel(Index);
					GUS->SetResolutionScaleValueEx(Resolution);
				}
			});
		// "Custom" is only shown when the groups differ; the player cannot pick it.
		Preset->SelectableCount = 5;

		UALKSetting_Action* AutoDetect = AddAction(TEXT("AutoDetect"), LOCTEXT("AutoDetect", "Auto-Detect Settings"),
			LOCTEXT("AutoDetectDesc", "Runs a short benchmark and picks graphics settings that suit your hardware. Applied immediately."),
			EALKSettingsCategory::Graphics, Quality, LOCTEXT("AutoDetectAction", "Detect"),
			[GUS, Self]()
			{
				GUS->RunHardwareBenchmark();
				GUS->ApplyHardwareBenchmarkResults();
				// Benchmark results are applied and saved immediately, so they become the new "applied" state.
				Self->CaptureAppliedState();
			});
		AutoDetect->bRequiresConfirmation = true;
		AutoDetect->ConfirmMessage = LOCTEXT("AutoDetectConfirm", "Run a short benchmark and apply recommended graphics settings?");

		struct FQualityGroup
		{
			FName Id;
			FText Name;
			FText Desc;
			TFunction<int32()> Get;
			TFunction<void(int32)> Set;
		};
		const TArray<FQualityGroup> Groups = {
			{ TEXT("ViewDistance"), LOCTEXT("ViewDistance", "View Distance"), LOCTEXT("ViewDistanceDesc", "How far away objects keep their detail."),
				[GUS]() { return GUS->GetViewDistanceQuality(); }, [GUS](int32 V) { GUS->SetViewDistanceQuality(V); } },
			{ TEXT("AntiAliasingQuality"), LOCTEXT("AntiAliasingQuality", "Anti-Aliasing Quality"), LOCTEXT("AntiAliasingQualityDesc", "Smooths jagged edges."),
				[GUS]() { return GUS->GetAntiAliasingQuality(); }, [GUS](int32 V) { GUS->SetAntiAliasingQuality(V); } },
			{ TEXT("Shadows"), LOCTEXT("Shadows", "Shadows"), LOCTEXT("ShadowsDesc", "Shadow resolution and distance. One of the most expensive options."),
				[GUS]() { return GUS->GetShadowQuality(); }, [GUS](int32 V) { GUS->SetShadowQuality(V); } },
			{ TEXT("GlobalIllumination"), LOCTEXT("GlobalIllumination", "Global Illumination"), LOCTEXT("GlobalIlluminationDesc", "Quality of bounced, indirect lighting."),
				[GUS]() { return GUS->GetGlobalIlluminationQuality(); }, [GUS](int32 V) { GUS->SetGlobalIlluminationQuality(V); } },
			{ TEXT("Reflections"), LOCTEXT("Reflections", "Reflections"), LOCTEXT("ReflectionsDesc", "Quality of reflections on shiny surfaces."),
				[GUS]() { return GUS->GetReflectionQuality(); }, [GUS](int32 V) { GUS->SetReflectionQuality(V); } },
			{ TEXT("PostProcessing"), LOCTEXT("PostProcessing", "Post Processing"), LOCTEXT("PostProcessingDesc", "Bloom, depth of field and other screen effects."),
				[GUS]() { return GUS->GetPostProcessingQuality(); }, [GUS](int32 V) { GUS->SetPostProcessingQuality(V); } },
			{ TEXT("Textures"), LOCTEXT("Textures", "Textures"), LOCTEXT("TexturesDesc", "Texture resolution. Lower it if you run out of video memory."),
				[GUS]() { return GUS->GetTextureQuality(); }, [GUS](int32 V) { GUS->SetTextureQuality(V); } },
			{ TEXT("Effects"), LOCTEXT("Effects", "Effects"), LOCTEXT("EffectsDesc", "Quality of particles and visual effects."),
				[GUS]() { return GUS->GetVisualEffectQuality(); }, [GUS](int32 V) { GUS->SetVisualEffectQuality(V); } },
			{ TEXT("Foliage"), LOCTEXT("Foliage", "Foliage"), LOCTEXT("FoliageDesc", "Amount of grass and small vegetation."),
				[GUS]() { return GUS->GetFoliageQuality(); }, [GUS](int32 V) { GUS->SetFoliageQuality(V); } },
			{ TEXT("Shading"), LOCTEXT("Shading", "Shading"), LOCTEXT("ShadingDesc", "Quality of material and surface shading."),
				[GUS]() { return GUS->GetShadingQuality(); }, [GUS](int32 V) { GUS->SetShadingQuality(V); } },
		};
		for (const FQualityGroup& Group : Groups)
		{
			AddOptions(Group.Id, Group.Name, Group.Desc, EALKSettingsCategory::Graphics, Details, QualityLevels(), 3, Group.Get, Group.Set);
		}

		AddOptions(TEXT("AntiAliasingMethod"), LOCTEXT("AntiAliasingMethod", "Anti-Aliasing Method"),
			LOCTEXT("AntiAliasingMethodDesc", "TSR gives the sharpest image, FXAA is the cheapest. Applied immediately."),
			EALKSettingsCategory::Graphics, Effects,
			{ LOCTEXT("AAOff", "Off"), LOCTEXT("AAFXAA", "FXAA"), LOCTEXT("AATAA", "TAA"), LOCTEXT("AATSR", "TSR") }, 3,
			[US]()
			{
				int32 Method = US->AntiAliasingMethod;
				if (Method < 0)
				{
					static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
					Method = CVar ? CVar->GetInt() : 4;
				}
				for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(AntiAliasingValues)); ++Index)
				{
					if (AntiAliasingValues[Index] == Method)
					{
						return Index;
					}
				}
				return 3;
			},
			[US, Self](int32 Index) { US->AntiAliasingMethod = AntiAliasingValues[FMath::Clamp(Index, 0, 3)]; Self->ApplyGraphicsOverrides(); });

		AddToggle(TEXT("MotionBlur"), LOCTEXT("MotionBlur", "Motion Blur"),
			LOCTEXT("MotionBlurDesc", "Blurs fast movement. Turn it off if it makes you uncomfortable."),
			EALKSettingsCategory::Graphics, Effects, true,
			[US]() { return US->bMotionBlur; },
			[US, Self](bool bValue) { US->bMotionBlur = bValue; Self->ApplyGraphicsOverrides(); });
	}

	// ============================================================ Audio
	{
		const FText Volume = LOCTEXT("SectionVolume", "Volume");
		const FText Options = LOCTEXT("SectionAudioOptions", "Options");

		auto AddVolume = [&](FName Id, const FText& Name, const FText& Desc, float Default, float UALKUserSettings::* Member)
		{
			AddSlider(Id, Name, Desc, EALKSettingsCategory::Audio, Volume, 0.f, 1.f, 0.05f, Default, EALKValueFormat::Percent,
				[US, Member]() { return US->*Member; },
				[US, Member, Self](float Value) { US->*Member = Value; Self->ApplyAudio(); });
		};
		AddVolume(TEXT("MasterVolume"), LOCTEXT("MasterVolume", "Master Volume"), LOCTEXT("MasterVolumeDesc", "Overall volume of the game."), 1.f, &UALKUserSettings::MasterVolume);
		AddVolume(TEXT("MusicVolume"), LOCTEXT("MusicVolume", "Music"), LOCTEXT("MusicVolumeDesc", "Volume of the soundtrack."), 0.8f, &UALKUserSettings::MusicVolume);
		AddVolume(TEXT("EffectsVolume"), LOCTEXT("EffectsVolume", "Sound Effects"), LOCTEXT("EffectsVolumeDesc", "Volume of gameplay sound effects."), 1.f, &UALKUserSettings::EffectsVolume);
		AddVolume(TEXT("VoiceVolume"), LOCTEXT("VoiceVolume", "Dialogue"), LOCTEXT("VoiceVolumeDesc", "Volume of character voices."), 1.f, &UALKUserSettings::VoiceVolume);
		AddVolume(TEXT("InterfaceVolume"), LOCTEXT("InterfaceVolume", "Interface"), LOCTEXT("InterfaceVolumeDesc", "Volume of menu sounds."), 1.f, &UALKUserSettings::InterfaceVolume);

		AddToggle(TEXT("MuteWhenUnfocused"), LOCTEXT("MuteWhenUnfocused", "Mute in Background"),
			LOCTEXT("MuteWhenUnfocusedDesc", "Silence the game while its window is not focused."),
			EALKSettingsCategory::Audio, Options, false,
			[US]() { return US->bMuteWhenUnfocused; },
			[US, Self](bool bValue) { US->bMuteWhenUnfocused = bValue; Self->ApplyAudio(); });
	}

	// ============================================================ Controls
	{
		const FText Mouse = LOCTEXT("SectionMouse", "Mouse");
		const FText Gamepad = LOCTEXT("SectionGamepad", "Gamepad");

		UALKSetting_Slider* MouseSens = AddSlider(TEXT("MouseSensitivity"), LOCTEXT("MouseSensitivity", "Mouse Sensitivity"),
			LOCTEXT("MouseSensitivityDesc", "How fast the camera turns with the mouse."),
			EALKSettingsCategory::Controls, Mouse, 0.1f, 5.f, 0.1f, 1.f, EALKValueFormat::OneDecimal,
			[US]() { return US->MouseSensitivity; },
			[US](float Value) { US->MouseSensitivity = Value; });
		MouseSens->Suffix = LOCTEXT("TimesSuffix", "x");

		AddToggle(TEXT("InvertMouseY"), LOCTEXT("InvertMouseY", "Invert Mouse Y"),
			LOCTEXT("InvertMouseYDesc", "Moving the mouse forward looks down."),
			EALKSettingsCategory::Controls, Mouse, false,
			[US]() { return US->bInvertMouseY; },
			[US](bool bValue) { US->bInvertMouseY = bValue; });

		UALKSetting_Slider* PadSens = AddSlider(TEXT("GamepadSensitivity"), LOCTEXT("GamepadSensitivity", "Stick Sensitivity"),
			LOCTEXT("GamepadSensitivityDesc", "How fast the camera turns with the right stick."),
			EALKSettingsCategory::Controls, Gamepad, 0.1f, 5.f, 0.1f, 1.f, EALKValueFormat::OneDecimal,
			[US]() { return US->GamepadSensitivity; },
			[US](float Value) { US->GamepadSensitivity = Value; });
		PadSens->Suffix = LOCTEXT("TimesSuffix", "x");

		AddToggle(TEXT("InvertGamepadY"), LOCTEXT("InvertGamepadY", "Invert Stick Y"),
			LOCTEXT("InvertGamepadYDesc", "Pushing the right stick up looks down."),
			EALKSettingsCategory::Controls, Gamepad, false,
			[US]() { return US->bInvertGamepadY; },
			[US](bool bValue) { US->bInvertGamepadY = bValue; });

		AddToggle(TEXT("Vibration"), LOCTEXT("Vibration", "Vibration"),
			LOCTEXT("VibrationDesc", "Controller rumble and force feedback."),
			EALKSettingsCategory::Controls, Gamepad, true,
			[US]() { return US->bVibration; },
			[US, Self](bool bValue) { US->bVibration = bValue; Self->ApplyToAllPlayers(); });

		// Key bindings come from Enhanced Input's player mappable keys.
		if (const UEnhancedInputUserSettings* InputSettings = UALKSetting_KeyBinding::GetInputUserSettings(LocalPlayer))
		{
			if (const UEnhancedPlayerMappableKeyProfile* Profile = UALKSetting_KeyBinding::GetKeyProfile(InputSettings))
			{
				const FText KeyBindings = LOCTEXT("SectionKeyBindings", "Key Bindings");

				struct FBindingRow
				{
					FName MappingName;
					FText DisplayName;
					FText Category;
				};
				TArray<FBindingRow> KeyboardRows;
				TArray<FBindingRow> GamepadRows;
				TArray<FName> RowNames;
				for (const TPair<FName, FKeyMappingRow>& Row : Profile->GetPlayerMappingRows())
				{
					const FPlayerKeyMapping* KeyboardMapping = nullptr;
					const FPlayerKeyMapping* GamepadMapping = nullptr;
					for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
					{
						const FKey& DefaultKey = Mapping.GetDefaultKey();
						const bool bIsGamepad = DefaultKey.IsValid() ? DefaultKey.IsGamepadKey() : Mapping.GetCurrentKey().IsGamepadKey();
						const FPlayerKeyMapping*& Target = bIsGamepad ? GamepadMapping : KeyboardMapping;
						if (!Target)
						{
							Target = &Mapping;
						}
					}

					auto MakeRow = [&Row](const FPlayerKeyMapping& Mapping)
					{
						FBindingRow BindingRow;
						BindingRow.MappingName = Row.Key;
						BindingRow.DisplayName = Mapping.GetDisplayName().IsEmpty() ? FText::FromName(Row.Key) : Mapping.GetDisplayName();
						BindingRow.Category = Mapping.GetDisplayCategory();
						return BindingRow;
					};
					if (KeyboardMapping)
					{
						KeyboardRows.Add(MakeRow(*KeyboardMapping));
					}
					if (GamepadMapping)
					{
						GamepadRows.Add(MakeRow(*GamepadMapping));
					}
					if (KeyboardMapping || GamepadMapping)
					{
						RowNames.Add(Row.Key);
					}
				}

				// Group keyboard rows by their Enhanced Input display category, keeping first-seen order.
				TArray<FString> CategoryOrder;
				for (const FBindingRow& BindingRow : KeyboardRows)
				{
					CategoryOrder.AddUnique(BindingRow.Category.ToString());
				}

				if (RowNames.Num() > 0)
				{
					TWeakObjectPtr<ULocalPlayer> WeakPlayer(LocalPlayer);
					UALKSetting_Action* Reset = AddAction(TEXT("ResetKeyBindings"), LOCTEXT("ResetKeyBindings", "Reset Key Bindings"),
						LOCTEXT("ResetKeyBindingsDesc", "Restore every key binding to its default."),
						EALKSettingsCategory::Controls, KeyBindings, LOCTEXT("ResetAction", "Reset"),
						[WeakPlayer, RowNames]()
						{
							if (UEnhancedInputUserSettings* Input = UALKSetting_KeyBinding::GetInputUserSettings(WeakPlayer.Get()))
							{
								for (const FName& RowName : RowNames)
								{
									FMapPlayerKeyArgs Args;
									Args.MappingName = RowName;
									FGameplayTagContainer Failure;
									Input->ResetAllPlayerKeysInRow(Args, Failure);
								}
								Input->ApplySettings();
								Input->AsyncSaveSettings();
							}
						});
					Reset->bRequiresConfirmation = true;
					Reset->ConfirmMessage = LOCTEXT("ResetKeyBindingsConfirm", "Reset every key binding to its default?");

					for (const FString& CategoryName : CategoryOrder)
					{
						for (const FBindingRow& BindingRow : KeyboardRows)
						{
							if (BindingRow.Category.ToString() != CategoryName)
							{
								continue;
							}
							const FText Section = BindingRow.Category.IsEmpty() ? KeyBindings : BindingRow.Category;
							UALKSetting_KeyBinding* Binding = NewSetting<UALKSetting_KeyBinding>(Outer, BindingRow.MappingName, BindingRow.DisplayName,
								LOCTEXT("KeyBindingDesc", "Pick a slot with Left / Right, then press Enter (or click it) and press the new key. Delete clears the slot. A key already used by another action moves here."),
								EALKSettingsCategory::Controls, Section);
							Binding->MappingName = BindingRow.MappingName;
							Binding->OwningPlayer = LocalPlayer;
							Binding->bGamepad = false;
							Settings.Add(Binding);
						}
					}

					const FText GamepadBindings = LOCTEXT("SectionGamepadBindings", "Gamepad Bindings");
					for (const FBindingRow& BindingRow : GamepadRows)
					{
						const FName Id(*(BindingRow.MappingName.ToString() + TEXT(".Gamepad")));
						UALKSetting_KeyBinding* Binding = NewSetting<UALKSetting_KeyBinding>(Outer, Id, BindingRow.DisplayName,
							LOCTEXT("GamepadBindingDesc", "Press A (or click the button), then press the new gamepad button. X clears it. A button already used by another action moves here."),
							EALKSettingsCategory::Controls, GamepadBindings);
						Binding->MappingName = BindingRow.MappingName;
						Binding->OwningPlayer = LocalPlayer;
						Binding->bGamepad = true;
						Settings.Add(Binding);
					}
				}
			}
		}
	}

	// ============================================================ Gameplay
	{
		const FText Camera = LOCTEXT("SectionCamera", "Camera");
		const FText Game = LOCTEXT("SectionGame", "Game");

		UALKSetting_Slider* Fov = AddSlider(TEXT("FieldOfView"), LOCTEXT("FieldOfView", "Field of View"),
			LOCTEXT("FieldOfViewDesc", "Horizontal field of view in degrees. Higher values show more of the world."),
			EALKSettingsCategory::Gameplay, Camera, 60.f, 120.f, 1.f, 90.f, EALKValueFormat::Integer,
			[US]() { return US->FieldOfView; },
			[US, Self](float Value) { US->FieldOfView = Value; Self->ApplyToAllPlayers(); });
		Fov->Suffix = FText::FromString(TEXT("°"));

		AddSlider(TEXT("CameraShake"), LOCTEXT("CameraShake", "Camera Shake"),
			LOCTEXT("CameraShakeDesc", "Intensity of camera shakes from explosions, impacts and movement."),
			EALKSettingsCategory::Gameplay, Camera, 0.f, 1.f, 0.05f, 1.f, EALKValueFormat::Percent,
			[US]() { return US->CameraShake; },
			[US](float Value) { US->CameraShake = Value; });

		AddOptions(TEXT("Difficulty"), LOCTEXT("Difficulty", "Difficulty"),
			LOCTEXT("DifficultyDesc", "How challenging combat and puzzles are."),
			EALKSettingsCategory::Gameplay, Game,
			{ LOCTEXT("Easy", "Easy"), LOCTEXT("Normal", "Normal"), LOCTEXT("Hard", "Hard") }, 1,
			[US]() { return US->Difficulty; },
			[US](int32 Index) { US->Difficulty = Index; });

		// The menu config's list, or every language the project is localized into.
		TArray<FString> CultureNames = Config->Languages;
		CultureNames.RemoveAll([](const FString& CultureName) { return CultureName.IsEmpty(); });
		if (CultureNames.Num() == 0)
		{
			CultureNames = FTextLocalizationManager::Get().GetLocalizedCultureNames(ELocalizationLoadFlags::Game);
			CultureNames.Sort();
		}
		if (CultureNames.Num() > 1)
		{
			TArray<FText> LanguageNames;
			for (const FString& CultureName : CultureNames)
			{
				const FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
				FString NativeName = Culture.IsValid() ? Culture->GetNativeName() : CultureName;
				// ICU names some languages in lower case ("français"); menus show them capitalized.
				NativeName = FText::FromString(NativeName.Left(1)).ToUpper().ToString() + NativeName.RightChop(1);
				LanguageNames.Add(FText::FromString(NativeName));
			}

			// The listed culture that best matches the current language: exact, else the longest "xx" prefix of "xx-YY".
			auto CurrentIndex = [CultureNames]()
			{
				const FString Current = FInternationalization::Get().GetCurrentLanguage()->GetName();
				int32 Best = INDEX_NONE;
				for (int32 Index = 0; Index < CultureNames.Num(); ++Index)
				{
					if (Current.Equals(CultureNames[Index], ESearchCase::IgnoreCase))
					{
						return Index;
					}
					if (Current.StartsWith(CultureNames[Index] + TEXT("-")) && (Best == INDEX_NONE || CultureNames[Index].Len() > CultureNames[Best].Len()))
					{
						Best = Index;
					}
				}
				return Best == INDEX_NONE ? 0 : Best;
			};

			// Default = current language, so Reset to Defaults never switches language.
			UALKSetting_Options* Language = AddOptions(TEXT("Language"), LOCTEXT("Language", "Language"),
				LOCTEXT("LanguageDesc", "Language used for text. Applied immediately."),
				EALKSettingsCategory::Gameplay, Game, LanguageNames, CurrentIndex(),
				CurrentIndex,
				[CultureNames](int32 Index)
				{
					if (CultureNames.IsValidIndex(Index))
					{
						UKismetInternationalizationLibrary::SetCurrentCulture(CultureNames[Index], true);
					}
				});
			Language->bSavesImmediately = true;
		}
	}

	// ============================================================ Accessibility
	{
		const FText Subtitles = LOCTEXT("SectionSubtitles", "Subtitles");
		const FText Vision = LOCTEXT("SectionVision", "Vision");

		AddToggle(TEXT("Subtitles"), LOCTEXT("Subtitles", "Subtitles"),
			LOCTEXT("SubtitlesDesc", "Show subtitles for dialogue and important sounds."),
			EALKSettingsCategory::Accessibility, Subtitles, true,
			[US]() { return US->bSubtitles; },
			[US, Self](bool bValue) { US->bSubtitles = bValue; Self->ApplyAccessibility(); });

		UALKSetting_Options* SubtitleSize = AddOptions(TEXT("SubtitleSize"), LOCTEXT("SubtitleSize", "Subtitle Size"),
			LOCTEXT("SubtitleSizeDesc", "How large subtitles are drawn."),
			EALKSettingsCategory::Accessibility, Subtitles,
			{ LOCTEXT("SizeSmall", "Small"), LOCTEXT("SizeMedium", "Medium"), LOCTEXT("SizeLarge", "Large"), LOCTEXT("SizeExtraLarge", "Extra Large") }, 1,
			[US]() { return US->SubtitleSize; },
			[US](int32 Index) { US->SubtitleSize = Index; });
		SubtitleSize->EditCondition = [US]() { return US->bSubtitles; };

		UALKSetting_Slider* SubtitleBackground = AddSlider(TEXT("SubtitleBackground"), LOCTEXT("SubtitleBackground", "Subtitle Background"),
			LOCTEXT("SubtitleBackgroundDesc", "Darkens the box behind subtitles so they stay readable over bright scenes. 0 shows the text alone."),
			EALKSettingsCategory::Accessibility, Subtitles, 0.f, 1.f, 0.1f, 0.5f, EALKValueFormat::Percent,
			[US]() { return US->SubtitleBackground; },
			[US](float Value) { US->SubtitleBackground = Value; });
		SubtitleBackground->EditCondition = [US]() { return US->bSubtitles; };

		AddOptions(TEXT("ColorBlindMode"), LOCTEXT("ColorBlindMode", "Colour Blind Mode"),
			LOCTEXT("ColorBlindModeDesc", "Adjusts colours across the whole screen for colour vision deficiencies."),
			EALKSettingsCategory::Accessibility, Vision,
			{ LOCTEXT("CBOff", "Off"), LOCTEXT("CBProtanopia", "Protanopia"), LOCTEXT("CBDeuteranopia", "Deuteranopia"), LOCTEXT("CBTritanopia", "Tritanopia") }, 0,
			[US]() { return US->ColorBlindMode; },
			[US, Self](int32 Index) { US->ColorBlindMode = Index; Self->ApplyAccessibility(); });

		UALKSetting_Slider* Strength = AddSlider(TEXT("ColorBlindStrength"), LOCTEXT("ColorBlindStrength", "Colour Correction Strength"),
			LOCTEXT("ColorBlindStrengthDesc", "How strongly colours are corrected."),
			EALKSettingsCategory::Accessibility, Vision, 0.f, 1.f, 0.1f, 1.f, EALKValueFormat::Percent,
			[US]() { return US->ColorBlindStrength; },
			[US, Self](float Value) { US->ColorBlindStrength = Value; Self->ApplyAccessibility(); });
		Strength->EditCondition = [US]() { return US->ColorBlindMode != 0; };

		const FText Interface = LOCTEXT("SectionInterface", "Interface");
		AddSlider(TEXT("UIScale"), LOCTEXT("UIScale", "Interface Scale"),
			LOCTEXT("UIScaleDesc", "Size of menus and the HUD."),
			EALKSettingsCategory::Accessibility, Interface, 0.75f, 1.5f, 0.05f, 1.f, EALKValueFormat::Percent,
			[US]() { return US->UIScale; },
			[US, Self](float Value) { US->UIScale = Value; Self->ApplyAccessibility(); });

		AddSlider(TEXT("HudOpacity"), LOCTEXT("HudOpacity", "HUD Opacity"),
			LOCTEXT("HudOpacityDesc", "How strongly prompts, subtitles, notifications and the saving indicator show over the game."),
			EALKSettingsCategory::Accessibility, Interface, 0.2f, 1.f, 0.05f, 1.f, EALKValueFormat::Percent,
			[US]() { return US->HudOpacity; },
			[US](float Value) { US->HudOpacity = Value; });

		AddToggle(TEXT("ReduceEffects"), LOCTEXT("ReduceEffects", "Reduce Effects"),
			LOCTEXT("ReduceEffectsDesc", "Turns off motion in menus and the HUD: sliding screens, the drifting background and spinning indicators. Screens simply fade in."),
			EALKSettingsCategory::Accessibility, Interface, false,
			[US]() { return US->bReduceEffects; },
			[US, Self](bool bValue) { US->bReduceEffects = bValue; Self->ApplyAccessibility(); });
	}

	// ============================================================ Multiplayer
	if (UALKOnlineSubsystem* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UALKOnlineSubsystem>() : nullptr)
	{
		const EALKSettingsCategory Multiplayer = EALKSettingsCategory::Multiplayer;
		const FText Network = LOCTEXT("SectionNetwork", "Network");
		const FText Chat = LOCTEXT("SectionChat", "Chat");
		const FText Voice = LOCTEXT("SectionVoice", "Voice Chat");

		// A list from the backend with "the backend decides" in front; hidden while the backend offers nothing.
		auto AddBackendList = [&](FName Id, const FText& Name, const FText& Desc, const FText& Section, const FText& First, const TArray<FALKOnlineOption>& Choices,
			FString UALKUserSettings::* Member)
		{
			if (Choices.Num() == 0)
			{
				return;
			}
			TArray<FText> Options = { First };
			TArray<FString> Ids = { FString() };
			for (const FALKOnlineOption& Choice : Choices)
			{
				Options.Add(Choice.DisplayName.IsEmpty() ? FText::FromString(Choice.Id) : Choice.DisplayName);
				Ids.Add(Choice.Id);
			}
			AddOptions(Id, Name, Desc, Multiplayer, Section, Options, 0,
				[US, Ids, Member]() { return FMath::Max(0, Ids.IndexOfByKey(US->*Member)); },
				[US, Self, Ids, Member](int32 Index) { US->*Member = Ids.IsValidIndex(Index) ? Ids[Index] : FString(); Self->ApplyMultiplayer(); });
		};

		// The name other players see. A platform backend brings the account's name: hide the row there (Hidden Settings).
		{
			TWeakObjectPtr<ULocalPlayer> WeakOwner(LocalPlayer);
			TWeakObjectPtr<UALKOnlineSubsystem> WeakOnline(Online);
			UALKSetting_Action* PlayerName = AddAction(TEXT("PlayerName"), LOCTEXT("PlayerName", "Player Name"),
				LOCTEXT("PlayerNameDesc", "The name other players see in lobbies, in chat and on the scoreboard."),
				Multiplayer, LOCTEXT("SectionProfile", "Profile"), FText::FromString(Online->GetLocalDisplayName()), nullptr);
			TWeakObjectPtr<UALKSetting_Action> WeakRow(PlayerName);
			PlayerName->Action = [WeakOwner, WeakOnline, WeakRow]()
			{
				UALKOnlineFlowSubsystem* Flow = WeakOwner.IsValid() ? WeakOwner->GetSubsystem<UALKOnlineFlowSubsystem>() : nullptr;
				if (!Flow || !WeakOnline.IsValid())
				{
					return;
				}
				FALKTextEntryDescriptor Descriptor;
				Descriptor.Title = LOCTEXT("PlayerName", "Player Name");
				Descriptor.Message = LOCTEXT("PlayerNameMessage", "Choose the name other players see. Leave it empty to use your account's name.");
				Descriptor.HintText = LOCTEXT("PlayerNameHint", "Your name");
				Descriptor.InitialText = WeakOnline->GetLocalDisplayName();
				Descriptor.ConfirmText = LOCTEXT("Save", "Save");
				Descriptor.MaxLength = 24;
				Descriptor.bAllowEmpty = true;
				Flow->ShowTextEntry(Descriptor, [WeakOnline, WeakRow](bool bConfirmed, const FString& Text, const FString&)
				{
					if (bConfirmed && WeakOnline.IsValid())
					{
						WeakOnline->SetPlayerName(Text);
						if (WeakRow.IsValid())
						{
							WeakRow->ActionText = FText::FromString(WeakOnline->GetLocalDisplayName());
							WeakRow->NotifyChanged();
						}
					}
				});
			};
		}

		AddBackendList(TEXT("Region"), LOCTEXT("Region", "Region"),
			LOCTEXT("RegionDesc", "Where to look for games and players. Automatic picks the region with the best connection."),
			Network, LOCTEXT("RegionAutomatic", "Automatic"), Online->GetRegions(), &UALKUserSettings::Region);

		AddToggle(TEXT("Crossplay"), LOCTEXT("Crossplay", "Crossplay"),
			LOCTEXT("CrossplayDesc", "Play with and against players on other platforms."),
			Multiplayer, Network, true,
			[US]() { return US->bCrossplay; },
			[US, Self](bool bValue) { US->bCrossplay = bValue; Self->ApplyMultiplayer(); });

		AddToggle(TEXT("ShowPing"), LOCTEXT("ShowPing", "Show Ping"),
			LOCTEXT("ShowPingDesc", "Show ping numbers in the server browser, the lobby and the scoreboard. The coloured bars always show."),
			Multiplayer, Network, true,
			[US]() { return US->bShowPing; },
			[US, Self](bool bValue) { US->bShowPing = bValue; Self->ApplyMultiplayer(); });

		AddToggle(TEXT("NetStats"), LOCTEXT("NetStats", "Network Stats Overlay"),
			LOCTEXT("NetStatsDesc", "Show your ping, packet loss and data rates in a corner of the screen during matches."),
			Multiplayer, Network, false,
			[US]() { return US->bNetStats; },
			[US, Self](bool bValue) { US->bNetStats = bValue; Self->ApplyMultiplayer(); });

		AddToggle(TEXT("TextChat"), LOCTEXT("TextChat", "Text Chat"),
			LOCTEXT("TextChatDesc", "Show text chat in lobbies and matches. Off hides every message and the chat box."),
			Multiplayer, Chat, true,
			[US]() { return US->bTextChat; },
			[US, Self](bool bValue) { US->bTextChat = bValue; Self->ApplyMultiplayer(); });

		UALKSetting_Options* ChatFilter = AddToggle(TEXT("ChatFilter"), LOCTEXT("ChatFilter", "Chat Filter"),
			LOCTEXT("ChatFilterDesc", "Mask offensive words in the chat messages you receive."),
			Multiplayer, Chat, true,
			[US]() { return US->bChatFilter; },
			[US, Self](bool bValue) { US->bChatFilter = bValue; Self->ApplyMultiplayer(); });
		ChatFilter->EditCondition = [US]() { return US->bTextChat; };

		AddToggle(TEXT("StreamerMode"), LOCTEXT("StreamerMode", "Streamer Mode"),
			LOCTEXT("StreamerModeDesc", "Hide other players' names, pictures and lobby codes everywhere in the interface, so nothing private shows up on a stream or in a recording."),
			Multiplayer, Chat, false,
			[US]() { return US->bStreamerMode; },
			[US, Self](bool bValue) { US->bStreamerMode = bValue; Self->ApplyMultiplayer(); });

		AddOptions(TEXT("VoiceMode"), LOCTEXT("VoiceMode", "Voice Chat"),
			LOCTEXT("VoiceModeDesc", "Off mutes voice chat. Push to Talk sends your voice while you hold the key. Open Mic sends it all the time."),
			Multiplayer, Voice, { LOCTEXT("VoiceOff", "Off"), LOCTEXT("VoicePushToTalk", "Push to Talk"), LOCTEXT("VoiceOpenMic", "Open Mic") }, 1,
			[US]() { return FMath::Clamp(US->VoiceMode, 0, 2); },
			[US, Self](int32 Index) { US->VoiceMode = FMath::Clamp(Index, 0, 2); Self->ApplyMultiplayer(); });

		// The talk key: picking the row asks for a key, like a key binding.
		TWeakObjectPtr<ULocalPlayer> WeakPlayer(LocalPlayer);
		auto TalkKeyLabel = [US, WeakPlayer]() { return FALKInputLabels::GetKeyLabel(FKey(*US->PushToTalkKey), WeakPlayer.Get()); };
		UALKSetting_Action* TalkKey = AddAction(TEXT("PushToTalkKey"), LOCTEXT("PushToTalkKey", "Push to Talk Key"),
			LOCTEXT("PushToTalkKeyDesc", "The key you hold to talk. Pick the row and press the new key."),
			Multiplayer, Voice, TalkKeyLabel(), nullptr);
		TWeakObjectPtr<UALKSetting_Action> WeakTalkKey(TalkKey);
		TalkKey->Action = [US, WeakPlayer, WeakTalkKey, TalkKeyLabel]()
		{
			UALKMenuSubsystem* Menu = WeakPlayer.IsValid() ? WeakPlayer->GetSubsystem<UALKMenuSubsystem>() : nullptr;
			if (!Menu || !WeakTalkKey.IsValid())
			{
				return;
			}
			Menu->ShowKeyCaptureDialog(WeakTalkKey->DisplayName, false, [US, WeakTalkKey, TalkKeyLabel](FKey Key)
			{
				if (Key.IsValid() && WeakTalkKey.IsValid())
				{
					US->PushToTalkKey = Key.GetFName().ToString();
					US->SaveConfig();
					WeakTalkKey->ActionText = TalkKeyLabel();
					WeakTalkKey->NotifyChanged();
				}
			});
		};
		TalkKey->EditCondition = [US]() { return US->VoiceMode == 1; };

		AddBackendList(TEXT("VoiceInputDevice"), LOCTEXT("VoiceInputDevice", "Microphone"),
			LOCTEXT("VoiceInputDeviceDesc", "The microphone used for voice chat."),
			Voice, LOCTEXT("DefaultDevice", "Default device"), Online->GetAudioInputDevices(), &UALKUserSettings::VoiceInputDevice);
		AddBackendList(TEXT("VoiceOutputDevice"), LOCTEXT("VoiceOutputDevice", "Speakers"),
			LOCTEXT("VoiceOutputDeviceDesc", "Where other players' voices are played."),
			Voice, LOCTEXT("DefaultDevice", "Default device"), Online->GetAudioOutputDevices(), &UALKUserSettings::VoiceOutputDevice);

		UALKSetting_Slider* MicVolume = AddSlider(TEXT("MicVolume"), LOCTEXT("MicVolume", "Microphone Volume"),
			LOCTEXT("MicVolumeDesc", "How loud other players hear you."),
			Multiplayer, Voice, 0.f, 1.f, 0.05f, 1.f, EALKValueFormat::Percent,
			[US]() { return US->MicVolume; },
			[US, Self](float Value) { US->MicVolume = Value; Self->ApplyMultiplayer(); });
		MicVolume->EditCondition = [US]() { return US->VoiceMode != 0; };

		UALKSetting_Slider* VoiceChatVolume = AddSlider(TEXT("VoiceChatVolume"), LOCTEXT("VoiceChatVolume", "Voice Chat Volume"),
			LOCTEXT("VoiceChatVolumeDesc", "How loud you hear other players."),
			Multiplayer, Voice, 0.f, 1.f, 0.05f, 1.f, EALKValueFormat::Percent,
			[US]() { return US->VoiceChatVolume; },
			[US, Self](float Value) { US->VoiceChatVolume = Value; Self->ApplyMultiplayer(); });
		VoiceChatVolume->EditCondition = [US]() { return US->VoiceMode != 0; };
	}

	// ============================================================ Custom (from the menu config)
	for (const FALKCustomSettingDefinition& Definition : Config->CustomSettings)
	{
		if (Definition.Id.IsNone())
		{
			continue;
		}
		const FName Id = Definition.Id;
		auto Get = [US, Id, Default = Definition.DefaultValue]() { const float* Value = US->CustomValues.Find(Id); return Value ? *Value : Default; };
		auto Set = [US, Id](float Value) { US->CustomValues.Add(Id, Value); };

		switch (Definition.Type)
		{
		case EALKCustomSettingType::Toggle:
			AddOptions(Id, Definition.DisplayName, Definition.Description, Definition.Category, Definition.Section, OnOff(),
				Definition.DefaultValue > 0.5f ? 1 : 0,
				[Get]() { return Get() > 0.5f ? 1 : 0; },
				[Set](int32 Index) { Set(Index == 1 ? 1.f : 0.f); })->bIsToggle = true;
			break;
		case EALKCustomSettingType::Options:
			AddOptions(Id, Definition.DisplayName, Definition.Description, Definition.Category, Definition.Section, Definition.Options,
				FMath::RoundToInt(Definition.DefaultValue),
				[Get]() { return FMath::RoundToInt(Get()); },
				[Set](int32 Index) { Set(static_cast<float>(Index)); });
			break;
		case EALKCustomSettingType::Slider:
			AddSlider(Id, Definition.DisplayName, Definition.Description, Definition.Category, Definition.Section,
				Definition.MinValue, Definition.MaxValue, Definition.StepSize, Definition.DefaultValue, Definition.Format, Get, Set);
			break;
		}
	}

	// Remove hidden settings.
	if (Config->HiddenSettings.Num() > 0)
	{
		Settings.RemoveAll([Config](const UALKSetting* Setting)
		{
			const UALKSetting_KeyBinding* Binding = Cast<UALKSetting_KeyBinding>(Setting);
			return Config->HiddenSettings.Contains(Setting->Id)
				|| (Binding && Config->HiddenSettings.Contains(FName(TEXT("KeyBindings"))))
				|| (Binding && Binding->bGamepad && Config->HiddenSettings.Contains(FName(TEXT("GamepadBindings"))));
		});
	}

	return Settings;
}

void UALKSettingsSubsystem::HandleSettingChanged(UALKSetting* Setting)
{
	if (!Setting)
	{
		return;
	}

	// Key bindings, actions and settings such as Language save themselves immediately.
	const EALKSettingType Type = Setting->GetSettingType();
	if (Type != EALKSettingType::KeyBinding && Type != EALKSettingType::Action && !Setting->bSavesImmediately)
	{
		bDirty = true;
	}
	OnSettingChanged.Broadcast(Setting->Id);
	OnSettingChangedNative.Broadcast(Setting);
}

void UALKSettingsSubsystem::ApplyAndSave()
{
	PreviousWindowMode = AppliedVideo.WindowMode;
	PreviousResolution = AppliedVideo.Resolution;
	PreviousMonitor = AppliedUserSettings ? AppliedUserSettings->Monitor : FString();
	PreviousWindowMonitor = ResolveMonitorId(PreviousMonitor);
	DisplayChangeFrame = GFrameCounter;
	const bool bMonitorChanging = ResolveMonitorId(UserSettings->Monitor) != PreviousWindowMonitor;

	if (UGameUserSettings* GUS = GetGameUserSettings())
	{
		GUS->ApplySettings(false);
	}
	if (bMonitorChanging)
	{
		MoveWindowToMonitor(UserSettings->Monitor);
	}
	SaveUserSettings();
	ApplyUserSettings();
	CaptureAppliedState();
	bDirty = false;
	OnSettingsApplied.Broadcast();
}

void UALKSettingsSubsystem::ConfirmDisplayMode()
{
	DisplayChangeFrame = GFrameCounter;
	if (UGameUserSettings* GUS = GetGameUserSettings())
	{
		GUS->ConfirmVideoMode();
		GUS->SaveSettings();
	}
	CaptureAppliedState();
}

void UALKSettingsSubsystem::RevertDisplayMode()
{
	DisplayChangeFrame = GFrameCounter;
	if (UGameUserSettings* GUS = GetGameUserSettings())
	{
		if (PreviousResolution.X > 0 && PreviousResolution.Y > 0)
		{
			GUS->SetFullscreenMode(static_cast<EWindowMode::Type>(PreviousWindowMode));
			GUS->SetScreenResolution(PreviousResolution);
		}
		else
		{
			GUS->RevertVideoMode();
		}
		GUS->ApplyResolutionSettings(false);
		GUS->ConfirmVideoMode();
		GUS->SaveSettings();
	}
	if (ResolveMonitorId(UserSettings->Monitor) != PreviousWindowMonitor)
	{
		MoveWindowToMonitor(PreviousWindowMonitor);
	}
	if (UserSettings->Monitor != PreviousMonitor)
	{
		UserSettings->Monitor = PreviousMonitor;
		SaveUserSettings();
	}
	CaptureAppliedState();
}

bool UALKSettingsSubsystem::IsDisplayChangePending() const
{
	const UGameUserSettings* GUS = GetGameUserSettings();
	const bool bVideo = GUS && (GUS->IsScreenResolutionDirty() || GUS->IsFullscreenModeDirty());
	return bVideo || (AppliedUserSettings && ResolveMonitorId(UserSettings->Monitor) != ResolveMonitorId(AppliedUserSettings->Monitor));
}

FString UALKSettingsSubsystem::ResolveMonitorId(const FString& MonitorId)
{
	const TArray<FMonitorInfo> Monitors = GetMonitors();
	if (!MonitorId.IsEmpty() && Monitors.ContainsByPredicate([&MonitorId](const FMonitorInfo& Monitor) { return Monitor.ID == MonitorId; }))
	{
		return MonitorId;
	}
	const int32 Current = GetWindowMonitorIndex();
	return Monitors.IsValidIndex(Current) ? Monitors[Current].ID : FString();
}

TArray<FMonitorInfo> UALKSettingsSubsystem::GetMonitors()
{
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	return Metrics.MonitorInfo;
}

int32 UALKSettingsSubsystem::GetWindowMonitorIndex()
{
	UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport.Get() : nullptr;
	const TSharedPtr<SWindow> Window = ViewportClient ? ViewportClient->GetWindow() : nullptr;
	if (!Window.IsValid())
	{
		return INDEX_NONE;
	}
	const FVector2D Center = Window->GetPositionInScreen() + Window->GetSizeInScreen() * 0.5;
	const TArray<FMonitorInfo> Monitors = GetMonitors();
	return Monitors.IndexOfByPredicate([&Center](const FMonitorInfo& Monitor)
	{
		const FPlatformRect& Rect = Monitor.DisplayRect;
		return Center.X >= Rect.Left && Center.X < Rect.Right && Center.Y >= Rect.Top && Center.Y < Rect.Bottom;
	});
}

void UALKSettingsSubsystem::MoveWindowToMonitor(const FString& MonitorId)
{
	// Everything goes through the engine's own resolution requests (applied on a later frame), and Windows reports the
	// window's new place a frame or two after it moves, so the move is a few steps over a few frames (TickWindowMove):
	// windowed first (a fullscreen window cannot move), then the move, then the chosen mode on the new monitor.
	if (GIsEditor || MonitorId.IsEmpty() || !GEngine || !GEngine->GameViewport || !GEngine->GameViewport->GetWindow().IsValid())
	{
		return;
	}
	const TArray<FMonitorInfo> Monitors = GetMonitors();
	if (!Monitors.ContainsByPredicate([&MonitorId](const FMonitorInfo& Monitor) { return Monitor.ID == MonitorId; }))
	{
		return;
	}
	MovingToMonitor = MonitorId;
	MoveStep = 0;
	MoveStepFrame = GFrameCounter;
	DisplayChangeFrame = GFrameCounter;
	if (!WindowMoveTicker.IsValid())
	{
		WindowMoveTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickWindowMove));
	}
	TickWindowMove(0.f);
}

bool UALKSettingsSubsystem::TickWindowMove(float DeltaTime)
{
	UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport.Get() : nullptr;
	const TSharedPtr<SWindow> Window = ViewportClient ? ViewportClient->GetWindow() : nullptr;
	FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	UGameUserSettings* GUS = GetGameUserSettings();
	const TArray<FMonitorInfo> Monitors = GetMonitors();
	const FMonitorInfo* Target = Monitors.FindByPredicate([this](const FMonitorInfo& Monitor) { return Monitor.ID == MovingToMonitor; });
	if (!Window.IsValid() || !Viewport || !GUS || !Target || GFrameCounter > MoveStepFrame + 120)
	{
		WindowMoveTicker.Reset();
		MovingToMonitor.Reset();
		return false;
	}
	DisplayChangeFrame = GFrameCounter;

	const FIntPoint Resolution = GUS->GetScreenResolution();
	const FPlatformRect& Area = Target->WorkArea;
	const FIntPoint AreaSize(Area.Right - Area.Left, Area.Bottom - Area.Top);
	// The windowed size while moving: the chosen resolution, kept inside the target monitor's work area.
	const FIntPoint Windowed(FMath::Min(Resolution.X > 0 ? Resolution.X : 1280, AreaSize.X * 9 / 10), FMath::Min(Resolution.Y > 0 ? Resolution.Y : 720, AreaSize.Y * 9 / 10));

	switch (MoveStep)
	{
	case 0:
		if (Viewport->GetWindowMode() != EWindowMode::Windowed)
		{
			FSystemResolution::RequestResolutionChange(Windowed.X, Windowed.Y, EWindowMode::Windowed);
		}
		MoveStep = 1;
		MoveStepFrame = GFrameCounter;
		return true;
	case 1:
		// Wait until the window is windowed.
		if (Viewport->GetWindowMode() != EWindowMode::Windowed)
		{
			return true;
		}
		Window->MoveWindowTo(FVector2D(Area.Left + (AreaSize.X - Windowed.X) / 2, Area.Top + (AreaSize.Y - Windowed.Y) / 2));
		MoveStep = 2;
		MoveStepFrame = GFrameCounter;
		return true;
	default:
		// Two frames for the move to arrive, then the chosen mode and resolution, now on the target monitor.
		if (GFrameCounter < MoveStepFrame + 2)
		{
			return true;
		}
		GSystemResolution.ForceRefresh();
		GUS->ApplyResolutionSettings(false);
		UE_LOG(LogALK, Log, TEXT("Moved the game window to monitor %s (%s)."), *MovingToMonitor, *Target->Name);
		WindowMoveTicker.Reset();
		MovingToMonitor.Reset();
		return false;
	}
}

bool UALKSettingsSubsystem::RestoreMonitorOnStartup(float DeltaTime)
{
	UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport.Get() : nullptr;
	if (!ViewportClient || !ViewportClient->GetWindow().IsValid() || !ViewportClient->Viewport)
	{
		return true;
	}
	const TArray<FMonitorInfo> Monitors = GetMonitors();
	const int32 Saved = Monitors.IndexOfByPredicate([this](const FMonitorInfo& Monitor) { return Monitor.ID == UserSettings->Monitor; });
	if (Saved != INDEX_NONE && Saved != GetWindowMonitorIndex())
	{
		MoveWindowToMonitor(UserSettings->Monitor);
	}
	return false;
}

void UALKSettingsSubsystem::SyncDisplayWithWindow()
{
	// A -windowed, -fullscreen or -ResX / -ResY command line, or Alt+Enter, changes the window without telling the engine's
	// display settings. The Video page would then show a mode the game is not in, and applying any video setting would
	// switch to that mode and ask to keep it. So while nothing is pending, the settings take the window's mode and size:
	// the values the engine compares them with to tell whether the display is changing. Not in the editor: Play In Editor
	// runs in the editor's own window.
	UGameUserSettings* GUS = GetGameUserSettings();
	UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport.Get() : nullptr;
	if (!GUS || GIsEditor || bDirty || !ViewportClient || !ViewportClient->Viewport || GFrameCounter < DisplayChangeFrame + 10
		|| GSystemResolution.ResX <= 0 || GSystemResolution.ResY <= 0)
	{
		return;
	}

	const EWindowMode::Type WindowMode = ViewportClient->Viewport->GetWindowMode();
	const FIntPoint Resolution(GSystemResolution.ResX, GSystemResolution.ResY);
	if (GUS->GetFullscreenMode() != WindowMode || GUS->GetScreenResolution() != Resolution)
	{
		GUS->SetFullscreenMode(WindowMode);
		GUS->SetScreenResolution(Resolution);
		GUS->ConfirmVideoMode();
		CaptureAppliedState();
	}
}

void UALKSettingsSubsystem::RevertUnsavedChanges()
{
	// Restore the snapshot rather than reloading the ini: values that were never saved are not in the ini at all.
	if (AppliedUserSettings)
	{
		for (TFieldIterator<FProperty> It(UALKUserSettings::StaticClass()); It; ++It)
		{
			It->CopyCompleteValue_InContainer(UserSettings, AppliedUserSettings);
		}
	}

	if (UGameUserSettings* GUS = GetGameUserSettings())
	{
		GUS->SetFullscreenMode(static_cast<EWindowMode::Type>(AppliedVideo.WindowMode));
		GUS->SetScreenResolution(AppliedVideo.Resolution);
		GUS->SetVSyncEnabled(AppliedVideo.bVSync);
		GUS->SetFrameRateLimit(AppliedVideo.FrameRateLimit);
		GUS->SetResolutionScaleValueEx(AppliedVideo.ResolutionScale);
		GUS->SetViewDistanceQuality(AppliedVideo.Quality[0]);
		GUS->SetAntiAliasingQuality(AppliedVideo.Quality[1]);
		GUS->SetShadowQuality(AppliedVideo.Quality[2]);
		GUS->SetGlobalIlluminationQuality(AppliedVideo.Quality[3]);
		GUS->SetReflectionQuality(AppliedVideo.Quality[4]);
		GUS->SetPostProcessingQuality(AppliedVideo.Quality[5]);
		GUS->SetTextureQuality(AppliedVideo.Quality[6]);
		GUS->SetVisualEffectQuality(AppliedVideo.Quality[7]);
		GUS->SetFoliageQuality(AppliedVideo.Quality[8]);
		GUS->SetShadingQuality(AppliedVideo.Quality[9]);
	}

	ApplyUserSettings();
	bDirty = false;
}

void UALKSettingsSubsystem::SaveUserSettings()
{
	UserSettings->SaveConfig();
}

void UALKSettingsSubsystem::ApplyUserSettings()
{
	ApplyAudio();
	ApplyBrightness();
	ApplyGraphicsOverrides();
	ApplyAccessibility();
	ApplyMultiplayer();
	ApplyToAllPlayers();
}

void UALKSettingsSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		ApplyAudio();
		ApplyToAllPlayers();
	}
}

void UALKSettingsSubsystem::ApplyAudio()
{
	if (GEngine)
	{
		FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice();
		if (AudioDevice.IsValid())
		{
			AudioDevice->SetTransientPrimaryVolume(UserSettings->MasterVolume);
		}
	}
	FApp::SetUnfocusedVolumeMultiplier(UserSettings->bMuteWhenUnfocused ? 0.f : 1.f);

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	const TPair<TSoftObjectPtr<USoundClass>, float> Channels[] = {
		{ Config->MusicSoundClass, UserSettings->MusicVolume },
		{ Config->EffectsSoundClass, UserSettings->EffectsVolume },
		{ Config->VoiceSoundClass, UserSettings->VoiceVolume },
		{ Config->InterfaceSoundClass, UserSettings->InterfaceVolume }
	};

	bool bAnyChannel = false;
	for (const TPair<TSoftObjectPtr<USoundClass>, float>& Channel : Channels)
	{
		if (USoundClass* SoundClass = Channel.Key.LoadSynchronous())
		{
			// The audio device keeps the class in its override by pointer only: hold it here, or the next garbage
			// collection (the first level load) destroys it and the override stops working, logging every audio frame.
			VolumeSoundClasses.AddUnique(SoundClass);
			if (!VolumeSoundMix)
			{
				VolumeSoundMix = NewObject<USoundMix>(this, TEXT("ALKVolumeMix"));
			}
			UGameplayStatics::SetSoundMixClassOverride(World, VolumeSoundMix, SoundClass, Channel.Value, 1.f, 0.f, true);
			bAnyChannel = true;
		}
	}
	if (bAnyChannel)
	{
		UGameplayStatics::PushSoundMixModifier(World, VolumeSoundMix);
	}
}

void UALKSettingsSubsystem::ApplyBrightness()
{
	if (GEngine)
	{
		GEngine->DisplayGamma = FMath::Lerp(1.7f, 2.7f, UserSettings->Brightness);
	}
}

void UALKSettingsSubsystem::ApplyGraphicsOverrides()
{
	// A player override, or the project's own method again after the override is reverted.
	const int32 AntiAliasingMethod = UserSettings->AntiAliasingMethod >= 0 ? UserSettings->AntiAliasingMethod : ProjectAntiAliasingMethod;
	if (AntiAliasingMethod >= 0)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
		if (CVar && CVar->GetInt() != AntiAliasingMethod)
		{
			CVar->Set(AntiAliasingMethod, ECVF_SetByGameSetting);
		}
	}

	// Only force motion blur when the player turned it off (or back on after turning it off), so scalability keeps control otherwise.
	static bool bMotionBlurOverridden = false;
	if (!UserSettings->bMotionBlur || bMotionBlurOverridden)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
		{
			CVar->Set(UserSettings->bMotionBlur ? 4 : 0, ECVF_SetByGameSetting);
			bMotionBlurOverridden = true;
		}
	}
}

void UALKSettingsSubsystem::ApplyMultiplayer()
{
	UALKOnlineSubsystem* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UALKOnlineSubsystem>() : nullptr;
	if (!Online || !UserSettings)
	{
		return;
	}
	Online->SetRegion(UserSettings->Region);
	Online->SetCrossplayEnabled(UserSettings->bCrossplay);
	Online->SetShowPing(UserSettings->bShowPing);
	Online->SetTextChatEnabled(UserSettings->bTextChat);
	Online->SetChatFilterEnabled(UserSettings->bChatFilter);
	Online->SetStreamerMode(UserSettings->bStreamerMode);
	Online->SetVoiceMode(static_cast<EALKVoiceMode>(FMath::Clamp(UserSettings->VoiceMode, 0, 2)));
	if (!UserSettings->VoiceInputDevice.IsEmpty())
	{
		Online->SetAudioInputDevice(UserSettings->VoiceInputDevice);
	}
	if (!UserSettings->VoiceOutputDevice.IsEmpty())
	{
		Online->SetAudioOutputDevice(UserSettings->VoiceOutputDevice);
	}
	Online->SetMicVolume(UserSettings->MicVolume);
	Online->SetVoiceVolume(UserSettings->VoiceChatVolume);
}

bool UALKSettingsSubsystem::IsNetStatsOverlayEnabled() const
{
	return UserSettings && UserSettings->bNetStats;
}

bool UALKSettingsSubsystem::IsPingShown() const
{
	return !UserSettings || UserSettings->bShowPing;
}

bool UALKSettingsSubsystem::IsStreamerModeEnabled() const
{
	return UserSettings && UserSettings->bStreamerMode;
}

FKey UALKSettingsSubsystem::GetPushToTalkKey() const
{
	return UserSettings ? FKey(*UserSettings->PushToTalkKey) : FKey();
}

void UALKSettingsSubsystem::ApplyAccessibility()
{
	if (GEngine)
	{
		GEngine->bSubtitlesEnabled = UserSettings->bSubtitles;
	}

	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		static const EColorVisionDeficiency Types[] = { EColorVisionDeficiency::NormalVision, EColorVisionDeficiency::Protanope, EColorVisionDeficiency::Deuteranope, EColorVisionDeficiency::Tritanope };
		const int32 Mode = FMath::Clamp(UserSettings->ColorBlindMode, 0, 3);
		const int32 Severity = FMath::RoundToInt(FMath::Clamp(UserSettings->ColorBlindStrength, 0.f, 1.f) * 10.f);
		FSlateApplication::Get().GetRenderer()->SetColorVisionDeficiencyType(Types[Mode], Severity, Mode != 0, false);
	}

	GetMutableDefault<UUserInterfaceSettings>()->ApplicationScale = FMath::Clamp(UserSettings->UIScale, 0.5f, 2.f);
	FALKEffects::SetReduced(UserSettings->bReduceEffects);
}

void UALKSettingsSubsystem::ApplyToAllPlayers()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(GameInstance->GetWorld()) : nullptr)
			{
				ApplyToPlayerController(PlayerController);
			}
		}
	}
}

void UALKSettingsSubsystem::ApplyToPlayerController(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	PlayerController->bForceFeedbackEnabled = UserSettings->bVibration;

	// A camera modifier rather than the camera manager's Default FOV: a pawn's Camera Component overrides that.
	if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
	{
		UALKCameraModifier_FieldOfView* FieldOfViewModifier = Cast<UALKCameraModifier_FieldOfView>(
			CameraManager->FindCameraModifierByClass(UALKCameraModifier_FieldOfView::StaticClass()));
		if (UALKDeveloperSettings::GetMenuConfig()->bApplyFieldOfViewToCamera)
		{
			if (!FieldOfViewModifier)
			{
				FieldOfViewModifier = Cast<UALKCameraModifier_FieldOfView>(CameraManager->AddNewCameraModifier(UALKCameraModifier_FieldOfView::StaticClass()));
			}
			if (FieldOfViewModifier)
			{
				FieldOfViewModifier->FieldOfView = UserSettings->FieldOfView;
			}
		}
		else if (FieldOfViewModifier)
		{
			CameraManager->RemoveCameraModifier(FieldOfViewModifier);
		}
	}
}

float UALKSettingsSubsystem::GetCustomSettingValue(FName SettingId) const
{
	if (const float* Value = UserSettings->CustomValues.Find(SettingId))
	{
		return *Value;
	}
	for (const FALKCustomSettingDefinition& Definition : UALKDeveloperSettings::GetMenuConfig()->CustomSettings)
	{
		if (Definition.Id == SettingId)
		{
			return Definition.DefaultValue;
		}
	}
	return 0.f;
}

void UALKSettingsSubsystem::SetCustomSettingValue(FName SettingId, float Value)
{
	UserSettings->CustomValues.Add(SettingId, Value);
	SaveUserSettings();
	OnSettingChanged.Broadcast(SettingId);
}

float UALKSettingsSubsystem::GetVolume(EALKVolumeChannel Channel) const
{
	switch (Channel)
	{
	case EALKVolumeChannel::Music: return UserSettings->MusicVolume;
	case EALKVolumeChannel::Effects: return UserSettings->EffectsVolume;
	case EALKVolumeChannel::Voice: return UserSettings->VoiceVolume;
	case EALKVolumeChannel::Interface: return UserSettings->InterfaceVolume;
	case EALKVolumeChannel::Master:
	default: return UserSettings->MasterVolume;
	}
}

float UALKSettingsSubsystem::GetMouseSensitivity() const { return UserSettings->MouseSensitivity; }
float UALKSettingsSubsystem::GetGamepadSensitivity() const { return UserSettings->GamepadSensitivity; }
bool UALKSettingsSubsystem::IsMouseYInverted() const { return UserSettings->bInvertMouseY; }
bool UALKSettingsSubsystem::IsGamepadYInverted() const { return UserSettings->bInvertGamepadY; }
float UALKSettingsSubsystem::GetFieldOfView() const { return UserSettings->FieldOfView; }
float UALKSettingsSubsystem::GetCameraShakeScale() const { return UserSettings->CameraShake; }
int32 UALKSettingsSubsystem::GetDifficulty() const { return UserSettings->Difficulty; }
bool UALKSettingsSubsystem::AreSubtitlesEnabled() const { return UserSettings->bSubtitles; }
bool UALKSettingsSubsystem::AreEffectsReduced() const { return UserSettings->bReduceEffects; }
float UALKSettingsSubsystem::GetSubtitleBackgroundOpacity() const { return FMath::Clamp(UserSettings->SubtitleBackground, 0.f, 1.f); }
float UALKSettingsSubsystem::GetHudOpacity() const { return FMath::Clamp(UserSettings->HudOpacity, 0.f, 1.f); }

float UALKSettingsSubsystem::GetSubtitleScale() const
{
	static const float Scales[] = { 0.8f, 1.f, 1.25f, 1.5f };
	return Scales[FMath::Clamp(UserSettings->SubtitleSize, 0, 3)];
}

#undef LOCTEXT_NAMESPACE
