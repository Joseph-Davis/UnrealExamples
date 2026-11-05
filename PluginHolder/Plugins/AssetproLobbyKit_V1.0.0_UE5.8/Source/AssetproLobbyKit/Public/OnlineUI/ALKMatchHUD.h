// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Types/SlateEnums.h"
#include "Online/ALKOnlineTypes.h"
#include "OnlineUI/ALKMatchHUDBase.h"
#include "ALKMatchHUD.generated.h"

class AALKLobbyGameState;
class AALKLobbyPlayerState;
class UALKFrame;
class UALKIconWidget;
class UALKOnlineSubsystem;
class UALKPlayerNetComponent;
class UALKScoreboard;
class UALKTextField;
class UBorder;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UVerticalBox;

/**
 * The kit's match overlay. The Online Flow adds it to the screen when a match starts and removes it when the match
 * ends. It never takes focus or the mouse, except for the chat line while the player types.
 *
 * Its parts, each of which a Show... switch can turn off:
 * - the score strip at the top: team scores either side of the match clock (your rank and the leader's score in a free for all);
 * - the event feed at the top right: the last few kill feed / event lines;
 * - who is talking, at the left;
 * - the chat at the bottom left, with a line to type in when the chat key is pressed;
 * - the network stats at the top left (when the player turned them on in Settings > Multiplayer);
 * - the spectator bar at the bottom while the local player is watching;
 * - announcements in the upper middle ("Match started", "Final minute", or your own with Show Announcement);
 * - the scoreboard while its key is held.
 *
 * Designer widget names: ScoreStrip, TeamBlocksLeft, TeamBlocksRight, ClockText, ModeText, FeedBox, VoiceBox, ChatPanel,
 * ChatLines, ChatEntryRow, ChatChannelChip, ChatField, NetStatsPanel, NetPingText, NetLossText, NetTrafficText,
 * NetServerText, SpectatorBar, SpectatorText, SpectatePrevKeyText, SpectateNextKeyText, AnnouncementBox,
 * AnnouncementTitle, AnnouncementSubtitle, ScoreboardDim, Scoreboard.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKMatchHUD : public UALKMatchHUDBase
{
	GENERATED_BODY()

public:
	UALKMatchHUD(const FObjectInitializer& ObjectInitializer);

	// ---- UALKMatchHUDBase
	virtual void SetScoreboardVisible_Implementation(bool bShow) override;
	virtual void OpenChat_Implementation(bool bTeamChat) override;
	virtual bool IsChatOpen_Implementation() const override;
	virtual void SpectateStep_Implementation(int32 Direction) override;
	virtual void ShowAnnouncement_Implementation(const FText& InTitle, const FText& InSubtitle, float Seconds) override;

	/** Closes the chat line and gives the keyboard back to the game. With Send the typed text is sent first. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|HUD")
	void CloseChat(bool bSend);

	/** Adds a line to the event feed (the game state's feed events arrive here by themselves). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|HUD")
	void AddFeedEvent(const FALKFeedEvent& Event);

	/** Shows or removes a player in the list of who is talking (the online subsystem's talking events arrive here by themselves). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|HUD")
	void SetPlayerTalking(const FString& UserId, bool bTalking);

	/** The scoreboard this overlay shows while the scoreboard key is held. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|HUD")
	UALKScoreboard* GetScoreboard() const { return Scoreboard; }

	/** Team scores and the match clock at the top. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD")
	bool bShowScoreStrip = true;

	/** The kill feed / event feed at the top right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD")
	bool bShowFeed = true;

	/** Who is talking, at the left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD")
	bool bShowVoiceList = true;

	/** The chat lines and the line to type in, at the bottom left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD")
	bool bShowChat = true;

	/** Ping, packet loss and traffic at the top left, when the player turned the overlay on in the settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD")
	bool bShowNetStats = true;

	/** "Spectating <name>" with the previous / next keys while the local player is watching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD")
	bool bShowSpectatorBar = true;

	/** Announcements in the upper middle, also the overlay's own "Match started" and "Final minute". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD")
	bool bShowAnnouncements = true;

	/** How many feed lines are shown at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxFeedLines = 5;

	/** Seconds a feed line stays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD", meta = (ClampMin = "1"))
	float FeedLineSeconds = 6.f;

	/** How many chat lines are shown at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxChatLines = 6;

	/** Seconds a chat line stays while the player is not typing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|HUD", meta = (ClampMin = "1"))
	float ChatLineSeconds = 8.f;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ScoreStrip;

	/** The first team's score block is added here (in a free for all: your rank). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> TeamBlocksLeft;

	/** The other teams' score blocks are added here (in a free for all: the leader's score). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> TeamBlocksRight;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> FeedBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VoiceBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ChatPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ChatLines;

	/** The channel chip and the text field; shown while typing. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ChatEntryRow;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> ChatChannelChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UALKTextField> ChatField;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> NetStatsPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NetPingText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NetLossText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NetTrafficText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NetServerText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SpectatorBar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpectatorText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpectatePrevKeyText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpectateNextKeyText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> AnnouncementBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AnnouncementTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AnnouncementSubtitle;

	/** Darkens the game behind the scoreboard. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ScoreboardDim;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UALKScoreboard> Scoreboard;

private:
	struct FTeamBlock
	{
		TWeakObjectPtr<UALKFrame> Frame;
		TWeakObjectPtr<UTextBlock> Text;
		FLinearColor Color = FLinearColor::Transparent;
		int32 ShownValue = MIN_int32;
	};

	struct FFeedLine
	{
		TWeakObjectPtr<UWidget> Widget;
		float Age = 0.f;
	};

	struct FChatEntry
	{
		FALKChatMessage Message;
		float Age = 0.f;
		TWeakObjectPtr<UWidget> Widget;
	};

	AALKLobbyGameState* ResolveGameState();
	void ResolveNetComponent();
	void UnbindAll();
	AALKLobbyPlayerState* GetLocalLobbyPlayer() const;

	void RebuildTeamBlocks(int32 TeamCount);
	void StyleTeamBlocks(const UALKTheme& Theme);
	void UpdateScoreStrip(AALKLobbyGameState* GameState);
	void UpdateFreeForAllBlocks(AALKLobbyGameState* GameState);

	UWidget* MakeFeedLine(const FALKFeedEvent& Event, const UALKTheme& Theme);
	void UpdateFeed(float DeltaTime);

	void RebuildVoiceList();

	void HandleChatMessage(const FALKChatMessage& Message);
	void HandleChatCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	void RebuildChatLines();
	UWidget* MakeChatLine(const FALKChatMessage& Message, const UALKTheme& Theme);
	void UpdateChat(float DeltaTime);
	void ApplyChatLook();

	void UpdateNetStats();
	void UpdateSpectatorBar();
	void UpdateAnnouncement(float DeltaTime);
	void UpdateScoreboard();
	void UpdateSlowParts();

	TArray<FTeamBlock> TeamBlocks;
	int32 BuiltTeamCount = MIN_int32;
	int32 ShownClockSeconds = MIN_int32;
	FName ShownModeId;
	float LastTimeRemaining = -1.f;
	bool bStartChecked = false;

	TArray<FFeedLine> FeedLines;
	TArray<FString> Talkers;
	TArray<FChatEntry> ChatEntries;

	bool bChatOpen = false;
	bool bChatTeam = false;
	int32 ChatFocusTries = 0;
	/** How strongly the panel behind the chat lines shows (0 = no line is up). */
	float ChatBacking = 0.f;

	float AnnouncementAge = 0.f;
	float AnnouncementDuration = 0.f;

	bool bScoreboardWanted = false;
	bool bScoreboardShown = false;

	float SlowTimer = 1.f;
	float NetTimer = 1.f;

	TWeakObjectPtr<AALKLobbyGameState> BoundGameState;
	TWeakObjectPtr<UALKPlayerNetComponent> BoundNetComponent;
	FDelegateHandle FeedHandle;
	FDelegateHandle PrivateFeedHandle;
	FDelegateHandle ChatHandle;
	FDelegateHandle TalkingHandle;
};
