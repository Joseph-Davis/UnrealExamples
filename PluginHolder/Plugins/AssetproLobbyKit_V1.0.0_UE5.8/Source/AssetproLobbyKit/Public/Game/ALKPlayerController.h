// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ALKPlayerController.generated.h"

/**
 * Optional player controller that wires the menu system up with no Blueprint work:
 * - Escape / gamepad Start toggle the pause menu (works while paused).
 * - F5 / F9 quick save and quick load (menu config: Quick Save Key, Quick Load Key).
 * - Saved settings (FOV, vibration) are applied when the controller starts.
 * - Can show the main menu when the level starts (see ALKMainMenuPlayerController).
 *
 * Using your own controller instead? Call Toggle Pause Menu on the Assetpro Menu Subsystem from your pause input.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AALKPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void TogglePauseMenu();

	/** Show the press start screen / main menu when this controller begins play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu")
	bool bShowMainMenuOnBeginPlay = false;

	/** Bind Escape and gamepad Start to the pause menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu")
	bool bEnablePauseMenuInput = true;

	/** Bind the menu config's Quick Save Key and Quick Load Key (F5 / F9). Off in main menu levels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu")
	bool bEnableQuickSaveInput = true;

	/** Saves to the quick save slot (Assetpro Save Subsystem), unless a menu is open. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void QuickSave();

	/** Loads the quick save, unless a menu is open. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void QuickLoad();

	/**
	 * Removes the engine's development debug commands on these keys (F5 shader complexity, F9 screenshot and the like),
	 * so the keys only do what the game binds. Shipping builds have no debug commands.
	 */
	void FreeDebugKeys(const TArray<FKey>& Keys);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
};

/** Controller for main menu levels: shows the startup menu and never opens the pause menu. */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKMainMenuPlayerController : public AALKPlayerController
{
	GENERATED_BODY()

public:
	AALKMainMenuPlayerController();
};
