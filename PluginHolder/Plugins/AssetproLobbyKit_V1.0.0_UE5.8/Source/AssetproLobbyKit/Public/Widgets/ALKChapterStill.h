// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ALKButton.h"
#include "ALKChapterStill.generated.h"

class UImage;
class UTexture2D;

/**
 * The chapter select's large picture: a button showing the chapter's still. Locked chapters are dimmed under a padlock.
 * A thin frame shows while it has focus.
 *
 * Designer widget names: Background, StillImage, LockedDim, LockImage, Frame.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKChapterStill : public UALKButton
{
	GENERATED_BODY()

public:
	UALKChapterStill(const FObjectInitializer& ObjectInitializer);

	/** Shows a chapter's still (or the theme's background alone without one), locked or not. */
	void SetStill(UTexture2D* InStill, bool bInLocked);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Chapters")
	bool IsLocked() const { return bLocked; }

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UImage> StillImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockedDim;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Chapters", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Frame;

private:
	void RefreshStill();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Still;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LockTexture;

	bool bLocked = false;
};
