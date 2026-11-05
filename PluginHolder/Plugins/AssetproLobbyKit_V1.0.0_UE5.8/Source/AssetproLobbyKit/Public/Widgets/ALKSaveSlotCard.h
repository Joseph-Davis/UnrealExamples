// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ALKButton.h"
#include "Save/ALKSaveTypes.h"
#include "ALKSaveSlotCard.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UALKSaveSlotCard;

/**
 * One save on the Load Game and Save Game screens: its screenshot, a tag (QUICKSAVE, AUTOSAVE, MANUAL 03), the chapter,
 * the playtime and date, and the chapter number. As the "New Save" card it shows an empty slot.
 *
 * Designer widget names: Background, ThumbImage, ThumbEmpty, TagFrame, TagText, Label (chapter line), InfoText,
 * NumberText, AgoText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSaveSlotCard : public UALKButton
{
	GENERATED_BODY()

public:
	UALKSaveSlotCard(const FObjectInitializer& ObjectInitializer);

	/** Shows a save. Read-only cards (autosaves on the Save Game screen) are dimmed and cannot be picked. */
	void SetSave(const FALKSaveSlotInfo& InInfo, UTexture2D* InThumbnail, bool bInReadOnly);

	/** Shows the "New Save" card: FreeSlots of MaxSlots are free. */
	void SetNewSave(int32 FreeSlots, int32 MaxSlots);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	const FALKSaveSlotInfo& GetSaveInfo() const { return Info; }

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool IsNewSaveCard() const { return bNewSave; }

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool IsReadOnly() const { return bReadOnly; }

	/** "Quicksave", "Autosave 2", "Manual 03". */
	static FText GetSlotLabel(const FALKSaveSlotInfo& Info);

	/** "04:12:33". */
	static FText FormatPlaytime(float Seconds);

	/** "2026.09.26 22:47" in local time. */
	static FText FormatDate(const FDateTime& UtcTime);

	/** "00:14 ago", "Yesterday", "3 days ago". */
	static FText FormatAge(const FDateTime& UtcTime);

	/** The chapter line: "Chapter 2 - Dockside", the level's name without a chapter. */
	static FText GetChapterLine(const FALKSaveSlotInfo& Info);

	/** Fired when the card gets keyboard / gamepad / mouse focus. */
	TMulticastDelegate<void(UALKSaveSlotCard*)> OnCardFocused;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ThumbImage;

	/** Shown instead of the screenshot on the New Save card and for saves without one. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ThumbEmpty;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TagFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TagText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InfoText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AgoText;

private:
	void RefreshContent();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Thumbnail;

	FALKSaveSlotInfo Info;
	bool bNewSave = false;
	bool bReadOnly = false;
	int32 FreeSlotCount = 0;
	int32 MaxSlotCount = 0;
};
