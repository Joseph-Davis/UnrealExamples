// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Widgets/ALKUserWidget.h"
#include "ALKQueueWidget.generated.h"

class UALKButton;
class UALKIconWidget;
class UALKSpinner;
class UBorder;
class UTextBlock;

/**
 * The quick match queue as a small bar: a spinner, "Searching for a match", the playlist with the backend's estimate, the
 * time spent in the queue and a Cancel button. Once a match is found it reads "Match found", then "Joining...". It shows
 * nothing while no quick match is running and finds the online subsystem by itself, so any screen can place one.
 *
 * The Cancel button is for the mouse; give gamepad and keyboard players a key that calls Cancel Search.
 *
 * Designer widget names: Panel, Spinner, DoneIcon, TitleText, DetailText, ElapsedText, CancelButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKQueueWidget : public UALKUserWidget
{
	GENERATED_BODY()

public:
	/** Reads the quick match again and updates the bar. It already does so whenever the quick match moves on. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Matchmaking")
	void RefreshQueue();

	/** Leaves the quick match queue: what the Cancel button does. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Matchmaking")
	void CancelSearch();

	/** True while the bar is on screen: a quick match is searching, was found or is joining. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Matchmaking")
	bool IsQueueShown() const { return bQueueShown; }

	/** Seconds as minutes and seconds: "1:10", or "01:10" with Pad Minutes. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Matchmaking")
	static FText FormatMinutesSeconds(float Seconds, bool bPadMinutes);

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** The bar's rounded box. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	/** Turns while searching and joining. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UALKSpinner> Spinner;

	/** A tick shown instead of the spinner once a match is found. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> DoneIcon;

	/** "Searching for a match", "Match found" or "Joining...". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The playlist's name and, while searching, the estimated wait. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailText;

	/** The time spent in the queue, "00:42". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ElapsedText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CancelButton;

private:
	void HandleMatchmakingChanged(EALKMatchmakingState NewState);
	void UpdateElapsed(bool bForce);

	FDelegateHandle MatchmakingHandle;
	bool bQueueShown = false;
	bool bSearching = false;
	int32 ShownSeconds = INDEX_NONE;
};
