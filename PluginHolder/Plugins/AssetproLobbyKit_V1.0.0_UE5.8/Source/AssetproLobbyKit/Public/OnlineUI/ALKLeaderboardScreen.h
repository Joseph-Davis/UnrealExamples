// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineTypes.h"
#include "Screens/ALKScreen.h"
#include "ALKLeaderboardScreen.generated.h"

class UALKButton;
class UALKScoreRow;
class UALKFrame;
class UALKSpinner;
class UBorder;
class UHorizontalBox;
class UImage;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

/**
 * The leaderboards: a tab per leaderboard of the Online Config (one "Global" tab when it lists none), and the rows the
 * online provider returns for it: rank, picture, name and the board's value. The local player's row is outlined, and
 * repeated in a line under the list when it is far down. Picking a row opens that player's profile.
 *
 * Open it with Open Leaderboards on the Online Flow subsystem (it can start on a given board).
 *
 * Designer widget names: BackgroundImage, TitleText, TabBox (Horizontal Box), ListPanel, RankHeader, PlayerHeader,
 * ValueHeader, RowList (Scroll Box), Spinner, StatusIcon, StatusText, PinnedBox (Vertical Box).
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKLeaderboardScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKLeaderboardScreen(const FObjectInitializer& ObjectInitializer);

	/** Shows a leaderboard by its place in the tab row and asks the online provider for its rows. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Leaderboards")
	void ShowBoard(int32 BoardIndex);

	/** The tab to the left (wraps round). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Leaderboards")
	void ShowPreviousBoard();

	/** The tab to the right (wraps round). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Leaderboards")
	void ShowNextBoard();

	/** Asks for the shown leaderboard's rows again. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Leaderboards")
	void RefreshBoard();

	/** The Id of the leaderboard that is shown. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Leaderboards")
	FName GetCurrentBoardId() const;

	/** How many rows are asked for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Leaderboards", meta = (ClampMin = "1", ClampMax = "200"))
	int32 MaxRows = 50;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The leaderboard tabs are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> TabBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ListPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RankHeader;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerHeader;

	/** The board's Value Header ("Wins", "Best time"). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueHeader;

	/** The rows are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> RowList;

	/** Turns while the rows load. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UALKSpinner> Spinner;

	/** The icon over the status line once loading is over (made by FALKWidgetBuilder::MakeIconDisc). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> StatusIcon;

	/** "Loading", or why there are no rows. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** The local player's row again, under the list, when it is far down. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Leaderboards", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PinnedBox;

private:
	void RebuildTabs();
	void HandleLeaderboardReceived(FName BoardId, const TArray<FALKLeaderboardRow>& InRows);
	void FillRows();
	UALKScoreRow* MakeRow(const FALKLeaderboardRow& Row, const UALKTheme& Theme);
	void HandleRowClicked(UALKScoreRow* Row);
	void ShowStatus(const FText& Text, bool bSpinning);
	void StyleScrollBar(const UALKTheme& Theme);
	bool IsTopScreen() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> TabButtons;

	TArray<FALKLeaderboardDefinition> Boards;
	TArray<FALKLeaderboardRow> Rows;
	TArray<TWeakObjectPtr<UALKScoreRow>> RowWidgets;
	FDelegateHandle ReceivedHandle;
	int32 ActiveBoard = 0;
	bool bLoading = false;
	float LoadingTime = 0.f;
	bool bFocusPending = false;
};
