// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "Online/ALKOnlineTypes.h"
#include "Rules/ALKGameRules.h"
#include "ALKOnlineConfig.generated.h"

class UALKOnlineProvider;
class AGameModeBase;
class UTexture2D;
class UWorld;

/** A game mode players can host: its name, team layout and player counts. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKModeDefinition
{
	GENERATED_BODY()

	/** The name your game code knows the mode by, for example "TeamDeathmatch". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
	FName Id;

	/** Shown on the host screen, in the server browser and in the lobby. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
	FText Name;

	/** A line or two about the mode, shown under its name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode", meta = (MultiLine = true))
	FText Description;

	/** How many teams. 0 = free for all (no teams). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode", meta = (ClampMin = "0", ClampMax = "8"))
	int32 TeamCount = 2;

	/** The player limit a new game of this mode starts with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode", meta = (ClampMin = "1"))
	int32 DefaultMaxPlayers = 8;

	/** The player limits the host can pick. Empty = the Online Config's Default Max Players Options. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
	TArray<int32> MaxPlayersOptions;

	/** Rules only this mode uses (rule definitions with All Modes turned off), by Id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
	TArray<FName> ExtraRules;

	/**
	 * The game mode class matches of this mode run with (added to the travel URL as "?game="). Leave empty to use the
	 * map level's own game mode.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
	TSoftClassPtr<AGameModeBase> GameModeClass;
};

/** A map players can host a match on. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKMapDefinition
{
	GENERATED_BODY()

	/** The name your game code knows the map by, for example "Arena". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	FName Id;

	/** Shown on the host screen, in the lobby and in the map vote. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	FText Name;

	/** Shown on the host screen, in the lobby and in the map vote. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/** The level the match is played in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	TSoftObjectPtr<UWorld> Level;

	/** The modes this map can be played in, by Id. Empty = every mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	TArray<FName> SupportedModes;

	bool SupportsMode(FName ModeId) const { return SupportedModes.Num() == 0 || ModeId.IsNone() || SupportedModes.Contains(ModeId); }
};

/** A quick match card on the Play screen. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKPlaylist
{
	GENERATED_BODY()

	/** The name Start Quick Match takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	FName Id;

	/** The card's title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	FText Name;

	/** A line under the card's title. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist", meta = (MultiLine = true))
	FText Description;

	/** The game mode the playlist searches for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	FName ModeId;

	/** A short tag on the card, for example "4v4" or "8 players". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	FText SizeLabel;

	/** The card's picture. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/** Drawn larger / first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	bool bFeatured = false;

	/** The player limit when a quick match has to host its own game. 0 = the mode's default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist", meta = (ClampMin = "0"))
	int32 MaxPlayers = 0;
};

/** Where a scoreboard column reads its number from. */
UENUM(BlueprintType)
enum class EALKScoreSource : uint8
{
	Kills,
	Deaths,
	Assists,
	/** The player state's Score. */
	Score,
	/** The player state's ping, in milliseconds. */
	Ping,
	/** A number your game sets with Add Custom Stat / Set Custom Stat on the ALK player state. */
	CustomStat
};

