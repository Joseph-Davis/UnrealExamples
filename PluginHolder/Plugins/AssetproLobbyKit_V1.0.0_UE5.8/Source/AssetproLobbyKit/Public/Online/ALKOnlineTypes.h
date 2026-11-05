// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "UObject/SoftObjectPtr.h"
#include "Rules/ALKGameRules.h"
#include "ALKOnlineTypes.generated.h"

class UTexture2D;
class UWorld;

// ---------------------------------------------------------------- States and errors

/** What the online subsystem is doing right now. */
UENUM(BlueprintType)
enum class EALKOnlineState : uint8
{
	/** In the menus, not in a game. */
	Idle,
	/** Creating a session, then opening the lobby level. */
	Hosting,
	/** Looking for games (server browser). */
	Searching,
	/** Joining a game: session join, then travel to the host. */
	Joining,
	/** In a lobby, as host or client. */
	InLobby,
	/** In a match. */
	InMatch,
	/** Leaving a game and going back to the main menu. */
	Leaving
};

/** Where a quick match is at. */
UENUM(BlueprintType)
enum class EALKMatchmakingState : uint8
{
	Idle,
	/** In the queue. */
	Searching,
	/** A match was found: accept or decline before the countdown ends. */
	MatchFound,
	/** This player accepted and waits for the others. */
	Accepted,
	/** Everyone accepted: joining the game. */
	Joining,
	/** The search failed or was declined. Goes back to Idle on the next search. */
	Failed
};

/** Why a connection ended or could not be made. Each has a ready title and message (Get Connection Error Text). */
UENUM(BlueprintType)
enum class EALKConnectionError : uint8
{
	None,
	TimedOut,
	ServerFull,
	WrongVersion,
	WrongPassword,
	HostLeft,
	Kicked,
	Banned,
	NetworkError,
	SessionNotFound,
	Cancelled,
	Unknown
};

/**
 * The reason text a server sends when it refuses or removes a player, and how each maps to a connection error.
 * The ALK game modes use these in Pre Login and when kicking; the online subsystem reads them back on the client.
 * Your own game mode can return the same strings from Pre Login to get the matching dialog.
 */
namespace ALKNetReason
{
	/** Pre Login: no free player slot. */
	static const TCHAR* const ServerFull = TEXT("ALK.ServerFull");
	/** Pre Login: the "ALKPw" option does not match the server's password. */
	static const TCHAR* const WrongPassword = TEXT("ALK.WrongPassword");
	/** Pre Login: the "ALKVer" option does not match the server's build version. */
	static const TCHAR* const WrongVersion = TEXT("ALK.WrongVersion");
	/** Pre Login, or sent before the connection is closed: the player is on the host's ban list. */
	static const TCHAR* const Banned = TEXT("ALK.Banned");
	/** Sent before the connection is closed: the host removed the player. */
	static const TCHAR* const Kicked = TEXT("ALK.Kicked");
	/** Sent before the host closes the game. */
	static const TCHAR* const HostLeft = TEXT("ALK.HostLeft");
}

/**
 * The level URL options the kit uses. Values are escaped with FALKOptionString (Blueprint: the rules and host settings
 * helpers do it for you).
 */
namespace ALKNetOption
{
	/** Server and travel URLs: the server name. */
	static const TCHAR* const ServerName = TEXT("ALKName");
	/** Server and travel URLs: the game mode Id. */
	static const TCHAR* const Mode = TEXT("ALKMode");
	/** Server and travel URLs: the map Id. */
	static const TCHAR* const Map = TEXT("ALKMap");
	/** Server and travel URLs: the player limit. */
	static const TCHAR* const MaxPlayers = TEXT("ALKMax");
	/** Server and travel URLs: how many bots. */
	static const TCHAR* const Bots = TEXT("ALKBots");
	/** Server and travel URLs: the match rules (FALKGameRules::ToOptionsString). */
	static const TCHAR* const Rules = TEXT("ALKRules");
	/** A joining client's URL only: the password it offers. A server's password is never put in a URL. */
	static const TCHAR* const Password = TEXT("ALKPw");
	/** A joining client's URL: its build version. */
	static const TCHAR* const Version = TEXT("ALKVer");
}

/** Titles and messages for connection errors, and the reason strings behind them. */
struct ASSETPROLOBBYKIT_API FALKConnectionErrors
{
	/** A localized title and message for the error dialog. Both are empty for None. */
	static void GetText(EALKConnectionError Error, FText& OutTitle, FText& OutMessage);

	/** The error a server's reason text stands for (the ALKNetReason strings, plus the engine's own "Server full." and timeouts). */
	static EALKConnectionError FromReason(const FString& Reason);

