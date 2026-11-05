// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ALKUserWidget.h"
#include "HUD/ALKHUDSubsystem.h"
#include "ALKHUDLayer.generated.h"

class UALKFrame;
class UALKIconWidget;
class UALKRule;
class UALKValueBar;
class UALKSpinner;
class UALKTitleText;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;
class UWidget;
enum class ECommonInputType : uint8;

/**
 * Draws the HUD essentials for one player (see UALKHUDSubsystem, which creates it). The default layout is built in code
 * from the theme. A Blueprint child with its own designer layout can bind: PromptBox, PromptMarker, PromptSubject,
 * PromptFrame, PromptKeyText, PromptAction, PromptHoldBar, SubtitleBox, NotificationBox, ChapterCard, ChapterNumber,
 * ChapterName, SavingBox and SavingText (all optional; lines and notifications are created into the boxes).
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKHUDLayer : public UALKUserWidget
{
	GENERATED_BODY()

public:
	void ShowPrompt(const FALKInteractionPrompt& Prompt);
	void HidePrompt();
	void SetPromptProgress(float Progress);
	bool IsPromptShown() const { return bPromptShown; }

	void AddSubtitle(const FText& Speaker, const FText& Line, float Duration);
	void ClearSubtitles();

	void AddNotification(const FText& Title, const FText& Detail, EALKIcon Icon, float Duration);

	/** Shows the chapter card after Delay seconds. */
	void ShowChapterCard(int32 ChapterNumber, const FText& ChapterName, float Delay = 0.f);

	void ShowSaving(float MinimumSeconds, bool bUntilHidden);
	void HideSaving();

	void SetHidden(bool bInHidden) { bHidden = bInHidden; }

	/** Moves the notifications down by this much, to leave the corner above them to something else (a kill feed). */
	void SetNotificationDrop(float InDrop);

	/** The key the prompt shows now ("E", "X"...), for tests. */
	FText GetPromptKeyText() const;

	/** Shown subtitle lines and notifications, for tests. */
	int32 GetSubtitleCount() const { return Subtitles.Num(); }
	int32 GetNotificationCount() const { return Notifications.Num(); }
	bool IsChapterCardShown() const { return CardTime >= 0.f; }
	/** How many chapter cards were shown since the layer was created, for tests. */
	int32 GetChapterCardCount() const { return ChapterCardCount; }
	/** Font size of the newest subtitle line (the Subtitle Size setting scales it), 0 when none is shown. */
	float GetNewestSubtitleFontSize() const;
	bool IsSavingShown() const;
	float GetShownOpacity() const { return CurrentOpacity; }

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PromptBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> PromptMarker;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptSubject;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> PromptFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> PromptKeyCap;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptKeyText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptAction;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UALKValueBar> PromptHoldBar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> SubtitleBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> NotificationBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ChapterCard;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChapterNumber;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChapterName;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SavingBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SavingText;

private:
	struct FTimedItem
	{
		TWeakObjectPtr<UWidget> Widget;
		float Age = 0.f;
		float Duration = 3.f;
	};

	void RefreshPromptKey();
	void UpdatePrompt(const FGeometry& MyGeometry);
	void StyleSubtitle(UWidget* Line, const UALKTheme& Theme) const;
	bool IsUsingGamepad() const;
	float GetSettingOpacity() const;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UALKRule> ChapterRule;

	UPROPERTY(Transient)
	TObjectPtr<UALKSpinner> SavingSpinner;

	FALKInteractionPrompt Prompt;
	TWeakObjectPtr<AActor> PromptTarget;
	bool bPromptShown = false;
	float PromptAge = 0.f;
	float PromptKeyRefresh = 0.f;
	/** Where the prompt points this frame (canvas space) and whether it has a target on screen. */
	FVector2D PromptAnchor = FVector2D::ZeroVector;
	bool bPromptAnchored = false;

	TArray<FTimedItem> Subtitles;
	TArray<FTimedItem> Notifications;

	float CardTime = -1.f;
	int32 ChapterCardCount = 0;
	bool bCardVisible = false;
	float CardDelay = 0.f;

	float SavingMinimum = 0.f;
	bool bSavingRequested = false;

	bool bHidden = false;
	float NotificationDrop = 0.f;
	float CurrentOpacity = 0.f;
	FDelegateHandle InputMethodChangedHandle;
};
