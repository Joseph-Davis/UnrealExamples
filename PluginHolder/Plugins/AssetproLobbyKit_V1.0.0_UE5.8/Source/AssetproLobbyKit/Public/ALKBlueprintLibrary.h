// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ALKTypes.h"
#include "ALKBlueprintLibrary.generated.h"

class UALKMenuSubsystem;
class UALKSettingsSubsystem;
class UALKHUDSubsystem;
class UALKSaveSubsystem;
class UALKTheme;
class UALKMenuConfig;
class UALKConfirmDialog;
class APlayerController;

/** Blueprint shortcuts for the Assetpro Lobby Kit. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The menu subsystem of a local player controller (defaults to the first local player). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Assetpro Menu Subsystem"))
	static UALKMenuSubsystem* GetMenuSubsystem(const UObject* WorldContextObject, APlayerController* PlayerController = nullptr);

	/** The HUD essentials (prompts, subtitles, notifications, chapter titles, saving) of a player, the first local one by default. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|HUD", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Assetpro HUD"))
	static UALKHUDSubsystem* GetHUD(const UObject* WorldContextObject, APlayerController* PlayerController = nullptr);

	/** Save and load slots: save, load, quick save, autosave, the list of saves and their events. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Assetpro Save Subsystem"))
	static UALKSaveSubsystem* GetSaveSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Assetpro Settings Subsystem"))
	static UALKSettingsSubsystem* GetSettingsSubsystem(const UObject* WorldContextObject);

	/** The theme in use (runtime override, project setting or built-in default). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Theme")
	static const UALKTheme* GetActiveTheme();

	/** Swap the menu theme at runtime; open menus restyle immediately. Pass None to return to the project setting. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Theme")
	static void SetRuntimeTheme(UALKTheme* NewTheme);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu")
	static const UALKMenuConfig* GetActiveMenuConfig();

	/** Shows a confirmation dialog and calls On Result with the player's choice. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "OnResult"))
	static UALKConfirmDialog* ShowConfirmationDialog(const UObject* WorldContextObject, APlayerController* PlayerController, FText Title, FText Message, FText ConfirmText, FText CancelText, bool bDestructive, const FALKDialogResultDelegate& OnResult);
};
