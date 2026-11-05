// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineProvider.h"
#include "Online/ALKSessionsOnlineProvider.h"
#include "Lobby/ALKLobbyGameMode.h"
#include "Lobby/ALKMatchGameMode.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GeneralProjectSettings.h"
#include "Misc/PackageName.h"
#include "AssetproLobbyKit.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKOnlineConfig)

#define LOCTEXT_NAMESPACE "ALKOnline"

namespace ALKOnlineConfigPrivate
{
	static const FName BotsRule(TEXT("Bots"));
	static constexpr int32 ServerNameMaxLength = 32;
	/** The engine's empty level: always there, in every project and every packaged game. */
	static const TCHAR* const EmptyLevel = TEXT("/Engine/Maps/Entry");
}

UALKOnlineConfig::UALKOnlineConfig()
{
	ProviderClass = UALKSessionsOnlineProvider::StaticClass();

	// ---- Modes
	{
		FALKModeDefinition TeamDeathmatch;
		TeamDeathmatch.Id = TEXT("TeamDeathmatch");
		TeamDeathmatch.Name = LOCTEXT("ModeTDM", "Team Deathmatch");
		TeamDeathmatch.Description = LOCTEXT("ModeTDMDesc", "Two teams. The first to reach the score limit wins.");
		TeamDeathmatch.TeamCount = 2;
		TeamDeathmatch.DefaultMaxPlayers = 8;
		TeamDeathmatch.MaxPlayersOptions = { 4, 6, 8, 10, 12, 16 };

		FALKModeDefinition FreeForAll;
		FreeForAll.Id = TEXT("FreeForAll");
		FreeForAll.Name = LOCTEXT("ModeFFA", "Free For All");
		FreeForAll.Description = LOCTEXT("ModeFFADesc", "Everyone for themselves. The highest score wins.");
		FreeForAll.TeamCount = 0;
		FreeForAll.DefaultMaxPlayers = 8;
		FreeForAll.MaxPlayersOptions = { 2, 4, 6, 8, 12, 16 };

		FALKModeDefinition CapturePoint;
		CapturePoint.Id = TEXT("CapturePoint");
		CapturePoint.Name = LOCTEXT("ModeCapture", "Capture Point");
		CapturePoint.Description = LOCTEXT("ModeCaptureDesc", "Two teams fight to hold the points. Holding them scores.");
		CapturePoint.TeamCount = 2;
		CapturePoint.DefaultMaxPlayers = 12;
		CapturePoint.MaxPlayersOptions = { 6, 8, 10, 12, 16 };
		CapturePoint.ExtraRules = { FName(TEXT("CaptureTime")) };

		FALKModeDefinition Elimination;
		Elimination.Id = TEXT("Elimination");
		Elimination.Name = LOCTEXT("ModeElimination", "Elimination");
		Elimination.Description = LOCTEXT("ModeEliminationDesc", "No respawns. The last team standing takes the round.");
		Elimination.TeamCount = 2;
		Elimination.DefaultMaxPlayers = 6;
		Elimination.MaxPlayersOptions = { 2, 4, 6, 8, 10 };
		Elimination.ExtraRules = { FName(TEXT("RoundsToWin")) };

		Modes = { TeamDeathmatch, FreeForAll, CapturePoint, Elimination };
	}

	// ---- Playlists
	{
		FALKPlaylist Quick;
		Quick.Id = TEXT("QuickTeams");
		Quick.Name = LOCTEXT("PlaylistTeams", "Team Deathmatch");
		Quick.Description = LOCTEXT("PlaylistTeamsDesc", "Jump into a team game.");
		Quick.ModeId = TEXT("TeamDeathmatch");
		Quick.SizeLabel = LOCTEXT("PlaylistTeamsSize", "4v4");
		Quick.bFeatured = true;
		Quick.MaxPlayers = 8;

		FALKPlaylist Solo;
		Solo.Id = TEXT("QuickSolo");
		Solo.Name = LOCTEXT("PlaylistSolo", "Free For All");
		Solo.Description = LOCTEXT("PlaylistSoloDesc", "Eight players, no teams.");
		Solo.ModeId = TEXT("FreeForAll");
		Solo.SizeLabel = LOCTEXT("PlaylistSoloSize", "8 players");
		Solo.MaxPlayers = 8;

		FALKPlaylist Capture;
		Capture.Id = TEXT("QuickCapture");
		Capture.Name = LOCTEXT("PlaylistCapture", "Capture Point");
		Capture.Description = LOCTEXT("PlaylistCaptureDesc", "Take the points and hold them.");
		Capture.ModeId = TEXT("CapturePoint");
		Capture.SizeLabel = LOCTEXT("PlaylistCaptureSize", "6v6");
		Capture.MaxPlayers = 12;

		FALKPlaylist Duel;
		Duel.Id = TEXT("QuickElimination");
		Duel.Name = LOCTEXT("PlaylistElimination", "Elimination");
		Duel.Description = LOCTEXT("PlaylistEliminationDesc", "Round based, no respawns.");
		Duel.ModeId = TEXT("Elimination");
		Duel.SizeLabel = LOCTEXT("PlaylistEliminationSize", "3v3");
		Duel.MaxPlayers = 6;

		Playlists = { Quick, Solo, Capture, Duel };
	}

	// ---- Rules
	{
		const FText Minutes = LOCTEXT("UnitMinutes", "min");
		const FText Seconds = LOCTEXT("UnitSeconds", "s");
		const FText Unlimited = LOCTEXT("RuleUnlimited", "Unlimited");
		const FText None = LOCTEXT("RuleNone", "None");

		RuleDefinitions.Add(FALKRuleDefinition::MakeSlider(TEXT("TimeLimit"), LOCTEXT("RuleTimeLimit", "Time Limit"),
			LOCTEXT("RuleTimeLimitDesc", "How long a match lasts."), 0.f, 30.f, 1.f, 10.f, EALKValueFormat::Integer, Minutes, Unlimited));
		RuleDefinitions.Add(FALKRuleDefinition::MakeSlider(TEXT("ScoreLimit"), LOCTEXT("RuleScoreLimit", "Score Limit"),
			LOCTEXT("RuleScoreLimitDesc", "The score that wins the match."), 0.f, 100.f, 5.f, 30.f, EALKValueFormat::Integer, FText::GetEmpty(), Unlimited));
		RuleDefinitions.Add(FALKRuleDefinition::MakeToggle(TEXT("FriendlyFire"), LOCTEXT("RuleFriendlyFire", "Friendly Fire"),
			LOCTEXT("RuleFriendlyFireDesc", "Players can hurt their own team."), false));
		RuleDefinitions.Add(FALKRuleDefinition::MakeSlider(TEXT("RespawnDelay"), LOCTEXT("RuleRespawnDelay", "Respawn Delay"),
			LOCTEXT("RuleRespawnDelayDesc", "Seconds before a player is back in the game."), 0.f, 30.f, 1.f, 5.f, EALKValueFormat::Integer, Seconds, None));
		RuleDefinitions.Add(FALKRuleDefinition::MakeSlider(ALKOnlineConfigPrivate::BotsRule, LOCTEXT("RuleBots", "Bots"),
			LOCTEXT("RuleBotsDesc", "Computer players that fill the game."), 0.f, 8.f, 1.f, 0.f, EALKValueFormat::Integer, FText::GetEmpty(), None));

		FALKRuleDefinition CaptureTime = FALKRuleDefinition::MakeSlider(TEXT("CaptureTime"), LOCTEXT("RuleCaptureTime", "Capture Time"),
			LOCTEXT("RuleCaptureTimeDesc", "Seconds it takes to capture a point."), 2.f, 30.f, 1.f, 8.f, EALKValueFormat::Integer, Seconds);
		CaptureTime.bAllModes = false;
		RuleDefinitions.Add(CaptureTime);

		FALKRuleDefinition RoundsToWin = FALKRuleDefinition::MakeSlider(TEXT("RoundsToWin"), LOCTEXT("RuleRoundsToWin", "Rounds To Win"),
			LOCTEXT("RuleRoundsToWinDesc", "How many rounds a team needs to win the match."), 1.f, 10.f, 1.f, 3.f);
		RoundsToWin.bAllModes = false;
		RuleDefinitions.Add(RoundsToWin);
	}

	// ---- Scoreboard
	ScoreColumns = {
		FALKScoreColumn(TEXT("Kills"), LOCTEXT("ColumnKills", "K"), EALKScoreSource::Kills, 56.f),
		FALKScoreColumn(TEXT("Deaths"), LOCTEXT("ColumnDeaths", "D"), EALKScoreSource::Deaths, 56.f),
		FALKScoreColumn(TEXT("Assists"), LOCTEXT("ColumnAssists", "A"), EALKScoreSource::Assists, 56.f),
		FALKScoreColumn(TEXT("Score"), LOCTEXT("ColumnScore", "Score"), EALKScoreSource::Score, 88.f),
		FALKScoreColumn(TEXT("Ping"), LOCTEXT("ColumnPing", "Ping"), EALKScoreSource::Ping, 72.f)
	};

	// ---- Loadouts, leaderboards, report reasons
	Loadouts = {
		FALKLoadoutDefinition(TEXT("Vanguard"), LOCTEXT("LoadoutVanguard", "Vanguard"), LOCTEXT("LoadoutVanguardDesc", "First through the door. Tough at close range.")),
		FALKLoadoutDefinition(TEXT("Scout"), LOCTEXT("LoadoutScout", "Scout"), LOCTEXT("LoadoutScoutDesc", "Fast and light. Finds the other team first.")),
		FALKLoadoutDefinition(TEXT("Support"), LOCTEXT("LoadoutSupport", "Support"), LOCTEXT("LoadoutSupportDesc", "Keeps the team going: supplies and repairs.")),
		FALKLoadoutDefinition(TEXT("Marksman"), LOCTEXT("LoadoutMarksman", "Marksman"), LOCTEXT("LoadoutMarksmanDesc", "Holds the long lines from a distance."), 5),
		FALKLoadoutDefinition(TEXT("Engineer"), LOCTEXT("LoadoutEngineer", "Engineer"), LOCTEXT("LoadoutEngineerDesc", "Builds cover and locks a point down."), 10)
	};
	Leaderboards = {
		FALKLeaderboardDefinition(TEXT("Wins"), LOCTEXT("BoardWins", "Wins"), LOCTEXT("BoardWinsHeader", "Wins")),
		FALKLeaderboardDefinition(TEXT("Score"), LOCTEXT("BoardScore", "Score"), LOCTEXT("BoardScoreHeader", "Score")),
		FALKLeaderboardDefinition(TEXT("Eliminations"), LOCTEXT("BoardEliminations", "Eliminations"), LOCTEXT("BoardEliminationsHeader", "Eliminations"))
	};
	ReportReasons = {
		FALKReportReason(TEXT("Cheating"), LOCTEXT("ReportCheating", "Cheating")),
		FALKReportReason(TEXT("AbusiveChat"), LOCTEXT("ReportAbusiveChat", "Abusive chat")),
		FALKReportReason(TEXT("AbusiveVoice"), LOCTEXT("ReportAbusiveVoice", "Abusive voice")),
		FALKReportReason(TEXT("OffensiveName"), LOCTEXT("ReportOffensiveName", "Offensive name")),
		FALKReportReason(TEXT("Leaving"), LOCTEXT("ReportLeaving", "Leaving matches")),
		FALKReportReason(TEXT("Other"), LOCTEXT("ReportOther", "Something else"))
	};

	// ---- Teams
	DefaultMaxPlayersOptions = { 2, 4, 6, 8, 10, 12, 16 };
	TeamNames = { LOCTEXT("TeamBlue", "Blue"), LOCTEXT("TeamOrange", "Orange") };
	TeamColors = { FLinearColor::FromSRGBColor(FColor(0x4D, 0xA3, 0xFF)), FLinearColor::FromSRGBColor(FColor(0xFF, 0x8A, 0x4C)) };
}

