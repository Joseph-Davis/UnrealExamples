// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Online/ALKSampleOnlineProvider.h"
#include "Online/ALKOnlineConfig.h"
#include "Engine/GameInstance.h"
#include "Misc/Crc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSampleOnlineProvider)

#define LOCTEXT_NAMESPACE "ALKOnlineSample"

namespace ALKSamplePrivate
{
	/** Made-up, neutral names. The first eight are friends, the rest recent players and leaderboard rows. */
	static const TCHAR* const Names[] = {
		TEXT("Kestrel"), TEXT("Juno_R"), TEXT("OddBadger"), TEXT("Mirela"), TEXT("tin_can"), TEXT("Vasquez77"),
		TEXT("Nomad"), TEXT("PixelMoth"), TEXT("Harrow"), TEXT("Lumen"), TEXT("Quill"), TEXT("Sable")
	};
	static constexpr int32 NameCount = UE_ARRAY_COUNT(Names);
	static constexpr int32 FriendCount = 8;
	static constexpr int32 PartyMaxSize = 4;

	/** Made-up games for the server browser. */
	static const TCHAR* const GameNames[] = {
		TEXT("Friday Night Arena"), TEXT("EU Casual 24/7"), TEXT("Low Ping Scrims"), TEXT("Beginners Welcome"), TEXT("Late Shift TDM"), TEXT("Capture Point Rotation"),
		TEXT("LAN Party Room"), TEXT("Weekend Warmup"), TEXT("No Bots Allowed"), TEXT("Sunday League"), TEXT("Midnight Mixer"), TEXT("Open Practice")
	};
	static constexpr int32 GameCount = UE_ARRAY_COUNT(GameNames);
	/** Map names for them when the Online Config lists no maps yet. */
	static const TCHAR* const MapNames[] = { TEXT("Foundry"), TEXT("Harbor"), TEXT("Overpass"), TEXT("Quarry"), TEXT("Atrium") };
	static constexpr int32 MapNameCount = UE_ARRAY_COUNT(MapNames);
	static const TCHAR* const RegionIds[] = { TEXT("europe"), TEXT("americas"), TEXT("asia") };
	static constexpr int32 RegionCount = UE_ARRAY_COUNT(RegionIds);
	static const TCHAR* const SampleSessionPrefix = TEXT("sample-game-");

	/** The friends' starting presence, turned by the seed. */
	static const EALKPresence StartPresence[FriendCount] = {
		EALKPresence::InLobby, EALKPresence::Online, EALKPresence::InGame, EALKPresence::Online,
		EALKPresence::Offline, EALKPresence::InGame, EALKPresence::Online, EALKPresence::Offline
	};

	static FString MakeUserId(int32 NameIndex)
	{
		return FString::Printf(TEXT("sample-%s"), *FString(Names[NameIndex]).ToLower());
	}

	/** A steady number for a piece of text: the same on every machine and every run. */
	static int32 StableHash(const FString& Text)
	{
		return static_cast<int32>(FCrc::StrCrc32(*Text.ToLower()) & 0x7FFFFFFF);
	}
}

// ---------------------------------------------------------------- Lifecycle

void UALKSampleOnlineProvider::InitializeProvider_Implementation(UGameInstance* InGameInstance)
{
	Super::InitializeProvider_Implementation(InGameInstance);
	ResetSimulation();
	// Every frame: a ticker with an interval is still handed one frame's time, so the simulation's clock would run slow.
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickSimulation));
}

void UALKSampleOnlineProvider::ShutdownProvider_Implementation()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Events.Reset();
	Super::ShutdownProvider_Implementation();
}

bool UALKSampleOnlineProvider::IsOnline_Implementation() const
{
	return true;
}

void UALKSampleOnlineProvider::ResetSimulation()
{
	Stream.Initialize(SimulationSeed);
	SimulationTime = 0.0;
	Events.Reset();
	PartyOthers.Reset();
	PartyLeaderId.Reset();
	bLocalPartyReady = false;
	bMatchQueueActive = false;
	bMatchFound = false;
	bLocalAccepted = false;
	MatchAccepted = 0;
	MatchNeeded = 0;
	SubmittedScores.Reset();
	BuildFriends();
	Schedule(PresenceChangeInterval, ESampleEvent::PresenceChange);
}

void UALKSampleOnlineProvider::SetSimulationPaused(bool bPaused)
{
	bSimulationPaused = bPaused;
}

void UALKSampleOnlineProvider::BuildFriends()
{
	using namespace ALKSamplePrivate;
	Friends.Reset();
	RecentPlayers.Reset();
	const int32 Turn = FMath::Abs(SimulationSeed) % FriendCount;
	for (int32 Index = 0; Index < NameCount; ++Index)
	{
		FALKFriend Friend;
		Friend.UserId = MakeUserId(Index);
		Friend.DisplayName = Names[Index];
		if (Index < FriendCount)
		{
			SetFriendPresence(Friend, StartPresence[(Index + Turn) % FriendCount]);
			Friends.Add(Friend);
		}
		else
		{
			// Recent players: one of them still around.
			SetFriendPresence(Friend, Index == FriendCount ? EALKPresence::Online : EALKPresence::Offline);
			RecentPlayers.Add(Friend);
		}
	}
}

