// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKGameSession.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKGameSession)

void AALKGameSession::HandleMatchHasStarted()
{
	// Not calling the engine's version: it would start the host's online session a second time.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (Controller && !Controller->IsLocalController())
		{
			Controller->ClientStartOnlineSession();
		}
	}
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->NotifyServerMatchStarted();
	}
}

void AALKGameSession::HandleMatchHasEnded()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (Controller && !Controller->IsLocalController())
		{
			Controller->ClientEndOnlineSession();
		}
	}
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->NotifyServerMatchEnded();
	}
}