const FALKModeDefinition* UALKOnlineConfig::FindMode(FName ModeId) const
{
	return Modes.FindByPredicate([ModeId](const FALKModeDefinition& Each) { return Each.Id == ModeId; });
}

const FALKMapDefinition* UALKOnlineConfig::FindMap(FName MapId) const
{
	return Maps.FindByPredicate([MapId](const FALKMapDefinition& Each) { return Each.Id == MapId; });
}

const FALKPlaylist* UALKOnlineConfig::FindPlaylist(FName PlaylistId) const
{
	return Playlists.FindByPredicate([PlaylistId](const FALKPlaylist& Each) { return Each.Id == PlaylistId; });
}

const FALKRuleDefinition* UALKOnlineConfig::FindRule(FName RuleId) const
{
	return RuleDefinitions.FindByPredicate([RuleId](const FALKRuleDefinition& Each) { return Each.Id == RuleId; });
}

const FALKLoadoutDefinition* UALKOnlineConfig::FindLoadout(FName LoadoutId) const
{
	return Loadouts.FindByPredicate([LoadoutId](const FALKLoadoutDefinition& Each) { return Each.Id == LoadoutId; });
}

const FALKLeaderboardDefinition* UALKOnlineConfig::FindLeaderboard(FName BoardId) const
{
	return Leaderboards.FindByPredicate([BoardId](const FALKLeaderboardDefinition& Each) { return Each.Id == BoardId; });
}

