// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "CoreMinimal.h"

// Console commands for trying the multiplayer flow from one machine (development builds only):
//   ALK.Online.Host [ModeId] [MapId]     host a local network lobby with the last / default settings
//   ALK.Online.Find                      search for games
//   ALK.Online.QuickMatch [PlaylistId]   start a quick match (the first playlist when none is named)
//   ALK.Online.Accept [0|1]              accept (1, the default) or decline (0) the match a quick match found
//   ALK.Online.Leave                     leave the game, back to the main menu
//   ALK.Online.Error <Error>             raise a connection error the normal way (TimedOut, ServerFull, Kicked...)
//   ALK.Online.Checks                    run the lobby logic checks and print the result
//   ALK.Lobby.Fill <Count>               add placeholder players (0 removes them)
//   ALK.Lobby.Ready [0|1]                ready / unready the local player
//   ALK.Lobby.PlaceholdersReady [0|1]    ready / unready every placeholder
//   ALK.Lobby.Start [force]              start the match as the host
//   ALK.Lobby.Chat <Text...>             a chat line from a random placeholder
//   ALK.Match.Simulate [0|1]             placeholders score every few seconds
//   ALK.Match.End                        end the match now
//   ALK.Match.ReturnToLobby              bring everyone back to the lobby

#if !UE_BUILD_SHIPPING