/** One column of the scoreboard. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKScoreColumn
{
	GENERATED_BODY()

	/** A name of your own for the column (not shown). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column")
	FName Id;

	/** The column's title, for example "K" or "Score". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column")
	FText Header;

	/** Which of the player's numbers the column shows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column")
	EALKScoreSource Source = EALKScoreSource::Score;

	/** The stat's name, for a Custom Stat column. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column", meta = (EditCondition = "Source == EALKScoreSource::CustomStat", EditConditionHides))
	FName CustomStat;

	/** The column's width, in the theme's units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Column", meta = (ClampMin = "20"))
	float Width = 72.f;

	FALKScoreColumn() = default;
	FALKScoreColumn(FName InId, const FText& InHeader, EALKScoreSource InSource, float InWidth = 72.f)
		: Id(InId), Header(InHeader), Source(InSource), Width(InWidth) {}
};

/** A loadout (a class, a character, a kit) players can pick on the loadout screen. Your game gives the Id its meaning. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKLoadoutDefinition
{
	GENERATED_BODY()

	/** The name your game code knows the loadout by. It is what the player state's Loadout Id holds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (MultiLine = true))
	FText Description;

	/** Shown on the loadout's card. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	/** The player level that unlocks it (the player state's Player Level). 1 = open to everyone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	FALKLoadoutDefinition() = default;
	FALKLoadoutDefinition(FName InId, const FText& InName, const FText& InDescription, int32 InRequiredLevel = 1)
		: Id(InId), Name(InName), Description(InDescription), RequiredLevel(InRequiredLevel) {}
};

/** A leaderboard the leaderboards screen offers. The online provider reads its rows by Id. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKLeaderboardDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	FName Id;

	/** The tab's label. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	FText Name;

	/** The heading above the numbers: "Wins", "Score", "Best time". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	FText ValueHeader;

	FALKLeaderboardDefinition() = default;
	FALKLeaderboardDefinition(FName InId, const FText& InName, const FText& InValueHeader)
		: Id(InId), Name(InName), ValueHeader(InValueHeader) {}
};

/** A reason on the report dialog. The Id reaches your moderation code with On Player Reported. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKReportReason
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Report")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Report")
	FText Name;

	FALKReportReason() = default;
	FALKReportReason(FName InId, const FText& InName) : Id(InId), Name(InName) {}
};

/**
 * The content of the multiplayer front end: the online backend, game modes, maps, quick match playlists, match rules,
 * scoreboard columns, teams and lobby behaviour. Everything has a working default, so the plugin runs with no asset at all.
 *
 * Make a data asset of this class (or a Blueprint child) and select it in Project Settings > Plugins > Assetpro Lobby Kit > Online.
 */