TArray<FALKRuleDefinition> UALKOnlineConfig::GetRulesForMode(FName ModeId) const
{
	TArray<FALKRuleDefinition> Result;
	const FALKModeDefinition* Mode = FindMode(ModeId);
	for (const FALKRuleDefinition& Rule : RuleDefinitions)
	{
		if (!Rule.Id.IsNone() && (Rule.bAllModes || (Mode && Mode->ExtraRules.Contains(Rule.Id))))
		{
			Result.Add(Rule);
		}
	}
	return Result;
}

TArray<FALKMapDefinition> UALKOnlineConfig::GetMapsForMode(FName ModeId) const
{
	TArray<FALKMapDefinition> Result;
	for (const FALKMapDefinition& Map : Maps)
	{
		if (Map.SupportsMode(ModeId))
		{
			Result.Add(Map);
		}
	}
	return Result;
}

TArray<int32> UALKOnlineConfig::GetMaxPlayersOptions(FName ModeId) const
{
	const FALKModeDefinition* Mode = FindMode(ModeId);
	TArray<int32> Options = Mode && Mode->MaxPlayersOptions.Num() > 0 ? Mode->MaxPlayersOptions : DefaultMaxPlayersOptions;
	Options.RemoveAll([](int32 Each) { return Each < 1; });
	if (Options.Num() == 0)
	{
		Options.Add(FMath::Max(1, Mode ? Mode->DefaultMaxPlayers : DefaultMaxPlayers));
	}
	Options.Sort();
	return Options;
}