#include "Lobby/ALKLobbyGameMode.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKMatchGameMode.h"
#include "Lobby/ALKNetGameModeBase.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace ALKOnlineConsole
{
	static UALKOnlineSubsystem* GetOnline(UWorld* World)
	{
		UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(World);
		if (!Online)
		{
			UE_LOG(LogALK, Warning, TEXT("This command needs a running game."));
		}
		return Online;
	}

	template <typename GameModeType>
	static GameModeType* GetGameMode(UWorld* World, const TCHAR* What)
	{
		GameModeType* GameMode = World ? World->GetAuthGameMode<GameModeType>() : nullptr;
		if (!GameMode)
		{
			UE_LOG(LogALK, Warning, TEXT("This command needs %s (on the host)."), What);
		}
		return GameMode;
	}

	static bool ReadSwitch(const TArray<FString>& Args, bool bDefault)
	{
		if (Args.Num() == 0)
		{
			return bDefault;
		}
		return Args[0] == TEXT("1") || Args[0].Equals(TEXT("true"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("on"), ESearchCase::IgnoreCase);
	}

	static void Host(const TArray<FString>& Args, UWorld* World)
	{
		UALKOnlineSubsystem* Online = GetOnline(World);
		if (!Online)
		{
			return;
		}
		FALKHostSettings Settings = Online->GetLastHostSettings();
		Settings.bLAN = true;
		Settings.bPrivate = false;
		Settings.Password.Reset();
		if (Args.Num() > 0)
		{
			Settings.ModeId = FName(*Args[0]);
		}
		if (Args.Num() > 1)
		{
			Settings.MapId = FName(*Args[1]);
		}
		const bool bStarted = Online->HostGame(Settings);
		UE_LOG(LogALK, Display, TEXT("ALK.Online.Host: %s"), bStarted ? TEXT("hosting...") : TEXT("could not start (busy, online turned off, or no lobby level)"));
	}

	static void Find(const TArray<FString>& Args, UWorld* World)
	{
		if (UALKOnlineSubsystem* Online = GetOnline(World))
		{
			Online->FindGames(FALKSessionFilter());
			UE_LOG(LogALK, Display, TEXT("ALK.Online.Find: searching..."));
		}
	}

	static void QuickMatch(const TArray<FString>& Args, UWorld* World)
	{
		UALKOnlineSubsystem* Online = GetOnline(World);
		if (!Online)
		{
			return;
		}
		const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
		const FName Playlist = Args.Num() > 0 ? FName(*Args[0]) : (Config->Playlists.Num() > 0 ? Config->Playlists[0].Id : NAME_None);
		const bool bStarted = Online->StartQuickMatch(Playlist);
		UE_LOG(LogALK, Display, TEXT("ALK.Online.QuickMatch %s: %s"), *Playlist.ToString(), bStarted ? TEXT("in the queue...") : TEXT("could not start"));
	}

	static void Accept(const TArray<FString>& Args, UWorld* World)
	{
		if (UALKOnlineSubsystem* Online = GetOnline(World))
		{
			Online->RespondToMatch(ReadSwitch(Args, true));
		}
	}

	static void Leave(const TArray<FString>& Args, UWorld* World)
	{
		if (UALKOnlineSubsystem* Online = GetOnline(World))
		{
			Online->CancelQuickMatch();
			Online->LeaveGame();
		}
	}

	static void Error(const TArray<FString>& Args, UWorld* World)
	{
		UALKOnlineSubsystem* Online = GetOnline(World);
		const UEnum* Enum = StaticEnum<EALKConnectionError>();
		if (!Online || !Enum)
		{
			return;
		}
		const int64 Value = Args.Num() > 0 ? Enum->GetValueByNameString(Args[0]) : INDEX_NONE;
		if (Value == INDEX_NONE || Value == static_cast<int64>(EALKConnectionError::None))
		{
			FString Names;
			for (int32 Index = 1; Index < Enum->NumEnums() - 1; ++Index)
			{
				Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Enum->GetNameStringByIndex(Index);
			}
			UE_LOG(LogALK, Display, TEXT("ALK.Online.Error <Error>: one of %s"), *Names);
			return;
		}
		Online->SimulateConnectionError(static_cast<EALKConnectionError>(Value));
	}

	static void Checks(const TArray<FString>& Args, UWorld* World)
	{
		TArray<FString> Failures;
		const int32 Count = UALKLobbyStatics::RunLogicChecks(Failures);
		for (const FString& Failure : Failures)
		{
			UE_LOG(LogALK, Error, TEXT("%s"), *Failure);
		}
		UE_LOG(LogALK, Display, TEXT("ALK.Online.Checks: %d checks, %d failed"), Count, Failures.Num());
	}

	static void Fill(const TArray<FString>& Args, UWorld* World)
	{
		AALKNetGameModeBase* GameMode = GetGameMode<AALKNetGameModeBase>(World, TEXT("an ALK lobby or match"));
		if (!GameMode)
		{
			return;
		}
		const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 3;
		if (Count <= 0)
		{
			GameMode->RemovePlaceholderPlayers();
			UE_LOG(LogALK, Display, TEXT("ALK.Lobby.Fill: placeholders removed"));
			return;
		}
		const int32 Added = GameMode->AddPlaceholderPlayers(Count);
		UE_LOG(LogALK, Display, TEXT("ALK.Lobby.Fill: %d placeholder players added"), Added);
	}

	static void Ready(const TArray<FString>& Args, UWorld* World)
	{
		if (UALKPlayerNetComponent* NetComponent = UALKLobbyStatics::GetLocalNetComponent(World))
		{
			NetComponent->ServerSetReady(ReadSwitch(Args, true));
		}
		else
		{
			UE_LOG(LogALK, Warning, TEXT("ALK.Lobby.Ready: the local player has no ALK Player Net Component (not in a lobby?)."));
		}
	}

	static void PlaceholdersReady(const TArray<FString>& Args, UWorld* World)
	{
		if (AALKNetGameModeBase* GameMode = GetGameMode<AALKNetGameModeBase>(World, TEXT("an ALK lobby or match")))
		{
			GameMode->SetPlaceholdersReady(ReadSwitch(Args, true));
		}
	}

	static void Start(const TArray<FString>& Args, UWorld* World)
	{
		const bool bForce = Args.Num() > 0 && (Args[0].Equals(TEXT("force"), ESearchCase::IgnoreCase) || Args[0] == TEXT("1"));
		if (UALKPlayerNetComponent* NetComponent = UALKLobbyStatics::GetLocalNetComponent(World))
		{
			NetComponent->ServerStartMatch(bForce);
		}
		else
		{
			UE_LOG(LogALK, Warning, TEXT("ALK.Lobby.Start: the local player has no ALK Player Net Component (not in a lobby?)."));
		}
	}

	static void Chat(const TArray<FString>& Args, UWorld* World)
	{
		const FString Text = FString::Join(Args, TEXT(" "));
		if (Text.IsEmpty())
		{
			UE_LOG(LogALK, Display, TEXT("ALK.Lobby.Chat <Text...>"));
			return;
		}
		AALKNetGameModeBase* GameMode = World ? World->GetAuthGameMode<AALKNetGameModeBase>() : nullptr;
		if (GameMode && GameMode->SendPlaceholderChat(Text))
		{
			return;
		}
		// No placeholder to speak: the local player says it.
		UALKOnlineSubsystem* Online = GetOnline(World);
		if (!Online || !Online->SendChat(EALKChatChannel::All, Text, FString()))
		{
			UE_LOG(LogALK, Warning, TEXT("ALK.Lobby.Chat: nobody to say it (not in a lobby or match?)."));
		}
	}

	static void Simulate(const TArray<FString>& Args, UWorld* World)
	{
		if (AALKMatchGameMode* GameMode = GetGameMode<AALKMatchGameMode>(World, TEXT("an ALK match")))
		{
			const bool bEnabled = ReadSwitch(Args, true);
			GameMode->SetMatchSimulation(bEnabled);
			UE_LOG(LogALK, Display, TEXT("ALK.Match.Simulate: %s (%d placeholder players)"), bEnabled ? TEXT("on") : TEXT("off"), GameMode->GetPlaceholderCount());
		}
	}

	static void End(const TArray<FString>& Args, UWorld* World)
	{
		if (AALKMatchGameMode* GameMode = GetGameMode<AALKMatchGameMode>(World, TEXT("an ALK match")))
		{
			GameMode->FinishMatch(INDEX_NONE);
		}
	}

	static void ReturnToLobby(const TArray<FString>& Args, UWorld* World)
	{
		if (AALKMatchGameMode* GameMode = GetGameMode<AALKMatchGameMode>(World, TEXT("an ALK match")))
		{
			GameMode->ReturnToLobby();
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GALKOnlineHostCommand(TEXT("ALK.Online.Host"),
	TEXT("Hosts a local network lobby with the last / default settings. Optional: ModeId, MapId."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Host));

static FAutoConsoleCommandWithWorldAndArgs GALKOnlineFindCommand(TEXT("ALK.Online.Find"),
	TEXT("Searches for games (the results go to On Sessions Found)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Find));

static FAutoConsoleCommandWithWorldAndArgs GALKOnlineQuickMatchCommand(TEXT("ALK.Online.QuickMatch"),
	TEXT("Starts a quick match. Optional: PlaylistId (the first playlist otherwise)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::QuickMatch));

static FAutoConsoleCommandWithWorldAndArgs GALKOnlineAcceptCommand(TEXT("ALK.Online.Accept"),
	TEXT("Accepts (1, the default) or declines (0) the match a quick match found."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Accept));

static FAutoConsoleCommandWithWorldAndArgs GALKOnlineLeaveCommand(TEXT("ALK.Online.Leave"),
	TEXT("Leaves the game and returns to the main menu."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Leave));

static FAutoConsoleCommandWithWorldAndArgs GALKOnlineErrorCommand(TEXT("ALK.Online.Error"),
	TEXT("Raises a connection error the normal way: TimedOut, ServerFull, WrongVersion, WrongPassword, HostLeft, Kicked, Banned, NetworkError, SessionNotFound..."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Error));

static FAutoConsoleCommandWithWorldAndArgs GALKOnlineChecksCommand(TEXT("ALK.Online.Checks"),
	TEXT("Runs the lobby logic checks and prints how many passed."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Checks));

static FAutoConsoleCommandWithWorldAndArgs GALKLobbyFillCommand(TEXT("ALK.Lobby.Fill"),
	TEXT("Adds placeholder players to the lobby or match (host). Count; 0 removes them."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Fill));

static FAutoConsoleCommandWithWorldAndArgs GALKLobbyReadyCommand(TEXT("ALK.Lobby.Ready"),
	TEXT("Readies (1, the default) or unreadies (0) the local player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Ready));

static FAutoConsoleCommandWithWorldAndArgs GALKLobbyPlaceholdersReadyCommand(TEXT("ALK.Lobby.PlaceholdersReady"),
	TEXT("Readies (1, the default) or unreadies (0) every placeholder player (host)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::PlaceholdersReady));

static FAutoConsoleCommandWithWorldAndArgs GALKLobbyStartCommand(TEXT("ALK.Lobby.Start"),
	TEXT("Starts the match as the host. Add 'force' to start before everyone is ready."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Start));

static FAutoConsoleCommandWithWorldAndArgs GALKLobbyChatCommand(TEXT("ALK.Lobby.Chat"),
	TEXT("A chat line from a random placeholder player (from the local player when there is none)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Chat));

static FAutoConsoleCommandWithWorldAndArgs GALKMatchSimulateCommand(TEXT("ALK.Match.Simulate"),
	TEXT("Turns the match simulation on (1, the default) or off (0): placeholder players score every few seconds (host)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::Simulate));

static FAutoConsoleCommandWithWorldAndArgs GALKMatchEndCommand(TEXT("ALK.Match.End"),
	TEXT("Ends the match now; the scores decide the winner (host)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::End));

static FAutoConsoleCommandWithWorldAndArgs GALKMatchReturnToLobbyCommand(TEXT("ALK.Match.ReturnToLobby"),
	TEXT("Brings everyone back to the lobby (host)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKOnlineConsole::ReturnToLobby));

#endif // !UE_BUILD_SHIPPING
