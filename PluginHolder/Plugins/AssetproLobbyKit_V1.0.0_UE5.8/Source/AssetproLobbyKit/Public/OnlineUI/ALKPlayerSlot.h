// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Widgets/ALKButton.h"
#include "ALKPlayerSlot.generated.h"

class UALKAvatar;
class UALKFrame;
class UALKIconWidget;
class UALKPlayerSlot;
class UHorizontalBox;
class UTextBlock;

/**
 * A seat in the lobby. Taken, it shows the player: team colour bar, picture (with a ring while they talk), name, a crown
 * on the host, "You" / "Speaking" / "Level 12", a speaker (or a crossed one for a muted player), ping and a Ready or
 * Not ready tag. The local player's seat has the accent outline. Free, it reads "Open slot".
 *
 * The seat is a button: a taken seat opens the player options, a free one the friends screen to invite someone.
 * Names, pictures and pings follow streamer mode and the Show Ping setting.
 *
 * Designer widget names: Background, PlayerBox, OpenBox, TeamBar, Avatar, Label (the name), CrownIcon, PlatformIcon,
 * StatusText, VoiceIcon, PingText, PingIcon, ReadyChip, OpenIcon, OpenText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKPlayerSlot : public UALKButton
{
	GENERATED_BODY()

public:
	UALKPlayerSlot(const FObjectInitializer& ObjectInitializer);

	/** Seats a player here. None makes it an open slot. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void SetPlayer(AALKLobbyPlayerState* InPlayer);

	/** The player in this seat, or none for an open slot. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	AALKLobbyPlayerState* GetPlayer() const { return Player.Get(); }

	/** Nobody sits here. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	bool IsOpenSlot() const { return !Player.IsValid(); }

	/** The seat belongs to the player on this machine. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	bool IsLocalPlayerSlot() const { return bLocalPlayer; }

	/** Shows the player as talking: a ring round the picture and "Speaking" under the name. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void SetTalking(bool bInTalking);

	/** Reads the player's values again: name, ready, host, team, level, mute, ping. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void RefreshSlot();

	/** The name shown for the player: their own for the local player, "Player 3" for others in streamer mode. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	FString GetShownName() const;

	/**
	 * Picking the seat opens the player options (a taken seat) or the friends screen (an open slot) by itself. Turn off to
	 * decide what happens in On Clicked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Lobby")
	bool bOpenOptionsWhenPicked = true;

	/** Height of a seat, in the theme's units. */
	static constexpr float SlotHeight = 66.f;

	/** Fired when the seat gets keyboard, gamepad or mouse focus. */
	TMulticastDelegate<void(UALKPlayerSlot*)> OnSlotFocused;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnClicked() override;

	/** Everything a taken seat shows. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PlayerBox;

	/** What a free seat shows: a plus and "Open slot". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> OpenBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> TeamBar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKAvatar> Avatar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> CrownIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> PlatformIcon;

	/** "You", "Speaking" or "Level 12". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> VoiceIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PingText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> PingIcon;

	/** "Ready" (filled, with a tick) or "Not ready" (outlined). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> ReadyChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> OpenIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Lobby", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OpenText;

private:
	/** Colours that depend on the player's values, in the theme's colours. */
	void ApplyPlayerColors(const UALKTheme& Theme);

	TWeakObjectPtr<AALKLobbyPlayerState> Player;
	bool bLocalPlayer = false;
	bool bTalking = false;
	bool bReady = false;
	bool bMuted = false;
	int32 PingMs = 0;
	int32 TeamId = INDEX_NONE;
};