void UALKSampleOnlineProvider::SetFriendPresence(FALKFriend& Friend, EALKPresence Presence)
{
	Friend.Presence = Presence;
	Friend.bInvitable = Presence == EALKPresence::Online || Presence == EALKPresence::InLobby;
	Friend.bJoinable = Presence == EALKPresence::InLobby;
	switch (Presence)
	{
	case EALKPresence::Online:
		Friend.StatusText = LOCTEXT("StatusOnline", "Online");
		break;
	case EALKPresence::InLobby:
		Friend.StatusText = LOCTEXT("StatusInLobby", "In a lobby");
		break;
	case EALKPresence::InGame:
	{
		// Which mode they play is picked by their name, so it stays the same between runs.
		const TArray<FALKModeDefinition>& Modes = GetOnlineConfig()->Modes;
		const FText ModeName = Modes.Num() > 0 ? Modes[ALKSamplePrivate::StableHash(Friend.UserId) % Modes.Num()].Name : FText::GetEmpty();
		Friend.StatusText = ModeName.IsEmpty() ? LOCTEXT("StatusInGame", "In a match") : FText::Format(LOCTEXT("StatusPlaying", "Playing {0}"), ModeName);
		break;
	}
	case EALKPresence::Offline:
	default:
		Friend.StatusText = LOCTEXT("StatusOffline", "Offline");
		break;
	}
}

FALKFriend* UALKSampleOnlineProvider::FindFriend(const FString& UserId)
{
	if (FALKFriend* Friend = Friends.FindByPredicate([&UserId](const FALKFriend& Each) { return Each.UserId == UserId; }))
	{
		return Friend;
	}
	return RecentPlayers.FindByPredicate([&UserId](const FALKFriend& Each) { return Each.UserId == UserId; });
}

const FALKFriend* UALKSampleOnlineProvider::FindFriend(const FString& UserId) const
{
	return const_cast<UALKSampleOnlineProvider*>(this)->FindFriend(UserId);
}

FString UALKSampleOnlineProvider::FindNameFor(const FString& UserId) const
{
	if (UserId == GetLocalUserId())
	{
		return GetLocalDisplayName();
	}
	const FALKFriend* Friend = FindFriend(UserId);
	return Friend ? Friend->DisplayName : UserId;
}

bool UALKSampleOnlineProvider::IsInParty(const FString& UserId) const
{
	return PartyOthers.ContainsByPredicate([&UserId](const FALKPartyMember& Each) { return Each.UserId == UserId; });
}

bool UALKSampleOnlineProvider::IsLocalPartyLeader() const
{
	return PartyLeaderId.IsEmpty();
}

FString UALKSampleOnlineProvider::PickLine(const TArray<FText>& Lines)
{
	return Lines.Num() > 0 ? Lines[Stream.RandRange(0, Lines.Num() - 1)].ToString() : FString();
}

// ---------------------------------------------------------------- The clock

void UALKSampleOnlineProvider::Schedule(float Delay, ESampleEvent Type, const FString& UserId, const FString& Text, int32 Value)
{
	FSampleEvent& Event = Events.AddDefaulted_GetRef();
	Event.Time = SimulationTime + FMath::Max(0.f, Delay);
	Event.Type = Type;
	Event.UserId = UserId;
	Event.Text = Text;
	Event.Value = Value;
}

void UALKSampleOnlineProvider::CancelEvents(ESampleEvent Type)
{
	Events.RemoveAll([Type](const FSampleEvent& Each) { return Each.Type == Type; });
}

bool UALKSampleOnlineProvider::TickSimulation(float DeltaTime)
{
	if (bSimulationPaused)
	{
		return true;
	}
	// A long hitch (a level load) should not fire everything at once out of order.
	SimulationTime += FMath::Min(DeltaTime, 0.5f);

	// Due events, oldest first. Running one may schedule more, so take them out before running them.
	TArray<FSampleEvent> Due;
	for (int32 Index = Events.Num() - 1; Index >= 0; --Index)
	{
		if (Events[Index].Time <= SimulationTime)
		{
			Due.Add(Events[Index]);
			Events.RemoveAt(Index);
		}
	}
	Due.StableSort([](const FSampleEvent& A, const FSampleEvent& B) { return A.Time < B.Time; });
	for (const FSampleEvent& Event : Due)
	{
		RunEvent(Event);
	}
	return true;
}

