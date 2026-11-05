// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "Save/ALKSaveTypes.h"
#include "ALKSaveSlotScreen.generated.h"

class UImage;
class UBorder;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UALKFrame;
class UALKSaveSlotCard;
class UALKValueBar;
class UALKSaveSubsystem;

/**
 * The Load Game and Save Game screens: the saves as cards on the left (newest first), the focused save's screenshot and
 * details on the right.
 *
 * - Load: Accept loads the save (asking first while a game is being played, menu config Confirm Load In Game).
 * - Save: a "New Save" card while a manual slot is free, manual saves to overwrite (asks first); autosaves and the quick
 *   save are shown dimmed, read only.
 * - Delete (Del, gamepad Y) deletes the focused save after asking.
 *
 * Open it with Open Load Game / Open Save Game (Assetpro Menu Subsystem) or the menu actions of the same names.
 *
 * Designer widget names: BackgroundImage, SystemLabelText, TitleText, ListPanel, SlotList, EmptyText, DetailPanel,
 * DetailHeader, PreviewSize, PreviewImage, ChapterLabelText, ChapterNameText, DetailRows, ProgressBar, ProgressText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSaveSlotScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKSaveSlotScreen(const FObjectInitializer& ObjectInitializer);

	/** Save Game (true) or Load Game (false). The menu subsystem sets it as it opens the screen. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	void SetSaveMode(bool bInSaveMode);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	bool IsSaveMode() const { return bSaveMode; }

	/** Rebuilds the cards from the saves (runs when the screen opens and when the saves change). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	void RefreshSlots();

	/** Loads or saves the focused card, as Accept does. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	void ActivateFocusedCard();

	/** Deletes the focused save after asking. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Save")
	void DeleteFocusedSave();

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Save")
	TArray<UALKSaveSlotCard*> GetCards() const;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ListPanel;

	/** The cards go in here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> SlotList;

	/** "No saved games". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DetailPanel;

	/** "SAVE DATA // QUICKSAVE". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailHeader;

	/** Sized 16:9 from the detail panel's width. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> PreviewSize;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UImage> PreviewImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChapterLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChapterNameText;

	/** Playtime, saved, difficulty and summary rows. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> DetailRows;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UALKValueBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Save", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

private:
	UALKSaveSubsystem* GetSaveSubsystem() const;
	UALKSaveSlotCard* MakeCard();
	void HandleCardFocused(UALKSaveSlotCard* Card);
	void HandleCardClicked(UALKSaveSlotCard* Card);
	void ShowDetails(const UALKSaveSlotCard* Card);
	void SetDetailRow(int32 Row, const FText& Name, const FText& Value);
	void StyleDetailRows(const UALKTheme& Theme);
	void UpdateTitles();

	UFUNCTION()
	void HandleSavesChanged();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKSaveSlotCard>> Cards;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PreviewTexture;

	TWeakObjectPtr<UALKSaveSlotCard> FocusedCard;
	/** The save focused when the list is rebuilt keeps focus (after saving over it, it comes to the top). */
	FString RefocusSlot;
	bool bSaveMode = false;
	bool bFocusPending = false;
	float LastPanelWidth = 0.f;
	float LastScreenHeight = 0.f;
};
