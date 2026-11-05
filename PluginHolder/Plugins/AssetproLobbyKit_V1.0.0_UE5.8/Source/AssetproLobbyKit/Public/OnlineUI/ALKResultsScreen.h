// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKResultsScreen.generated.h"

class AALKLobbyGameState;
class AALKLobbyPlayerState;
class UALKButton;
class UALKScoreboard;
class UBorder;
class UHorizontalBox;
class UImage;
class UTextBlock;

/**
 * The results screen. The Online Flow opens it when a match ends. It shows who won ("Victory", "Defeat", "Draw", or
 * your place in a free for all), the final score, a podium with the three best players (the most valuable player gets
 * a star), the final scoreboard, the countdown back to the lobby and the rematch vote, with buttons to return to the
 * lobby (host), open the leaderboards or leave.
 *
 * It reads the ALK game state of the current level. Opened without a match it says so and can be closed with Back.
 *
 * Designer widget names: BackgroundImage, HeaderBand (a border round the header: it takes the winning team's colour),
 * TitleText, SubtitleText, ScoreBox (Horizontal Box), PodiumPanel, PodiumTitle,
 * PodiumBox (Horizontal Box), Scoreboard, EmptyText, CountdownText, RematchText, RematchButton, ReturnButton,
 * LeaderboardsButton, LeaveButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKResultsScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKResultsScreen(const FObjectInitializer& ObjectInitializer);

	/** Reads the game state again now (the screen also does so by itself when something changes). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Results")
	void RefreshResults();

	/** Votes for a rematch, or takes the vote back. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Results")
	void ToggleRematchVote();

	/** Host: brings everyone back to the lobby now instead of waiting for the countdown. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Results")
	void ReturnToLobbyNow();

	/** Opens the leaderboards. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Results")
	void OpenLeaderboards();

	/** Asks "Leave the game?" and leaves when confirmed. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Results")
	void LeaveGame();

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool OnBackRequested_Implementation() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	/** "Victory", "Defeat", "Draw" or "You placed 2". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** Mode and map. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	/** The final team scores are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ScoreBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PodiumPanel;

	/** The band the header sits on: it takes the winning team's colour once a team has won (see the theme's MakeBandFrame). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> HeaderBand;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PodiumTitle;

	/** The three podium places are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PodiumBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UALKScoreboard> Scoreboard;

	/** Shown when there is no match to read. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	/** "Returning to lobby in 12". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountdownText;

	/** "3 of 8 want a rematch". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RematchText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> RematchButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ReturnButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> LeaderboardsButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Results", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> LeaveButton;

private:
	AALKLobbyGameState* ResolveGameState();
	void UnbindGameState();
	void MarkDirty();
	void RebuildScoreAndPodium(const AALKLobbyGameState& GameState, const TArray<AALKLobbyPlayerState*>& Ranked, const UALKTheme& Theme);
	void AddPodiumPlace(int32 Place, const AALKLobbyPlayerState* Player, bool bMostValuable, const UALKTheme& Theme);
	void UpdateCountdown();

	TWeakObjectPtr<AALKLobbyGameState> BoundGameState;
	/** What the score blocks and the podium were built for. */
	FString BuiltSignature;
	bool bDirty = true;
	bool bHasResults = false;
	int32 ShownCountdown = MIN_int32;
	/** The outcome picks the title's colour: 1 won, -1 lost, 0 neither. */
	int32 Outcome = 0;
};
