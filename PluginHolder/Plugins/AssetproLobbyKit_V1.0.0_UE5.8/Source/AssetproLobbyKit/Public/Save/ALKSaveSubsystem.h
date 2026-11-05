// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/ALKSaveTypes.h"
#include "ALKSaveSubsystem.generated.h"

class UALKSaveGame;
class UALKSaveIndex;
class UALKMenuSubsystem;
class UTexture2D;
class UWorld;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnGatherSaveData, UALKSaveGame*, SaveGame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnSaveLoaded, UALKSaveGame*, SaveGame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FALKOnSaveFinished, const FALKSaveSlotInfo&, Info, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnSavesChanged);

/**
 * Save and load slots for the menus (menu config: Save Slots).
 *
 * - Manual slots (the Save Game screen), autosaves that take turns through a few slots (Autosave, Checkpoint Volumes)
 *   and a quick save (Quick Save / Quick Load, F5 / F9 on ALKPlayerController).
 * - Every save stores the level, the player's position and view, the chapter and the playtime, with a screenshot for
 *   the slot screens; loading opens the level and puts the player back.
 * - Your data: select a child of ALKSaveGame as the menu config's Save Game Class, fill it in On Gather Save Data and
 *   read it in On Save Loaded.
 *
 * Get it with Get Game Instance Subsystem (Assetpro Save Subsystem) or Get Assetpro Save Subsystem.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------------------------------------------------------------- Saves

	/** Every save, newest first. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	TArray<FALKSaveSlotInfo> GetSaves() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool HasAnySave() const;

	/** The newest save (manual, auto or quick). False when there is none. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool GetNewestSave(FALKSaveSlotInfo& OutInfo) const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool FindSave(const FString& SlotName, FALKSaveSlotInfo& OutInfo) const;

	/** The lowest free manual slot number, or -1 when every slot is taken. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	int32 GetFreeManualSlot() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	int32 GetManualSaveCount() const;

	/** The save's screenshot, or null. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	UTexture2D* GetThumbnail(const FString& SlotName);

	/** The view the next save from the pause menu will use (taken as the pause menu opened), or null. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	UTexture2D* GetCurrentViewThumbnail();

	/** A save's slot name: Save Slot Prefix + "_Manual_03", "_Auto_1" or "_Quick". */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	static FString MakeSlotName(EALKSaveSlotType Type, int32 Number);

	// ---------------------------------------------------------------- Saving

	/** Saves to a manual slot (1..Manual Slots), replacing what is there. False when saving is not allowed now. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	bool SaveToManualSlot(int32 Number);

	/** Saves to the lowest free manual slot. False when none is free or saving is not allowed now. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	bool SaveToNewSlot();

	/** Saves to the next autosave slot (replacing the oldest). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	bool Autosave();

	/** Saves to the quick save slot and says so on the HUD (or why it could not). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	bool QuickSave();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	bool DeleteSave(const FString& SlotName);

	/** True from a save's start until it is written. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool IsSaving() const { return PendingSave != nullptr; }

	// ---------------------------------------------------------------- Loading

	/** Opens the save's level and puts the player back where they were, then fires On Save Loaded. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	bool LoadSave(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	bool LoadNewestSave();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	bool QuickLoad();

	/** The save this game session was loaded from (null after New Game). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	UALKSaveGame* GetLoadedSave() const { return LoadedSave; }

	// ---------------------------------------------------------------- When saving is allowed

	/**
	 * Allows or blocks saving (Save Game, Quick Save, autosaves), for example during combat or a cutscene. The reason is
	 * shown when the player tries to save.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	void SetSavingAllowed(bool bAllowed, FText Reason);

	/** Saving is on, not blocked, a game is being played (not the main menu) and nothing is being saved or loaded. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool IsSavingAllowed() const;

	/** Why saving is not allowed now, or empty. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	FText GetSavingBlockedReason() const;

	/** A gameplay level is running (not the main menu level). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool IsInGame() const;

	// ---------------------------------------------------------------- The game being played

	/** Starts counting a new game: playtime from zero, at this chapter. New Game and Start Chapter call it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	void BeginNewGame(FName ChapterId);

	/** The chapter saves record. Start Chapter and New Game set it; set it yourself when the story moves on. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	void SetCurrentChapter(FName ChapterId);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	FName GetCurrentChapter() const { return CurrentChapter; }

	/** A line shown with the next saves on the slot screens (the current objective, the area...). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	void SetSaveSummary(const FText& Summary);

	/** Seconds played in this game, not counting pauses and menus. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	float GetPlaytime() const { return PlaytimeSeconds; }

	/** Takes the next save's screenshot now (the pause menu calls it as it opens, before the game behind it dims). */
	void CaptureThumbnail();

	// ---------------------------------------------------------------- Events

	/** A save is about to be written: fill in your own variables (your Save Game Class). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Save")
	FALKOnGatherSaveData OnGatherSaveData;

	/** A loaded save's level is running and the player is back in place: apply your own variables. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Save")
	FALKOnSaveLoaded OnSaveLoaded;

	/** A save was written (or failed). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Save")
	FALKOnSaveFinished OnSaveFinished;

	/** The list of saves changed (saved, deleted). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Save")
	FALKOnSavesChanged OnSavesChanged;

private:
	bool StartSave(EALKSaveSlotType Type, int32 Number);
	void WritePendingSave();
	void HandleSaveWritten(const FString& SlotName, int32 UserIndex, bool bSuccess);
	void HandleScreenshot(int32 Width, int32 Height, const TArray<FColor>& Colors);
	bool TickSession(float DeltaTime);
	void HandlePostLoadMap(UWorld* World);
	bool TickApplyLoad(float DeltaTime);
	void LoadIndex();
	void SaveIndex();
	void RebuildIndexFromFiles();
	void NotifySavesChanged();
	void ShowHUDNotice(const FText& Title, const FText& Detail, bool bSaved) const;
	void SetSavingIndicator(bool bShown) const;
	UWorld* GetGameWorld() const;
	APlayerController* GetFirstPlayerController() const;
	UALKMenuSubsystem* GetMenuSubsystem() const;
	bool IsEnabled() const;

	UPROPERTY(Transient)
	TObjectPtr<UALKSaveIndex> Index;

	/** Saved while its screenshot is taken, then written. */
	UPROPERTY(Transient)
	TObjectPtr<UALKSaveGame> PendingSave;

	/** Loaded, waiting for its level to start. */
	UPROPERTY(Transient)
	TObjectPtr<UALKSaveGame> PendingLoad;

	UPROPERTY(Transient)
	TObjectPtr<UALKSaveGame> LoadedSave;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> ThumbnailCache;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentViewTexture;
	double CurrentViewTextureTime = -1.0;

	/** The next save's screenshot (JPEG) and when it was taken. */
	TArray<uint8> PendingThumbnail;
	double PendingThumbnailTime = -1.0;
	/** The screenshot written with the save in progress. */
	TArray<uint8> WrittenThumbnail;
	/** When the save in progress started; a screenshot taken after it belongs to it. */
	double SaveStartTime = 0.0;
	/** The save in progress uses the screenshot taken as the pause menu opened. */
	bool bUsePauseThumbnail = false;
	bool bCaptureRequested = false;
	bool bWriteAfterCapture = false;
	double CaptureRequestTime = 0.0;
	FDelegateHandle ScreenshotHandle;

	FName CurrentChapter;
	FText SaveSummary;
	float PlaytimeSeconds = 0.f;
	bool bSavingBlocked = false;
	FText SavingBlockedReason;
	double LoadStartTime = 0.0;

	FTSTicker::FDelegateHandle SessionTicker;
	FTSTicker::FDelegateHandle ApplyLoadTicker;
	FDelegateHandle PostLoadMapHandle;
};