void UALKSampleOnlineProvider::RunEvent(const FSampleEvent& Event)
{
	switch (Event.Type)
	{
	case ESampleEvent::FriendsLoaded:
		NotifyFriendsChanged();
		break;

	case ESampleEvent::PresenceChange:
		if (bSimulateActivity)
		{
			ChangeSomeonesPresence();
		}
		Schedule(PresenceChangeInterval, ESampleEvent::PresenceChange);
		break;

	case ESampleEvent::PartyJoin:
	{
		FALKFriend* Friend = FindFriend(Event.UserId);
		if (!Friend || IsInParty(Event.UserId) || PartyOthers.Num() + 1 >= ALKSamplePrivate::PartyMaxSize)
		{
			break;
		}
		FALKPartyMember Member;
		Member.UserId = Friend->UserId;
		Member.DisplayName = Friend->DisplayName;
		Member.Avatar = Friend->Avatar;
		PartyOthers.Add(Member);
		Friend->Presence = EALKPresence::InLobby;
		Friend->bInvitable = false;
		Friend->bJoinable = false;
		Friend->StatusText = LOCTEXT("StatusInParty", "In your party");
		NotifyPartyChanged();
		NotifyFriendsChanged();
		NotifyPartyChatReceived(Friend->UserId, Friend->DisplayName, PickLine({
			LOCTEXT("PartyHello1", "Hey!"), LOCTEXT("PartyHello2", "Thanks for the invite."), LOCTEXT("PartyHello3", "Hi, what are we playing?") }));
		Schedule(Stream.FRandRange(2.f, 4.f), ESampleEvent::PartyReady, Friend->UserId);
		break;
	}

	case ESampleEvent::PartyReady:
		if (FALKPartyMember* Member = PartyOthers.FindByPredicate([&Event](const FALKPartyMember& Each) { return Each.UserId == Event.UserId; }))
		{
			if (!Member->bReady)
			{
				Member->bReady = true;
				NotifyPartyChanged();
			}
		}
		break;

	case ESampleEvent::PartyChatReply:
		if (IsInParty(Event.UserId))
		{
			NotifyPartyChatReceived(Event.UserId, FindNameFor(Event.UserId), Event.Text);
		}
		break;

	case ESampleEvent::WhisperReply:
		NotifyWhisperReceived(Event.UserId, FindNameFor(Event.UserId), GetLocalUserId(), Event.Text);
		break;

	case ESampleEvent::MatchFound:
	{
		if (!bMatchQueueActive)
		{
			break;
		}
		const UALKOnlineConfig* Config = GetOnlineConfig();
		const FALKPlaylist* Playlist = Config->FindPlaylist(MatchPlaylist);
		const FALKModeDefinition* Mode = Config->FindMode(Playlist ? Playlist->ModeId : NAME_None);

		FALKSessionInfo Session;
		Session.ServerName = Playlist ? Playlist->Name.ToString() : LOCTEXT("SampleMatchName", "Quick Match").ToString();
		Session.ModeId = Playlist ? Playlist->ModeId : NAME_None;
		Session.MaxPlayers = Playlist && Playlist->MaxPlayers > 0 ? Playlist->MaxPlayers : (Mode ? Mode->DefaultMaxPlayers : Config->DefaultMaxPlayers);
		Session.CurrentPlayers = Session.MaxPlayers;
		Session.PingMs = 20 + ALKSamplePrivate::StableHash(MatchPlaylist.ToString()) % 40;

		bMatchFound = true;
		bLocalAccepted = false;
		MatchAccepted = 0;
		MatchNeeded = FMath::Clamp(Session.MaxPlayers, 2, 8);
		NotifyMatchFound(Session, Config->MatchFoundAcceptSeconds, MatchAccepted, MatchNeeded);

		// The other players accept one by one, well inside the countdown.
		float Delay = 0.f;
		for (int32 Index = 0; Index < MatchNeeded - 1; ++Index)
		{
			Delay += Stream.FRandRange(0.35f, 0.9f);
			Schedule(FMath::Min(Delay, Config->MatchFoundAcceptSeconds * 0.8f), ESampleEvent::MatchPlayerAccepted);
		}
		Schedule(Config->MatchFoundAcceptSeconds, ESampleEvent::MatchAcceptTimeout);
		break;
	}

	case ESampleEvent::MatchPlayerAccepted:
		if (bMatchFound && MatchAccepted < MatchNeeded - (bLocalAccepted ? 0 : 1))
		{
			++MatchAccepted;
			NotifyMatchAcceptProgress(MatchAccepted, MatchNeeded);
			CheckMatchAccepted();
		}
		break;

	case ESampleEvent::MatchAcceptTimeout:
		if (bMatchFound)
		{
			// The countdown ran out before everyone accepted.
			StopMatchSimulation();
			NotifyMatchmakingFailed(bLocalAccepted ? EALKConnectionError::Cancelled : EALKConnectionError::TimedOut);
		}
		break;

	case ESampleEvent::LeaderboardLoaded:
	{
		using namespace ALKSamplePrivate;
		const FName BoardId(*Event.Text);
		const int32 MaxRows = FMath::Max(1, Event.Value);

		// The same board always has the same rows: scores come from the board's and the players' names.
		TArray<FALKLeaderboardRow> Rows;
		for (int32 Index = 0; Index < NameCount; ++Index)
		{
			FALKLeaderboardRow Row;
			Row.UserId = MakeUserId(Index);
			Row.DisplayName = Names[Index];
			Row.Score = 1200.0 + (StableHash(Event.Text + Row.UserId) % 8600);
			Rows.Add(Row);
		}
		FALKLeaderboardRow Local;
		Local.UserId = GetLocalUserId();
		Local.DisplayName = GetLocalDisplayName();
		Local.bLocalPlayer = true;
		const double* Submitted = SubmittedScores.Find(BoardId);
		Local.Score = Submitted ? *Submitted : 1200.0 + (StableHash(Event.Text + TEXT("local")) % 8600);
		Rows.Add(Local);

		Rows.StableSort([](const FALKLeaderboardRow& A, const FALKLeaderboardRow& B) { return A.Score > B.Score; });
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			Rows[Index].Rank = Index + 1;
			Rows[Index].ScoreText = FText::AsNumber(FMath::RoundToInt32(Rows[Index].Score));
		}
		// The top rows, and the local player's own row when it is further down.
		if (Rows.Num() > MaxRows)
		{
			const int32 LocalIndex = Rows.IndexOfByPredicate([](const FALKLeaderboardRow& Each) { return Each.bLocalPlayer; });
			const FALKLeaderboardRow LocalRow = Rows[LocalIndex];
			Rows.SetNum(MaxRows);
			if (LocalIndex >= MaxRows)
			{
				Rows.Last() = LocalRow;
			}
		}
		NotifyLeaderboardReceived(BoardId, Rows);
		break;
	}

	case ESampleEvent::SampleJoinFailed:
		// A made-up game cannot be joined: it ends like a game that closed, or one that never answered.
		NotifyJoinComplete(false, Event.Value % 2 == 0 ? EALKConnectionError::SessionNotFound : EALKConnectionError::TimedOut, FString());
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------- Made-up games

void UALKSampleOnlineProvider::AugmentFindResults(const FALKSessionFilter& Filter, TArray<FALKSessionInfo>& Results)
{
	using namespace ALKSamplePrivate;
	if (!bSimulateSessions)
	{
		return;
	}
	const UALKOnlineConfig* Config = GetOnlineConfig();
	const FString LocalVersion = Config->GetResolvedBuildVersion();
	// A stream of its own: the list is the same on every search and does not disturb the rest of the simulation.
	FRandomStream Local(SimulationSeed * 31 + 7);

	for (int32 Index = 0; Index < GameCount; ++Index)
	{
		FALKSessionInfo Session;
		Session.SessionId = FString::Printf(TEXT("%s%02d"), SampleSessionPrefix, Index);
		Session.ServerName = GameNames[Index];
		const int32 HostIndex = (Index * 5 + 3) % NameCount;
		Session.HostName = Names[HostIndex];
		Session.HostUserId = MakeUserId(HostIndex);
		Session.SearchResultIndex = -1000 - Index;

		// The mode: the one the name hints at when the config has it, any listed mode otherwise.
		const FALKModeDefinition* Mode = nullptr;
		if (Index == 4)
		{
			Mode = Config->FindMode(TEXT("TeamDeathmatch"));
		}
		else if (Index == 5)
		{
			Mode = Config->FindMode(TEXT("CapturePoint"));
		}
		const int32 ModePick = Local.RandRange(0, FMath::Max(0, Config->Modes.Num() - 1));
		if (!Mode && Config->Modes.Num() > 0)
		{
			Mode = &Config->Modes[ModePick];
		}
		Session.ModeId = Mode ? Mode->Id : NAME_None;

		const TArray<FALKMapDefinition> ModeMaps = Config->GetMapsForMode(Session.ModeId);
		const int32 MapPick = Local.RandRange(0, 1000);
		Session.MapId = ModeMaps.Num() > 0 ? ModeMaps[MapPick % ModeMaps.Num()].Id : FName(MapNames[MapPick % MapNameCount]);

		const TArray<int32> Limits = Config->GetMaxPlayersOptions(Session.ModeId);
		Session.MaxPlayers = Limits[Local.RandRange(0, Limits.Num() - 1)];
		Session.CurrentPlayers = Local.RandRange(1, FMath::Max(1, Session.MaxPlayers - 1));
		Session.PingMs = 3 + Local.RandRange(0, 137);
		Session.Region = RegionIds[Local.RandRange(0, RegionCount - 1)];
		Session.bInProgress = Local.RandRange(0, 9) < 4;
		Session.BuildVersion = LocalVersion;
		Session.bVersionMatches = true;
		if (Mode)
		{
			FALKGameRules Rules;
			Rules.Randomize(Config->GetRulesForMode(Mode->Id), SimulationSeed + Index);
			Session.Rules = Rules.ToMap();
		}

		// A few of each kind, so every column and filter has something to show.
		switch (Index)
		{
		case 1:
			Session.Region = RegionIds[0];
			break;
		case 2:
			Session.PingMs = 3 + Local.RandRange(0, 9);
			Session.CurrentPlayers = Session.MaxPlayers;
			Session.bPasswordProtected = true;
			break;
		case 6:
			Session.bLAN = true;
			Session.PingMs = 3;
			break;
		case 9:
			Session.bPasswordProtected = true;
			break;
		case 10:
			Session.BuildVersion = LocalVersion + TEXT("-old");
			Session.bVersionMatches = false;
			break;
		case 11:
			Session.CurrentPlayers = 0;
			Session.bInProgress = false;
			break;
		default:
			break;
		}
		Results.Add(Session);
	}
}

void UALKSampleOnlineProvider::JoinSession_Implementation(const FALKSessionInfo& Session, const FString& Password)
{
	if (Session.SessionId.StartsWith(ALKSamplePrivate::SampleSessionPrefix))
	{
		CancelEvents(ESampleEvent::SampleJoinFailed);
		Schedule(2.5f, ESampleEvent::SampleJoinFailed, FString(), FString(), FMath::Abs(Session.SearchResultIndex));
		return;
	}
	Super::JoinSession_Implementation(Session, Password);
}

void UALKSampleOnlineProvider::ChangeSomeonesPresence()
{
	// A friend who is not in the party moves on to something else.
	TArray<int32> Candidates;
	for (int32 Index = 0; Index < Friends.Num(); ++Index)
	{
		if (!IsInParty(Friends[Index].UserId))
		{
			Candidates.Add(Index);
		}
	}
	if (Candidates.Num() == 0)
	{
		return;
	}
	FALKFriend& Friend = Friends[Candidates[Stream.RandRange(0, Candidates.Num() - 1)]];
	static const EALKPresence Order[] = { EALKPresence::Online, EALKPresence::InLobby, EALKPresence::InGame, EALKPresence::Online, EALKPresence::Offline };
	EALKPresence Next = Order[Stream.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Order)) - 1)];
	if (Next == Friend.Presence)
	{
		Next = Friend.Presence == EALKPresence::Online ? EALKPresence::InLobby : EALKPresence::Online;
	}
	SetFriendPresence(Friend, Next);
	NotifyFriendsChanged();
}

