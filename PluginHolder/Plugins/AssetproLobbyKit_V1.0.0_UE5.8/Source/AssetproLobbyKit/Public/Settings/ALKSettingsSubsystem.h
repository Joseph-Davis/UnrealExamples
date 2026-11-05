// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Containers/Ticker.h"
#include "GenericPlatform/GenericApplication.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ALKTypes.h"
#include "ALKSettingsSubsystem.generated.h"

class UALKSetting;
class UALKUserSettings;
class ULocalPlayer;
class APlayerController;
class USoundClass;
class USoundMix;
class UGameUserSettings;

UENUM(BlueprintType)
enum class EALKVolumeChannel : uint8
{
	Master,
	Music,
	Effects,
	Voice,
	Interface
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnSettingChanged, FName, SettingId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnSettingsApplied);

/**
 * Owns the player's settings: builds the list shown by the settings screen, applies values to the engine and saves
 * them. Game code reads values from here (sensitivity, FOV, difficulty, custom settings...).
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Builds every setting (built-in and custom) for a player. Hidden settings are left out. */
	TArray<UALKSetting*> CreateSettings(UObject* Outer, ULocalPlayer* LocalPlayer);

	/** Applies video settings and saves everything. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void ApplyAndSave();

	/** Throws away changes made since the last Apply. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void RevertUnsavedChanges();

	/** Keeps a window mode / resolution change made by the last Apply (answer to "Keep display settings?"). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void ConfirmDisplayMode();

	/** Returns to the window mode and resolution from before the last Apply. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void RevertDisplayMode();

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool HasUnsavedChanges() const { return bDirty; }

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	UALKUserSettings* GetUserSettings() const { return UserSettings; }

	/** Pushes every saved value to the engine (audio, brightness, accessibility, controls). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void ApplyUserSettings();

	/** Applies per-player values (FOV, vibration) to a player controller. Called automatically for every local player controller. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void ApplyToPlayerController(APlayerController* PlayerController);

	/**
	 * Records the current values as the "last applied" state that Revert Unsaved Changes returns to. Called after every
	 * apply; call it yourself if you change and save settings outside the settings screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void CaptureAppliedState();

	// ---------------------------------------------------------------- Values for game code

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetCustomSettingValue(FName SettingId) const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool GetCustomSettingBool(FName SettingId) const { return GetCustomSettingValue(SettingId) > 0.5f; }

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void SetCustomSettingValue(FName SettingId, float Value);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetVolume(EALKVolumeChannel Channel) const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetMouseSensitivity() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetGamepadSensitivity() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool IsMouseYInverted() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool IsGamepadYInverted() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetFieldOfView() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetCameraShakeScale() const;

	/** 0 = Easy, 1 = Normal, 2 = Hard. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	int32 GetDifficulty() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool AreSubtitlesEnabled() const;

	/**
	 * True while a change waits for Apply that moves or resizes the game window (window mode, resolution or monitor):
	 * applying it asks the player to keep the new display.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool IsDisplayChangePending() const;

	/** The monitors the game can run on, in the order of the Monitor setting (the platform's display list). */
	static TArray<FMonitorInfo> GetMonitors();

	/** The index (in Get Monitors) of the monitor the game window is on now, or -1. */
	static int32 GetWindowMonitorIndex();

	/** The monitor a Monitor setting value means: that monitor while it is connected, otherwise the one the window is on. */
	static FString ResolveMonitorId(const FString& MonitorId);

	/** Size of subtitles as a scale of the theme's subtitle font: 0.8 (Small) to 1.5 (Extra Large). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetSubtitleScale() const;

	/** Opacity (0..1) of the dark box behind subtitles. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetSubtitleBackgroundOpacity() const;

	/** True when the player turned the interface's motion off (Reduce Effects). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool AreEffectsReduced() const;

	/** Opacity (0..1) of the HUD: prompts, subtitles, notifications and the saving indicator. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	float GetHudOpacity() const;

	/** True when the player wants the network stats overlay during matches (Settings > Multiplayer). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool IsNetStatsOverlayEnabled() const;

	/** True when ping numbers are shown (Settings > Multiplayer > Show Ping). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool IsPingShown() const;

	/** True when other players' names, pictures and lobby codes are hidden (Settings > Multiplayer > Streamer Mode). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool IsStreamerModeEnabled() const;

	/** The key held to talk (Settings > Multiplayer). Bind it in your player controller, or use the kit's lobby controller. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	FKey GetPushToTalkKey() const;

	/** Hands the saved Multiplayer settings to the online subsystem. Runs with Apply User Settings and on every change. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void ApplyMultiplayer();

	/** Broadcast when any setting changes, with its Id. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Settings")
	FALKOnSettingChanged OnSettingChanged;

	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Settings")
	FALKOnSettingsApplied OnSettingsApplied;

	/** Native version of On Setting Changed, fired after the unsaved-changes state is updated. */
	TMulticastDelegate<void(UALKSetting*)> OnSettingChangedNative;

	/** Every built-in setting Id, for documentation and the ALK.ListSettings console command. */
	static TArray<FName> GetBuiltInSettingIds();

