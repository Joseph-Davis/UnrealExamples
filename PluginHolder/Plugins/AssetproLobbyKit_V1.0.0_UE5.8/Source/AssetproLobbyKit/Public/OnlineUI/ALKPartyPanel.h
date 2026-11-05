// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKUserWidget.h"
#include "ALKPartyPanel.generated.h"

class UALKAvatar;
class UALKFrame;
class UALKOnlineSubsystem;
class UBorder;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

/**
 * One line of the party panel: a party member (picture with a presence dot, name, "Party leader" / "Ready" / "Not
 * ready", a crown on the leader and a tick when ready), or a free seat that reads "Invite a friend".
 *
 * Designer widget names: Background, MemberAvatar, PlusIcon, Label (the name), CrownIcon, StatusText, ReadyIcon.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKPartyMemberRow : public UALKButton
{
	GENERATED_BODY()

public:
	UALKPartyMemberRow(const FObjectInitializer& ObjectInitializer);

	/**
	 * Shows a party member. Shown Name is the name as it may be displayed (streamer mode hides other players' names),
	 * bInAnonymous hides the picture too, bInAlone leaves the party words out for a player who is not in a party.
	 */
	void SetMember(const FALKPartyMember& InMember, const FString& InShownName, bool bInAnonymous, bool bInAlone);

	/** Shows a free seat: "Invite a friend". */
	void SetInviteSeat();

	/** True for a free seat, false for a member. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Party")
	bool IsInviteSeat() const { return bInviteSeat; }

	/** The member this line shows (empty for a free seat). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Party")
	const FALKPartyMember& GetMember() const { return Member; }

	/** The member's name as it is displayed. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Party")
	const FString& GetShownName() const { return ShownName; }

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) override;

	/** The member's picture. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UALKAvatar> MemberAvatar;

	/** Shown instead of the picture on a free seat. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> PlusIcon;

	/** Beside the party leader's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> CrownIcon;

	/** "Party leader", "Ready" or "Not ready". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** A tick at the right while the member is ready. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> ReadyIcon;

private:
	void RefreshContent();

	FALKPartyMember Member;
	FString ShownName;
	bool bInviteSeat = false;
	bool bAnonymous = false;
	bool bAlone = false;
};

/**
 * The party on a panel: "Party" with a "2 / 4" chip, a line per member, "Invite a friend" lines for the free seats, and
 * the buttons Invite, Leave party and Ready (the last two only in a party of two or more). Picking another member opens
 * the player options; Invite and the free seats open the friends screen. It finds the online subsystem by itself and
 * follows the party as it changes, so any screen can place one.
 *
 * Designer widget names: Panel, TitleText, CountChip, MemberList, ButtonBox, ReadyButton, InviteButton, LeaveButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKPartyPanel : public UALKUserWidget
{
	GENERATED_BODY()

public:
	/** Reads the party again and updates the panel. It already does so whenever the party changes. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Party")
	void RefreshParty();

	/** Puts the focus on the panel's first line or button. False when it has nothing to focus. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Party")
	bool FocusFirst();

	/** The first line or button that can take the focus, or null. For a screen's default focus. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Party")
	UWidget* GetFirstFocusTarget() const;

	/** A narrow panel (a side column): "Leave party" is shortened to "Leave" so it fits beside Invite. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Party")
	void SetCompact(bool bInCompact);

	/** True while the panel uses the narrow layout. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Party")
	bool IsCompact() const { return bCompact; }

	/** How many players are in the party, the local player included (1 = not in a party). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Party")
	int32 GetMemberCount() const { return MemberCount; }

	/**
	 * On (the default): Invite and the free seats open the friends screen. Turn off on a screen that already shows the
	 * friends list and bind On Invite Requested instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Party")
	bool bInviteOpensSocial = true;

	/** Invite or a free seat was picked while Invite Opens Social is off. */
	FSimpleMulticastDelegate OnInviteRequested;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** "2 / 4": the party's size and how many it holds. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> CountChip;

	/** Holds a line per member and free seat. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> MemberList;

	/** Holds the buttons under the list. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ButtonBox;

	/** "Ready" / "Not ready" for the local player. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ReadyButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> InviteButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Party", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> LeaveButton;

private:
	void HandleRowClicked(TWeakObjectPtr<UALKPartyMemberRow> WeakRow);
	void HandleInviteClicked();
	void HandleLeaveClicked();
	void HandleReadyClicked();
	void UpdateLeaveLabel();
	void StyleList(const UALKTheme& Theme);
	bool HasFocusInside() const;

	/** The lines of the list, top to bottom. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKPartyMemberRow>> Rows;

	/** Who is listed and how many free seats: the lines are only made again when this changes. */
	FString StructureKey;

	/** A line or button to focus once it has been laid out. */
	TWeakObjectPtr<UWidget> PendingFocus;

	FDelegateHandle PartyHandle;
	int32 MemberCount = 1;
	bool bLocalReady = false;
	bool bCompact = false;
};