UCLASS(BlueprintType, Blueprintable)
class ASSETPROLOBBYKIT_API UALKOnlineConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UALKOnlineConfig();

	// ---------------------------------------------------------------- Backend

	/** Turns the multiplayer front end on. Off: no provider is created and every online action fails quietly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backend")
	bool bEnableOnline = true;

	/**
	 * The online backend. The default uses the engine's Online Subsystem sessions (LAN out of the box; a platform
	 * service by config). Select ALK Sample Online Provider to see friends, party and matchmaking with made-up data, or
	 * your own child of ALK Online Provider (C++ or Blueprint).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backend", meta = (EditCondition = "bEnableOnline"))
	TSubclassOf<UALKOnlineProvider> ProviderClass;

	/** The version players must share to play together. Empty = the Project Version from Project Settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backend")
	FString BuildVersion;

	/** Most games a search returns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backend", meta = (ClampMin = "1", ClampMax = "500"))
	int32 MaxSearchResults = 50;

	/** Seconds a search may take before it ends with what it has. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backend", meta = (ClampMin = "2", ClampMax = "60"))
	float FindTimeoutSeconds = 15.f;

	/** Seconds after a lost connection in which "Rejoin Match" is offered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backend", meta = (ClampMin = "0"))
	float ReconnectWindowSeconds = 180.f;

	// ---------------------------------------------------------------- Content

	/** The game modes players can host, in the order they are offered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (TitleProperty = "Name"))
	TArray<FALKModeDefinition> Modes;

	/** The maps players can host. Empty by default: add yours. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (TitleProperty = "Name"))
	TArray<FALKMapDefinition> Maps;

	/** The quick match cards. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (TitleProperty = "Name"))
	TArray<FALKPlaylist> Playlists;

	/** The match rules on the host screen, in order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (TitleProperty = "DisplayName"))
	TArray<FALKRuleDefinition> RuleDefinitions;

	/** The scoreboard's columns, left to right (after the player's name). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (TitleProperty = "Header"))
	TArray<FALKScoreColumn> ScoreColumns;

	/** The choices on the loadout screen. Empty hides the screen's cards (and says where to add them). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (TitleProperty = "Name"))
	TArray<FALKLoadoutDefinition> Loadouts;

	/** The tabs of the leaderboards screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (TitleProperty = "Name"))
	TArray<FALKLeaderboardDefinition> Leaderboards;

	/** The reasons on the report dialog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (TitleProperty = "Name"))
	TArray<FALKReportReason> ReportReasons;

	/**
	 * The level a hosted game opens first: the lobby. Its game mode should be ALK Lobby Game Mode or a child.
	 * Left empty, the engine's empty level (/Engine/Maps/Entry) is used with ALK Lobby Game Mode forced on it (see Use
	 * Empty Level Fallback), so hosting works before you have made a lobby level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	TSoftObjectPtr<UWorld> LobbyLevel;

	/**
	 * With no Lobby Level, or a map without a Level, host and start the match in the engine's empty level
	 * (/Engine/Maps/Entry) with the ALK lobby / match game mode forced through the "?game=" URL option. This lets the
	 * whole flow run with no assets at all. Turn off once your levels are in: a missing level then stops the start instead.
	 * A packaged game needs that level cooked: it is when it is your Game Default Map; otherwise add it to Maps To Cook.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	bool bUseEmptyLevelFallback = true;

	/** The player limit used when a mode is not listed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content", meta = (ClampMin = "1", ClampMax = "64"))
	int32 DefaultMaxPlayers = 8;

	/** The player limits the host can pick when the mode lists none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	TArray<int32> DefaultMaxPlayersOptions;

	/** Team names, by team number. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	TArray<FText> TeamNames;

	/** Team colours, by team number. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Content")
	TArray<FLinearColor> TeamColors;

	// ---------------------------------------------------------------- Lobby

	/** Seconds counted down once everyone is ready. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby", meta = (ClampMin = "0", ClampMax = "60"))
	float LobbyCountdownSeconds = 5.f;

	/** Seconds counted down when the host forces the start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby", meta = (ClampMin = "0", ClampMax = "60"))
	float ForceStartCountdownSeconds = 3.f;

	/** The fewest players (spectators not counted) a match starts with. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby", meta = (ClampMin = "1"))
	int32 MinPlayersToStart = 1;

	/** The host can start before everyone is ready. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby")
	bool bAllowForceStart = true;

	/**
	 * The "Bots" rule seats that many placeholder players in the lobby (names, teams, ready-up, scoreboard rows, nobody
	 * behind them): one person can fill a lobby and see every screen with players in it. For demos, tutorials and testing.
	 * Off (the default): the rule only reaches your game through On Bot Count Changed, for bots of your own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby")
	bool bBotsArePlaceholders = false;

	/** Players vote for the map; the most voted one is played. Off: the host's map is played. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby")
	bool bAllowMapVote = true;

	/** Players can pick their team. Off: only the host moves players. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby")
	bool bAllowTeamChoice = true;

	/** Joins and leaves appear as system lines in the chat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby")
	bool bAnnounceJoinLeaveInChat = true;

	/** Seconds the results stay up before everyone returns to the lobby. 0 = until the host chooses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby", meta = (ClampMin = "0"))
	float PostMatchSeconds = 30.f;

	/** How many loadout colours players can pick from (the lobby clamps Color Index to it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby", meta = (ClampMin = "1", ClampMax = "32"))
	int32 PlayerColorCount = 8;

	// ---------------------------------------------------------------- Matchmaking

	/** Seconds a player has to accept a found match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Matchmaking", meta = (ClampMin = "3", ClampMax = "60"))
	float MatchFoundAcceptSeconds = 10.f;

	/** A quick match that finds no game hosts one of its own instead of failing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Matchmaking")
	bool bQuickMatchHostsWhenNothingFound = true;

	// ---------------------------------------------------------------- Chat and social

	/** Words masked with asterisks when the chat filter is on (whole words, any case). Empty by default: add your own list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chat")
	TArray<FString> FilteredWords;

	/** Longest chat message, in characters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chat", meta = (ClampMin = "16", ClampMax = "512"))
	int32 ChatMaxLength = 160;

	/** How many chat lines are kept. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chat", meta = (ClampMin = "10", ClampMax = "500"))
	int32 ChatHistorySize = 100;

	/** Longest player name, in characters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chat", meta = (ClampMin = "3", ClampMax = "48"))
	int32 PlayerNameMaxLength = 24;

	/** How many recently met players are remembered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Social", meta = (ClampMin = "0", ClampMax = "200"))
	int32 RecentPlayersMax = 30;

	/** How many recently played servers are remembered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Social", meta = (ClampMin = "0", ClampMax = "100"))
	int32 RecentServersMax = 15;

	// ---------------------------------------------------------------- Lookups

	/** A mode by Id, or null. */
	const FALKModeDefinition* FindMode(FName ModeId) const;

	/** A map by Id, or null. */
	const FALKMapDefinition* FindMap(FName MapId) const;

	/** A playlist by Id, or null. */
	const FALKPlaylist* FindPlaylist(FName PlaylistId) const;

	/** A rule definition by Id, or null. */
	const FALKRuleDefinition* FindRule(FName RuleId) const;

	/** A loadout by Id, or null. */
	const FALKLoadoutDefinition* FindLoadout(FName LoadoutId) const;

	/** A leaderboard by Id, or null. */
	const FALKLeaderboardDefinition* FindLeaderboard(FName BoardId) const;

	/** The rules a mode offers: every All Modes rule, then the mode's Extra Rules. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	TArray<FALKRuleDefinition> GetRulesForMode(FName ModeId) const;

	/** The maps a mode can be played on. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<FALKMapDefinition> GetMapsForMode(FName ModeId) const;

	/** The player limits a mode offers (never empty). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<int32> GetMaxPlayersOptions(FName ModeId) const;

	/** How many teams a mode has. 0 = free for all, also for an unknown mode. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	int32 GetTeamCount(FName ModeId) const;

	/** A mode's name, or its Id as text when it is not listed. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FText GetModeName(FName ModeId) const;

	/** A map's name, or its Id as text when it is not listed. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FText GetMapName(FName MapId) const;

	/** "Blue", "Orange"... or "Team 3" past the listed names. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FText GetTeamName(int32 TeamId) const;

	/** A team's colour; white past the listed colours. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FLinearColor GetTeamColor(int32 TeamId) const;

	/** Build Version, or the project version when it is empty. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetResolvedBuildVersion() const;

	/** The package name of the level a map (or a raw "/Game/..." path used as a map Id) is played in. Empty when there is none. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetMapLevelPath(FName MapId) const;

	/**
	 * The start of the URL a host opens for the lobby: the Lobby Level, or the empty level with "?game=" ALK Lobby Game
	 * Mode when there is none and the fallback is on. Empty when there is nothing to open.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetLobbyTravelURL() const;

	/**
	 * The start of the URL a match travels to: the map's level (with the mode's game mode as "?game=" when it names one),
	 * or the empty level with "?game=" the mode's game mode / ALK Match Game Mode when the map has no level and the
	 * fallback is on. Empty when there is nothing to travel to.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetMatchTravelURL(FName ModeId, FName MapId) const;

	/** A ready set of host settings: the first mode, its first map, default player limit and default rules. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FALKHostSettings MakeDefaultHostSettings(const FString& HostName) const;

	/**
	 * Host settings made valid: a known mode, a map of that mode, a player limit the mode offers, the mode's rules with
	 * valid values, a trimmed server name (Fallback Name when empty) and the bot count from the "Bots" rule.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FALKHostSettings SanitizeHostSettings(const FALKHostSettings& Settings, const FString& FallbackName) const;
};
