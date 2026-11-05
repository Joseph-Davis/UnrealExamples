// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "ALKMenuConfig.h"
#include "GeneralProjectSettings.h"
#include "Widgets/ALKRootLayout.h"
#include "Widgets/ALKButton.h"
#include "Screens/ALKPressStartScreen.h"
#include "Screens/ALKMainMenuScreen.h"
#include "Screens/ALKSettingsMenuScreen.h"
#include "Screens/ALKPauseMenuScreen.h"
#include "Screens/ALKChapterSelectScreen.h"
#include "Screens/ALKControllerLayoutScreen.h"
#include "Screens/ALKSettingsScreen.h"
#include "Screens/ALKCreditsScreen.h"
#include "Screens/ALKConfirmDialog.h"
#include "Screens/ALKKeyCaptureDialog.h"
#include "Settings/ALKSettingRows.h"
#include "Save/ALKSaveGame.h"
#include "Screens/ALKSaveSlotScreen.h"
#include "Widgets/ALKSaveSlotCard.h"
#include "Engine/Texture2D.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKMenuConfig)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

UALKMenuConfig::UALKMenuConfig()
{
	GameTitle = LOCTEXT("DefaultGameTitle", "Your Game");
	GameSubtitle = LOCTEXT("DefaultGameSubtitle", "A game made with Assetpro Lobby Kit");
	PressStartText = LOCTEXT("PressStart", "Press any key to start");
	PauseTitle = LOCTEXT("PauseTitle", "Paused");
	LoadingText = LOCTEXT("Loading", "Loading");

	{
		// Continue loads the newest save and Load Game opens the saves; both stay greyed out until there is one.
		FALKMenuEntry Quit(TEXT("Quit"), LOCTEXT("Quit", "Quit"), EALKMenuAction::QuitGame);
		Quit.bRequiresConfirmation = true;
		Quit.ConfirmTitle = LOCTEXT("QuitTitle", "Quit Game");
		Quit.ConfirmMessage = LOCTEXT("QuitMessage", "Are you sure you want to quit to the desktop?");
		MainMenuEntries = {
			FALKMenuEntry(TEXT("PlayOnline"), LOCTEXT("PlayOnline", "Play Online"), EALKMenuAction::OpenPlayOnline),
			FALKMenuEntry(TEXT("Continue"), LOCTEXT("Continue", "Continue"), EALKMenuAction::ContinueGame),
			FALKMenuEntry(TEXT("NewGame"), LOCTEXT("NewGame", "New Game"), EALKMenuAction::StartGame),
			FALKMenuEntry(TEXT("LoadGame"), LOCTEXT("LoadGame", "Load Game"), EALKMenuAction::OpenLoadGame),
			FALKMenuEntry(TEXT("SelectChapter"), LOCTEXT("SelectChapter", "Select Chapter"), EALKMenuAction::OpenChapterSelect),
			FALKMenuEntry(TEXT("Settings"), LOCTEXT("Settings", "Settings"), EALKMenuAction::OpenSettings),
			FALKMenuEntry(TEXT("Credits"), LOCTEXT("Credits", "Credits"), EALKMenuAction::OpenCredits),
			Quit
		};
	}

	{
		auto Page = [](FName Id, const FText& Label, TArray<EALKSettingsCategory> Categories)
		{
			FALKMenuEntry Entry(Id, Label, EALKMenuAction::OpenSettingsPage);
			Entry.SettingsCategories = MoveTemp(Categories);
			return Entry;
		};
		SettingsMenuEntries = {
			Page(TEXT("General"), LOCTEXT("PageGeneral", "General"), { EALKSettingsCategory::Gameplay, EALKSettingsCategory::Accessibility }),
			Page(TEXT("Video"), LOCTEXT("PageVideo", "Video"), { EALKSettingsCategory::Video, EALKSettingsCategory::Graphics }),
			Page(TEXT("Audio"), LOCTEXT("PageAudio", "Audio"), { EALKSettingsCategory::Audio }),
			Page(TEXT("Controls"), LOCTEXT("PageControls", "Controls"), { EALKSettingsCategory::Controls }),
			Page(TEXT("Multiplayer"), LOCTEXT("PageMultiplayer", "Multiplayer"), { EALKSettingsCategory::Multiplayer }),
			FALKMenuEntry(TEXT("ControllerLayout"), LOCTEXT("ControllerLayout", "Controller Layout"), EALKMenuAction::OpenControllerLayout)
		};
	}

	// Inputs the controller layout cannot learn from the rebindable actions.
	ControllerLayoutLabels.Add(EKeys::Gamepad_Left2D, LOCTEXT("LayoutMove", "Move"));
	ControllerLayoutLabels.Add(EKeys::Gamepad_Right2D, LOCTEXT("LayoutLook", "Look"));
	ControllerLayoutLabels.Add(EKeys::Gamepad_Special_Right, LOCTEXT("LayoutPause", "Pause Menu"));

	{
		FALKMenuEntry Restart(TEXT("Restart"), LOCTEXT("Restart", "Restart Level"), EALKMenuAction::RestartLevel);
		Restart.bRequiresConfirmation = true;
		Restart.ConfirmTitle = LOCTEXT("RestartTitle", "Restart Level");
		Restart.ConfirmMessage = LOCTEXT("RestartMessage", "Any progress since the last checkpoint will be lost.");
		FALKMenuEntry MainMenu(TEXT("MainMenu"), LOCTEXT("MainMenu", "Main Menu"), EALKMenuAction::ReturnToMainMenu);
		MainMenu.bRequiresConfirmation = true;
		MainMenu.ConfirmTitle = LOCTEXT("MainMenuTitle", "Return to Main Menu");
		MainMenu.ConfirmMessage = LOCTEXT("MainMenuMessage", "Unsaved progress will be lost. Return to the main menu?");
		FALKMenuEntry Quit(TEXT("Quit"), LOCTEXT("QuitDesktop", "Quit to Desktop"), EALKMenuAction::QuitGame);
		Quit.bRequiresConfirmation = true;
		Quit.ConfirmTitle = LOCTEXT("QuitTitle", "Quit Game");
		Quit.ConfirmMessage = LOCTEXT("QuitMessage", "Are you sure you want to quit to the desktop?");
		// Online the game keeps running: no save, load or restart, and leaving asks first (the flow subsystem's dialog).
		OnlinePauseMenuEntries = {
			FALKMenuEntry(TEXT("Resume"), LOCTEXT("Resume", "Resume"), EALKMenuAction::Resume),
			FALKMenuEntry(TEXT("Settings"), LOCTEXT("Settings", "Settings"), EALKMenuAction::OpenSettings),
			FALKMenuEntry(TEXT("HostPanel"), LOCTEXT("HostPanel", "Host Panel"), EALKMenuAction::OpenAdminPanel),
			FALKMenuEntry(TEXT("LeaveGame"), LOCTEXT("LeaveGame", "Leave Game"), EALKMenuAction::LeaveOnlineGame),
			Quit
		};
		MainMenuSideWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Script/AssetproLobbyKit.ALKSocialStrip")));
		OnlinePauseTitle = LOCTEXT("OnlinePauseTitle", "Game Menu");
		OnlinePauseNote = LOCTEXT("OnlinePauseNote", "Online game: the match keeps running");

		// Entry Ids another menu system can use to open the kit's screens (see Other Menu Entries).
		OtherMenuEntries.Add(TEXT("PlayOnline"), EALKMenuAction::OpenPlayOnline);
		OtherMenuEntries.Add(TEXT("Multiplayer"), EALKMenuAction::OpenPlayOnline);
		OtherMenuEntries.Add(TEXT("HostGame"), EALKMenuAction::OpenHostGame);
		OtherMenuEntries.Add(TEXT("BrowseGames"), EALKMenuAction::OpenServerBrowser);
		OtherMenuEntries.Add(TEXT("JoinByCode"), EALKMenuAction::OpenJoinByCode);
		OtherMenuEntries.Add(TEXT("Friends"), EALKMenuAction::OpenSocial);
		OtherMenuEntries.Add(TEXT("Leaderboards"), EALKMenuAction::OpenLeaderboards);
		OtherMenuEntries.Add(TEXT("Loadout"), EALKMenuAction::OpenLoadout);
		OtherMenuEntries.Add(TEXT("LeaveGame"), EALKMenuAction::LeaveOnlineGame);
		PauseMenuEntries = {
			FALKMenuEntry(TEXT("Resume"), LOCTEXT("Resume", "Resume"), EALKMenuAction::Resume),
			FALKMenuEntry(TEXT("SaveGame"), LOCTEXT("SaveGame", "Save Game"), EALKMenuAction::OpenSaveGame),
			FALKMenuEntry(TEXT("LoadGame"), LOCTEXT("LoadGame", "Load Game"), EALKMenuAction::OpenLoadGame),
			Restart,
			FALKMenuEntry(TEXT("SelectChapter"), LOCTEXT("SelectChapter", "Select Chapter"), EALKMenuAction::OpenChapterSelect),
			FALKMenuEntry(TEXT("Settings"), LOCTEXT("Settings", "Settings"), EALKMenuAction::OpenSettings),
			MainMenu,
			Quit
		};
	}

	{
		FALKCreditsSection Design;
		Design.Heading = LOCTEXT("CreditsDesign", "Game Design");
		Design.Lines = { LOCTEXT("CreditsName1", "Your Name Here"), LOCTEXT("CreditsName2", "Another Name") };
		FALKCreditsSection Programming;
		Programming.Heading = LOCTEXT("CreditsProgramming", "Programming");
		Programming.Lines = { LOCTEXT("CreditsName3", "Lead Programmer"), LOCTEXT("CreditsName4", "Gameplay Programmer") };
		FALKCreditsSection Art;
		Art.Heading = LOCTEXT("CreditsArt", "Art & Audio");
		Art.Lines = { LOCTEXT("CreditsName5", "Environment Artist"), LOCTEXT("CreditsName6", "Sound Designer") };
		FALKCreditsSection Tech;
		Tech.Heading = LOCTEXT("CreditsTech", "Built With");
		Tech.Lines = { LOCTEXT("CreditsUE", "Unreal Engine"), LOCTEXT("CreditsALK", "Assetpro Lobby Kit") };
		FALKCreditsSection Thanks;
		Thanks.Heading = LOCTEXT("CreditsThanks", "Special Thanks");
		Thanks.Lines = { LOCTEXT("CreditsPlayer", "You, for playing") };
		Credits = { Design, Programming, Art, Tech, Thanks };
	}

	// The plugin's sixteen player emblems, as far as its content has them (the kit works without: avatars then draw a
	// coloured silhouette).
	for (int32 Index = 1; Index <= 16; ++Index)
	{
		const FString Name = FString::Printf(TEXT("T_ALK_Avatar_%02d"), Index);
		const FString Package = TEXT("/AssetproLobbyKit/Avatars/") + Name;
		if (FPackageName::DoesPackageExist(Package))
		{
			DefaultAvatars.Add(TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Package + TEXT(".") + Name)));
		}
	}

	LoadingTips = {
		LOCTEXT("Tip1", "Tip: You can rebind every action in Settings > Controls."),
		LOCTEXT("Tip2", "Tip: Lower the Shadows setting first if you need more frames per second."),
		LOCTEXT("Tip3", "Tip: Subtitles and colour blind modes are in Settings > General.")
	};

	SettingsTabs = {
		FALKSettingsTab(EALKSettingsCategory::Video),
		FALKSettingsTab(EALKSettingsCategory::Graphics),
		FALKSettingsTab(EALKSettingsCategory::Audio),
		FALKSettingsTab(EALKSettingsCategory::Controls),
		FALKSettingsTab(EALKSettingsCategory::Gameplay),
		FALKSettingsTab(EALKSettingsCategory::Multiplayer),
		FALKSettingsTab(EALKSettingsCategory::Accessibility)
	};

	RootLayoutClass = UALKRootLayout::StaticClass();
	PressStartScreenClass = UALKPressStartScreen::StaticClass();
	MainMenuScreenClass = UALKMainMenuScreen::StaticClass();
	SettingsMenuScreenClass = UALKSettingsMenuScreen::StaticClass();
	PauseMenuScreenClass = UALKPauseMenuScreen::StaticClass();
	SettingsScreenClass = UALKSettingsScreen::StaticClass();
	CreditsScreenClass = UALKCreditsScreen::StaticClass();
	ChapterSelectScreenClass = UALKChapterSelectScreen::StaticClass();
	ControllerLayoutScreenClass = UALKControllerLayoutScreen::StaticClass();
	SaveGameClass = UALKSaveGame::StaticClass();
	SaveSlotScreenClass = UALKSaveSlotScreen::StaticClass();
	SaveSlotCardClass = UALKSaveSlotCard::StaticClass();
	ConfirmDialogClass = UALKConfirmDialog::StaticClass();
	KeyCaptureDialogClass = UALKKeyCaptureDialog::StaticClass();
	ButtonClass = UALKButton::StaticClass();
	OptionRowClass = UALKSettingRow_Options::StaticClass();
	SliderRowClass = UALKSettingRow_Slider::StaticClass();
	KeyBindingRowClass = UALKSettingRow_KeyBinding::StaticClass();
	ActionRowClass = UALKSettingRow_Action::StaticClass();
}

