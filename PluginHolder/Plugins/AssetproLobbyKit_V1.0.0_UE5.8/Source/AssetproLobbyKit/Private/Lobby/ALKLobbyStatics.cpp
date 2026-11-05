// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineSubsystem.h"
#include "ALKDeveloperSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Math/RandomStream.h"
#include "Misc/Char.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLobbyStatics)

#define LOCTEXT_NAMESPACE "ALKLobby"

namespace ALKLobbyStaticsPrivate
{
	static UWorld* GetWorld(const UObject* WorldContextObject)
	{
		return GEngine && WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	}

	static APlayerController* GetLocalController(const UObject* WorldContextObject)
	{
		const UWorld* World = GetWorld(WorldContextObject);
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	}

	static bool IsControlChar(TCHAR Char)
	{
		return Char < 0x20 || Char == 0x7F;
	}
}

// ---------------------------------------------------------------- Shortcuts

UALKOnlineSubsystem* UALKLobbyStatics::GetOnlineSubsystem(const UObject* WorldContextObject)
{
	return UALKOnlineSubsystem::Get(WorldContextObject);
}

const UALKOnlineConfig* UALKLobbyStatics::GetOnlineConfig()
{
	return UALKDeveloperSettings::GetOnlineConfig();
}

AALKLobbyGameState* UALKLobbyStatics::GetLobbyGameState(const UObject* WorldContextObject)
{
	const UWorld* World = ALKLobbyStaticsPrivate::GetWorld(WorldContextObject);
	return World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
}

UALKPlayerNetComponent* UALKLobbyStatics::GetLocalNetComponent(const UObject* WorldContextObject)
{
	return UALKPlayerNetComponent::Get(ALKLobbyStaticsPrivate::GetLocalController(WorldContextObject));
}

AALKLobbyPlayerState* UALKLobbyStatics::GetLocalPlayerState(const UObject* WorldContextObject)
{
	const APlayerController* Controller = ALKLobbyStaticsPrivate::GetLocalController(WorldContextObject);
	return Controller ? Controller->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
}

// ---------------------------------------------------------------- Slots and teams

