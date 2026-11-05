// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/ALKPlayerController.h"
#include "ALKLobbyPlayerController.generated.h"

class UALKPlayerNetComponent;

/**
 * The player controller of the ALK lobby and match game modes: ALK Player Controller (pause menu on Escape / Start)
 * with the ALK Player Net Component already on it, so ready, team, chat, votes and the host's actions work with no
 * Blueprint work. Quick save and quick load are off: a network game is not saved to a slot.
 *
 * Have a controller class of your own? Add an ALK Player Net Component to it instead of using this class.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKLobbyPlayerController : public AALKPlayerController
{
	GENERATED_BODY()

public:
	AALKLobbyPlayerController();

	/** How this player talks to the server: ready, team, chat, votes and the host's actions. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	UALKPlayerNetComponent* GetNetComponent() const { return NetComponent; }

private:
	/** This player's line to the server. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UALKPlayerNetComponent> NetComponent;
};
