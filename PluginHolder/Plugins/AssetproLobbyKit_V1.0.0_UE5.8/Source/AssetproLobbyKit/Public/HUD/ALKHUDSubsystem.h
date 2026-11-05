// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputCoreTypes.h"
#include "Widgets/ALKDecor.h"
#include "ALKHUDSubsystem.generated.h"

class AActor;
class UInputAction;
class UALKHUDLayer;
class UWorld;

/** An interaction prompt such as "[E] OPEN" (see UALKHUDSubsystem::ShowInteractionPrompt). */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKInteractionPrompt
{
	GENERATED_BODY()

	/**
	 * The input the player presses. The prompt shows the key it is bound to, following the player's rebinding and the
	 * input method: the keyboard key for mouse and keyboard, the controller button for gamepads.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	TObjectPtr<UInputAction> Action;

	/** Shown for mouse and keyboard when Action is empty or has no keyboard key. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FKey KeyboardKey = EKeys::E;

	/** Shown for gamepads when Action is empty or has no gamepad button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FKey GamepadKey = EKeys::Gamepad_FaceButton_Left;

	/** What pressing it does: "Open", "Talk", "Pick Up". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FText ActionText;

	/** Optional small line over the prompt naming what it is on ("Maintenance door"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FText SubjectText;

	/** The prompt sits beside this actor, joined to it by a thin line. Empty shows it in the lower middle of the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	TObjectPtr<AActor> Target;

	/** Offset from the target's location, in world units (up is +Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FVector WorldOffset = FVector::ZeroVector;

	/** The input is held: Set Interaction Progress fills a bar under the prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	bool bHold = false;
};

/**
 * The HUD essentials, per player, styled by the menu's theme: an interaction prompt that follows the input method,
 * subtitles, notifications, chapter title cards and a saving indicator. Get it with "Get Assetpro HUD" (or
 * LocalPlayer->GetSubsystem<UALKHUDSubsystem>()). Everything hides while a menu is open and follows the HUD Opacity,
 * Subtitles and Reduce Effects settings. There is no health, ammo, crosshair or map: those belong to your game.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKHUDSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Shows an interaction prompt (one at a time: a new one replaces the old). It fades in. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void ShowInteractionPrompt(const FALKInteractionPrompt& Prompt);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void HideInteractionPrompt();

	/** Fills the prompt's hold bar (0..1) for prompts with Hold. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void SetInteractionProgress(float Progress);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|HUD")
	bool IsInteractionPromptShown() const;

	/**
	 * Shows a subtitle line at the bottom of the screen: the speaker in the accent colour, then the line. Duration 0
	 * picks one from the line's length. Does nothing while the Subtitles setting is off; size and background follow the
	 * Subtitle Size and Subtitle Background settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD", meta = (AdvancedDisplay = "Duration"))
	void ShowSubtitle(const FText& Speaker, const FText& Line, float Duration = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void ClearSubtitles();

	/** Shows a notification in the top right corner ("CHECKPOINT / Progress saved"). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD", meta = (AdvancedDisplay = "Duration"))
	void ShowNotification(const FText& Title, const FText& Detail, EALKIcon Icon = EALKIcon::Info, float Duration = 3.5f);

	/**
	 * Moves the HUD's notifications down by this many units, so something of yours can have the top right corner (the
	 * match overlay's feed does). 0 puts them back.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void SetNotificationDrop(float Drop);

	/** The same, always drawn above menus (a lobby, the server browser): for notices while a menu is open. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD", meta = (AdvancedDisplay = "Duration"))
	void ShowMenuNotification(const FText& Title, const FText& Detail, EALKIcon Icon = EALKIcon::Info, float Duration = 3.5f);

	/** The "Checkpoint / Progress saved" notification, translated. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void ShowCheckpointNotification();

	/** Shows a chapter's number and name as a title card that fades in. Starting a chapter does this for you. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void ShowChapterTitle(int32 ChapterNumber, const FText& ChapterName);

	/** True while a chapter title card is shown, or waits for its chapter's level to load. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|HUD")
	bool IsChapterTitleActive() const;

	/**
	 * Shows the saving indicator for MinimumSeconds. With Until Hidden it stays after that until Hide Saving Indicator
	 * (for saves that take a while).
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void ShowSavingIndicator(float MinimumSeconds = 1.5f, bool bUntilHidden = false);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void HideSavingIndicator();

	/** Hides the whole HUD (cutscenes, photo mode) until called again with false. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void SetHUDHidden(bool bHidden);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|HUD")
	bool IsHUDHidden() const { return bHUDHidden; }

	/** The HUD layer for the player's current world, created on first use (Menu Config: HUD Layer Class). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	UALKHUDLayer* GetHUDLayer();

private:
	UFUNCTION()
	void HandleChapterStarted(FName ChapterId);

	void HandleChapterUnlocked(FName ChapterId);
	void HandlePostLoadMap(UWorld* World);

	/** The layer notifications use while a menu covers the screen: it sits above the menus and shows nothing else. */
	UALKHUDLayer* GetOverlayLayer();

	UPROPERTY(Transient)
	TObjectPtr<UALKHUDLayer> Layer;

	UPROPERTY(Transient)
	TObjectPtr<UALKHUDLayer> OverlayLayer;

	bool bHUDHidden = false;
	float NotificationDrop = 0.f;

	/** A chapter card waiting for the chapter's level to load. */
	int32 PendingChapterNumber = 0;
	FText PendingChapterName;

	FDelegateHandle UnlockedHandle;
	FDelegateHandle PostLoadMapHandle;
};