int32 UALKLobbyStatics::FindFreeSlot(const TArray<int32>& TakenSlots, int32 MaxPlayers)
{
	for (int32 Slot = 0; Slot < MaxPlayers; ++Slot)
	{
		if (!TakenSlots.Contains(Slot))
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

int32 UALKLobbyStatics::GetTeamCapacity(int32 MaxPlayers, int32 TeamCount)
{
	const int32 Players = FMath::Max(0, MaxPlayers);
	return TeamCount <= 0 ? Players : (Players + TeamCount - 1) / TeamCount;
}

int32 UALKLobbyStatics::CountTeam(const TArray<int32>& PlayerTeamIds, int32 TeamId)
{
	int32 Count = 0;
	for (const int32 Each : PlayerTeamIds)
	{
		Count += Each == TeamId ? 1 : 0;
	}
	return Count;
}

int32 UALKLobbyStatics::PickSmallestTeam(const TArray<int32>& PlayerTeamIds, int32 TeamCount)
{
	if (TeamCount <= 0)
	{
		return INDEX_NONE;
	}
	int32 Best = 0;
	int32 BestCount = MAX_int32;
	for (int32 Team = 0; Team < TeamCount; ++Team)
	{
		const int32 Count = CountTeam(PlayerTeamIds, Team);
		if (Count < BestCount)
		{
			Best = Team;
			BestCount = Count;
		}
	}
	return Best;
}

bool UALKLobbyStatics::CanJoinTeam(const TArray<int32>& PlayerTeamIds, int32 TeamCount, int32 MaxPlayers, int32 TeamId)
{
	if (TeamCount <= 0)
	{
		// No teams: "no team" is the only place to be.
		return TeamId == INDEX_NONE;
	}
	if (TeamId < 0 || TeamId >= TeamCount)
	{
		return false;
	}
	return CountTeam(PlayerTeamIds, TeamId) < GetTeamCapacity(MaxPlayers, TeamCount);
}

// ---------------------------------------------------------------- Starting, votes, results

EALKStartBlocker UALKLobbyStatics::GetStartBlocker(int32 ActivePlayers, int32 ReadyPlayers, int32 MinPlayers, bool bHasMap, bool bForce)
{
	if (!bHasMap)
	{
		return EALKStartBlocker::NoMap;
	}
	if (ActivePlayers < FMath::Max(1, MinPlayers))
	{
		return EALKStartBlocker::NotEnoughPlayers;
	}
	if (!bForce && ReadyPlayers < ActivePlayers)
	{
		return EALKStartBlocker::NotAllReady;
	}
	return EALKStartBlocker::None;
}

FText UALKLobbyStatics::GetStartBlockerText(EALKStartBlocker Blocker)
{
	switch (Blocker)
	{
	case EALKStartBlocker::NotEnoughPlayers: return LOCTEXT("BlockerPlayers", "Waiting for more players");
	case EALKStartBlocker::NotAllReady: return LOCTEXT("BlockerReady", "Waiting for everyone to be ready");
	case EALKStartBlocker::NoMap: return LOCTEXT("BlockerMap", "No map is selected");
	default: return FText::GetEmpty();
	}
}

TArray<FALKMapVote> UALKLobbyStatics::TallyMapVotes(const TArray<FName>& Votes)
{
	TArray<FALKMapVote> Tallies;
	for (const FName Vote : Votes)
	{
		if (Vote.IsNone())
		{
			continue;
		}
		if (FALKMapVote* Existing = Tallies.FindByPredicate([Vote](const FALKMapVote& Each) { return Each.MapId == Vote; }))
		{
			++Existing->Votes;
		}
		else
		{
			Tallies.Emplace(Vote, 1);
		}
	}
	Tallies.StableSort([](const FALKMapVote& A, const FALKMapVote& B) { return A.Votes > B.Votes; });
	return Tallies;
}

FName UALKLobbyStatics::ResolveMapVote(const TArray<FALKMapVote>& Tallies, FName FallbackMapId, int32 Seed)
{
	int32 Most = 0;
	for (const FALKMapVote& Each : Tallies)
	{
		Most = FMath::Max(Most, Each.Votes);
	}
	if (Most <= 0)
	{
		return FallbackMapId;
	}
	TArray<FName> Leaders;
	for (const FALKMapVote& Each : Tallies)
	{
		if (Each.Votes == Most && !Each.MapId.IsNone())
		{
			Leaders.AddUnique(Each.MapId);
		}
	}
	if (Leaders.Num() == 0)
	{
		return FallbackMapId;
	}
	if (Leaders.Num() == 1)
	{
		return Leaders[0];
	}
	const FRandomStream Stream(Seed);
	return Leaders[Stream.RandRange(0, Leaders.Num() - 1)];
}

bool UALKLobbyStatics::IsRematchVotePassed(int32 YesVotes, int32 Voters)
{
	return Voters > 0 && YesVotes * 2 > Voters;
}

int32 UALKLobbyStatics::PickWinningTeam(const TArray<int32>& TeamScores)
{
	int32 Best = INDEX_NONE;
	bool bTied = false;
	for (int32 Team = 0; Team < TeamScores.Num(); ++Team)
	{
		if (Best == INDEX_NONE || TeamScores[Team] > TeamScores[Best])
		{
			Best = Team;
			bTied = false;
		}
		else if (TeamScores[Team] == TeamScores[Best])
		{
			bTied = true;
		}
	}
	return bTied ? INDEX_NONE : Best;
}

int32 UALKLobbyStatics::PickMVP(const TArray<FALKScoreLine>& Players, int32 WinnerTeam)
{
	// The best of the winning team; of everyone when there is no winning team or nobody is on it.
	const bool bWinnersOnly = WinnerTeam >= 0 && Players.ContainsByPredicate([WinnerTeam](const FALKScoreLine& Each) { return !Each.bSpectating && Each.TeamId == WinnerTeam; });

	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const FALKScoreLine& Line = Players[Index];
		if (Line.bSpectating || (bWinnersOnly && Line.TeamId != WinnerTeam))
		{
			continue;
		}
		if (Best == INDEX_NONE)
		{
			Best = Index;
			continue;
		}
		const FALKScoreLine& Top = Players[Best];
		const bool bBetter =
			Line.Score != Top.Score ? Line.Score > Top.Score :
			Line.Kills != Top.Kills ? Line.Kills > Top.Kills :
			Line.Deaths != Top.Deaths ? Line.Deaths < Top.Deaths :
			Line.Assists > Top.Assists;
		if (bBetter)
		{
			Best = Index;
		}
	}
	return Best;
}

bool UALKLobbyStatics::IsLimitReached(float Value, float Limit)
{
	return Limit > 0.f && Value >= Limit;
}

// ---------------------------------------------------------------- Lobby codes

FString UALKLobbyStatics::EncodeLobbyCode(const FString& Address)
{
	return FALKLobbyCode::Encode(Address);
}

bool UALKLobbyStatics::DecodeLobbyCode(const FString& Code, FString& OutAddress)
{
	return FALKLobbyCode::Decode(Code, OutAddress);
}

bool UALKLobbyStatics::ResolveJoinTarget(const FString& CodeOrAddress, FString& OutAddress)
{
	return FALKLobbyCode::ResolveJoinTarget(CodeOrAddress, OutAddress);
}

FString UALKLobbyStatics::NormalizeLobbyCode(const FString& Code)
{
	return FALKLobbyCode::Normalize(Code);
}

// ---------------------------------------------------------------- Sessions

bool UALKLobbyStatics::DoesSessionMatchFilter(const FALKSessionInfo& Session, const FALKSessionFilter& Filter)
{
	return Filter.Matches(Session);
}

TArray<FALKSessionInfo> UALKLobbyStatics::FilterSessions(const TArray<FALKSessionInfo>& Sessions, const FALKSessionFilter& Filter)
{
	return FALKSessionFilter::Apply(Filter, Sessions);
}

TArray<FALKSessionInfo> UALKLobbyStatics::SortSessions(const TArray<FALKSessionInfo>& Sessions, EALKSessionSort SortBy, bool bAscending)
{
	TArray<FALKSessionInfo> Sorted = Sessions;
	FALKSessionFilter::Sort(Sorted, SortBy, bAscending);
	return Sorted;
}

bool UALKLobbyStatics::PickBestSession(const TArray<FALKSessionInfo>& Sessions, const FALKSessionFilter& Filter, FALKSessionInfo& OutSession)
{
	const FALKSessionInfo* Best = nullptr;
	for (const FALKSessionInfo& Session : Sessions)
	{
		if (!Filter.Matches(Session) || !Session.bVersionMatches || Session.IsFull() || Session.bPasswordProtected)
		{
			continue;
		}
		if (!Best)
		{
			Best = &Session;
			continue;
		}
		const int32 Ping = Session.PingMs < 0 ? MAX_int32 : Session.PingMs;
		const int32 BestPing = Best->PingMs < 0 ? MAX_int32 : Best->PingMs;
		const bool bBetter =
			Ping != BestPing ? Ping < BestPing :
			Session.CurrentPlayers != Best->CurrentPlayers ? Session.CurrentPlayers > Best->CurrentPlayers :
			Session.ServerName.Compare(Best->ServerName, ESearchCase::IgnoreCase) < 0;
		if (bBetter)
		{
			Best = &Session;
		}
	}
	if (Best)
	{
		OutSession = *Best;
	}
	return Best != nullptr;
}

void UALKLobbyStatics::GetConnectionErrorText(EALKConnectionError Error, FText& OutTitle, FText& OutMessage)
{
	FALKConnectionErrors::GetText(Error, OutTitle, OutMessage);
}

FALKNetStats UALKLobbyStatics::GetNetStats(const UObject* WorldContextObject)
{
	return FALKNetStats::FromWorld(ALKLobbyStaticsPrivate::GetWorld(WorldContextObject));
}

// ---------------------------------------------------------------- Rules

FALKGameRules UALKLobbyStatics::MakeDefaultRules(const TArray<FALKRuleDefinition>& Definitions)
{
	FALKGameRules Rules;
	Rules.ApplyDefaults(Definitions);
	return Rules;
}

FALKGameRules UALKLobbyStatics::SanitizeRules(const FALKGameRules& Rules, const TArray<FALKRuleDefinition>& Definitions)
{
	FALKGameRules Result = Rules;
	Result.Sanitize(Definitions);
	return Result;
}

FALKGameRules UALKLobbyStatics::RandomizeRules(const TArray<FALKRuleDefinition>& Definitions, int32 Seed)
{
	FALKGameRules Rules;
	Rules.Randomize(Definitions, Seed);
	return Rules;
}

FString UALKLobbyStatics::RulesToString(const FALKGameRules& Rules)
{
	return Rules.ToOptionsString();
}

FALKGameRules UALKLobbyStatics::RulesFromString(const FString& Options)
{
	return FALKGameRules::FromOptionsString(Options);
}

bool UALKLobbyStatics::GetRuleBool(const FALKGameRules& Rules, FName RuleId, bool bDefault)
{
	return Rules.GetBool(RuleId, bDefault);
}

int32 UALKLobbyStatics::GetRuleInt(const FALKGameRules& Rules, FName RuleId, int32 Default)
{
	return Rules.GetInt(RuleId, Default);
}

float UALKLobbyStatics::GetRuleFloat(const FALKGameRules& Rules, FName RuleId, float Default)
{
	return Rules.GetFloat(RuleId, Default);
}

FString UALKLobbyStatics::GetRuleString(const FALKGameRules& Rules, FName RuleId, const FString& Default)
{
	return Rules.GetString(RuleId, Default);
}

void UALKLobbyStatics::SetRuleValue(FALKGameRules& Rules, FName RuleId, const FString& Value)
{
	Rules.SetString(RuleId, Value);
}

FText UALKLobbyStatics::GetRuleValueText(const FALKRuleDefinition& Definition, const FString& Value)
{
	return Definition.GetValueText(Value);
}

FString UALKLobbyStatics::SanitizeRuleValue(const FALKRuleDefinition& Definition, const FString& Value)
{
	return Definition.SanitizeValue(Value);
}

// ---------------------------------------------------------------- Chat and names

FString UALKLobbyStatics::MaskFilteredWords(const FString& Text, const TArray<FString>& Words)
{
	if (Words.Num() == 0 || Text.IsEmpty())
	{
		return Text;
	}
	FString Result = Text;
	int32 Index = 0;
	while (Index < Result.Len())
	{
		if (!FChar::IsAlnum(Result[Index]))
		{
			++Index;
			continue;
		}
		// One word: a run of letters and digits.
		int32 End = Index;
		while (End < Result.Len() && FChar::IsAlnum(Result[End]))
		{
			++End;
		}
		const FString Word = Result.Mid(Index, End - Index);
		const bool bFiltered = Words.ContainsByPredicate([&Word](const FString& Each) { return !Each.IsEmpty() && Each.Equals(Word, ESearchCase::IgnoreCase); });
		if (bFiltered)
		{
			for (int32 Char = Index; Char < End; ++Char)
			{
				Result[Char] = TEXT('*');
			}
		}
		Index = End;
	}
	return Result;
}

FString UALKLobbyStatics::SanitizeChatText(const FString& Text, int32 MaxLength)
{
	FString Result;
	Result.Reserve(Text.Len());
	for (const TCHAR Char : Text)
	{
		if (Char == TEXT('\n') || Char == TEXT('\r') || Char == TEXT('\t'))
		{
			Result.AppendChar(TEXT(' '));
		}
		else if (!ALKLobbyStaticsPrivate::IsControlChar(Char))
		{
			Result.AppendChar(Char);
		}
	}
	Result.TrimStartAndEndInline();
	if (MaxLength > 0 && Result.Len() > MaxLength)
	{
		Result.LeftInline(MaxLength);
		Result.TrimEndInline();
	}
	return Result;
}

FString UALKLobbyStatics::SanitizePlayerName(const FString& Name, int32 MaxLength)
{
	FString Result;
	Result.Reserve(Name.Len());
	bool bLastWasSpace = true;
	for (const TCHAR Char : Name)
	{
		// Out: control characters, the characters level URLs split on, and the ones rich text reads as tags.
		if (ALKLobbyStaticsPrivate::IsControlChar(Char) && Char != TEXT('\t') && Char != TEXT('\n') && Char != TEXT('\r'))
		{
			continue;
		}
		if (Char == TEXT('?') || Char == TEXT('#') || Char == TEXT('<') || Char == TEXT('>'))
		{
			continue;
		}
		const bool bSpace = FChar::IsWhitespace(Char);
		if (bSpace && bLastWasSpace)
		{
			continue;
		}
		Result.AppendChar(bSpace ? TEXT(' ') : Char);
		bLastWasSpace = bSpace;
	}
	Result.TrimStartAndEndInline();
	if (MaxLength > 0 && Result.Len() > MaxLength)
	{
		Result.LeftInline(MaxLength);
		Result.TrimEndInline();
	}
	return Result;
}

FString UALKLobbyStatics::MakeUniquePlayerName(const FString& DesiredName, const TArray<FString>& TakenNames)
{
	auto IsTaken = [&TakenNames](const FString& Candidate)
	{
		return TakenNames.ContainsByPredicate([&Candidate](const FString& Each) { return Each.Equals(Candidate, ESearchCase::IgnoreCase); });
	};
	if (!IsTaken(DesiredName))
	{
		return DesiredName;
	}
	for (int32 Number = 2; Number < 1000; ++Number)
	{
		const FString Candidate = FString::Printf(TEXT("%s (%d)"), *DesiredName, Number);
		if (!IsTaken(Candidate))
		{
			return Candidate;
		}
	}
	return DesiredName;
}

bool UALKLobbyStatics::IsChatAllowed(const TArray<double>& RecentTimes, double Now, int32 MaxMessages, double WindowSeconds)
{
	int32 InWindow = 0;
	for (const double Time : RecentTimes)
	{
		InWindow += (Time > Now - WindowSeconds && Time <= Now) ? 1 : 0;
	}
	return InWindow < FMath::Max(1, MaxMessages);
}

// ---------------------------------------------------------------- Scoreboard

float UALKLobbyStatics::GetScoreColumnValue(const APlayerState* PlayerState, const FALKScoreColumn& Column)
{
	if (!PlayerState)
	{
		return 0.f;
	}
	const AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(PlayerState);
	switch (Column.Source)
	{
	case EALKScoreSource::Kills: return Player ? static_cast<float>(Player->Kills) : 0.f;
	case EALKScoreSource::Deaths: return Player ? static_cast<float>(Player->Deaths) : 0.f;
	case EALKScoreSource::Assists: return Player ? static_cast<float>(Player->Assists) : 0.f;
	case EALKScoreSource::Ping: return PlayerState->GetPingInMilliseconds();
	case EALKScoreSource::CustomStat: return Player ? Player->GetCustomStat(Column.CustomStat) : 0.f;
	case EALKScoreSource::Score:
	default: return PlayerState->GetScore();
	}
}

FText UALKLobbyStatics::GetScoreColumnText(const APlayerState* PlayerState, const FALKScoreColumn& Column)
{
	if (!PlayerState)
	{
		return FText::GetEmpty();
	}
	const float Value = GetScoreColumnValue(PlayerState, Column);
	if (Column.Source == EALKScoreSource::CustomStat && !FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value), 0.005f))
	{
		FNumberFormattingOptions OneDecimal;
		OneDecimal.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
		return FText::AsNumber(Value, &OneDecimal);
	}
	return FText::AsNumber(FMath::RoundToInt(Value));
}