	/** The ALKNetReason string for an error, or empty when it has none. */
	static FString ToReason(EALKConnectionError Error);
};

// ---------------------------------------------------------------- Sessions

/** One game in the server browser, or the game the player is in. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKSessionInfo
{
	GENERATED_BODY()

	/** The backend's id for the session. Empty when this is not a real session. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString SessionId;

	/** The game's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString ServerName;

	/** The host's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString HostName;

	/** The host's user id, when the backend gives one. Favourites are remembered by it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString HostUserId;

	/** The game mode's Id (see the Online Config's Modes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FName ModeId;

	/** The map's Id (see the Online Config's Maps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FName MapId;

	/** Players in the game now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	int32 CurrentPlayers = 0;

	/** The player limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	int32 MaxPlayers = 0;

	/** Milliseconds, or -1 when unknown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	int32 PingMs = -1;

	/** A password is needed to join. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	bool bPasswordProtected = false;

	/** A local network game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	bool bLAN = false;

	/** The match already started (join in progress). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	bool bInProgress = false;

	/** The region's Id, when the host set one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString Region;

	/** The host's build version. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString BuildVersion;

	/** False when the host runs another build version; joining is refused with Wrong Version. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	bool bVersionMatches = true;

	/** The match rules the host advertises, by rule Id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	TMap<FName, FString> Rules;

	/** For the provider that found the session: which of its search results this is. Do not change it. */
	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 SearchResultIndex = INDEX_NONE;

	bool IsValid() const { return !SessionId.IsEmpty() || !ServerName.IsEmpty(); }
	bool IsFull() const { return MaxPlayers > 0 && CurrentPlayers >= MaxPlayers; }
	bool IsEmpty() const { return CurrentPlayers <= 0; }

	/** What a favourite is remembered by: the host's user id, or the server name when there is none. */
	FString GetFavouriteKey() const { return HostUserId.IsEmpty() ? ServerName : HostUserId; }
};

/** What the server browser sorts by. */
UENUM(BlueprintType)
enum class EALKSessionSort : uint8
{
	Name,
	Mode,
	Map,
	Players,
	Ping
};

/** The server browser's filters. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKSessionFilter
{
	GENERATED_BODY()

	/** Leave out games with no free player slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bHideFull = false;

	/** Leave out games nobody is in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bHideEmpty = false;

	/** Leave out games that need a password. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bHidePasswordProtected = false;

	/** Only games in this region. Empty = any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	FString Region;

	/** Only games whose server, host, mode or map name contains this text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	FString SearchText;

	/** Search the local network only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bLANOnly = false;

	/** Only games of this mode. None = any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	FName ModeId;

	/** True when the session passes every filter. */
	bool Matches(const FALKSessionInfo& Session) const;

	/** The sessions that pass, in the same order. */
	static TArray<FALKSessionInfo> Apply(const FALKSessionFilter& Filter, const TArray<FALKSessionInfo>& Sessions);

	/** Sorts in place. Ties keep a steady order (by server name, then session id). */
	static void Sort(TArray<FALKSessionInfo>& Sessions, EALKSessionSort SortBy, bool bAscending);
};

/** What the Host Game screen collects. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKHostSettings
{
	GENERATED_BODY()

	/** The name shown in the server browser and in the lobby. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	FString ServerName;

	/** The game mode's Id (see the Online Config's Modes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	FName ModeId;

	/** The map's Id (see the Online Config's Maps). In the lobby the map vote can change it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	FName MapId;

	/** The player limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	int32 MaxPlayers = 8;

	/** Hidden from the server browser: players join by code, address or invite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	bool bPrivate = false;

	/** Players must give this to join. Empty = no password. Kept on the host; never sent in a level URL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	FString Password;

	/** A local network game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	bool bLAN = false;

	/** How many bots the match should add. Taken from the "Bots" rule when the rules have one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	int32 BotCount = 0;

	/** The match rules (see the Online Config's Rule Definitions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Host")
	FALKGameRules Rules;

	bool HasPassword() const { return !Password.IsEmpty(); }

	/** "?ALKName=..?ALKMode=..?ALKMap=..?ALKMax=..?ALKBots=..?ALKRules=..": everything but the password, for a level URL. */
	FString ToTravelOptions() const;

	/** Overwrites the fields that the URL options carry. Returns true when at least one was there. */
	bool ApplyTravelOptions(const FString& Options);
};

