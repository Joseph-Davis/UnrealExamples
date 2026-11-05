// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKPlayerOptionsDialog.generated.h"

class AALKLobbyPlayerState;
class UALKAvatar;
class UALKButton;
class UBorder;
class UImage;
class UScrollBox;
class UTextBlock;

/** What the player options dialog can do with a player. */
UENUM(BlueprintType)
enum class EALKPlayerOption : uint8
{
	/** Opens the player's profile card. */
	ViewProfile,
	/** Mutes the player, or unmutes a muted one. */
	ToggleMute,
	/** Blocks the player, or unblocks a blocked one. */
	ToggleBlock,
	/** Starts a whisper to the player in the chat. */
	Whisper,
	/** Invites the player to the local player's party. */
	InviteToParty,
	/** Party leader only: makes the player the party's leader. */
	PromoteInParty,
	/** Party leader only: removes the player from the party. */
	RemoveFromParty,
	/** Host only: moves the player to the next team. */
	MoveToOtherTeam,
	/** Host only: removes the player from the game (asks first). */
	Kick,
	/** Host only: removes the player and keeps them out (asks first). */
	Ban,
	/** Opens the report dialog for the player. */
	Report
};

/**
 * What you can do with one player: view their profile, mute, block, whisper, invite to the party, and for the party
 * leader and the host the things only they may do (promote, remove, move to the other team, kick, ban), then report.
 * Only the entries that apply to this player right now are listed.
 *
 * Open it with Show Player Options on the Online Flow subsystem: the dialog reads the player from the flow's Get
 * Context User Id / Display Name when it opens. A Blueprint child with its own layout can use Get Available Options,
 * Get Option Text and Run Option for its buttons.
 *
 * Designer widget names: DimImage, Panel, Avatar, TitleText, SubtitleText, ActionList, CloseButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKPlayerOptionsDialog : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKPlayerOptionsDialog(const FObjectInitializer& ObjectInitializer);

	/** The user id of the player the dialog is about. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Dialog")
	FString GetTargetUserId() const { return TargetUserId; }

	/** The player's name as it may be shown ("Player 3" in streamer mode). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Dialog")
	FString GetTargetDisplayName() const { return TargetName; }

	/** The entries that apply to this player right now, in the order they are listed. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Dialog")
	TArray<EALKPlayerOption> GetAvailableOptions() const;

	/** The label of an entry: "Mute" or "Unmute", "Move to Orange"... */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Dialog")
	FText GetOptionText(EALKPlayerOption Option) const;

	/** Closes the dialog and does what the entry says. Kick and Ban ask first. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Dialog")
	void RunOption(EALKPlayerOption Option);

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKAvatar> Avatar;

	/** The player's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** A line about the player: team, level, host, muted, blocked, in your party. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	/** One button per entry, made when the dialog opens. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ActionList;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CloseButton;

private:
	/** Reads the player from the flow and fills the title and the list. */
	void RefreshTarget();
	/** The player's state in the current game, or null when they are not in it. */
	AALKLobbyPlayerState* FindTargetInGame() const;
	bool IsTargetLocal() const;
	bool IsTargetInParty() const;
	/** The team Move To Other Team moves the player to, or -1 when there is none. */
	int32 GetOtherTeam() const;
	void AskKickOrBan(bool bBan);

	FString TargetUserId;
	FString TargetName;
};