int32 UALKOnlineConfig::GetTeamCount(FName ModeId) const
{
	const FALKModeDefinition* Mode = FindMode(ModeId);
	return Mode ? FMath::Max(0, Mode->TeamCount) : 0;
}

FText UALKOnlineConfig::GetModeName(FName ModeId) const
{
	const FALKModeDefinition* Mode = FindMode(ModeId);
	return Mode && !Mode->Name.IsEmpty() ? Mode->Name : (ModeId.IsNone() ? FText::GetEmpty() : FText::FromName(ModeId));
}

FText UALKOnlineConfig::GetMapName(FName MapId) const
{
	const FALKMapDefinition* Map = FindMap(MapId);
	if (Map && !Map->Name.IsEmpty())
	{
		return Map->Name;
	}
	if (MapId.IsNone())
	{
		return FText::GetEmpty();
	}
	// A raw level path used as a map Id reads as the level's short name.
	FString Name = MapId.ToString();
	int32 LastSlash = INDEX_NONE;
	if (Name.FindLastChar(TEXT('/'), LastSlash))
	{
		Name.RightChopInline(LastSlash + 1);
	}
	return FText::FromString(Name);
}

FText UALKOnlineConfig::GetTeamName(int32 TeamId) const
{
	if (TeamNames.IsValidIndex(TeamId) && !TeamNames[TeamId].IsEmpty())
	{
		return TeamNames[TeamId];
	}
	return TeamId < 0 ? FText::GetEmpty() : FText::Format(LOCTEXT("TeamNumbered", "Team {0}"), FText::AsNumber(TeamId + 1));
}

FLinearColor UALKOnlineConfig::GetTeamColor(int32 TeamId) const
{
	return TeamColors.IsValidIndex(TeamId) ? TeamColors[TeamId] : FLinearColor::White;
}

FString UALKOnlineConfig::GetResolvedBuildVersion() const
{
	return BuildVersion.IsEmpty() ? GetDefault<UGeneralProjectSettings>()->ProjectVersion : BuildVersion;
}

FString UALKOnlineConfig::GetMapLevelPath(FName MapId) const
{
	if (const FALKMapDefinition* Map = FindMap(MapId))
	{
		const FString Level = Map->Level.IsNull() ? FString() : Map->Level.ToSoftObjectPath().GetLongPackageName();
		// A map whose level is not there (not made yet, or left out of the package) counts as having none, so the match
		// falls back as it does for a map without a level instead of travelling nowhere.
		if (!Level.IsEmpty() && !FPackageName::DoesPackageExist(Level))
		{
			UE_LOG(LogALK, Warning, TEXT("Online Config: the level of map %s (%s) does not exist."), *MapId.ToString(), *Level);
			return FString();
		}
		return Level;
	}
	const FString Raw = MapId.IsNone() ? FString() : MapId.ToString();
	return Raw.StartsWith(TEXT("/")) ? Raw : FString();
}

FString UALKOnlineConfig::GetLobbyTravelURL() const
{
	const FString Level = LobbyLevel.IsNull() ? FString() : LobbyLevel.ToSoftObjectPath().GetLongPackageName();
	if (!Level.IsEmpty())
	{
		return Level;
	}
	if (!bUseEmptyLevelFallback)
	{
		return FString();
	}
	return FString::Printf(TEXT("%s?game=%s"), ALKOnlineConfigPrivate::EmptyLevel, *AALKLobbyGameMode::StaticClass()->GetPathName());
}

