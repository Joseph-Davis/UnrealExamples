// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "ALKGameSession.generated.h"

/**
 * The game session of the ALK game modes. The engine's own class starts and ends the online session by itself when a
 * match starts and ends; here that is left to the online provider (through the online subsystem), so a backend of your
 * own sees it too and the session is never started twice. Clients are still told to start and end theirs.
 */
UCLASS()
class ASSETPROLOBBYKIT_API AALKGameSession : public AGameSession
{
	GENERATED_BODY()

public:
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
};