// ---------------------------------------------------------------- Matchmaking

void UALKSampleOnlineProvider::StartMatchmaking_Implementation(FName PlaylistId)
{
	StopMatchSimulation();
	StopQuickMatchSearch();
	MatchPlaylist = PlaylistId;
	bMatchQueueActive = true;
	Schedule(QueueSeconds, ESampleEvent::MatchFound);
}

void UALKSampleOnlineProvider::CancelMatchmaking_Implementation()
{
	StopMatchSimulation();
	StopQuickMatchSearch();
	NotifyMatchmakingFailed(EALKConnectionError::Cancelled);
}

float UALKSampleOnlineProvider::GetEstimatedWaitSeconds_Implementation(FName PlaylistId) const
{
	return 8.f + ALKSamplePrivate::StableHash(PlaylistId.ToString()) % 18;
}

void UALKSampleOnlineProvider::RespondToMatch_Implementation(bool bAccept)
{
	if (!bMatchFound || bLocalAccepted)
	{
		return;
	}
	if (!bAccept)
	{
		StopMatchSimulation();
		NotifyMatchmakingFailed(EALKConnectionError::Cancelled);
		return;
	}
	bLocalAccepted = true;
	++MatchAccepted;
	NotifyMatchAcceptProgress(MatchAccepted, MatchNeeded);
	CheckMatchAccepted();
}

