// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ALKSaveTypes.generated.h"

/** What kind of slot a save is in. */
UENUM(BlueprintType)
enum class EALKSaveSlotType : uint8
{
	/** Saved by the player on the Save Game screen. */
	Manual,
	/** Saved by the game (a checkpoint, Autosave). Autosaves take turns through Autosave Slots. */
	Auto,
	/** Quick Save (F5 by default). */
	Quick
};

/**
 * What the slot screens show about a save, kept in a small index file beside the saves so the screens never load the
 * game's own save data just to draw a card.
 */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKSaveSlotInfo
{
	GENERATED_BODY()

	/** The save file's slot name (Save Slot Prefix + "_Manual_03", "_Auto_1", "_Quick"). */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	EALKSaveSlotType Type = EALKSaveSlotType::Manual;

	/** Manual slots: 1..Manual Slots. Autosaves: 1..Autosave Slots. Quick saves: 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	int32 Number = 1;

	/** The chapter being played when the game was saved (menu config Chapters), or None. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FName ChapterId;

	/** The chapter's name when it was saved, for a chapter that is no longer in the menu config. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FText ChapterName;

	/** The level the save loads (long package name, /Game/Maps/MyLevel). */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FString LevelName;

	/** A line of your own, shown on the card's details (the current objective, the area...). See Set Save Summary. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FText Summary;

	/** Seconds played, not counting paused time and menus. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	float PlaytimeSeconds = 0.f;

	/** When it was saved (UTC). */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	FDateTime SavedAt;

	/** The Difficulty setting when it was saved. */
	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	int32 Difficulty = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Save Slot")
	bool bHasThumbnail = false;

	bool IsValid() const { return !SlotName.IsEmpty(); }
};

/** One save's screenshot (JPEG), kept in the index. */
USTRUCT()
struct ASSETPROLOBBYKIT_API FALKSaveThumbnail
{
	GENERATED_BODY()

	UPROPERTY()
	FString SlotName;

	UPROPERTY()
	TArray<uint8> Jpeg;
};
