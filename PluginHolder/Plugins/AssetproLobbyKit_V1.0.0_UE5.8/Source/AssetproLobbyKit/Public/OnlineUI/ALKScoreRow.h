// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ALKButton.h"
#include "ALKScoreRow.generated.h"

class UALKAvatar;
class UALKFrame;
class UALKIconWidget;
class UHorizontalBox;
class UTextBlock;
class UALKScoreRow;

/**
 * One player line of a scoreboard or a leaderboard: rank, picture, name, small marks (You, host crown, MVP star, muted,
 * talking, "Left"), a number per column and the ping. The scoreboard and the leaderboards screen fill it; it knows
 * nothing about the game itself, so you can use it for any list of players.
 *
 * Designer widget names: Background, RankText, Avatar, Label (the name), YouChip, HostIcon, MvpIcon, MutedIcon,
 * TalkIcon, LeftText, CellBox (a Horizontal Box the number cells are added to), PingText, PingIcon.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKScoreRow : public UALKButton
{
	GENERATED_BODY()

public:
	UALKScoreRow(const FObjectInitializer& ObjectInitializer);

	/** Width of the ping column (number and bars), shared with the table headers so they line up. */
	static constexpr float PingColumnWidth = 96.f;
	/** Padding left and right of a row's content, shared with the table headers. */
	static constexpr float SidePadding = 16.f;
	/** Height of a row. */
	static constexpr int32 RowHeight = 48;

	/** Makes one number cell per width, left to right. With Ping adds the ping number and bars at the end. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetColumnWidths(const TArray<float>& Widths, bool bWithPing);

	/** The rank in front (0 hides it), the name as it should be shown, and the player's user id (kept for whoever handles the click). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetIdentity(int32 Rank, const FString& ShownName, const FString& InUserId, bool bAnonymousAvatar);

	/** The texts of the number cells, left to right. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetValues(const TArray<FText>& Values);

	/** The ping in milliseconds: the bars always show, the number only with Show Number. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetPing(int32 PingMs, bool bShowNumber);

	/** The small marks beside the name. Local outlines the row in the accent colour and adds the "You" chip. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetMarks(bool bInLocal, bool bInHost, bool bInMVP, bool bInMuted, bool bInTalking, bool bInLeft);

	/** A colour for the rank number (gold, silver, bronze on a leaderboard). Alpha 0 uses the theme's muted text colour. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetRankColor(FLinearColor InColor);

	/** Which number cell is drawn bold (the score). -1 for none. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetBoldColumn(int32 ColumnIndex);

	/**
	 * On: the row takes focus and can be picked. Off: it only shows (the scoreboard while playing). Set it before the row
	 * is added to a screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Scoreboard")
	void SetInteractive(bool bInInteractive);

	/** The row can be focused and picked. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	bool IsInteractive() const { return bInteractive; }

	/** The user id given with Set Identity. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	const FString& GetUserId() const { return UserId; }

	/** The name given with Set Identity (already safe for streamer mode). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	const FString& GetShownName() const { return ShownPlayerName; }

	/** True for the local player's row. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Scoreboard")
	bool IsLocalRow() const { return bLocal; }

	/** Fired when the row gets keyboard / gamepad / mouse focus. */
	TMulticastDelegate<void(UALKScoreRow*)> OnRowFocused;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RankText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UALKAvatar> Avatar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> YouChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> HostIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> MvpIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> MutedIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> TalkIcon;

	/** "Left", for a player who disconnected. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LeftText;

	/** The number cells are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> CellBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PingText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Scoreboard", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> PingIcon;

private:
	void RefreshMarks();
	void RestyleNow();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CellTexts;

	FString UserId;
	FString ShownPlayerName;
	FLinearColor RankColor = FLinearColor::Transparent;
	int32 BoldColumn = INDEX_NONE;
	int32 ShownPing = -1;
	bool bPingNumber = true;
	bool bHasPing = false;
	bool bInteractive = true;
	bool bLocal = false;
	bool bHost = false;
	bool bMVP = false;
	bool bMuted = false;
	bool bTalking = false;
	bool bLeft = false;
};