private:
	void HandleSettingChanged(UALKSetting* Setting);
	void HandlePostLoadMap(UWorld* World);
	void SyncDisplayWithWindow();

	/**
	 * Moves the game window onto a monitor, then applies the saved window mode and resolution there. A fullscreen window
	 * cannot move, so it goes windowed first; takes a few frames.
	 */
	void MoveWindowToMonitor(const FString& MonitorId);

	/** Once the game window exists at startup, moves it to the saved monitor. */
	bool RestoreMonitorOnStartup(float DeltaTime);

	/** One step of Move Window To Monitor a frame. */
	bool TickWindowMove(float DeltaTime);

	FString MovingToMonitor;
	int32 MoveStep = 0;
	uint64 MoveStepFrame = 0;
	FTSTicker::FDelegateHandle WindowMoveTicker;
	void InitializeCustomDefaults();
	void ApplyAudio();
	void ApplyBrightness();
	void ApplyGraphicsOverrides();
	void ApplyAccessibility();
	void ApplyToAllPlayers();
	void SaveUserSettings();
	UGameUserSettings* GetGameUserSettings() const;

	UPROPERTY(Transient)
	TObjectPtr<UALKUserSettings> UserSettings;

	/** Copy of UserSettings at the last apply; revert copies it back. */
	UPROPERTY(Transient)
	TObjectPtr<UALKUserSettings> AppliedUserSettings;

	/** Video settings at the last apply (UGameUserSettings keeps scalability outside of UPROPERTYs). */
	struct FVideoState
	{
		int32 WindowMode = 0;
		FIntPoint Resolution = FIntPoint::ZeroValue;
		bool bVSync = false;
		float FrameRateLimit = 0.f;
		float ResolutionScale = 0.f;
		int32 Quality[10] = {};
	};
	FVideoState AppliedVideo;

	/**
	 * Window mode and resolution before the last apply. Kept here rather than relying on UGameUserSettings'
	 * "last confirmed" values: the engine confirms every windowed resize itself (UGameEngine::OnViewportResized),
	 * including the one caused by applying a new resolution.
	 */
	int32 PreviousWindowMode = 0;
	FIntPoint PreviousResolution = FIntPoint::ZeroValue;
	FString PreviousMonitor;
	/** The monitor the window was on before the last Apply, which a revert moves it back to. */
	FString PreviousWindowMonitor;

	/** The frame of the menu's last display change (apply, keep or revert): the window follows a frame or two later. */
	uint64 DisplayChangeFrame = 0;

	/** r.AntiAliasingMethod before the player overrode it, restored when they go back to the default. */
	int32 ProjectAntiAliasingMethod = -1;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> VolumeSoundMix;

	/** The sound classes the volume settings adjust, kept loaded while the mix overrides them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundClass>> VolumeSoundClasses;

	bool bDirty = false;
	bool bOverridesFocusRule = false;
	FDelegateHandle SettingChangedHandle;
	FDelegateHandle PostLoadMapHandle;
};
