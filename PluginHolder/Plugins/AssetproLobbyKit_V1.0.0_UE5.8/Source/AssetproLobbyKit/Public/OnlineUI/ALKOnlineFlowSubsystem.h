// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "ALKTypes.h"
#include "Online/ALKOnlineTypes.h"
#include "OnlineUI/ALKOnlineUITypes.h"
#include "OnlineUI/ALKTextEntryDialog.h"
#include "ALKOnlineFlowSubsystem.generated.h"

class UALKMenuSubsystem;
class UALKOnlineSubsystem;
class UALKPlayerNetComponent;
class UALKConfirmDialog;
class UCommonActivatableWidget;
class UUserWidget;
class APlayerController;
class FALKOnlineInputProcessor;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FALKOnOnlineStageChanged, EALKOnlineStage, NewStage, EALKOnlineStage, OldStage);

/**
 * Per-player guide through the multiplayer front end. It opens the online screens (the classes come from the menu
 * config's Online Screens) and keeps the right one up as the game moves between the menus, a lobby, a match and the
 * results: the lobby screen appears when a lobby is entered, the match overlay when the match starts, the results
 * screen when it ends. It also shows the "connecting" dialog, the match found dialog, connection errors (with Rejoin
 * when the match can be rejoined), invites and the server's notices, and reads the match keys (scoreboard, chat,
 * spectating, push to talk).
 *
 * Get it from Blueprint with Get Local Player Subsystem (ALK Online Flow Subsystem) or UALKOnlineFlowSubsystem::Get(this)
 * in C++. Set Automatic Screens to false to place every screen yourself and keep only the Open... functions.
 *
 * Screens that are opened for something (a player, a leaderboard, editing the current lobby) read what they were
 * opened for from the "context" getters below when they activate.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKOnlineFlowSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The flow of the player who owns a widget (or of the first local player for any other object). */
	static UALKOnlineFlowSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	UALKOnlineSubsystem* GetOnline() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	UALKMenuSubsystem* GetMenu() const;

	// ---------------------------------------------------------------- Where the game is

	/** Menus, lobby, match or results. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	EALKOnlineStage GetStage() const { return Stage; }

	/** The stage changed: the lobby was entered, the match started or ended, the game was left. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Flow")
	FALKOnOnlineStageChanged OnStageChanged;
	TMulticastDelegate<void(EALKOnlineStage /*NewStage*/, EALKOnlineStage /*OldStage*/)> OnStageChangedNative;

	/**
	 * On (the default): the flow shows the lobby screen, the match overlay, the results screen and its dialogs by itself.
	 * Off: nothing appears by itself; open what you want from On Stage Changed and the online subsystem's events.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	void SetAutomaticScreens(bool bEnabled) { bAutomaticScreens = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	bool AreScreensAutomatic() const { return bAutomaticScreens; }

	// ---------------------------------------------------------------- Screens

	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenPlayHub();

	/** Opens the host game screen. With bEditCurrentGame it edits the lobby you are hosting instead of creating one. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenHostGame(bool bEditCurrentGame = false);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenServerBrowser();

	/** Friends, recent players, party and invites. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenSocial();

	/** Asks for a lobby code or an address (and a password), then joins. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	void OpenJoinByCode();

	/** The lobby screen (while in a lobby). The flow opens it by itself when a lobby is entered. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenLobby();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenResults();

	/** Opens the leaderboards on BoardId (None: the first board). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenLeaderboards(FName BoardId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenLoadout();

	/** The host's panel (kick, ban list, change map, pause). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* OpenAdminPanel();

	/** What you can do with one player. The dialog reads the player from Get Context User Id / Display Name. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* ShowPlayerOptions(const FString& UserId, const FString& DisplayName);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* ShowProfile(const FString& UserId, const FString& DisplayName);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	UCommonActivatableWidget* ShowReport(const FString& UserId, const FString& DisplayName);

	/** Asks for a line of text. The callback gets whether it was confirmed, the text and the second field's text. */
	UALKTextEntryDialog* ShowTextEntry(const FALKTextEntryDescriptor& Descriptor, TFunction<void(bool, const FString&, const FString&)> OnResult);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow", meta = (DisplayName = "Show Text Entry", AutoCreateRefTerm = "OnResult"))
	UALKTextEntryDialog* K2_ShowTextEntry(const FALKTextEntryDescriptor& Descriptor, const FALKTextEntryResultDelegate& OnResult);

	// ---------------------------------------------------------------- Actions with their questions

	/** Joins a found game; asks for the password first when the game has one. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	void JoinGameWithPrompt(const FALKSessionInfo& Session);

	/** Asks "Leave the game?" and leaves when confirmed (the host is told the lobby closes for everyone). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	void LeaveGameWithConfirm();

	/** Copies text (a lobby code, an address) to the clipboard and shows a short notice. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	void CopyToClipboard(const FString& Text, const FText& Notice);

	/** A short notice in the corner of the screen, above any open menu. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Flow")
	void ShowNotice(const FText& Title, const FText& Detail);

	// ---------------------------------------------------------------- Context of the screen opened last

	/** The player a player dialog (options, profile, report) was opened for. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	FString GetContextUserId() const { return ContextUserId; }

	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	FString GetContextDisplayName() const { return ContextDisplayName; }

	/** True when the host game screen was opened to edit the lobby that is open. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	bool IsEditingCurrentGame() const { return bEditingCurrentGame; }

	/** The leaderboard the leaderboards screen was opened on. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	FName GetContextBoardId() const { return ContextBoardId; }

	/** The match overlay while a match is on (the class from the menu config's Online Screens), else null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Flow")
	UUserWidget* GetMatchHUD() const { return MatchHUD; }

	/** A key went down or up anywhere in the game (the flow's input processor calls it). True = the key is used up. */
	bool HandleKey(const FKey& Key, bool bDown, bool bRepeat);

