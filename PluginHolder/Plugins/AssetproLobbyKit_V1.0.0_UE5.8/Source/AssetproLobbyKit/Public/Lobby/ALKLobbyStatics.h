// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Lobby/ALKLobbyTypes.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineTypes.h"
#include "Rules/ALKGameRules.h"
#include "ALKLobbyStatics.generated.h"

class UALKOnlineSubsystem;
class UALKPlayerNetComponent;
class AALKLobbyGameState;
class AALKLobbyPlayerState;
class APlayerController;
class APlayerState;

/**
 * The lobby and match rules as plain functions with no network in them: who gets which slot and team, when a match can
 * start, which map a vote picks, who the MVP is, lobby codes, session filters, rule values and chat cleaning. The ALK
 * game modes call exactly these, so what Run Logic Checks proves is what runs in a game. Also the Blueprint shortcuts
 * for the online subsystem and the helper structs.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKLobbyStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------- Shortcuts

	/** The online subsystem: host, find, join, quick match, party, friends, chat. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Assetpro Online Subsystem"))
	static UALKOnlineSubsystem* GetOnlineSubsystem(const UObject* WorldContextObject);

	/** The active Online Config (project setting or built-in default). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online", meta = (DisplayName = "Get Assetpro Online Config"))
	static const UALKOnlineConfig* GetOnlineConfig();

	/** The ALK lobby / match game state of the current level, or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Assetpro Lobby Game State"))
	static AALKLobbyGameState* GetLobbyGameState(const UObject* WorldContextObject);

	/** The local player's net component (ready, team, chat, votes, host actions), or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Local Player Net Component"))
	static UALKPlayerNetComponent* GetLocalNetComponent(const UObject* WorldContextObject);

	/** The local player's ALK player state, or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Local Lobby Player State"))
	static AALKLobbyPlayerState* GetLocalPlayerState(const UObject* WorldContextObject);

	// ---------------------------------------------------------------- Slots and teams

	/** The lowest slot number nobody has, or -1 when all Max Players slots are taken. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static int32 FindFreeSlot(const TArray<int32>& TakenSlots, int32 MaxPlayers);

	/** How many players a team can hold: the player limit shared out, rounded up. 0 teams = everyone in one group. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static int32 GetTeamCapacity(int32 MaxPlayers, int32 TeamCount);

	/** How many of the listed players are on a team. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static int32 CountTeam(const TArray<int32>& PlayerTeamIds, int32 TeamId);

	/** The team a new player joins: the one with the fewest players, the lowest number on a tie. -1 with no teams. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static int32 PickSmallestTeam(const TArray<int32>& PlayerTeamIds, int32 TeamCount);

	/** Whether one more player fits on a team (Player Team Ids = the teams of everyone else). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static bool CanJoinTeam(const TArray<int32>& PlayerTeamIds, int32 TeamCount, int32 MaxPlayers, int32 TeamId);

	// ---------------------------------------------------------------- Starting, votes, results

	/**
	 * Why the match cannot start, or None when it can. Active Players = players who are not spectating; Ready Players =
	 * how many of them are ready. A forced start skips the ready check, nothing else.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static EALKStartBlocker GetStartBlocker(int32 ActivePlayers, int32 ReadyPlayers, int32 MinPlayers, bool bHasMap, bool bForce);

	/** A line for the start button's hint: "Waiting for players", "Not everyone is ready"... Empty for None. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static FText GetStartBlockerText(EALKStartBlocker Blocker);

	/** Counts the votes per map, most voted first (ties in the order the maps were first voted for). Empty votes are skipped. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static TArray<FALKMapVote> TallyMapVotes(const TArray<FName>& Votes);

	/** The map a vote picks: the most voted one, one of the tied ones by Seed, or Fallback Map Id when nobody voted. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static FName ResolveMapVote(const TArray<FALKMapVote>& Tallies, FName FallbackMapId, int32 Seed);

	/** A rematch vote passes when more than half of the voters said yes. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	static bool IsRematchVotePassed(int32 YesVotes, int32 Voters);

	/** The team with the highest score, or -1 for a draw (or no teams). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	static int32 PickWinningTeam(const TArray<int32>& TeamScores);

	/**
	 * The index of the match's best player: the highest score on the winning team (on any team when Winner Team is -1),
	 * then more kills, fewer deaths, more assists, and the earlier player. -1 when there is nobody.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	static int32 PickMVP(const TArray<FALKScoreLine>& Players, int32 WinnerTeam);

	/** Whether a limit from the rules is reached. A limit of 0 or less means "no limit". */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	static bool IsLimitReached(float Value, float Limit);

	// ---------------------------------------------------------------- Lobby codes

	/** The code for an address ("192.168.1.20:7777"), for example "7K2-M9Q-4XD". Empty when it is not an IPv4 address. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static FString EncodeLobbyCode(const FString& Address);

	/** The address a code stands for. False when the code is mistyped. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static bool DecodeLobbyCode(const FString& Code, FString& OutAddress);

	/** What a "join by code" box accepts: a lobby code or a raw address. Gives the address to connect to. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static bool ResolveJoinTarget(const FString& CodeOrAddress, FString& OutAddress);

	/** A code as typed, tidied for display: upper case, grouped in threes. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static FString NormalizeLobbyCode(const FString& Code);

	// ---------------------------------------------------------------- Sessions

	/** Whether a game passes the server browser's filters. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static bool DoesSessionMatchFilter(const FALKSessionInfo& Session, const FALKSessionFilter& Filter);

	/** The games that pass the filters, in the same order. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static TArray<FALKSessionInfo> FilterSessions(const TArray<FALKSessionInfo>& Sessions, const FALKSessionFilter& Filter);

	/** The games sorted by a column of the server browser. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static TArray<FALKSessionInfo> SortSessions(const TArray<FALKSessionInfo>& Sessions, EALKSessionSort SortBy, bool bAscending);

	/**
	 * The game a quick match joins: passes the filter, same version, not full, no password, then the lowest ping (an
	 * unknown ping counts as the worst) and the fullest lobby. False when no game qualifies.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static bool PickBestSession(const TArray<FALKSessionInfo>& Sessions, const FALKSessionFilter& Filter, FALKSessionInfo& OutSession);

	/** The game has no free player slot. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static bool IsSessionFull(const FALKSessionInfo& Session) { return Session.IsFull(); }

	/** A localized title and message for a connection error dialog. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static void GetConnectionErrorText(EALKConnectionError Error, FText& OutTitle, FText& OutMessage);

	/** Ping, packet loss and traffic of the current connection, for a net stats overlay. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online", meta = (WorldContext = "WorldContextObject"))
	static FALKNetStats GetNetStats(const UObject* WorldContextObject);

	// ---------------------------------------------------------------- Rules

	/** A set of rules with every definition at its default. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static FALKGameRules MakeDefaultRules(const TArray<FALKRuleDefinition>& Definitions);

	/** Every value made valid for its definition; missing rules added, unknown ones dropped. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static FALKGameRules SanitizeRules(const FALKGameRules& Rules, const TArray<FALKRuleDefinition>& Definitions);

	/** A random valid value for every rule. The same seed gives the same rules. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static FALKGameRules RandomizeRules(const TArray<FALKRuleDefinition>& Definitions, int32 Seed);

	/** "TimeLimit=10,ScoreLimit=50": for a level URL ("?ALKRules=...") or a session setting. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static FString RulesToString(const FALKGameRules& Rules);

	/** Reads Rules To String back; also accepts a whole level URL options string. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static FALKGameRules RulesFromString(const FString& Options);

	/** A rule's value as a switch (true for "1"). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static bool GetRuleBool(const FALKGameRules& Rules, FName RuleId, bool bDefault = false);

	/** A rule's value as a whole number. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static int32 GetRuleInt(const FALKGameRules& Rules, FName RuleId, int32 Default = 0);

	/** A rule's value as a decimal number. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static float GetRuleFloat(const FALKGameRules& Rules, FName RuleId, float Default = 0.f);

	/** A rule's value as text. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static FString GetRuleString(const FALKGameRules& Rules, FName RuleId, const FString& Default = TEXT(""));

	/** Sets one rule's value, as text ("1", "10", "0.5", "Seed text"). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Rules")
	static void SetRuleValue(UPARAM(ref) FALKGameRules& Rules, FName RuleId, const FString& Value);

	/** A rule's value as the player reads it: "On", "10 min", "Unlimited", the option's name. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static FText GetRuleValueText(const FALKRuleDefinition& Definition, const FString& Value);

	/** A value made valid for a rule (clamped, on a step, an existing option). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	static FString SanitizeRuleValue(const FALKRuleDefinition& Definition, const FString& Value);

	// ---------------------------------------------------------------- Chat and names

	/** Replaces every listed word with asterisks: whole words only, any letter case. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static FString MaskFilteredWords(const FString& Text, const TArray<FString>& Words);

	/** A chat line made safe to send: one line, no control characters, trimmed, at most Max Length characters. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static FString SanitizeChatText(const FString& Text, int32 MaxLength);

	/** A player name made safe to show: one line, no control characters or URL characters, trimmed, at most Max Length. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static FString SanitizePlayerName(const FString& Name, int32 MaxLength);

	/** The name, or the name with " (2)", " (3)"... when someone already has it (any letter case). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static FString MakeUniquePlayerName(const FString& DesiredName, const TArray<FString>& TakenNames);

	/** Whether a player may send a chat line now: at most Max Messages in the last Window Seconds. Recent Times are the send times kept so far. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	static bool IsChatAllowed(const TArray<double>& RecentTimes, double Now, int32 MaxMessages, double WindowSeconds);

	// ---------------------------------------------------------------- Scoreboard

	/** What a scoreboard column shows for a player: kills, deaths, assists, score, ping or a custom stat. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	static FText GetScoreColumnText(const APlayerState* PlayerState, const FALKScoreColumn& Column);

	/** The same as a number, for sorting. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	static float GetScoreColumnValue(const APlayerState* PlayerState, const FALKScoreColumn& Column);

	// ---------------------------------------------------------------- Self check

	/**
	 * Runs the functions above against known answers (slots, teams, start conditions, votes, MVP, lobby codes, filters,
	 * sorting, rules, chat). Returns how many checks ran; Out Failures gets a line for every one that failed (empty = all good).
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	static int32 RunLogicChecks(TArray<FString>& OutFailures);
};
