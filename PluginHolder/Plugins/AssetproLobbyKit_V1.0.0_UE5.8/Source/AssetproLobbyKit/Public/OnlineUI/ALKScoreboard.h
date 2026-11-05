// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Online/ALKOnlineConfig.h"
#include "Widgets/ALKUserWidget.h"
#include "ALKScoreboard.generated.h"

class AALKLobbyGameState;
class AALKLobbyPlayerState;
class UALKFrame;
class UALKScoreRow;
class UBorder;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

/**
 * The scoreboard: a panel with the mode, map, server name and clock, then one table per team (or a single table in a
 * free for all) with a line per player, sorted by score, and the spectators underneath. The columns are the Online
 * Config's Score Columns, plus the ping. It reads the ALK game state of the current level by itself and keeps itself up
 * to date.
 *
 * The match overlay shows it while the scoreboard key is held (not interactive: it never takes focus); the results
 * screen shows it interactive, where picking a player opens the player options.
 *
 * Designer widget names: Panel, ModeText, DetailText, ClockText, ClockLabel, TableList (Scroll Box), EmptyText,
 * SpectatorText, HintText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKScoreboard : public UALKUserWidget
{
	GENERATED_BODY()

public:
	UALKScoreboard(const FObjectInitializer& ObjectInitializer);

	/** On: the player lines take focus and picking one opens the player options. Off (the default): it only shows. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetInteractive(bool bInInteractive);

	/** Player lines can be focused and picked. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	bool IsInteractive() const { return bInteractive; }

	/** Reads the game state again now (it also does so by itself whenever something changes). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void Refresh();

	/** The first player line, or null: where focus goes when the scoreboard is interactive. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	UALKScoreRow* GetFirstRow() const;

	/** The local player's line, or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	UALKScoreRow* GetLocalRow() const;

	/** Seconds as a clock: "4:12", "0:07". Negative counts as 0. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	static FText FormatClock(float Seconds);

	/** A player's name as it may be shown: the real name, or "Player 3" for other players in streamer mode. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	static FString GetShownPlayerName(const AALKLobbyPlayerState* Player);

	/** The number columns of the scoreboard: the Online Config's Score Columns without the ping, or Score, Eliminations, Deaths, Assists when it lists none. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	static TArray<FALKScoreColumn> GetScoreColumns();

	/** Best first: score, then eliminations, fewer deaths, and the lower slot. */
	static void SortPlayersByScore(TArray<AALKLobbyPlayerState*>& Players);

	/** Show the top line with the mode, map, server name and clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Scoreboard")
	bool bShowHeader = true;

	/** Show the bottom line with the spectators and the key hint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Scoreboard")
	bool bShowFooter = true;

	/** The class of a player line. Empty uses ALK Score Row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Scoreboard")
	TSubclassOf<UALKScoreRow> RowClass;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeText;

	/** Map, server name and score limit. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClockLabel;

	/** The team tables are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> TableList;

	/** Shown when there is no match to read. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpectatorText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

private:
	/** One team's table: its header and its player lines. */
	struct FTable
	{
		int32 TeamId = INDEX_NONE;
		TWeakObjectPtr<UALKFrame> Header;
		TWeakObjectPtr<UTextBlock> NameText;
		TWeakObjectPtr<UTextBlock> ScoreText;
		TArray<TWeakObjectPtr<UTextBlock>> ColumnTexts;
		TArray<TWeakObjectPtr<UALKScoreRow>> Rows;
		FLinearColor Color = FLinearColor::Transparent;
		bool bHasTeam = false;
	};

	AALKLobbyGameState* ResolveGameState();
	void UnbindGameState();
	void MarkDirty();
	void HandlePlayerTalking(const FString& UserId, bool bTalking);
	void HandleRowClicked(UALKScoreRow* Row);
	void RebuildTables(const TArray<int32>& TeamIds, const TArray<int32>& RowCounts, const TArray<FALKScoreColumn>& Columns);
	void StyleTable(FTable& Table, const UALKTheme& Theme);
	void StyleScrollBar(const UALKTheme& Theme);
	void UpdateClock();

	TArray<FTable> Tables;
	/** What the tables were built for: rebuilt only when it changes, so focus stays put while scores change. */
	FString BuiltSignature;
	TWeakObjectPtr<AALKLobbyGameState> BoundGameState;
	TSet<FString> Talkers;
	FDelegateHandle TalkingHandle;
	FDelegateHandle ListsHandle;
	bool bInteractive = false;
	bool bDirty = true;
	float SecondTimer = 0.f;
	int32 ShownClockSeconds = MIN_int32;
};
