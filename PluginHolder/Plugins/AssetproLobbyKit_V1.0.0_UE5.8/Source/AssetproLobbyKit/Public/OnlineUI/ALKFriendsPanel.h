// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKUserWidget.h"
#include "ALKFriendsPanel.generated.h"

class UALKAvatar;
class UBorder;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;

/**
 * One friend in the friends list: a picture with a presence dot (online, in a lobby, in a game or offline), the name and
 * a status line. Picking it opens the player options.
 *
 * Designer widget names: Background, FriendAvatar, Label (the name), StatusText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKFriendRow : public UALKButton
{
	GENERATED_BODY()

public:
	UALKFriendRow(const FObjectInitializer& ObjectInitializer);

	/**
	 * Shows a friend. Shown Name is the name as it may be displayed (streamer mode hides other players' names) and
	 * bInAnonymous hides the picture too.
	 */
	void SetFriend(const FALKFriend& InFriend, const FString& InShownName, bool bInAnonymous);

	/** The friend this line shows. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Friends")
	const FALKFriend& GetFriend() const { return Friend; }

	/** The friend's name as it is displayed. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Friends")
	const FString& GetShownName() const { return ShownName; }

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) override;

	/** The friend's picture, with the presence dot. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UALKAvatar> FriendAvatar;

	/** What the friend is doing: "Online", "In a lobby"... */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	void RefreshContent();

	FALKFriend Friend;
	FString ShownName;
	bool bAnonymous = false;
};

/**
 * The friends list on a panel: "Friends", the tabs "Online 4" and "Recent" (recent players), and a scrolling list. Each
 * line has one button at its right: Join when the friend's game can be joined, else Invite (to the party, or to the game
 * while you are in one). Picking the line itself opens the player options. It asks for the friends list when it is shown
 * and follows it as it changes, so any screen can place one.
 *
 * Compact (a side column): no tabs, friends who are online only, a few lines, then an "All friends" button that opens
 * the friends screen.
 *
 * Designer widget names: Panel, TitleText, TabBox, OnlineTab, RecentTab, FriendList, EmptyText, AllFriendsButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKFriendsPanel : public UALKUserWidget
{
	GENERATED_BODY()

public:
	/** Reads the friends again and updates the list. It already does so whenever the friends list changes. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Friends")
	void RefreshFriends();

	/** Shows the recent players (true) or the friends (false): what the two tabs do. Bind your tab keys to it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Friends")
	void ShowRecentPlayers(bool bInShowRecent);

	/** True while the Recent tab is shown. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Friends")
	bool IsShowingRecentPlayers() const { return bShowRecent; }

	/** The short form for a side column: no tabs, online friends only, a few lines and an "All friends" button. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Friends")
	void SetCompact(bool bInCompact);

	/** True while the panel is in its short form. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Friends")
	bool IsCompact() const { return bCompact; }

	/** Puts the focus on the first friend (or the "All friends" button). False when there is nothing to focus. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Friends")
	bool FocusFirst();

	/** The first line or button that can take the focus, or null. For a screen's default focus. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Friends")
	UWidget* GetFirstFocusTarget() const;

	/** How many friends are online, in a lobby or in a game. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Friends")
	int32 GetOnlineFriendCount() const { return OnlineCount; }

	/** How many friends the compact form lists before the "All friends" button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Friends", meta = (ClampMin = "1", ClampMax = "20"))
	int32 CompactRows = 5;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** Holds the two tabs; hidden in the compact form. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> TabBox;

	/** "Online 4": the friends list. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> OnlineTab;

	/** "Recent": players met in lobbies and matches. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> RecentTab;

	/** Holds a line per friend. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> FriendList;

	/** A friendly line shown while the list is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	/** "All friends (8)": opens the friends screen from the compact form. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Friends", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> AllFriendsButton;

private:
	/** What the button at the right of a line does. */
	enum class EFriendAction : uint8
	{
		None,
		Join,
		InviteToParty,
		InviteToGame
	};

	void EnsureLines(int32 Count);
	void HandleRowClicked(int32 Index);
	void HandleActionClicked(int32 Index);
	void HandleAllFriendsClicked();
	void StyleList(const UALKTheme& Theme);

	/** The lines of the list, reused as the list changes: a row, its button and the box that holds both. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKFriendRow>> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> ActionButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHorizontalBox>> Lines;

	/** What each shown line's button does. */
	TArray<EFriendAction> Actions;

	/** A line or button to focus once it has been laid out. */
	TWeakObjectPtr<UWidget> PendingFocus;

	FDelegateHandle FriendsHandle;
	FDelegateHandle PartyHandle;
	FDelegateHandle StateHandle;
	int32 ShownCount = 0;
	int32 OnlineCount = 0;
	bool bShowRecent = false;
	bool bCompact = false;
};