FString UALKOnlineConfig::GetMatchTravelURL(FName ModeId, FName MapId) const
{
	const FALKModeDefinition* Mode = FindMode(ModeId);
	const FString ModeGameMode = Mode && !Mode->GameModeClass.IsNull() ? Mode->GameModeClass.ToString() : FString();
	const FString Level = GetMapLevelPath(MapId);
	if (!Level.IsEmpty())
	{
		return ModeGameMode.IsEmpty() ? Level : FString::Printf(TEXT("%s?game=%s"), *Level, *ModeGameMode);
	}
	if (!bUseEmptyLevelFallback)
	{
		return FString();
	}
	return FString::Printf(TEXT("%s?game=%s"), ALKOnlineConfigPrivate::EmptyLevel, ModeGameMode.IsEmpty() ? *AALKMatchGameMode::StaticClass()->GetPathName() : *ModeGameMode);
}

FALKHostSettings UALKOnlineConfig::MakeDefaultHostSettings(const FString& HostName) const
{
	FALKHostSettings Settings;
	Settings.ModeId = Modes.Num() > 0 ? Modes[0].Id : NAME_None;
	Settings.MaxPlayers = Modes.Num() > 0 ? Modes[0].DefaultMaxPlayers : DefaultMaxPlayers;
	return SanitizeHostSettings(Settings, HostName);
}

FALKHostSettings UALKOnlineConfig::SanitizeHostSettings(const FALKHostSettings& Settings, const FString& FallbackName) const
{
	FALKHostSettings Result = Settings;

	// Name: one line, trimmed, not too long.
	Result.ServerName = Result.ServerName.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\t"), TEXT(" ")).TrimStartAndEnd();
	if (Result.ServerName.IsEmpty())
	{
		const FString Host = FallbackName.TrimStartAndEnd();
		Result.ServerName = Host.IsEmpty()
			? LOCTEXT("DefaultServerName", "New Game").ToString()
			: FText::Format(LOCTEXT("DefaultServerNameFormat", "{0}'s Game"), FText::FromString(Host)).ToString();
	}
	Result.ServerName.LeftInline(ALKOnlineConfigPrivate::ServerNameMaxLength);
	Result.Password = Result.Password.TrimStartAndEnd().Left(32);

	// Mode: one that is listed (unless nothing is listed at all).
	if (Modes.Num() > 0 && !FindMode(Result.ModeId))
	{
		Result.ModeId = Modes[0].Id;
	}

	// Map: one that is listed for the mode. A raw "/Game/..." path is kept as it is.
	const bool bRawPath = !Result.MapId.IsNone() && Result.MapId.ToString().StartsWith(TEXT("/"));
	if (!bRawPath)
	{
		const FALKMapDefinition* Map = FindMap(Result.MapId);
		if (!Map || !Map->SupportsMode(Result.ModeId))
		{
			const TArray<FALKMapDefinition> ModeMaps = GetMapsForMode(Result.ModeId);
			Result.MapId = ModeMaps.Num() > 0 ? ModeMaps[0].Id : NAME_None;
		}
	}

	// Player limit: one the mode offers; the closest one otherwise.
	const TArray<int32> Limits = GetMaxPlayersOptions(Result.ModeId);
	if (!Limits.Contains(Result.MaxPlayers))
	{
		int32 Closest = Limits[0];
		for (const int32 Limit : Limits)
		{
			if (FMath::Abs(Limit - Result.MaxPlayers) < FMath::Abs(Closest - Result.MaxPlayers))
			{
				Closest = Limit;
			}
		}
		Result.MaxPlayers = Closest;
	}

	// Rules: the mode's, each with a valid value. The bot count follows the Bots rule when there is one.
	const TArray<FALKRuleDefinition> ModeRules = GetRulesForMode(Result.ModeId);
	if (!Result.Rules.Has(ALKOnlineConfigPrivate::BotsRule) && Result.BotCount > 0)
	{
		Result.Rules.SetInt(ALKOnlineConfigPrivate::BotsRule, Result.BotCount);
	}
	Result.Rules.Sanitize(ModeRules);
	if (Result.Rules.Has(ALKOnlineConfigPrivate::BotsRule))
	{
		Result.BotCount = Result.Rules.GetInt(ALKOnlineConfigPrivate::BotsRule);
	}
	Result.BotCount = FMath::Clamp(Result.BotCount, 0, 64);
	return Result;
}

#undef LOCTEXT_NAMESPACE
