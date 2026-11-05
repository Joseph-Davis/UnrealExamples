// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Online/ALKOnlineTypes.h"
#include "Rules/ALKGameRules.h"
#include "ALKOnlineSaveGame.generated.h"

/** A player met in a lobby or match, remembered for the Recent Players list. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKRecentPlayer
{
	GENERATED_BODY()

	/** The player's id on the backend. */
	UPROPERTY(BlueprintReadOnly, Category = "Recent Player")
	FString UserId;

	/** Their name when last seen. */
	UPROPERTY(BlueprintReadOnly, Category = "Recent Player")
	FString DisplayName;

	/** When they were last seen (UTC). */
	UPROPERTY(BlueprintReadOnly, Category = "Recent Player")
	FDateTime LastSeen;
};

/**
 * What the online subsystem remembers between runs, in one small save file of its own (Save Slot Prefix + "_Online"):
 * favourite and recent servers, recent players, muted and blocked players, rule presets, the host's ban list, the last
 * host settings and the match to rejoin after a lost connection. Read and changed through the ALK Online Subsystem.
 */
UCLASS(BlueprintType)
class ASSETPROLOBBYKIT_API UALKOnlineSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** The name the player chose to show to others. Empty = the backend's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	FString PlayerName;

	/** Servers the player marked as favourites. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	TArray<FALKServerBookmark> FavouriteServers;

	/** Newest first. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	TArray<FALKServerBookmark> RecentServers;

	/** Newest first. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	TArray<FALKRecentPlayer> RecentPlayers;

	/** User ids whose chat and voice are hidden. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	TArray<FString> MutedPlayers;

	/** User ids whose chat, voice, whispers and invites are dropped. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	TArray<FString> BlockedPlayers;

	/** Rule sets the player saved on the host screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	TArray<FALKRulePreset> RulePresets;

	/** Players this machine refuses when it hosts. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	TArray<FALKBanEntry> BannedPlayers;

	/** What the player last hosted with (the password is not kept). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	FALKHostSettings LastHostSettings;

	/** False until the player has hosted once. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	bool bHasLastHostSettings = false;

	/** The game to offer "Rejoin Match" for: where it is, and when this player was last connected to it (UTC). A password is never kept. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	FString RejoinConnectString;

	/** The backend's id of that game, when it had one. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	FString RejoinSessionId;

	/** That game's name, for the "Rejoin Match" prompt. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	FString RejoinServerName;

	/** When this player was last connected to it (UTC). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Online")
	FDateTime RejoinTime;
};
