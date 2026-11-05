// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Widgets/ALKButton.h"
#include "ALKServerRow.generated.h"

class UALKFrame;
class UALKIconWidget;
class UALKServerRow;
class UTextBlock;

/**
 * The widths of the server browser's columns, in the theme's units. The table header and every row read them from
 * here, so the columns always line up. The server name takes whatever width is left.
 */
namespace ALKServerColumns
{
	/** Space between the row's edge and its first and last column. */
	inline constexpr float SidePadding = 16.f;
	/** The favourite star. */
	inline constexpr float Star = 40.f;
	inline constexpr float Mode = 232.f;
	inline constexpr float Map = 188.f;
	/** "6 / 8", right aligned. */
	inline constexpr float Players = 100.f;
	/** The ping number (right aligned) and the bars after it. */
	inline constexpr float Ping = 148.f;
	/** The part of the ping column the bars take, so the header can line up with the number. */
	inline constexpr float PingBars = 32.f;
	/** The padlock of password protected games. */
	inline constexpr float Lock = 44.f;
	/** Height of one row. */
	inline constexpr float RowHeight = 52.f;
}

/**
 * One line of the server browser: favourite star, server name, mode, map, players, ping with bars and a padlock when the
 * game needs a password. A game of another version is dimmed and tagged "Different version". The whole line is a
 * button: picking it joins the game, and On Row Focused tells the browser which game to show the details of.
 *
 * Designer widget names: Background, StarIcon, Label (the server name), VersionTag, ModeText, MapText, PlayersText,
 * PingText, PingIcon, LockIcon.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKServerRow : public UALKButton
{
	GENERATED_BODY()

public:
	UALKServerRow(const FObjectInitializer& ObjectInitializer);

	/** Shows a game. Favourite lights the star; Show Ping adds the ping number in front of the bars. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void SetSession(const FALKSessionInfo& InSession, bool bInFavourite, bool bInShowPing);

	/** Lights or dims the favourite star. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void SetFavourite(bool bInFavourite);

	/** The game this row shows. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Server Browser")
	const FALKSessionInfo& GetSession() const { return Session; }

	/** Whether the star is lit. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Server Browser")
	bool IsFavourite() const { return bFavourite; }

	/** The row's place in the browser's list (the browser sets it). */
	UPROPERTY(BlueprintReadWrite, Category = "Assetpro Lobby|Server Browser")
	int32 EntryIndex = INDEX_NONE;

	/** Fired when the row gets keyboard, gamepad or mouse focus. */
	TMulticastDelegate<void(UALKServerRow*)> OnRowFocused;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> StarIcon;

	/** The "Different version" tag after the name. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> VersionTag;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayersText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PingText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> PingIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> LockIcon;

private:
	void RefreshContent();

	FALKSessionInfo Session;
	bool bFavourite = false;
	bool bShowPing = true;
};