void UALKSampleOnlineProvider::CheckMatchAccepted()
{
	if (bMatchFound && bLocalAccepted && MatchAccepted >= MatchNeeded)
	{
		// Everyone is in: from here on it is the parent's real quick match (join the best game, or host one).
		const FName Playlist = MatchPlaylist;
		StopMatchSimulation();
		BeginQuickMatchSearch(Playlist);
	}
}

void UALKSampleOnlineProvider::StopMatchSimulation()
{
	bMatchQueueActive = false;
	bMatchFound = false;
	bLocalAccepted = false;
	MatchAccepted = 0;
	MatchNeeded = 0;
	CancelEvents(ESampleEvent::MatchFound);
	CancelEvents(ESampleEvent::MatchPlayerAccepted);
	CancelEvents(ESampleEvent::MatchAcceptTimeout);
}

// ---------------------------------------------------------------- Party

TArray<FALKPartyMember> UALKSampleOnlineProvider::GetPartyMembers_Implementation() const
{
	TArray<FALKPartyMember> Members;
	FALKPartyMember Local;
	Local.UserId = GetLocalUserId();
	Local.DisplayName = GetLocalDisplayName();
	Local.bLocal = true;
	Local.bLeader = IsLocalPartyLeader();
	Local.bReady = bLocalPartyReady;
	Members.Add(Local);
	for (const FALKPartyMember& Other : PartyOthers)
	{
		FALKPartyMember Member = Other;
		Member.bLeader = Member.UserId == PartyLeaderId;
		// The leader is listed first.
		if (Member.bLeader)
		{
			Members.Insert(Member, 0);
		}
		else
		{
			Members.Add(Member);
		}
	}
	return Members;
}