FText UALKMenuConfig::GetTabDisplayName(EALKSettingsCategory Category) const
{
	for (const FALKSettingsTab& Tab : SettingsTabs)
	{
		if (Tab.Category == Category && !Tab.DisplayName.IsEmpty())
		{
			return Tab.DisplayName;
		}
	}

	switch (Category)
	{
	case EALKSettingsCategory::Video: return LOCTEXT("TabVideo", "Video");
	case EALKSettingsCategory::Graphics: return LOCTEXT("TabGraphics", "Graphics");
	case EALKSettingsCategory::Audio: return LOCTEXT("TabAudio", "Audio");
	case EALKSettingsCategory::Controls: return LOCTEXT("TabControls", "Controls");
	case EALKSettingsCategory::Gameplay: return LOCTEXT("TabGameplay", "Gameplay");
	case EALKSettingsCategory::Accessibility: return LOCTEXT("TabAccessibility", "Accessibility");
	case EALKSettingsCategory::Multiplayer: return LOCTEXT("TabMultiplayer", "Multiplayer");
	default: return FText::GetEmpty();
	}
}

UTexture2D* UALKMenuConfig::GetDefaultAvatar(const FString& PlayerName) const
{
	if (PlayerName.IsEmpty() || DefaultAvatars.IsEmpty())
	{
		return nullptr;
	}
	// The same picture for the same name on every machine and every run (lower case: names compare without case).
	// The multiplier spreads the hash's high bits, which pick the picture: names that differ little get different ones.
	const uint32 Hash = FCrc::StrCrc32(*PlayerName.ToLower()) * 7575u;
	const TSoftObjectPtr<UTexture2D>& Avatar = DefaultAvatars[(Hash >> 24) % DefaultAvatars.Num()];
	return Avatar.IsNull() ? nullptr : Avatar.LoadSynchronous();
}

const FALKChapter* UALKMenuConfig::FindChapter(FName ChapterId) const
{
	return Chapters.FindByPredicate([ChapterId](const FALKChapter& Chapter) { return Chapter.Id == ChapterId; });
}

FText UALKMenuConfig::GetResolvedVersionText() const
{
	if (!VersionText.IsEmpty())
	{
		return VersionText;
	}

	const FString& ProjectVersion = GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	return FText::Format(LOCTEXT("VersionFormat", "Version {0}"), FText::FromString(ProjectVersion));
}

#undef LOCTEXT_NAMESPACE
