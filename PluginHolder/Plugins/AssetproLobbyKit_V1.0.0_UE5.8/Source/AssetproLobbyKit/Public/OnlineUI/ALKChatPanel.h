// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Types/SlateEnums.h"
#include "Widgets/ALKUserWidget.h"
#include "ALKChatPanel.generated.h"

class SWidget;
class UALKButton;
class UALKFrame;
class UALKRule;
class UALKTextField;
class UHorizontalBox;
class UOverlay;
class UScrollBox;
class UTextBlock;

/** Someone asked to whisper to a player (the player options dialog's "Whisper"). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FALKOnWhisperRequested, const FString& /*UserId*/, const FString& /*DisplayName*/);

/**
 * The text chat of a lobby: channel tabs (All, Team, Party, and Whisper while someone is picked to whisper to), the
 * lines so far in a list that stays at its newest line, and a box to type in. Enter sends and keeps the keyboard in
 * the box, Tab moves to the next channel, Escape gives the keyboard back to where it was.
 *
 * It shows what the online subsystem keeps (Get Chat History) and every new line as it arrives, with names as streamer
 * mode wants them. With text chat turned off in the settings it shows one line saying so instead.
 *
 * Put it on any screen; a screen that has a chat key calls Focus Input. Designer widget names: TabRow, AllTab, TeamTab,
 * PartyTab, WhisperTab, FilterNoteText, HeaderRule, MessageList, EmptyText, InputRow, InputField, SendKeyChip.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKChatPanel : public UALKUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Fire this to start a whisper from anywhere (the player options dialog does): every chat panel on screen switches to
	 * the Whisper channel for that player. C++: UALKChatPanel::OnWhisperRequested().Broadcast(UserId, Name).
	 */
	static FALKOnWhisperRequested& OnWhisperRequested();

	/** Shows a channel and sends to it: All, Team, Party or Whisper. A channel that cannot be used now falls back to All. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Chat")
	void SetChannel(EALKChatChannel InChannel);

	/** The channel that is shown and written to. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Chat")
	EALKChatChannel GetChannel() const { return Channel; }

	/** Moves to the next channel tab that is shown. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Chat")
	void ShowNextChannel();

	/** Picks who the Whisper channel writes to, shows its tab and switches to it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Chat")
	void SetWhisperTarget(const FString& UserId, const FString& DisplayName);

	/** Forgets the whisper target and hides the Whisper tab. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Chat")
	void ClearWhisperTarget();

	/** Puts the keyboard in the message box (for a screen's chat key). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Chat")
	void FocusInput();

	/** Gives the keyboard back to what had it before the message box (what Escape does). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Chat")
	void ReleaseInput();

	/** True while the message box has the keyboard. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Chat")
	bool IsTyping() const;

	/** Sends a line to the shown channel. False when it could not be sent (empty, text chat off, nobody to send to). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Chat")
	bool SendChatLine(const FString& Text);

	/** Fills the list again from the chat history. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Chat")
	void RefreshMessages();

	/** Fired when the player leaves the message box and there is nothing to give the keyboard back to: the screen picks a widget. */
	FSimpleMulticastDelegate OnInputReleased;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

	/** The channel tabs. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> TabRow;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> AllTab;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> TeamTab;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> PartyTab;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> WhisperTab;

	/** "Chat filter on", shown while the chat filter is. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FilterNoteText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UALKRule> HeaderRule;

	/** The chat lines, oldest first. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> MessageList;

	/** Shown instead of the lines: "No messages yet", or that text chat is turned off. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> InputRow;

	/** The message box. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UALKTextField> InputField;

	/** The "Enter" key at the end of the message box. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Chat", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> SendKeyChip;

private:
	UALKButton* MakeTab(UWidgetTree& Tree, FName Name, const FText& Text);
	UALKButton* GetTab(EALKChatChannel InChannel) const;
	bool IsChannelAvailable(EALKChatChannel InChannel) const;
	bool ShouldShow(const FALKChatMessage& Message) const;
	void AddMessageRow(const FALKChatMessage& Message, const UALKTheme& Theme);
	FText GetSenderText(const FALKChatMessage& Message) const;
	/** Reads the settings and the game the panel depends on, and updates what is shown when one of them changed. */
	void RefreshState(bool bForce);
	void UpdateTabs();
	void UpdateEmptyText();
	void HandleTabClicked(EALKChatChannel InChannel);
	void HandleChatMessage(const FALKChatMessage& Message);
	void HandleInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	void HandleWhisperRequested(const FString& UserId, const FString& DisplayName);

	EALKChatChannel Channel = EALKChatChannel::All;
	FString WhisperTargetId;
	FString WhisperTargetName;

	/** What the panel last showed for: a change of any of these refreshes it. */
	bool bChatEnabled = true;
	bool bFilterOn = false;
	bool bStreamerMode = false;
	bool bHasTeams = false;

	float PollTime = 0.f;
	/** Seconds until the message box takes the keyboard (after a whisper request); below zero = not waiting. */
	float FocusDelay = -1.f;
	/** What had the keyboard before the message box took it. */
	TWeakPtr<SWidget> FocusBeforeTyping;

	FDelegateHandle ChatHandle;
	FDelegateHandle WhisperHandle;
};