void UALKSampleOnlineProvider::InviteToParty_Implementation(const FString& UserId)
{
	const FALKFriend* Friend = FindFriend(UserId);
	if (!Friend || !Friend->bInvitable || IsInParty(UserId) || PartyOthers.Num() + 1 >= ALKSamplePrivate::PartyMaxSize)
	{
		return;
	}
	// Every invite is accepted after a moment.
	const bool bAlreadyInvited = Events.ContainsByPredicate([&UserId](const FSampleEvent& Each) { return Each.Type == ESampleEvent::PartyJoin && Each.UserId == UserId; });
	if (!bAlreadyInvited)
	{
		Schedule(PartyJoinDelay, ESampleEvent::PartyJoin, UserId);
	}
}

void UALKSampleOnlineProvider::RespondToPartyInvite_Implementation(const FString& FromUserId, bool bAccept)
{
	FALKFriend* Friend = FindFriend(FromUserId);
	if (!bAccept || !Friend)
	{
		return;
	}
	// The player joins the friend's party: the friend leads it.
	for (const FALKPartyMember& Old : PartyOthers)
	{
		if (FALKFriend* OldFriend = FindFriend(Old.UserId))
		{
			SetFriendPresence(*OldFriend, EALKPresence::Online);
		}
	}
	PartyOthers.Reset();
	FALKPartyMember Leader;
	Leader.UserId = Friend->UserId;
	Leader.DisplayName = Friend->DisplayName;
	Leader.bReady = true;
	PartyOthers.Add(Leader);
	PartyLeaderId = Friend->UserId;
	bLocalPartyReady = false;
	Friend->Presence = EALKPresence::InLobby;
	Friend->bInvitable = false;
	Friend->bJoinable = false;
	Friend->StatusText = LOCTEXT("StatusInParty", "In your party");
	NotifyPartyChanged();
	NotifyFriendsChanged();
}

void UALKSampleOnlineProvider::KickFromParty_Implementation(const FString& UserId)
{
	if (!IsLocalPartyLeader())
	{
		return;
	}
	if (PartyOthers.RemoveAll([&UserId](const FALKPartyMember& Each) { return Each.UserId == UserId; }) > 0)
	{
		if (FALKFriend* Friend = FindFriend(UserId))
		{
			SetFriendPresence(*Friend, EALKPresence::Online);
		}
		NotifyPartyChanged();
		NotifyFriendsChanged();
	}
}

void UALKSampleOnlineProvider::PromotePartyMember_Implementation(const FString& UserId)
{
	if (IsLocalPartyLeader() && IsInParty(UserId))
	{
		PartyLeaderId = UserId;
		NotifyPartyChanged();
	}
}

void UALKSampleOnlineProvider::LeaveParty_Implementation()
{
	if (PartyOthers.Num() == 0)
	{
		return;
	}
	for (const FALKPartyMember& Member : PartyOthers)
	{
		if (FALKFriend* Friend = FindFriend(Member.UserId))
		{
			SetFriendPresence(*Friend, EALKPresence::Online);
		}
	}
	PartyOthers.Reset();
	PartyLeaderId.Reset();
	bLocalPartyReady = false;
	CancelEvents(ESampleEvent::PartyReady);
	CancelEvents(ESampleEvent::PartyChatReply);
	NotifyPartyChanged();
	NotifyFriendsChanged();
}

void UALKSampleOnlineProvider::SetPartyReady_Implementation(bool bReady)
{
	if (bLocalPartyReady != bReady)
	{
		bLocalPartyReady = bReady;
		NotifyPartyChanged();
	}
}

void UALKSampleOnlineProvider::SendPartyChat_Implementation(const FString& Text)
{
	NotifyPartyChatReceived(GetLocalUserId(), GetLocalDisplayName(), Text);
	if (PartyOthers.Num() > 0)
	{
		const FALKPartyMember& Member = PartyOthers[Stream.RandRange(0, PartyOthers.Num() - 1)];
		const FString Reply = PickLine({
			LOCTEXT("PartyReply1", "Sounds good."), LOCTEXT("PartyReply2", "Ready when you are."), LOCTEXT("PartyReply3", "One more game?"),
			LOCTEXT("PartyReply4", "Let's go!"), LOCTEXT("PartyReply5", "Give me a second.") });
		Schedule(Stream.FRandRange(1.2f, 2.5f), ESampleEvent::PartyChatReply, Member.UserId, Reply);
	}
}

int32 UALKSampleOnlineProvider::GetPartyMaxSize_Implementation() const
{
	return ALKSamplePrivate::PartyMaxSize;
}

// ---------------------------------------------------------------- Friends

void UALKSampleOnlineProvider::QueryFriends_Implementation()
{
	// A real backend answers a little later.
	CancelEvents(ESampleEvent::FriendsLoaded);
	Schedule(0.3f, ESampleEvent::FriendsLoaded);
}

