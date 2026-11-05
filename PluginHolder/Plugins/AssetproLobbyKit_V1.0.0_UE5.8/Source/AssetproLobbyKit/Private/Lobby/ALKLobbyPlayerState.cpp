// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLobbyPlayerState)

AALKLobbyPlayerState::AALKLobbyPlayerState()
{
	// Every change made through the setters below forces a net update, so the slow default update rate is enough.
}

void AALKLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AALKLobbyPlayerState, UserId);
	DOREPLIFETIME(AALKLobbyPlayerState, Slot);
	DOREPLIFETIME(AALKLobbyPlayerState, TeamId);
	DOREPLIFETIME(AALKLobbyPlayerState, bReady);
	DOREPLIFETIME(AALKLobbyPlayerState, bHost);
	DOREPLIFETIME(AALKLobbyPlayerState, bPlaceholder);
	DOREPLIFETIME(AALKLobbyPlayerState, LoadoutId);
	DOREPLIFETIME(AALKLobbyPlayerState, ColorIndex);
	DOREPLIFETIME(AALKLobbyPlayerState, PlayerLevel);
	DOREPLIFETIME(AALKLobbyPlayerState, bSpectating);
	DOREPLIFETIME(AALKLobbyPlayerState, VotedMapId);
	DOREPLIFETIME(AALKLobbyPlayerState, bRematchVote);
	DOREPLIFETIME(AALKLobbyPlayerState, PartyId);
	DOREPLIFETIME(AALKLobbyPlayerState, Kills);
	DOREPLIFETIME(AALKLobbyPlayerState, Deaths);
	DOREPLIFETIME(AALKLobbyPlayerState, Assists);
	DOREPLIFETIME(AALKLobbyPlayerState, CustomStats);
}

void AALKLobbyPlayerState::BeginPlay()
{
	Super::BeginPlay();
	NoteAsRecentPlayer();
}

void AALKLobbyPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	NoteAsRecentPlayer();
	NotifyChanged();
}

void AALKLobbyPlayerState::OnRep_LobbyState()
{
	NoteAsRecentPlayer();
	NotifyChanged();
}

void AALKLobbyPlayerState::NotifyChanged()
{
	OnPlayerStateChanged.Broadcast();
	OnPlayerStateChangedNative.Broadcast();
	const UWorld* World = GetWorld();
	if (AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr)
	{
		GameState->NotifyPlayersChanged();
	}
}

void AALKLobbyPlayerState::MarkChanged()
{
	if (HasAuthority())
	{
		ForceNetUpdate();
		// The server (a listen host has menus too) hears about it without waiting for replication.
		NotifyChanged();
	}
}

void AALKLobbyPlayerState::NoteAsRecentPlayer()
{
	const FString Name = GetPlayerName();
	if (bPlaceholder || UserId.IsEmpty() || Name.IsEmpty() || Name == NotedName || IsLocalPlayerState() || GetNetMode() == NM_Standalone || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		NotedName = Name;
		Online->NoteRecentPlayer(UserId, Name);
	}
}

// ---------------------------------------------------------------- Reading

int32 AALKLobbyPlayerState::GetStableIndex() const
{
	return Slot >= 0 ? Slot : FMath::Abs(GetPlayerId()) % 100;
}

bool AALKLobbyPlayerState::IsLocalPlayerState() const
{
	const APlayerController* Controller = GetPlayerController();
	return Controller && Controller->IsLocalController();
}

float AALKLobbyPlayerState::GetCustomStat(FName StatId) const
{
	const FALKCustomStat* Found = CustomStats.FindByPredicate([StatId](const FALKCustomStat& Each) { return Each.Id == StatId; });
	return Found ? Found->Value : 0.f;
}

FALKScoreLine AALKLobbyPlayerState::GetScoreLine() const
{
	FALKScoreLine Line(TeamId, GetScore(), Kills, Deaths, Assists);
	Line.bSpectating = bSpectating;
	return Line;
}

// ---------------------------------------------------------------- Match numbers

void AALKLobbyPlayerState::AddKill(int32 Amount)
{
	if (HasAuthority() && Amount != 0)
	{
		Kills = FMath::Max(0, Kills + Amount);
		MarkChanged();
	}
}

void AALKLobbyPlayerState::AddDeath(int32 Amount)
{
	if (HasAuthority() && Amount != 0)
	{
		Deaths = FMath::Max(0, Deaths + Amount);
		MarkChanged();
	}
}

void AALKLobbyPlayerState::AddAssist(int32 Amount)
{
	if (HasAuthority() && Amount != 0)
	{
		Assists = FMath::Max(0, Assists + Amount);
		MarkChanged();
	}
}

void AALKLobbyPlayerState::AddScore(float Amount)
{
	if (HasAuthority() && !FMath::IsNearlyZero(Amount))
	{
		SetScore(GetScore() + Amount);
		MarkChanged();
	}
}

void AALKLobbyPlayerState::AddCustomStat(FName StatId, float Amount)
{
	SetCustomStat(StatId, GetCustomStat(StatId) + Amount);
}

