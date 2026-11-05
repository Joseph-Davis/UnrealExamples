// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKAdminScreen.generated.h"

class AALKLobbyGameState;
class AALKLobbyPlayerState;
class UALKButton;
class UALKPlayerNetComponent;
class UBorder;
class UHorizontalBox;
class UImage;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

/**
 * The host's tools, on three tabs:
 * - Players: everyone in the game with their team and ping, and for each one Move to the next team, Kick and Ban
 *   (kick and ban ask first);
 * - Bans: the host's ban list, with Unban;
 * - Match: in the lobby Change settings, the map, Start match, Force start and Cancel countdown; in a match Pause /
 *   Resume and End match and return to lobby.
 *
 * Only the host can use it: everyone else sees one line and Back. Every action goes to the server through the local
 * player's ALK Player Net Component. Open it with Open Admin Panel on the Online Flow subsystem.
 *
 * Designer widget names: BackgroundImage, TitleText, TabBox (Horizontal Box), BodyPanel, PlayersList (Scroll Box),
 * BansList (Scroll Box), MatchBox (Vertical Box), MatchStatusText, MatchHintText, ChangeSettingsButton, MapButton,
 * StartButton, ForceStartButton, CancelCountdownButton, PauseButton, EndMatchButton, StatusText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKAdminScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKAdminScreen(const FObjectInitializer& ObjectInitializer);

	/** Shows a tab: 0 Players, 1 Bans, 2 Match. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Tools")
	void ShowTab(int32 TabIndex);

	/** The tab to the left (wraps round). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Tools")
	void ShowPreviousTab();

	/** The tab to the right (wraps round). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Tools")
	void ShowNextTab();

	/** Reads the game again now (the screen also does so by itself when something changes). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Tools")
	void RefreshAll();

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The three tab buttons are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> TabBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BodyPanel;

	/** The player lines are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> PlayersList;

	/** The ban list's lines are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> BansList;

	/** The Match tab: a status line and the buttons. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> MatchBox;

	/** "In the lobby", "Match running", "Paused"... */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchStatusText;

	/** Why the match cannot start yet. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchHintText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ChangeSettingsButton;

	/** Cycles through the maps of the mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> MapButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> StartButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ForceStartButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CancelCountdownButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> PauseButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> EndMatchButton;

	/** "Nobody is banned", "Only the host can use these tools". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Tools", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	/** A player line's parts that change without the line being made again. */
	struct FPlayerLine
	{
		TWeakObjectPtr<AALKLobbyPlayerState> Player;
		TWeakObjectPtr<UTextBlock> DetailText;
	};

	AALKLobbyGameState* ResolveGameState();
	void UnbindGameState();
	void MarkDirty();
	UALKPlayerNetComponent* GetNetComponent() const;
	bool IsLocalHost() const;
	bool IsTopScreen() const;

	void RebuildTabs();
	void RefreshPlayers(AALKLobbyGameState* GameState, const UALKTheme& Theme);
	void RefreshBans(const UALKTheme& Theme);
	void RefreshMatch(AALKLobbyGameState* GameState);
	void StyleScrollBar(UScrollBox* List, const UALKTheme& Theme) const;
	UWidget* FindFirstButton(UWidget* Within) const;

	void AskKick(int32 PlayerId, FString ShownName, bool bBan);
	void MovePlayer(int32 PlayerId, int32 TeamId);
	void Unban(FString BannedUserId);
	void ChangeSettings();
	void CycleMap();
	void StartMatch(bool bForce);
	void CancelCountdown();
	void TogglePause();
	void EndMatch();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> TabButtons;

	TArray<FPlayerLine> PlayerLines;
	TWeakObjectPtr<AALKLobbyGameState> BoundGameState;
	FDelegateHandle ListsHandle;
	FString PlayersSignature;
	FString BansSignature;
	int32 ActiveTab = 0;
	bool bDirty = true;
	bool bRestoreFocus = false;
	float SecondTimer = 0.f;
};
