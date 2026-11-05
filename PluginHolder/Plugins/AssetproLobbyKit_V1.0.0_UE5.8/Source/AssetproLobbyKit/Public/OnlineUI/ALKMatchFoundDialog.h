// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Screens/ALKScreen.h"
#include "ALKMatchFoundDialog.generated.h"

class SALKCountdownRing;
class UALKButton;
class UALKFrame;
class UALKIconWidget;
class UALKThumbnail;
class UBorder;
class UHorizontalBox;
class UImage;
class UNativeWidgetHost;
class UTextBlock;

/**
 * The dialog a quick match shows when it found a game: a ring that counts the seconds left to answer, "Match found",
 * the mode, map and size, a dot per player (filled with a tick once they accepted), and Accept / Decline. Accepting
 * waits for the other players; Decline and Back leave the match. It closes by itself when the match starts, falls
 * through or times out. The Online Flow subsystem opens it; nothing has to be passed in.
 *
 * Designer widget names: DimImage, Panel, MapBand (the map's picture across the top of the panel, when the match has
 * one), RingHost (a Native Widget Host), SecondsText, TitleText, DetailText, DotBox, CountText, WaitingText,
 * AcceptButton, DeclineButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKMatchFoundDialog : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKMatchFoundDialog(const FObjectInitializer& ObjectInitializer);

	/** Accepts the match and waits for the other players (what the Accept button does). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Matchmaking")
	void AcceptMatch();

	/** Turns the match down, or leaves the queue after accepting, and closes the dialog (Decline and Back). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Matchmaking")
	void DeclineMatch();

	/** Most dots the dialog draws; a match that needs more players shows the count line only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Matchmaking", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaxDots = 12;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool OnBackRequested_Implementation() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Darkens the screen behind the dialog. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	/** The picture of the match's map, as a soft band across the top of the panel. Hidden when the map has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKThumbnail> MapBand;

	/** Holds the countdown ring, which is drawn in code. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UNativeWidgetHost> RingHost;

	/** The seconds left, in the middle of the ring. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SecondsText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** "Team Deathmatch - Arena - 4v4". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailText;

	/** Holds a dot per player. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> DotBox;

	/** "5 of 8 players accepted". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** "Waiting for the other players...", shown once this player accepted. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WaitingText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> AcceptButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Matchmaking", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> DeclineButton;

private:
	void HandleMatchmakingChanged(EALKMatchmakingState NewState);
	void RefreshMatch();
	void RefreshCountdown(bool bPlaySound);
	void StyleDots(const UALKTheme& Theme);
	bool IsMatchPending() const;

	/** The dots' circles and ticks, left to right. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKFrame>> DotFrames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKIconWidget>> DotTicks;

	TSharedPtr<SALKCountdownRing> Ring;
	FDelegateHandle MatchmakingHandle;
	/** The longest time left seen for this match: the ring is full at this many seconds. */
	float TotalSeconds = 0.f;
	int32 ShownSeconds = INDEX_NONE;
	int32 AcceptedCount = 0;
	bool bAccepted = false;
};