void AALKLobbyPlayerState::SetCustomStat(FName StatId, float Value)
{
	if (!HasAuthority() || StatId.IsNone())
	{
		return;
	}
	if (FALKCustomStat* Found = CustomStats.FindByPredicate([StatId](const FALKCustomStat& Each) { return Each.Id == StatId; }))
	{
		if (Found->Value == Value)
		{
			return;
		}
		Found->Value = Value;
	}
	else
	{
		// A small list by design: every entry replicates to every player.
		if (CustomStats.Num() >= 16)
		{
			return;
		}
		CustomStats.Emplace(StatId, Value);
	}
	MarkChanged();
}

void AALKLobbyPlayerState::ResetMatchStats()
{
	if (!HasAuthority())
	{
		return;
	}
	Kills = 0;
	Deaths = 0;
	Assists = 0;
	CustomStats.Reset();
	bRematchVote = false;
	SetScore(0.f);
	MarkChanged();
}

void AALKLobbyPlayerState::SetPlayerLevel(int32 NewLevel)
{
	if (HasAuthority() && PlayerLevel != NewLevel)
	{
		PlayerLevel = FMath::Max(1, NewLevel);
		MarkChanged();
	}
}

// ---------------------------------------------------------------- Lobby values

void AALKLobbyPlayerState::SetUserId(const FString& NewUserId)
{
	if (HasAuthority() && UserId != NewUserId)
	{
		UserId = NewUserId;
		NoteAsRecentPlayer();
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetSlot(int32 NewSlot)
{
	if (HasAuthority() && Slot != NewSlot)
	{
		Slot = NewSlot;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetTeamId(int32 NewTeamId)
{
	if (HasAuthority() && TeamId != NewTeamId)
	{
		TeamId = NewTeamId;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetReady(bool bNewReady)
{
	if (HasAuthority() && bReady != bNewReady)
	{
		bReady = bNewReady;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetHost(bool bNewHost)
{
	if (HasAuthority() && bHost != bNewHost)
	{
		bHost = bNewHost;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetPlaceholder(bool bNewPlaceholder)
{
	if (HasAuthority() && bPlaceholder != bNewPlaceholder)
	{
		bPlaceholder = bNewPlaceholder;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetLoadoutId(FName NewLoadoutId)
{
	if (HasAuthority() && LoadoutId != NewLoadoutId)
	{
		LoadoutId = NewLoadoutId;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetColorIndex(int32 NewColorIndex)
{
	if (HasAuthority() && ColorIndex != NewColorIndex)
	{
		ColorIndex = NewColorIndex;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetSpectating(bool bNewSpectating)
{
	if (HasAuthority() && bSpectating != bNewSpectating)
	{
		bSpectating = bNewSpectating;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetVotedMapId(FName NewMapId)
{
	if (HasAuthority() && VotedMapId != NewMapId)
	{
		VotedMapId = NewMapId;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetRematchVote(bool bNewVote)
{
	if (HasAuthority() && bRematchVote != bNewVote)
	{
		bRematchVote = bNewVote;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetPartyId(const FString& NewPartyId)
{
	if (HasAuthority() && PartyId != NewPartyId)
	{
		PartyId = NewPartyId;
		MarkChanged();
	}
}

void AALKLobbyPlayerState::SetDisplayedPing(int32 PingMs)
{
	if (HasAuthority())
	{
		// The replicated ping is kept in steps of 4 ms.
		SetCompressedPing(static_cast<uint8>(FMath::Clamp(PingMs / 4, 0, 255)));
		ExactPing = static_cast<float>(FMath::Max(0, PingMs));
	}
}

// ---------------------------------------------------------------- Travel and reconnect

void AALKLobbyPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// Seamless travel: this is the old state, Player State the one made for the new level.
	if (AALKLobbyPlayerState* Other = Cast<AALKLobbyPlayerState>(PlayerState))
	{
		Other->UserId = UserId;
		Other->Slot = Slot;
		Other->TeamId = TeamId;
		Other->bReady = bReady;
		Other->bHost = bHost;
		Other->bPlaceholder = bPlaceholder;
		Other->LoadoutId = LoadoutId;
		Other->ColorIndex = ColorIndex;
		Other->PlayerLevel = PlayerLevel;
		Other->bSpectating = bSpectating;
		Other->VotedMapId = VotedMapId;
		Other->bRematchVote = bRematchVote;
		Other->PartyId = PartyId;
		Other->Kills = Kills;
		Other->Deaths = Deaths;
		Other->Assists = Assists;
		Other->CustomStats = CustomStats;
	}
}

void AALKLobbyPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	// A reconnect: this is the state the player had before they dropped (kept by the game mode, with their slot, team
	// and match numbers); Player State is the fresh one their new login made, and only lends its name and id (the base
	// class takes those). The kept values stay; the player just is not ready and has not voted.
	bReady = false;
	bRematchVote = false;
	VotedMapId = NAME_None;
}

void AALKLobbyPlayerState::Reset()
{
	Super::Reset();
	if (HasAuthority())
	{
		Kills = 0;
		Deaths = 0;
		Assists = 0;
		CustomStats.Reset();
		MarkChanged();
	}
}
