// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ALKLoadingScreenSubsystem.generated.h"

class UTexture2D;

/**
 * Shows a themed loading screen (background, loading indicator and a random tip) while levels load.
 * Uses the engine's movie player, so it appears in standalone and packaged games (not in Play In Editor).
 * If your game sets up its own movie player loading screen, that one is used instead.
 * Toggle it in Project Settings > Plugins > Assetpro Lobby Kit.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** How many times this session the themed loading screen was handed to the movie player. */
	static int32 GetShownCount();

private:
	/** Called by the movie player right before it starts, when no loading screen has been set up yet. */
	void HandlePrepareLoadingScreen();

	/** Hands the themed loading screen to the movie player. Returns false when loading screens are turned off. */
	bool SetupLoadingScreen(bool bReuseTip, float MinimumDisplayTime);

	/** Keeps the loading screen up while a game started from the editor scans its content at the end of startup. */
	void HandleEngineLoopInitComplete();

	FDelegateHandle PrepareLoadingScreenHandle;
	FDelegateHandle EngineLoopInitCompleteHandle;

	/** The tip on the current loading screen, so a screen that is shown again keeps it. */
	FText CurrentTip;

	/** The grain and vignette textures drawn by the loading screen. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> OverlayTextures;
};
