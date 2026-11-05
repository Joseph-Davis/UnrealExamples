// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKChapterSelectScreen.generated.h"

class UImage;
class UTextBlock;
class USizeBox;
class UALKButton;
class UALKChapterStill;
class UALKChapterStrip;

/**
 * Chapter select: one chapter at a time, as a large still under a strip with a bar per chapter. Left / Right (keys,
 * d-pad, stick, mouse wheel, the arrows beside the still) browse; Accept or a click on the still starts the chapter.
 * Locked chapters can be browsed but not started: their still is dimmed under a padlock and their name hidden. The
 * screen opens on the chapter it showed last, or on the furthest chapter the player has reached.
 *
 * Designer widget names: BackgroundImage, TitleText, Strip, Still, StillSize, NumberText, NameText, PreviousButton,
 * NextButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKChapterSelectScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKChapterSelectScreen(const FObjectInitializer& ObjectInitializer);

	/** Shows the chapter at this index of the menu config's Chapters (locked ones too). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Chapters")
	void ShowChapterAt(int32 Index);

	/** Shows a chapter by Id. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Chapters")
	void ShowChapter(FName ChapterId);

	/** Shows the previous (-1) or next (+1) chapter. False at either end. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Chapters")
	bool BrowseChapters(int32 Direction);

	/** Starts the shown chapter; a locked one plays the Locked sound instead. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Chapters")
	void PlayShownChapter();

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Chapters")
	int32 GetShownChapterIndex() const { return ShownIndex; }

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Chapters")
	FName GetShownChapterId() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Chapters")
	bool IsShownChapterLocked() const;

	/** Re-reads which chapters are unlocked. Runs when the screen is shown and when progress changes. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Chapters")
	void RefreshLocks();

	/** The still: the button that has focus while the screen is open. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Chapters")
	UALKChapterStill* GetStill() const { return Still; }

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UALKChapterStrip> Strip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UALKChapterStill> Still;

	/** Sized from the theme's Chapter Still Width, 16:9. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> StillSize;

	/** "Chapter 3" (menu config: Number Chapters). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NumberText;

	/** The chapter's name, or "Locked". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> PreviousButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> NextButton;

private:
	void UpdateShownChapter(bool bAnimate, int32 Direction);
	void HandleProgressChanged();
	void HandleSegmentClicked(int32 Index);
	int32 GetDefaultIndex() const;

	UFUNCTION()
	UWidget* HandleStillNavigation(EUINavigation Direction);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ShownTexture;

	FDelegateHandle ProgressChangedHandle;
	int32 ShownIndex = INDEX_NONE;
	/** The chapter shown when the screen closed; it opens there again. */
	int32 RememberedIndex = INDEX_NONE;
	bool bFocusPending = false;
	float SlideElapsed = -1.f;
	float SlideDirection = 0.f;
	float LastLayoutWidth = 0.f;
	float LastLayoutHeight = 0.f;
};