private:
	const FALKOnlineScreenClasses& GetClasses() const;
	UCommonActivatableWidget* Push(const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass, EALKLayer Layer, const TCHAR* What);
	UCommonActivatableWidget* FindOpenScreen(const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass, EALKLayer Layer) const;
	void SetPlayerContext(const FString& UserId, const FString& DisplayName);

	/** Only the first local player drives the online screens (split screen guests share the game, not the menus). */
	bool IsFlowOwner() const;
	/** The player controller in a world that is ready for widgets: begun, and no level change or connection under way. */
	APlayerController* GetReadyController() const;

	bool TickFlow(float DeltaTime);
	EALKOnlineStage ComputeStage(const UWorld* World) const;
	void SetStage(EALKOnlineStage NewStage, UWorld* World);
	void MaintainStage(APlayerController* Controller);
	void EnsureMatchHUD(APlayerController* Controller);
	void RemoveMatchHUD();
	void BindNetComponent();
	void ShowQueuedInvite();
	bool IsTypingText() const;
	bool IsOurWindowActive() const;

	void HandleOnlineStateChanged(EALKOnlineState NewState, EALKOnlineState OldState);
	void HandleConnectionError(EALKConnectionError Error, const FText& Title, const FText& Message);
	void HandleMatchmakingChanged(EALKMatchmakingState NewState);
	void HandleInviteReceived(const FALKInvite& Invite);
	void HandleServerNotice(const FText& Text);

	/** Listens to the "On Menu Action" event of other menu systems on this player (menu config: Other Menu Entries). */
	void BindOtherMenus();

	UFUNCTION()
	void HandleOtherMenuAction(FName EntryId);

	FString ContextUserId;
	FString ContextDisplayName;
	FName ContextBoardId;
	bool bEditingCurrentGame = false;
	bool bAutomaticScreens = true;

	EALKOnlineStage Stage = EALKOnlineStage::None;
	TWeakObjectPtr<UWorld> StageWorld;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MatchHUD;

	TWeakObjectPtr<UALKPlayerNetComponent> BoundNetComponent;
	FDelegateHandle NoticeHandle;

	/** Invites that arrived and wait for a moment in which a dialog can ask (not in the middle of a match). */
	TArray<FALKInvite> QueuedInvites;
	TWeakObjectPtr<UALKConfirmDialog> InviteDialog;

	/** A chat key that went down while the game had the keyboard: its release opens the chat line. */
	FKey PendingChatKey;
	EALKMatchmakingState LastMatchmakingState = EALKMatchmakingState::Idle;

	/** The menu subsystems of other plugins whose On Menu Action event is bound. */
	TArray<TWeakObjectPtr<UObject>> BoundOtherMenus;

	/** Classes that could not be loaded, so the log says it once. */
	TSet<FSoftObjectPath> ReportedMissingClasses;

	FTSTicker::FDelegateHandle TickerHandle;
	TSharedPtr<FALKOnlineInputProcessor> InputProcessor;
};
