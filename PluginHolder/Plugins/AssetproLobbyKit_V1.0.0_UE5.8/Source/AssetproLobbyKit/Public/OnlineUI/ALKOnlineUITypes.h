// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "ALKOnlineUITypes.generated.h"

class UCommonActivatableWidget;
class UUserWidget;

/** Where the multiplayer front end is: nowhere (menus), in a lobby, in a match or on the results screen. */
UENUM(BlueprintType)
enum class EALKOnlineStage : uint8
{
	/** Not in a game: the menus. */
	None,
	Lobby,
	Match,
	Results
};

/**
 * The keys of the match overlay. They are read straight from the keyboard and gamepad while a match is on and no menu
 * is open, so they need no input mapping. Push to talk is the player's own choice (Settings > Multiplayer).
 */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKOnlineKeys
{
	GENERATED_BODY()

	/** Hold to see the scoreboard. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey Scoreboard = EKeys::Tab;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey ScoreboardGamepad = EKeys::Gamepad_Special_Left;

	/** Opens the chat line (everyone). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey Chat = EKeys::Enter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey ChatAlternate = EKeys::T;

	/** Opens the chat line for the team. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey TeamChat = EKeys::Y;

	/** While spectating: the previous / next player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey SpectatePrevious = EKeys::Left;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey SpectateNext = EKeys::Right;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey SpectatePreviousGamepad = EKeys::Gamepad_LeftShoulder;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey SpectateNextGamepad = EKeys::Gamepad_RightShoulder;

	/** Push to talk on a gamepad (the keyboard key is the player's setting). Leave unset for none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Keys")
	FKey PushToTalkGamepad;
};

/**
 * The widget classes of the multiplayer front end. Each one is the kit's own screen by default; point one at your
 * Blueprint child (or any screen of your own) to replace it. The Online Flow subsystem opens them.
 */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKOnlineScreenClasses
{
	GENERATED_BODY()

	FALKOnlineScreenClasses();

	/** Quick Match, Host Game, Browse Games, Join by Code, Party and the playlists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens")
	TSoftClassPtr<UCommonActivatableWidget> PlayHub;

	/** Server name, mode, map, players, password and the match rules; also edits a lobby that is already open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens")
	TSoftClassPtr<UCommonActivatableWidget> HostGame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens")
	TSoftClassPtr<UCommonActivatableWidget> ServerBrowser;

	/** Friends, recent players, the party and invites. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens")
	TSoftClassPtr<UCommonActivatableWidget> Social;

	/** Player slots, teams, ready-up, chat and the host's controls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens")
	TSoftClassPtr<UCommonActivatableWidget> Lobby;

	/** Shown while hosting, searching or joining, with Cancel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Dialogs")
	TSoftClassPtr<UCommonActivatableWidget> ConnectingDialog;

	/** Accept or decline a found match before its countdown ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Dialogs")
	TSoftClassPtr<UCommonActivatableWidget> MatchFoundDialog;

	/** A line of text: a lobby code, a password, a preset's name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Dialogs")
	TSoftClassPtr<UCommonActivatableWidget> TextEntryDialog;

	/** What you can do with one player: profile, mute, whisper, invite, move, kick, ban, report. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Dialogs")
	TSoftClassPtr<UCommonActivatableWidget> PlayerOptionsDialog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Dialogs")
	TSoftClassPtr<UCommonActivatableWidget> ProfileDialog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Dialogs")
	TSoftClassPtr<UCommonActivatableWidget> ReportDialog;

	/** Winner, most valuable player, the final scoreboard, rematch vote, back to the lobby. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Match")
	TSoftClassPtr<UCommonActivatableWidget> Results;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Match")
	TSoftClassPtr<UCommonActivatableWidget> Leaderboards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Match")
	TSoftClassPtr<UCommonActivatableWidget> Loadout;

	/** The host's panel: kick, ban list, change map, pause. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Match")
	TSoftClassPtr<UCommonActivatableWidget> AdminPanel;

	/** The match overlay: score, timer, event feed, chat, who is talking, network stats, and the scoreboard while its key is held. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online Screens|Match")
	TSoftClassPtr<UUserWidget> MatchHUD;
};