TArray<FALKFriend> UALKSampleOnlineProvider::GetFriends_Implementation() const
{
	// Who can be reached first; the made-up order inside each group never changes.
	TArray<FALKFriend> Sorted = Friends;
	Sorted.StableSort([](const FALKFriend& A, const FALKFriend& B)
	{
		return (A.Presence != EALKPresence::Offline) && (B.Presence == EALKPresence::Offline);
	});
	return Sorted;
}

TArray<FALKFriend> UALKSampleOnlineProvider::GetRecentPlayers_Implementation() const
{
	return RecentPlayers;
}

void UALKSampleOnlineProvider::InviteFriendToGame_Implementation(const FString& UserId)
{
	const FALKFriend* Friend = FindFriend(UserId);
	if (Friend && Friend->Presence != EALKPresence::Offline)
	{
		// Nobody can really join from here: the friend answers instead.
		Schedule(Stream.FRandRange(1.5f, 3.f), ESampleEvent::WhisperReply, UserId, PickLine({
			LOCTEXT("InviteReply1", "Thanks! Finishing a match first."), LOCTEXT("InviteReply2", "Give me a few minutes."), LOCTEXT("InviteReply3", "Maybe the next one!") }));
	}
}

void UALKSampleOnlineProvider::JoinFriend_Implementation(const FString& UserId)
{
	const FALKFriend* Friend = FindFriend(UserId);
	if (Friend && Friend->Presence != EALKPresence::Offline)
	{
		// The friend's game does not exist: they say so.
		Schedule(Stream.FRandRange(1.f, 2.f), ESampleEvent::WhisperReply, UserId, LOCTEXT("JoinReply", "Our lobby just filled up. Invite me to yours?").ToString());
	}
}

void UALKSampleOnlineProvider::RespondToGameInvite_Implementation(const FString& FromUserId, bool bAccept)
{
	if (bAccept && FindFriend(FromUserId))
	{
		Schedule(Stream.FRandRange(1.f, 2.f), ESampleEvent::WhisperReply, FromUserId, LOCTEXT("GameInviteReply", "Too late, the match started. Next one!").ToString());
	}
}

void UALKSampleOnlineProvider::SendWhisper_Implementation(const FString& UserId, const FString& Text)
{
	// The sender sees the line too.
	NotifyWhisperReceived(GetLocalUserId(), GetLocalDisplayName(), UserId, Text);
	const FALKFriend* Friend = FindFriend(UserId);
	if (Friend && Friend->Presence != EALKPresence::Offline)
	{
		Schedule(Stream.FRandRange(1.5f, 3.f), ESampleEvent::WhisperReply, UserId, PickLine({
			LOCTEXT("WhisperReply1", "Hey! Good to hear from you."), LOCTEXT("WhisperReply2", "Sure, sounds good."), LOCTEXT("WhisperReply3", "In a match, talk soon."),
			LOCTEXT("WhisperReply4", "Ha, nice.") }));
	}
}

void UALKSampleOnlineProvider::SimulatePartyInvite(const FString& FromUserId)
{
	const FALKFriend* Friend = FromUserId.IsEmpty()
		? Friends.FindByPredicate([this](const FALKFriend& Each) { return Each.bInvitable && !IsInParty(Each.UserId); })
		: FindFriend(FromUserId);
	if (Friend)
	{
		NotifyPartyInviteReceived(Friend->UserId, Friend->DisplayName);
	}
}

void UALKSampleOnlineProvider::SimulateGameInvite(const FString& FromUserId)
{
	const FALKFriend* Friend = FromUserId.IsEmpty()
		? Friends.FindByPredicate([](const FALKFriend& Each) { return Each.Presence == EALKPresence::InLobby || Each.Presence == EALKPresence::InGame; })
		: FindFriend(FromUserId);
	if (!Friend)
	{
		return;
	}
	const UALKOnlineConfig* Config = GetOnlineConfig();
	FALKSessionInfo Session;
	Session.ServerName = FText::Format(LOCTEXT("SampleInviteGame", "{0}'s Game"), FText::FromString(Friend->DisplayName)).ToString();
	Session.HostName = Friend->DisplayName;
	Session.HostUserId = Friend->UserId;
	if (Config->Modes.Num() > 0)
	{
		const FALKModeDefinition& Mode = Config->Modes[ALKSamplePrivate::StableHash(Friend->UserId) % Config->Modes.Num()];
		Session.ModeId = Mode.Id;
		Session.MaxPlayers = Mode.DefaultMaxPlayers;
		Session.CurrentPlayers = FMath::Max(1, Mode.DefaultMaxPlayers / 2);
	}
	Session.PingMs = 24;
	NotifyGameInviteReceived(Friend->UserId, Friend->DisplayName, Session);
}

void UALKSampleOnlineProvider::SimulateWhisper(const FString& FromUserId, const FString& Text)
{
	const FALKFriend* Friend = FromUserId.IsEmpty()
		? Friends.FindByPredicate([](const FALKFriend& Each) { return Each.Presence != EALKPresence::Offline; })
		: FindFriend(FromUserId);
	if (Friend)
	{
		NotifyWhisperReceived(Friend->UserId, Friend->DisplayName, GetLocalUserId(), Text.IsEmpty() ? LOCTEXT("SampleWhisper", "Up for a game?").ToString() : Text);
	}
}