// ---------------------------------------------------------------- Self check

int32 UALKLobbyStatics::RunLogicChecks(TArray<FString>& OutFailures)
{
	OutFailures.Reset();
	int32 Count = 0;
	auto Check = [&OutFailures, &Count](bool bPassed, const TCHAR* What)
	{
		++Count;
		if (!bPassed)
		{
			OutFailures.Add(FString::Printf(TEXT("Logic check %d failed: %s"), Count, What));
		}
	};

	// ---- Slots
	Check(FindFreeSlot({}, 8) == 0, TEXT("first slot of an empty lobby is 0"));
	Check(FindFreeSlot({ 0, 1, 3 }, 8) == 2, TEXT("the gap in the slots is filled first"));
	Check(FindFreeSlot({ 1, 2 }, 8) == 0, TEXT("slot 0 is free again after its player left"));
	Check(FindFreeSlot({ 0, 1, 2, 3 }, 4) == INDEX_NONE, TEXT("a full lobby has no free slot"));

	// ---- Teams
	Check(GetTeamCapacity(8, 2) == 4, TEXT("8 players in 2 teams is 4 a side"));
	Check(GetTeamCapacity(9, 2) == 5, TEXT("an odd player limit rounds the team size up"));
	Check(GetTeamCapacity(8, 0) == 8, TEXT("free for all holds everyone in one group"));
	Check(CountTeam({ 0, 1, 0, -1 }, 0) == 2, TEXT("count the players of a team"));
	Check(PickSmallestTeam({}, 2) == 0, TEXT("the first player joins team 0"));
	Check(PickSmallestTeam({ 0 }, 2) == 1, TEXT("the second player joins the other team"));
	Check(PickSmallestTeam({ 0, 1 }, 2) == 0, TEXT("even teams: the lower number"));
	Check(PickSmallestTeam({ 0, 0, 1 }, 2) == 1, TEXT("the smaller team gets the new player"));
	Check(PickSmallestTeam({ 0, 1, -1 }, 2) == 0, TEXT("players without a team are not counted"));
	Check(PickSmallestTeam({ 0, 0, 1, 1, 2 }, 3) == 2, TEXT("three teams: the smallest of them"));
	Check(PickSmallestTeam({ 0, 1 }, 0) == INDEX_NONE, TEXT("free for all has no team to pick"));
	Check(!CanJoinTeam({ 0, 0, 0, 0 }, 2, 8, 0), TEXT("a full team takes nobody"));
	Check(CanJoinTeam({ 0, 0, 0, 0 }, 2, 8, 1), TEXT("the other team has room"));
	Check(!CanJoinTeam({}, 2, 8, 2), TEXT("a team number past the team count is refused"));
	Check(CanJoinTeam({}, 0, 8, INDEX_NONE) && !CanJoinTeam({}, 0, 8, 0), TEXT("free for all: only 'no team'"));

	// ---- Start conditions
	Check(GetStartBlocker(0, 0, 1, true, false) == EALKStartBlocker::NotEnoughPlayers, TEXT("nobody to play: not enough players"));
	Check(GetStartBlocker(2, 1, 1, true, false) == EALKStartBlocker::NotAllReady, TEXT("one of two ready: not all ready"));
	Check(GetStartBlocker(2, 2, 1, true, false) == EALKStartBlocker::None, TEXT("everyone ready: start"));
	Check(GetStartBlocker(2, 1, 1, true, true) == EALKStartBlocker::None, TEXT("a forced start skips the ready check"));
	Check(GetStartBlocker(1, 1, 2, true, true) == EALKStartBlocker::NotEnoughPlayers, TEXT("a forced start still needs the minimum players"));
	Check(GetStartBlocker(2, 2, 1, false, true) == EALKStartBlocker::NoMap, TEXT("no map: no start, forced or not"));
	Check(GetStartBlockerText(EALKStartBlocker::None).IsEmpty() && !GetStartBlockerText(EALKStartBlocker::NotAllReady).IsEmpty(), TEXT("blockers have a line of text, None has not"));

	// ---- Map vote
	{
		const FName MapA(TEXT("MapA"));
		const FName MapB(TEXT("MapB"));
		const FName MapC(TEXT("MapC"));
		const TArray<FALKMapVote> Tallies = TallyMapVotes({ MapA, MapB, MapA, NAME_None, MapC, MapB, MapA });
		Check(Tallies.Num() == 3, TEXT("three maps were voted for"));
		Check(Tallies.Num() == 3 && Tallies[0].MapId == MapA && Tallies[0].Votes == 3, TEXT("the most voted map is listed first"));
		Check(Tallies.Num() == 3 && Tallies[1].MapId == MapB && Tallies[1].Votes == 2 && Tallies[2].Votes == 1, TEXT("votes are counted per map"));
		Check(ResolveMapVote(Tallies, MapC, 1) == MapA, TEXT("the most voted map wins"));
		Check(ResolveMapVote({}, MapC, 1) == MapC, TEXT("no votes: the host's map"));
		const TArray<FALKMapVote> Tie = { FALKMapVote(MapA, 2), FALKMapVote(MapB, 2), FALKMapVote(MapC, 1) };
		const FName TieWinner = ResolveMapVote(Tie, MapC, 7);
		Check(TieWinner == MapA || TieWinner == MapB, TEXT("a tie picks one of the tied maps"));
		Check(ResolveMapVote(Tie, MapC, 7) == TieWinner, TEXT("the same seed breaks a tie the same way"));
	}
	Check(IsRematchVotePassed(2, 3), TEXT("2 of 3 want a rematch: passed"));
	Check(!IsRematchVotePassed(1, 2), TEXT("half is not a majority"));
	Check(!IsRematchVotePassed(0, 0), TEXT("no voters: no rematch"));

	// ---- Results
	Check(PickWinningTeam({ 3, 5 }) == 1, TEXT("the higher team score wins"));
	Check(PickWinningTeam({ 5, 5 }) == INDEX_NONE, TEXT("equal scores are a draw"));
	Check(PickWinningTeam({}) == INDEX_NONE, TEXT("no teams: no winning team"));
	Check(PickWinningTeam({ 2, 9, 9 }) == INDEX_NONE && PickWinningTeam({ 9, 2, 3 }) == 0, TEXT("three teams: a tie at the top is a draw"));
	{
		TArray<FALKScoreLine> Lines = { FALKScoreLine(0, 10.f, 5, 2, 1), FALKScoreLine(1, 30.f, 9, 1, 0), FALKScoreLine(0, 20.f, 7, 3, 2) };
		Check(PickMVP(Lines, 0) == 2, TEXT("the MVP is the best of the winning team"));
		Check(PickMVP(Lines, INDEX_NONE) == 1, TEXT("with no winning team the MVP is the best of all"));
		Check(PickMVP(Lines, 5) == 1, TEXT("a winning team with nobody on it: the best of all"));
		Lines[0].Score = 20.f;
		Lines[0].Kills = 8;
		Check(PickMVP(Lines, 0) == 0, TEXT("equal score: more kills"));
		Lines[1].bSpectating = true;
		Check(PickMVP(Lines, INDEX_NONE) == 0, TEXT("spectators are never the MVP"));
		Check(PickMVP({}, 0) == INDEX_NONE, TEXT("nobody played: no MVP"));
	}
	Check(IsLimitReached(30.f, 30.f) && !IsLimitReached(29.f, 30.f), TEXT("a limit is reached at its value"));
	Check(!IsLimitReached(100.f, 0.f), TEXT("a limit of 0 is no limit"));

	// ---- Lobby codes
	{
		const FString Address = TEXT("192.168.1.20:7777");
		const FString Code = FALKLobbyCode::Encode(Address);
		FString Decoded;
		Check(Code.Len() == 11 && Code[3] == TEXT('-') && Code[7] == TEXT('-'), TEXT("a default-port code is 9 characters in groups of 3"));
		Check(FALKLobbyCode::Decode(Code, Decoded) && Decoded == Address, TEXT("a code decodes to the address it was made from"));
		Check(FALKLobbyCode::Decode(Code.ToLower().Replace(TEXT("-"), TEXT("")), Decoded) && Decoded == Address, TEXT("lower case without dashes still decodes"));
		Check(FALKLobbyCode::Encode(TEXT("192.168.1.20")) == Code, TEXT("an address without a port is the default port"));
		Check(FALKLobbyCode::Encode(TEXT("192.168.1.21:7777")) != Code, TEXT("another address is another code"));
		Check(FALKLobbyCode::Normalize(Code.ToLower().Replace(TEXT("-"), TEXT(" "))) == Code, TEXT("a typed code is tidied back to its printed form"));

		// One wrong character must not lead anywhere.
		bool bTyposRefused = true;
		for (int32 Index = 0; Index < Code.Len(); ++Index)
		{
			if (Code[Index] == TEXT('-'))
			{
				continue;
			}
			FString Typo = Code;
			Typo[Index] = Code[Index] == TEXT('7') ? TEXT('8') : TEXT('7');
			bTyposRefused &= !FALKLobbyCode::Decode(Typo, Decoded);
		}
		Check(bTyposRefused, TEXT("a code with one wrong character is refused"));

		const FString LongAddress = TEXT("10.0.0.5:27015");
		const FString LongCode = FALKLobbyCode::Encode(LongAddress);
		Check(LongCode.Len() == 15, TEXT("another port gives a 12 character code"));
		Check(FALKLobbyCode::Decode(LongCode, Decoded) && Decoded == LongAddress, TEXT("a long code decodes to its address and port"));
		Check(FALKLobbyCode::Encode(TEXT("not an address")).IsEmpty() && FALKLobbyCode::Encode(TEXT("300.1.1.1")).IsEmpty(), TEXT("only IPv4 addresses get a code"));
		Check(!FALKLobbyCode::Decode(TEXT("HELLO"), Decoded) && !FALKLobbyCode::Decode(TEXT(""), Decoded), TEXT("text of the wrong length is not a code"));
		Check(FALKLobbyCode::Mask(Code) == TEXT("***-***-***"), TEXT("streamer mode hides every character of the code"));

		FString Target;
		Check(FALKLobbyCode::ResolveJoinTarget(Code, Target) && Target == Address, TEXT("join by code: a code"));
		Check(FALKLobbyCode::ResolveJoinTarget(TEXT(" 192.168.1.20 "), Target) && Target == Address, TEXT("join by code: a raw address gets the default port"));
		Check(FALKLobbyCode::ResolveJoinTarget(TEXT("192.168.1.20:7778"), Target) && Target == TEXT("192.168.1.20:7778"), TEXT("join by code: a raw address with a port"));
		Check(FALKLobbyCode::ResolveJoinTarget(TEXT("localhost:7777"), Target) && Target == TEXT("localhost:7777"), TEXT("join by code: a host name"));
		Check(!FALKLobbyCode::ResolveJoinTarget(TEXT(""), Target) && !FALKLobbyCode::ResolveJoinTarget(TEXT("999.1.1.1"), Target) && !FALKLobbyCode::ResolveJoinTarget(TEXT("1.2.3.4:99999"), Target),
			TEXT("join by code: nonsense is refused"));
	}

	// ---- Sessions: filter, sort, quick match pick
	{
		const FName TeamMode(TEXT("TeamDeathmatch"));
		const FName SoloMode(TEXT("FreeForAll"));
		auto MakeSession = [](const TCHAR* Name, FName Mode, int32 Players, int32 Ping)
		{
			FALKSessionInfo Session;
			Session.SessionId = Name;
			Session.ServerName = Name;
			Session.ModeId = Mode;
			Session.CurrentPlayers = Players;
			Session.MaxPlayers = 8;
			Session.PingMs = Ping;
			return Session;
		};
		TArray<FALKSessionInfo> Sessions = {
			MakeSession(TEXT("Charlie"), TeamMode, 0, 90), MakeSession(TEXT("Alpha"), TeamMode, 4, 40),
			MakeSession(TEXT("Delta"), TeamMode, 2, INDEX_NONE), MakeSession(TEXT("Bravo"), SoloMode, 8, 20) };
		Sessions[0].bPasswordProtected = true;
		Sessions[2].bVersionMatches = false;

		FALKSessionFilter Filter;
		Check(FilterSessions(Sessions, Filter).Num() == 4, TEXT("an empty filter keeps every game"));
		Filter.bHideFull = true;
		Check(FilterSessions(Sessions, Filter).Num() == 3, TEXT("hide full games"));
		Filter = FALKSessionFilter();
		Filter.bHideEmpty = true;
		Check(FilterSessions(Sessions, Filter).Num() == 3, TEXT("hide empty games"));
		Filter = FALKSessionFilter();
		Filter.bHidePasswordProtected = true;
		Check(FilterSessions(Sessions, Filter).Num() == 3, TEXT("hide password protected games"));
		Filter = FALKSessionFilter();
		Filter.ModeId = TeamMode;
		Check(FilterSessions(Sessions, Filter).Num() == 3, TEXT("filter by mode"));
		Filter = FALKSessionFilter();
		Filter.SearchText = TEXT("brav");
		const TArray<FALKSessionInfo> Found = FilterSessions(Sessions, Filter);
		Check(Found.Num() == 1 && Found[0].ServerName == TEXT("Bravo"), TEXT("search text matches part of the name, any case"));

		TArray<FALKSessionInfo> Sorted = SortSessions(Sessions, EALKSessionSort::Name, true);
		Check(Sorted[0].ServerName == TEXT("Alpha") && Sorted[3].ServerName == TEXT("Delta"), TEXT("sort by name, A to Z"));
		Sorted = SortSessions(Sessions, EALKSessionSort::Name, false);
		Check(Sorted[0].ServerName == TEXT("Delta") && Sorted[3].ServerName == TEXT("Alpha"), TEXT("sort by name, Z to A"));
		Sorted = SortSessions(Sessions, EALKSessionSort::Ping, true);
		Check(Sorted[0].ServerName == TEXT("Bravo") && Sorted[1].ServerName == TEXT("Alpha") && Sorted[3].ServerName == TEXT("Delta"), TEXT("sort by ping: lowest first, unknown last"));
		Sorted = SortSessions(Sessions, EALKSessionSort::Players, false);
		Check(Sorted[0].ServerName == TEXT("Bravo") && Sorted[3].ServerName == TEXT("Charlie"), TEXT("sort by players, fullest first"));
		Sorted = SortSessions(Sessions, EALKSessionSort::Mode, true);
		Check(Sorted[0].ServerName == TEXT("Bravo") && Sorted[1].ServerName == TEXT("Alpha"), TEXT("sort by mode, then by name inside a mode"));

		FALKSessionInfo Best;
		Filter = FALKSessionFilter();
		Filter.ModeId = TeamMode;
		Check(PickBestSession(Sessions, Filter, Best) && Best.ServerName == TEXT("Alpha"), TEXT("quick match joins the open game of the same version"));
		Filter.ModeId = SoloMode;
		Check(!PickBestSession(Sessions, Filter, Best), TEXT("quick match does not join a full game"));
		Check(Sessions[3].IsFull() && Sessions[0].IsEmpty() && !Sessions[1].IsFull() && !Sessions[1].IsEmpty(), TEXT("full and empty are told apart"));
	}

	// ---- Rules
	{
		TArray<FALKRuleDefinition> Definitions;
		Definitions.Add(FALKRuleDefinition::MakeToggle(TEXT("Fire"), FText::GetEmpty(), FText::GetEmpty(), false));
		Definitions.Add(FALKRuleDefinition::MakeSlider(TEXT("Time"), FText::GetEmpty(), FText::GetEmpty(), 0.f, 30.f, 1.f, 10.f));
		Definitions.Add(FALKRuleDefinition::MakeSlider(TEXT("Speed"), FText::GetEmpty(), FText::GetEmpty(), 0.5f, 2.f, 0.25f, 1.f, EALKValueFormat::TwoDecimals));
		Definitions.Add(FALKRuleDefinition::MakeOptions(TEXT("Size"), FText::GetEmpty(), FText::GetEmpty(), { FText::FromString(TEXT("S")), FText::FromString(TEXT("M")), FText::FromString(TEXT("L")) }, 1));
		Definitions.Add(FALKRuleDefinition::MakeText(TEXT("Seed"), FText::GetEmpty(), FText::GetEmpty(), TEXT("abc")));

		FALKGameRules Rules = MakeDefaultRules(Definitions);
		Check(Rules.Num() == 5, TEXT("every rule gets a default value"));
		Check(Rules.GetInt(TEXT("Time")) == 10 && !Rules.GetBool(TEXT("Fire"), true) && Rules.GetInt(TEXT("Size")) == 1, TEXT("defaults read back as numbers and switches"));
		Check(FMath::IsNearlyEqual(Rules.GetFloat(TEXT("Speed")), 1.f) && Rules.GetString(TEXT("Seed")) == TEXT("abc"), TEXT("defaults read back as decimals and text"));
		Check(Rules.GetInt(TEXT("Missing"), 42) == 42 && !Rules.Has(TEXT("Missing")), TEXT("a rule that is not there gives the fallback"));

		Rules.SetString(TEXT("Time"), TEXT("99"));
		Rules.SetString(TEXT("Speed"), TEXT("1.3"));
		Rules.SetString(TEXT("Size"), TEXT("7"));
		Rules.SetString(TEXT("Fire"), TEXT("true"));
		Rules.SetString(TEXT("Junk"), TEXT("1"));
		Rules = SanitizeRules(Rules, Definitions);
		Check(Rules.GetInt(TEXT("Time")) == 30, TEXT("a slider value is clamped to its maximum"));
		Check(FMath::IsNearlyEqual(Rules.GetFloat(TEXT("Speed")), 1.25f), TEXT("a slider value snaps to its step"));
		Check(Rules.GetInt(TEXT("Size")) == 2, TEXT("an option index is clamped to the last option"));
		Check(Rules.GetBool(TEXT("Fire")) && Rules.GetString(TEXT("Fire")) == TEXT("1"), TEXT("a switch is stored as 1 or 0"));
		Check(!Rules.Has(TEXT("Junk")) && Rules.Num() == 5, TEXT("rules without a definition are dropped"));
		Rules.SetString(TEXT("Time"), TEXT("-5"));
		Check(SanitizeRules(Rules, Definitions).GetInt(TEXT("Time")) == 0, TEXT("a slider value is clamped to its minimum"));

		Rules.SetString(TEXT("Seed"), TEXT("a b?c#d=e,f~g+h:%\\"));
		const FString Packed = RulesToString(Rules);
		Check(RulesFromString(Packed) == Rules, TEXT("rules survive the trip through a string"));
		Check(!Packed.Contains(TEXT("?")) && !Packed.Contains(TEXT("#")) && !Packed.Contains(TEXT(" ")) && !Packed.Contains(TEXT(":")) && !Packed.Contains(TEXT("%")) && !Packed.Contains(TEXT("\\")),
			TEXT("a rules string has nothing in it that a level URL chokes on"));
		const FALKGameRules FromUrl = RulesFromString(TEXT("?listen?ALKRules=Time=5,Fire=1?ALKName=My+Game"));
		Check(FromUrl.Num() == 2 && FromUrl.GetInt(TEXT("Time")) == 5 && FromUrl.GetBool(TEXT("Fire")), TEXT("rules are read out of a whole level URL"));
		Check(RulesFromString(TEXT("?listen?ALKName=x")).Num() == 0 && RulesFromString(TEXT("")).Num() == 0, TEXT("a URL without rules gives no rules"));

		FALKGameRules GameNamed;
		GameNamed.SetInt(TEXT("Game"), 2);
		GameNamed.SetInt(TEXT("End_Game"), 1);
		const FString GamePacked = RulesToString(GameNamed);
		Check(!GamePacked.Contains(TEXT("game="), ESearchCase::IgnoreCase) && RulesFromString(GamePacked) == GameNamed,
			TEXT("a rule named like the engine's 'game=' URL option cannot be mistaken for it"));

		const FALKGameRules RandomA = RandomizeRules(Definitions, 1234);
		Check(RandomA == RandomizeRules(Definitions, 1234), TEXT("the same seed gives the same random rules"));
		Check(RandomA.Num() == 5 && SanitizeRules(RandomA, Definitions) == RandomA, TEXT("random rules are valid as they are"));

		Check(Definitions[1].SanitizeValue(TEXT("abc")) == TEXT("0") && Definitions[3].SanitizeValue(TEXT("-3")) == TEXT("0"), TEXT("nonsense values fall back to the lowest valid one"));
		Check(Definitions[3].GetValueText(TEXT("2")).ToString() == TEXT("L"), TEXT("an option shows its name"));
		Check(FALKGameRules::NumberToString(10.f) == TEXT("10") && FALKGameRules::NumberToString(0.25f) == TEXT("0.25") && FALKGameRules::NumberToString(1.5f) == TEXT("1.5"), TEXT("numbers are written without padding"));
	}

	// ---- URL options and host settings
	{
		const FString Odd = FString(TEXT("Kestrel's Room: #1 ?")) + FString::Chr(static_cast<TCHAR>(0x00E9)) + FString::Chr(static_cast<TCHAR>(0x30C4));
		Check(FALKOptionString::Unescape(FALKOptionString::Escape(Odd)) == Odd, TEXT("escaped text reads back the same, accents and all"));
		Check(FALKOptionString::Escape(TEXT("a b")) == TEXT("a+b") && FALKOptionString::Escape(TEXT("a+b")) != TEXT("a+b"), TEXT("a space is a plus, a plus is escaped"));

		FString Value;
		const FString Options = TEXT("?XALKName=no?ALKName=My+Game?ALKMax=8");
		Check(FALKOptionString::GetOption(Options, ALKNetOption::ServerName, Value) && Value == TEXT("My Game"), TEXT("an option is found by its whole key"));
		Check(FALKOptionString::GetIntOption(Options, ALKNetOption::MaxPlayers, 0) == 8 && FALKOptionString::GetIntOption(Options, ALKNetOption::Bots, 3) == 3, TEXT("number options, with a fallback when missing"));
		Check(!FALKOptionString::GetOption(Options, ALKNetOption::Password, Value), TEXT("a missing option is reported as missing"));

		FALKHostSettings Host;
		Host.ServerName = Odd;
		Host.ModeId = TEXT("TeamDeathmatch");
		Host.MapId = TEXT("Arena");
		Host.MaxPlayers = 12;
		Host.BotCount = 3;
		Host.Password = TEXT("secret-word");
		Host.Rules.SetInt(TEXT("Time"), 15);
		const FString Travel = Host.ToTravelOptions();
		FALKHostSettings Read;
		Check(Read.ApplyTravelOptions(Travel) && Read.ServerName == Host.ServerName && Read.ModeId == Host.ModeId && Read.MapId == Host.MapId, TEXT("host settings travel in the level URL: name, mode, map"));
		Check(Read.MaxPlayers == 12 && Read.BotCount == 3 && Read.Rules == Host.Rules, TEXT("host settings travel in the level URL: player limit, bots, rules"));
		Check(!Travel.Contains(TEXT("secret-word")) && Read.Password.IsEmpty(), TEXT("the password never goes into a level URL"));
		Check(!Travel.Contains(TEXT(":")) && !Travel.Contains(TEXT("%")) && !Travel.Contains(TEXT("\\")) && !Travel.Contains(TEXT("#")) && !Travel.Contains(TEXT(" ")), TEXT("travel options pass the engine's server travel check"));
		Check(!FALKHostSettings().ApplyTravelOptions(TEXT("?listen")), TEXT("a URL without the kit's options changes nothing"));
	}

	// ---- Chat and names
	Check(MaskFilteredWords(TEXT("You are a Badword, not badwordish"), { TEXT("badword") }) == TEXT("You are a *******, not badwordish"), TEXT("filtered words are masked: whole words, any case"));
	Check(MaskFilteredWords(TEXT("Hello there"), {}) == TEXT("Hello there"), TEXT("no word list: the text is untouched"));
	Check(SanitizeChatText(TEXT("  hi\nthere\t!  "), 100) == TEXT("hi there !"), TEXT("a chat line is one trimmed line"));
	Check(SanitizeChatText(TEXT("abcdef"), 3) == TEXT("abc"), TEXT("a chat line is cut at the length limit"));
	Check(SanitizeChatText(TEXT(" \n\t "), 100).IsEmpty(), TEXT("a blank chat line is empty"));
	Check(SanitizePlayerName(TEXT("  Bob?#<b>   Lee  "), 24) == TEXT("Bobb Lee"), TEXT("a player name loses URL and tag characters and extra spaces"));
	Check(SanitizePlayerName(TEXT("LongPlayerName"), 4) == TEXT("Long"), TEXT("a player name is cut at the length limit"));
	Check(MakeUniquePlayerName(TEXT("Ann"), { TEXT("Bob") }) == TEXT("Ann"), TEXT("a free name is kept"));
	Check(MakeUniquePlayerName(TEXT("Bob"), { TEXT("bob") }) == TEXT("Bob (2)"), TEXT("a taken name gets a number"));
	Check(MakeUniquePlayerName(TEXT("Bob"), { TEXT("bob"), TEXT("Bob (2)") }) == TEXT("Bob (3)"), TEXT("the next free number is used"));
	Check(IsChatAllowed({}, 10.0, 3, 5.0), TEXT("the first chat line is allowed"));
	Check(!IsChatAllowed({ 1.0, 2.0, 3.0 }, 3.5, 3, 5.0), TEXT("too many chat lines in a short time are refused"));
	Check(IsChatAllowed({ 1.0, 2.0, 3.0 }, 7.5, 3, 5.0), TEXT("chat is allowed again once the old lines are out of the window"));

	// ---- Connection errors
	{
		bool bAllHaveText = true;
		for (int32 Index = static_cast<int32>(EALKConnectionError::TimedOut); Index <= static_cast<int32>(EALKConnectionError::Unknown); ++Index)
		{
			FText Title;
			FText Message;
			FALKConnectionErrors::GetText(static_cast<EALKConnectionError>(Index), Title, Message);
			bAllHaveText &= !Title.IsEmpty() && !Message.IsEmpty();
		}
		Check(bAllHaveText, TEXT("every connection error has a title and a message"));

		FText NoTitle;
		FText NoMessage;
		FALKConnectionErrors::GetText(EALKConnectionError::None, NoTitle, NoMessage);
		Check(NoTitle.IsEmpty() && NoMessage.IsEmpty(), TEXT("no error: no text"));

		bool bReasonsRoundTrip = true;
		for (const EALKConnectionError Error : { EALKConnectionError::ServerFull, EALKConnectionError::WrongPassword, EALKConnectionError::WrongVersion,
			EALKConnectionError::Banned, EALKConnectionError::Kicked, EALKConnectionError::HostLeft })
		{
			bReasonsRoundTrip &= FALKConnectionErrors::FromReason(FALKConnectionErrors::ToReason(Error)) == Error;
		}
		Check(bReasonsRoundTrip, TEXT("each refusal reason maps back to its error"));
		Check(FALKConnectionErrors::FromReason(TEXT("Server full.")) == EALKConnectionError::ServerFull, TEXT("the engine's own 'Server full.' is understood"));
		Check(FALKConnectionErrors::FromReason(TEXT("")) == EALKConnectionError::None && FALKConnectionErrors::FromReason(TEXT("something else")) == EALKConnectionError::Unknown, TEXT("no reason is no error; an unknown reason is Unknown"));
	}

	return Count;
}

#undef LOCTEXT_NAMESPACE
