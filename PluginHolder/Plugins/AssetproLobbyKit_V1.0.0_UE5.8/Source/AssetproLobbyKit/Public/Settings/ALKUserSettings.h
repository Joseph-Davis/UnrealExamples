// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ALKUserSettings.generated.h"

/**
 * Player settings that are not part of UGameUserSettings (audio, controls, gameplay, accessibility, custom).
 * Saved to GameUserSettings.ini next to the engine's video settings, so it never conflicts with a project's own
 * UGameUserSettings subclass.
 */
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults, BlueprintType)
class ASSETPROLOBBYKIT_API UALKUserSettings : public UObject
{
	GENERATED_BODY()

public:
	// Audio
	UPROPERTY(Config, BlueprintReadOnly, Category = "Audio") float MasterVolume = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Audio") float MusicVolume = 0.8f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Audio") float EffectsVolume = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Audio") float VoiceVolume = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Audio") float InterfaceVolume = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Audio") bool bMuteWhenUnfocused = false;

	// Display
	UPROPERTY(Config, BlueprintReadOnly, Category = "Display") float Brightness = 0.5f;

	/** -1 keeps the project default. Otherwise a value for r.AntiAliasingMethod. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Display") int32 AntiAliasingMethod = -1;

	UPROPERTY(Config, BlueprintReadOnly, Category = "Display") bool bMotionBlur = true;

	// Controls
	UPROPERTY(Config, BlueprintReadOnly, Category = "Controls") float MouseSensitivity = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Controls") bool bInvertMouseY = false;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Controls") float GamepadSensitivity = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Controls") bool bInvertGamepadY = false;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Controls") bool bVibration = true;

	// Gameplay
	UPROPERTY(Config, BlueprintReadOnly, Category = "Gameplay") float FieldOfView = 90.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Gameplay") float CameraShake = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Gameplay") int32 Difficulty = 1;

	// Accessibility
	UPROPERTY(Config, BlueprintReadOnly, Category = "Accessibility") bool bSubtitles = true;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Accessibility") int32 ColorBlindMode = 0;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Accessibility") float ColorBlindStrength = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Accessibility") float UIScale = 1.f;
	/** The monitor the game runs on (the platform's monitor Id); empty uses the one the window opens on. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Display") FString Monitor;
	/** 0 Small, 1 Medium, 2 Large, 3 Extra Large. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Accessibility") int32 SubtitleSize = 1;
	/** Opacity of the dark box behind subtitles. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Accessibility") float SubtitleBackground = 0.5f;
	/** Turns off the interface's motion. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Accessibility") bool bReduceEffects = false;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Accessibility") float HudOpacity = 1.f;

	// Multiplayer
	/** The region to play in (the backend's region Id); empty lets the backend pick. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") FString Region;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") bool bCrossplay = true;
	/** Show ping numbers in the server browser, the lobby and the scoreboard. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") bool bShowPing = true;
	/** Show the network stats overlay (ping, packet loss, data rates) during matches. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") bool bNetStats = false;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") bool bTextChat = true;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") bool bChatFilter = true;
	/** Hide other players' names, pictures and lobby codes. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") bool bStreamerMode = false;
	/** 0 Off, 1 Push to Talk, 2 Open Mic. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") int32 VoiceMode = 1;
	/** Name of the key held to talk (an FKey name). */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") FString PushToTalkKey = TEXT("V");
	/** The microphone and the speakers used for voice chat (the backend's device Ids); empty uses the default device. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") FString VoiceInputDevice;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") FString VoiceOutputDevice;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") float MicVolume = 1.f;
	UPROPERTY(Config, BlueprintReadOnly, Category = "Multiplayer") float VoiceChatVolume = 1.f;

	/** Values of the Custom Settings defined in the menu config, by Id. */
	UPROPERTY(Config, BlueprintReadOnly, Category = "Custom") TMap<FName, float> CustomValues;
};