void UALKSampleOnlineProvider::SimulateTalking(const FString& UserId, bool bTalking)
{
	NotifyPlayerTalking(UserId, bTalking);
}

// ---------------------------------------------------------------- Profiles and leaderboards

bool UALKSampleOnlineProvider::GetPlayerProfile_Implementation(const FString& UserId, FALKPlayerProfile& OutProfile) const
{
	if (UserId.IsEmpty())
	{
		return false;
	}
	// Made up from the id, so a player's card reads the same every time.
	const int32 Hash = ALKSamplePrivate::StableHash(UserId);
	const int32 Matches = 20 + Hash % 480;
	const int32 Wins = FMath::RoundToInt(Matches * (0.35f + (Hash / 7 % 30) / 100.f));
	const float KillDeath = 0.6f + (Hash / 13 % 140) / 100.f;
	const int32 Hours = 4 + Hash / 31 % 300;

	OutProfile = FALKPlayerProfile();
	OutProfile.UserId = UserId;
	const FALKFriend* Friend = FindFriend(UserId);
	OutProfile.DisplayName = UserId == GetLocalUserId() ? GetLocalDisplayName() : (Friend ? Friend->DisplayName : FString());
	OutProfile.Level = 1 + Hash / 3 % 60;
	OutProfile.XPProgress = (Hash / 17 % 100) / 100.f;
	if (Friend)
	{
		OutProfile.Avatar = Friend->Avatar;
	}

	FNumberFormattingOptions TwoDecimals;
	TwoDecimals.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);

	FALKProfileStat Stat;
	Stat.Label = LOCTEXT("ProfileMatches", "Matches");
	Stat.Value = FText::AsNumber(Matches);
	OutProfile.Stats.Add(Stat);
	Stat.Label = LOCTEXT("ProfileWins", "Wins");
	Stat.Value = FText::AsNumber(Wins);
	OutProfile.Stats.Add(Stat);
	Stat.Label = LOCTEXT("ProfileWinRate", "Win Rate");
	Stat.Value = FText::AsPercent(static_cast<float>(Wins) / Matches);
	OutProfile.Stats.Add(Stat);
	Stat.Label = LOCTEXT("ProfileKD", "K/D");
	Stat.Value = FText::AsNumber(KillDeath, &TwoDecimals);
	OutProfile.Stats.Add(Stat);
	Stat.Label = LOCTEXT("ProfilePlayTime", "Play Time");
	Stat.Value = FText::Format(LOCTEXT("ProfileHours", "{0} h"), FText::AsNumber(Hours));
	OutProfile.Stats.Add(Stat);
	return true;
}

void UALKSampleOnlineProvider::QueryLeaderboard_Implementation(FName BoardId, int32 MaxRows)
{
	Schedule(0.4f, ESampleEvent::LeaderboardLoaded, FString(), BoardId.ToString(), MaxRows);
}

void UALKSampleOnlineProvider::SubmitScore_Implementation(FName BoardId, double Score)
{
	// Only a better score replaces the old one.
	double& Best = SubmittedScores.FindOrAdd(BoardId, Score);
	Best = FMath::Max(Best, Score);
}

// ---------------------------------------------------------------- Lists

TArray<FALKOnlineOption> UALKSampleOnlineProvider::GetAudioInputDevices_Implementation() const
{
	return {
		FALKOnlineOption(TEXT("default"), LOCTEXT("DeviceDefault", "System Default")),
		FALKOnlineOption(TEXT("headset"), LOCTEXT("DeviceHeadsetMic", "Headset Microphone")),
		FALKOnlineOption(TEXT("desk"), LOCTEXT("DeviceDeskMic", "Desk Microphone"))
	};
}

TArray<FALKOnlineOption> UALKSampleOnlineProvider::GetAudioOutputDevices_Implementation() const
{
	return {
		FALKOnlineOption(TEXT("default"), LOCTEXT("DeviceDefault", "System Default")),
		FALKOnlineOption(TEXT("headset"), LOCTEXT("DeviceHeadset", "Headset")),
		FALKOnlineOption(TEXT("speakers"), LOCTEXT("DeviceSpeakers", "Speakers"))
	};
}

TArray<FALKOnlineOption> UALKSampleOnlineProvider::GetRegions_Implementation() const
{
	// The settings row puts "Automatic" (an empty region Id) in front of these.
	return {
		FALKOnlineOption(TEXT("americas"), LOCTEXT("RegionAmericas", "Americas")),
		FALKOnlineOption(TEXT("europe"), LOCTEXT("RegionEurope", "Europe")),
		FALKOnlineOption(TEXT("asia"), LOCTEXT("RegionAsia", "Asia Pacific"))
	};
}

#undef LOCTEXT_NAMESPACE
