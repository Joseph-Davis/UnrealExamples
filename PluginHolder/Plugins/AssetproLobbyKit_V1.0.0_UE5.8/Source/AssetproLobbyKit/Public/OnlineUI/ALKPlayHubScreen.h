// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Screens/ALKScreen.h"
#include "Types/SlateEnums.h"
#include "ALKPlayHubScreen.generated.h"

class UALKCardButton;
class UALKIconWidget;
class UALKPartyPanel;
class UALKQueueWidget;
class UBorder;
class UHorizontalBox;
class UImage;
class UScrollBox;
class USizeBox;
class UTextBlock;

/**
 * The play hub: where a player chooses how to play online.
 *
 * - The featured tile starts a quick match for the Online Config's featured playlist (the first one when none is
 *   featured); while searching it cancels the search instead.
 * - Four tiles: Host Game, Browse Games, Join by Code and Party.
 * - The party panel at the right, and the quick match queue at the top while a search runs.
 * - A card per playlist; picking one starts a quick match for it.
 * - A "Rejoin" banner while a match the player dropped out of can still be rejoined.
 *
 * Open it with Open Play Hub (Online Flow subsystem) or the Open Play Online menu action.
 *
 * Designer widget names: BackgroundImage, SystemLabelText, TitleText, QueueWidget, RejoinBanner, RejoinText, RejoinButton,
 * DismissButton, OfflineText, FeaturedTile, HostTile, BrowseTile, CodeTile, PartyTile, PartyPanel, PlaylistLabel,
 * PlaylistList.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKPlayHubScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKPlayHubScreen(const FObjectInitializer& ObjectInitializer);

	/** Reads the quick match, the party and the playlists again. It already does so whenever they change. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Play Hub")
	void RefreshHub();

	/** Starts a quick match for a playlist; a search that is already running moves over to it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Play Hub")
	void StartQuickMatch(FName PlaylistId);

	/** What the featured tile does: starts a quick match for the featured playlist, or cancels the running search. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Play Hub")
	void ToggleFeaturedSearch();

	/** Leaves the quick match queue. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Play Hub")
	void CancelSearch();

	/** The playlist the featured tile searches: the Online Config's featured one, else the first. None when there is none. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Play Hub")
	FName GetFeaturedPlaylist() const;

	/** How many playlist cards fit side by side; more playlists scroll sideways. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Play Hub", meta = (ClampMin = "2", ClampMax = "8"))
	int32 PlaylistColumns = 4;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The quick match queue, top centre. It shows itself while a search runs. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKQueueWidget> QueueWidget;

	/** Shown while a match can be rejoined. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> RejoinBanner;

	/** "Rejoin Friday Night Arena". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RejoinText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> RejoinButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> DismissButton;

	/** One line shown while online play is turned off. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> OfflineText;

	/** The large Quick Match tile. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKCardButton> FeaturedTile;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKCardButton> HostTile;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKCardButton> BrowseTile;

	/** Join by Code. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKCardButton> CodeTile;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKCardButton> PartyTile;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UALKPartyPanel> PartyPanel;

	/** "Playlists". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlaylistLabel;

	/** Holds a card per playlist; scrolls sideways when there are more than fit. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Play Hub", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> PlaylistList;

private:
	void RebuildPlaylists();
	void LayoutPlaylists();
	void HandlePlaylistClicked(int32 Index);
	void HandleRejoinClicked();
	void HandleDismissClicked();
	void FocusParty();
	bool IsOnlineEnabled() const;

	/** Up from the first row goes to the rejoin banner while it is shown (it does not sit above every tile). */
	UFUNCTION()
	UWidget* HandleNavigateToBanner(EUINavigation Direction);

	/** The playlist cards, left to right, with the boxes that size them and the playlists they start. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKCardButton>> PlaylistCards;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USizeBox>> PlaylistSizes;

	TArray<FName> PlaylistIds;

	/** The "Cancel search" hint, shown only while searching. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> CancelHints;

	/** The rejoin banner's icon. */
	UPROPERTY(Transient)
	TObjectPtr<UALKIconWidget> RejoinIcon;

	FDelegateHandle MatchmakingHandle;
	FDelegateHandle PartyHandle;
	FDelegateHandle StateHandle;
	float RefreshClock = 0.f;
	float LastListWidth = 0.f;
	bool bRejoinShown = false;
};
