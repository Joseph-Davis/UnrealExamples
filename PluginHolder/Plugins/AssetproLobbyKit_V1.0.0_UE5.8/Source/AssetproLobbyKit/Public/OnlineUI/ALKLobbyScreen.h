// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "Templates/SubclassOf.h"
#include "ALKLobbyScreen.generated.h"

class AALKLobbyGameState;
class UALKButton;
class UALKChatPanel;
class UALKFrame;
class UALKPlayerSlot;
class UALKThumbnail;
class UBorder;
class UHorizontalBox;
class UImage;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWrapBox;

/**
 * The lobby: who is here, on which team and who is ready; the chat; the match that is about to be played; and the
 * Ready up button. The host also gets Change settings, Start match (Force start while someone is not ready) and
 * Cancel during the countdown.
 *
 * It reads the lobby from the ALK Lobby Game State and its player states and sends what the player does through the
 * local ALK Player Net Component, so it works the same for the host and for everyone who joined. The Online Flow
 * subsystem opens it by itself when a lobby is entered. Back does not close it: it asks whether to leave the game.
 *
 * Keys: T chat, X / left face button switch team, Y / top face button invite, Tab / view button player options.
 *
 * Designer widget names: BackgroundImage, TitleText, ServerNameText, CodeChip, InfoChip, TeamsSize, TeamsScroll,
 * TeamsBox, SpectatorsText, ChatFrame, ChatPanel, MatchPanel, MatchPicture, ModeText, MapText, RuleChips,
 * SettingsButton, VoteButton, VoteText, ReadyPanel, ReadyCountText, ReadyDots, ReadyButton, StartButton, LeaveButton,
 * CountdownText, StatusText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKLobbyScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKLobbyScreen(const FObjectInitializer& ObjectInitializer);

	/** Readies the local player, or takes it back. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ToggleReady();

	/** Host: starts the match (forced when someone is not ready and that is allowed), or cancels a running countdown. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void StartOrCancelMatch();

	/** Asks to move the local player to the next team. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void SwitchTeam();

	/** Moves the local player's map vote to the next map (after the last one: no vote). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void VoteNextMap();

	/** Copies the lobby code others type to join. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void CopyLobbyCode();

	/** Opens the friends screen to invite someone. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void OpenInvite();

	/** Opens the options of the focused seat's player. With no seat focused it moves the focus to the players first. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void OpenPlayerOptions();

	/** Host: opens the host game screen to change the settings of this lobby. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void OpenChangeSettings();

	/** Asks whether to leave the game, and leaves when confirmed. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void LeaveLobby();

	/** Puts the keyboard in the chat's message box. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void FocusChat();

	/** Reads the whole lobby again. The screen does it by itself whenever the lobby changes. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void RefreshLobby();

	/** The widget made for each seat. Point it at your Blueprint child of ALK Player Slot to restyle the seats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	TSubclassOf<UALKPlayerSlot> SlotClass;

	/** How many of the match's rules the match card lists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (ClampMin = "0", ClampMax = "12"))
	int32 MaxRuleChips = 6;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual bool OnBackRequested_Implementation() override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ServerNameText;

	/** The lobby code; picking it copies the code. Hidden when the game has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CodeChip;

	/** "Public - Europe", "Private - LAN". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> InfoChip;

	/** Keeps the teams from taking the chat's room: past its height they scroll. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> TeamsSize;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> TeamsScroll;

	/** The team panels are made in here, two side by side. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> TeamsBox;

	/** "Watching: Nomad, Sable", shown while someone spectates. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpectatorsText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ChatFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKChatPanel> ChatPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MatchPanel;

	/** The map's picture, or placeholder art picked by the map's Id. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKThumbnail> MatchPicture;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeText;

	/** "Gridlock - 4v4". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapText;

	/** A chip per rule ("Time Limit: 10 min") is made in here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> RuleChips;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> SettingsButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> VoteButton;

	/** The map vote so far and the local player's vote. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> VoteText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ReadyPanel;

	/** "6 of 8 players ready". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyCountText;

	/** A dot per player, lit for the ready ones. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ReadyDots;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ReadyButton;

	/** Host: Start match, Force start, or Cancel during the countdown. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> StartButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> LeaveButton;

	/** "Starting in 3", large, during the countdown. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountdownText;

	/** Why the match cannot start yet. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	/** A team's panel: its header and its seats. The widgets live in the screen's tree. */
	struct FTeamPanel
	{
		/** The team's number, or -1 for the one panel of a free for all. */
		int32 TeamId = INDEX_NONE;
		TWeakObjectPtr<UALKFrame> Frame;
		TWeakObjectPtr<UALKFrame> Square;
		TWeakObjectPtr<UTextBlock> NameText;
		TWeakObjectPtr<UTextBlock> CountText;
		/** One column of seats for a team, two for a free for all. */
		TArray<TWeakObjectPtr<UVerticalBox>> Columns;
		TArray<TWeakObjectPtr<UALKPlayerSlot>> Seats;
	};

	AALKLobbyGameState* GetLobbyState() const;
	/** Follows the game state of the current level (it is not there yet right after a level change). */
	void BindGameState();
	void UnbindGameState();
	void MarkDirty() { bDirty = true; }

	void RefreshHeader(const AALKLobbyGameState* GameState);
	void RefreshMatchCard(const AALKLobbyGameState* GameState);
	void RefreshPlayers(const AALKLobbyGameState* GameState);
	void RefreshReadyPanel(const AALKLobbyGameState* GameState);
	/** Makes the team panels for a team count and a number of seats per team. */
	void RebuildTeams(int32 TeamCount, int32 Capacity);
	void StyleTeamPanel(const FTeamPanel& Panel, const UALKTheme& Theme) const;
	UALKPlayerSlot* AddSeat(FTeamPanel& Panel);
	/** Sets where Up and Down go from every seat. */
	void LinkSeats();
	void UpdateCountdown(const AALKLobbyGameState* GameState, bool bPlaySound);
	/** The blocker the Start button goes by: as if the host, who readies by pressing it, already were. */
	bool GetStartState(const AALKLobbyGameState* GameState, bool& bOutForce) const;
	bool IsTypingChat() const;

	void HandleSeatFocused(UALKPlayerSlot* Seat);
	void HandlePlayerTalking(const FString& UserId, bool bTalking);
	void HandleChatReleased();

	TArray<FTeamPanel> TeamPanels;
	int32 BuiltTeamCount = INDEX_NONE;
	int32 BuiltCapacity = INDEX_NONE;
	/** The user ids of the players who are talking right now. */
	TSet<FString> TalkingUsers;
	TWeakObjectPtr<UALKPlayerSlot> FocusedSeat;
	TWeakObjectPtr<AALKLobbyGameState> BoundGameState;
	TWeakObjectPtr<UTextBlock> ReadyLabel;

	/** What the rule chips were last made for. */
	FString ShownRules;
	/** The second the countdown last showed (and ticked for). */
	int32 ShownCountdown = INDEX_NONE;
	float PollTime = 0.f;
	/** Something in the lobby changed: the screen reads it again on its next tick (many changes arrive together). */
	bool bDirty = false;
	/** The chat key went down: the message box opens when it comes up, so its letter is not typed into it. */
	bool bChatKeyDown = false;
	/** A seat was added or removed: Up and Down between the seats are set again. */
	bool bSeatsChanged = false;
	/** The teams need more height than the chat leaves them: they scroll. */
	bool bTeamsScroll = false;
	/** What the teams' scroll bar takes while it shows: the panels keep their width by giving up that much of their side padding. */
	float TeamsBarSpace = 0.f;
	/** The height the teams may take, as last worked out. */
	float LastMaxTeams = 0.f;

	FDelegateHandle StateHandle;
	FDelegateHandle PlayersHandle;
	FDelegateHandle TalkingHandle;
	FDelegateHandle ListsHandle;
	FDelegateHandle ChatReleasedHandle;
};
