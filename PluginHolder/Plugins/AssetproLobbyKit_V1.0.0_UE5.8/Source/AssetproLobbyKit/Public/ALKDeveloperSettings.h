// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ALKDeveloperSettings.generated.h"

class UALKTheme;
class UALKMenuConfig;
class UALKOnlineConfig;

/**
 * Project Settings > Plugins > Assetpro Lobby Kit.
 * Point the plugin at your Theme (look), Menu Config (content) and Online Config (multiplayer). Leave any of them empty
 * to use the built-in defaults.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Assetpro Lobby Kit"))
class ASSETPROLOBBYKIT_API UALKDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UALKDeveloperSettings();

	/** The look of every menu. */
	UPROPERTY(Config, EditAnywhere, Category = "Menu", meta = (AllowedClasses = "/Script/AssetproLobbyKit.ALKTheme"))
	TSoftObjectPtr<UALKTheme> Theme;

	/** Menu entries, levels, credits, settings tabs and widget classes. */
	UPROPERTY(Config, EditAnywhere, Category = "Menu", meta = (AllowedClasses = "/Script/AssetproLobbyKit.ALKMenuConfig"))
	TSoftObjectPtr<UALKMenuConfig> MenuConfig;

	/** Alternative to Theme: a C++ or Blueprint class whose defaults define the theme. Used when Theme is empty. */
	UPROPERTY(Config, EditAnywhere, Category = "Menu", AdvancedDisplay)
	TSoftClassPtr<UALKTheme> ThemeClass;

	/** Alternative to Menu Config: a C++ or Blueprint class whose defaults define the config. Used when Menu Config is empty. */
	UPROPERTY(Config, EditAnywhere, Category = "Menu", AdvancedDisplay)
	TSoftClassPtr<UALKMenuConfig> MenuConfigClass;

	/**
	 * Hide the engine's keyboard focus rectangle while the game runs; the menu draws its own focus highlight.
	 * Your Render Focus Rule project setting is restored when the game ends.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Menu", AdvancedDisplay)
	bool bHideEngineFocusRectangle = true;

	/** Apply the player's saved audio, accessibility and control settings when the game starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	bool bApplySavedSettingsOnStartup = true;

	/** Show the themed loading screen while levels load (standalone and packaged games only). */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	bool bEnableLoadingScreen = true;

	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0", EditCondition = "bEnableLoadingScreen"))
	float MinimumLoadingScreenDisplayTime = 1.5f;

	/** The multiplayer front end: online backend, game modes, maps, playlists, match rules, scoreboard columns and lobby behaviour. */
	UPROPERTY(Config, EditAnywhere, Category = "Online", meta = (AllowedClasses = "/Script/AssetproLobbyKit.ALKOnlineConfig"))
	TSoftObjectPtr<UALKOnlineConfig> OnlineConfig;

	/** Alternative to Online Config: a C++ or Blueprint class whose defaults define the config. Used when Online Config is empty. */
	UPROPERTY(Config, EditAnywhere, Category = "Online", AdvancedDisplay)
	TSoftClassPtr<UALKOnlineConfig> OnlineConfigClass;

	virtual FName GetCategoryName() const override;

	/** The active theme: runtime override, then the project setting, then the built-in default. Never null. */
	static const UALKTheme* GetTheme();

	/** The active menu config: project setting, then the built-in default. Never null. */
	static const UALKMenuConfig* GetMenuConfig();

	/** The active online config: project setting, then the built-in default. Never null. */
	static const UALKOnlineConfig* GetOnlineConfig();

	/** Swap the theme at runtime (for example from a "UI style" option). Pass null to go back to the project setting. */
	static void SetRuntimeTheme(UALKTheme* NewTheme);

private:
	UPROPERTY(Transient)
	TObjectPtr<UALKTheme> RuntimeTheme;

	UPROPERTY(Transient)
	mutable TObjectPtr<UALKTheme> LoadedTheme;

	UPROPERTY(Transient)
	mutable TObjectPtr<UALKMenuConfig> LoadedMenuConfig;

	UPROPERTY(Transient)
	mutable TObjectPtr<UALKOnlineConfig> LoadedOnlineConfig;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