/** A server the player marked as favourite or played on recently. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKServerBookmark
{
	GENERATED_BODY()

	/** The session as it was last seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bookmark")
	FALKSessionInfo Session;

	/** The address to connect to, when it is known (always for recent servers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bookmark")
	FString ConnectString;

	/** When the player last played there or marked it (UTC). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bookmark")
	FDateTime Time;
};

// ---------------------------------------------------------------- Friends, party, profile

UENUM(BlueprintType)
enum class EALKPresence : uint8
{
	Offline,
	Online,
	InLobby,
	InGame
};

USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKFriend
{
	GENERATED_BODY()

	/** The friend's id on the backend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	FString UserId;

	/** The friend's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	FString DisplayName;

	/** Offline, online, in a lobby or in a game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	EALKPresence Presence = EALKPresence::Offline;

	/** A short line under the name, for example the mode and map they are playing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	FText StatusText;

	/** Their game can be joined (Join Friend). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	bool bJoinable = false;

	/** They can be invited to the party or the game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	bool bInvitable = false;

	/** Optional picture. Empty = the theme's placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Friend")
	TSoftObjectPtr<UTexture2D> Avatar;
};

USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKPartyMember
{
	GENERATED_BODY()

	/** The member's id on the backend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party")
	FString UserId;

	/** The member's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party")
	FString DisplayName;

	/** The party's leader. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party")
	bool bLeader = false;

	/** Ready to play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party")
	bool bReady = false;

	/** This is the local player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party")
	bool bLocal = false;

	/** Optional picture. Empty = the theme's placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party")
	TSoftObjectPtr<UTexture2D> Avatar;
};

/** One line on a profile card, for example "Matches" / "128". */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKProfileStat
{
	GENERATED_BODY()

	/** What the number is: "Matches", "Win Rate"... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FText Label;

	/** The number, as it should be shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FText Value;
};

USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKPlayerProfile
{
	GENERATED_BODY()

	/** The player's id on the backend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FString UserId;

	/** The player's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FString DisplayName;

	/** The player's level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	int32 Level = 1;

	/** Progress to the next level, 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0", ClampMax = "1"))
	float XPProgress = 0.f;

	/** Optional picture. Empty = the theme's placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	TSoftObjectPtr<UTexture2D> Avatar;

	/** The lines of the card, top to bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	TArray<FALKProfileStat> Stats;
};

/** An invitation to a party or a game. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKInvite
{
	GENERATED_BODY()

	/** Who sent the invite (their id on the backend). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invite")
	FString FromUserId;

	/** Their name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invite")
	FString FromName;

	/** True for a party invite, false for a game invite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invite")
	bool bPartyInvite = false;

	/** The game, for a game invite (may be empty when the backend does not say). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invite")
	FALKSessionInfo Session;
};

/** A choice in a list the backend provides: a region, a microphone, a speaker. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKOnlineOption
{
	GENERATED_BODY()

	/** What the backend knows the choice by. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Option")
	FString Id;

	/** What the player reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Option")
	FText DisplayName;

	FALKOnlineOption() = default;
	FALKOnlineOption(const FString& InId, const FText& InName) : Id(InId), DisplayName(InName) {}
};

UENUM(BlueprintType)
enum class EALKVoiceMode : uint8
{
	/** No voice chat. */
	Off,
	/** The microphone is live while the push-to-talk key is held. */
	PushToTalk,
	/** The microphone is always live. */
	OpenMic
};

/** A player on the host's ban list. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKBanEntry
{
	GENERATED_BODY()

	/** The banned player's id on the backend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ban")
	FString UserId;

	/** Their name when they were banned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ban")
	FString DisplayName;

	/** The player's network address without the port, when it was known. A match on either bans. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ban")
	FString Address;

	/** When they were banned (UTC). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ban")
	FDateTime BannedAt;
};

// ---------------------------------------------------------------- Chat, feed, leaderboards

UENUM(BlueprintType)
enum class EALKChatChannel : uint8
{
	/** Everyone in the lobby or match. */
	All,
	/** The sender's team. */
	Team,
	/** The sender's party. */
	Party,
	/** One player. */
	Whisper,
	/** Messages from the game: joins, leaves, notices. */
	System
};

USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKChatMessage
{
	GENERATED_BODY()

	/** When the message was sent (UTC). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	FDateTime Time;

	/** Who the line is for: everyone, the team, the party, one player, or a system line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	EALKChatChannel Channel = EALKChatChannel::All;

	/** The sender's user id. Empty for system lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	FString SenderId;

	/** The sender's name (show it through the online subsystem's Get Display Name for streamer mode). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	FString SenderName;

	/** The sender's team, or -1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	int32 TeamId = INDEX_NONE;

	/** The sender's lobby slot, or -1. Streamer mode names ("Player 3") come from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	int32 SenderStableIndex = INDEX_NONE;

	/** The line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	FString Text;

	/** Who a whisper is for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	FString WhisperTargetId;
};

/** One line of the kill feed / event feed. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKFeedEvent
{
	GENERATED_BODY()

	/** Who did it: the player who scored, captured, joined or left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feed")
	FString ActorName;

	/** The actor's team, or -1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feed")
	int32 ActorTeam = INDEX_NONE;

	/** The actor's lobby slot, or -1 (streamer mode names). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feed")
	int32 ActorStableIndex = INDEX_NONE;

	/** Who it was done to, when there is someone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feed")
	FString TargetName;

	/** The target's team, or -1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feed")
	int32 TargetTeam = INDEX_NONE;

	/** The target's lobby slot, or -1 (streamer mode names). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feed")
	int32 TargetStableIndex = INDEX_NONE;

	/** What happened: "Kill", "Capture", "Join", "Leave" or any name of your own. Pick the icon by it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feed")
	FName EventId;

	/** The whole line as text, for feeds that show plain text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feed")
	FText Text;
};

USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKLeaderboardRow
{
	GENERATED_BODY()

	/** The place on the board; 1 is the best. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	int32 Rank = 0;

	/** The player's id on the backend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	FString UserId;

	/** The player's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	FString DisplayName;

	/** The score as a number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	double Score = 0.0;

	/** The score as it should be shown ("1:23.456", "12,400"). Empty = print Score. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	FText ScoreText;

	/** This row is the local player's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaderboard")
	bool bLocalPlayer = false;
};

// ---------------------------------------------------------------- Net stats

/** Connection quality for the net stats overlay. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKNetStats
{
	GENERATED_BODY()

	/** False in a single player game: nothing to show. */
	UPROPERTY(BlueprintReadOnly, Category = "Net Stats")
	bool bConnected = false;

	/** Milliseconds to the server and back. 0 on the host. */
	UPROPERTY(BlueprintReadOnly, Category = "Net Stats")
	int32 PingMs = 0;

	/** Incoming packets lost, 0 to 100. */
	UPROPERTY(BlueprintReadOnly, Category = "Net Stats")
	float PacketLossInPercent = 0.f;

	/** Outgoing packets lost, 0 to 100. */
	UPROPERTY(BlueprintReadOnly, Category = "Net Stats")
	float PacketLossOutPercent = 0.f;

	/** Incoming traffic, in kilobits per second. */
	UPROPERTY(BlueprintReadOnly, Category = "Net Stats")
	float InKbps = 0.f;

	/** Outgoing traffic, in kilobits per second. */
	UPROPERTY(BlueprintReadOnly, Category = "Net Stats")
	float OutKbps = 0.f;

	/** The server's frames per second (from the ALK game state), or 0 when unknown. */
	UPROPERTY(BlueprintReadOnly, Category = "Net Stats")
	int32 ServerTickRate = 0;

	/** Reads the stats of a world's connection: the local player's ping, the net driver's loss and traffic. */
	static FALKNetStats FromWorld(const UWorld* World);
};

// ---------------------------------------------------------------- Lobby codes

/**
 * Short codes players read out to join a game: an IPv4 address and port as letters and digits in groups of three,
 * for example "7K2-M9Q-4XD". Crockford base 32: no I, L, O or U, and typing those (or lower case) still works.
 * The last character is a check digit, so a typo is refused instead of leading somewhere else.
 *
 * Ports 7777 to 8032 give a 9 character code; any other port a 12 character one.
 */
struct ASSETPROLOBBYKIT_API FALKLobbyCode
{
	/** The engine's default game port; the short code counts ports from here. */
	static constexpr int32 BasePort = 7777;

	/** The code for an address ("192.168.1.20:7777", or without a port for 7777). Empty when it is not an IPv4 address. */
	static FString Encode(const FString& Address);

	/** The code for an IPv4 address (host byte order: 192.168.1.20 is 0xC0A80114) and a port. Empty for a port outside 1..65535. */
	static FString EncodeIPv4(uint32 Address, int32 Port);

	/** The "ip:port" a code stands for. False when the code is mistyped or not a code. */
	static bool Decode(const FString& Code, FString& OutAddress);

	/**
	 * What "Join by code" accepts: a lobby code, or a raw address ("192.168.1.20", "192.168.1.20:7778", "host.name:7777").
	 * Gives the address to connect to. False when the text is neither.
	 */
	static bool ResolveJoinTarget(const FString& CodeOrAddress, FString& OutAddress);

	/** Splits "a.b.c.d[:port]". The port is Base Port when left out. */
	static bool ParseIPv4(const FString& Address, uint32& OutAddress, int32& OutPort);

	/** Upper case, look-alike letters fixed, grouped in threes: "7k2m9q4xd" becomes "7K2-M9Q-4XD". */
	static FString Normalize(const FString& Code);

	/** The code with every character hidden ("***-***-***"), for streamer mode. */
	static FString Mask(const FString& Code);
};
