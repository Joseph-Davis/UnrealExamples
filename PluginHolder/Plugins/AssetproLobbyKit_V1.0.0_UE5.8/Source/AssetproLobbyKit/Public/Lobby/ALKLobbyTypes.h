// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ALKLobbyTypes.generated.h"

/** Where a lobby or match is at. Replicated by the ALK lobby game state. */
UENUM(BlueprintType)
enum class EALKLobbyPhase : uint8
{
	/** In the lobby, waiting for players to ready up. */
	Waiting,
	/** In the lobby, counting down to the match. */
	Countdown,
	/** The countdown ended: travelling to the match map. */
	Starting,
	/** The match is being played. */
	InMatch,
	/** The match ended: results, rematch vote. */
	PostMatch
};

/** Why the match cannot start yet. */
UENUM(BlueprintType)
enum class EALKStartBlocker : uint8
{
	/** Nothing: the match can start. */
	None,
	/** Fewer players than the Online Config's Min Players To Start. */
	NotEnoughPlayers,
	/** Someone is not ready (a host can still force the start). */
	NotAllReady,
	/** No map is chosen, or the chosen map has no level. */
	NoMap
};

/** How many votes a map has in the lobby's map vote. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKMapVote
{
	GENERATED_BODY()

	/** The map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vote")
	FName MapId;

	/** How many players voted for it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vote")
	int32 Votes = 0;

	FALKMapVote() = default;
	FALKMapVote(FName InMapId, int32 InVotes) : MapId(InMapId), Votes(InVotes) {}
};

/** One extra scoreboard number of a player, by name (a Custom Stat score column shows it). */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKCustomStat
{
	GENERATED_BODY()

	/** The stat's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
	FName Id;

	/** Its value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
	float Value = 0.f;

	FALKCustomStat() = default;
	FALKCustomStat(FName InId, float InValue) : Id(InId), Value(InValue) {}
};

/** A player's match numbers, as the pure scoring helpers take them (Pick MVP). */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKScoreLine
{
	GENERATED_BODY()

	/** The player's team, or -1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	int32 TeamId = INDEX_NONE;

	/** The player's score. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	float Score = 0.f;

	/** The player's kills. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	int32 Kills = 0;

	/** The player's deaths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	int32 Deaths = 0;

	/** The player's assists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	int32 Assists = 0;

	/** Spectators are never picked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score")
	bool bSpectating = false;

	FALKScoreLine() = default;
	FALKScoreLine(int32 InTeamId, float InScore, int32 InKills, int32 InDeaths, int32 InAssists)
		: TeamId(InTeamId), Score(InScore), Kills(InKills), Deaths(InDeaths), Assists(InAssists) {}
};

/**
 * What the server remembers about a player across a level change that is not seamless (play in editor, or a project
 * with seamless travel turned off): the lobby writes it before travelling and the next game mode gives it back.
 */
USTRUCT()
struct ASSETPROLOBBYKIT_API FALKRosterEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FString UserId;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	int32 Slot = INDEX_NONE;

	UPROPERTY()
	int32 TeamId = INDEX_NONE;

	UPROPERTY()
	FName LoadoutId;

	UPROPERTY()
	int32 ColorIndex = 0;

	UPROPERTY()
	int32 PlayerLevel = 1;

	UPROPERTY()
	bool bSpectating = false;

	/** A placeholder player (no controller): the next game mode makes it again from this entry. */
	UPROPERTY()
	bool bPlaceholder = false;

	UPROPERTY()
	int32 PingMs = 0;
};
