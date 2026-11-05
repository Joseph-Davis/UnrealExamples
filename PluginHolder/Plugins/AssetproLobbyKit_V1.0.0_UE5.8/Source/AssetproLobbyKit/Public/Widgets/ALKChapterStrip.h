// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "ALKChapterStrip.generated.h"

class SALKChapterStrip;
class UALKTheme;

DECLARE_MULTICAST_DELEGATE_OneParam(FALKOnChapterSegmentClicked, int32 /*ChapterIndex*/);

/**
 * The row of short bars over the chapter select's picture: one bar per chapter, the shown chapter lit and taller,
 * reached chapters grey, locked ones faint. Clicking a bar shows that chapter.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKChapterStrip : public UWidget
{
	GENERATED_BODY()

public:
	/** One entry per chapter: true when it can be played. */
	void SetChapters(const TArray<bool>& InUnlocked);

	/** The chapter shown now (lit). */
	void SetCurrent(int32 InIndex);

	/** The bars' colours come from the theme: text colours for shown, reached and locked chapters. */
	void ApplyTheme(const UALKTheme& Theme);

	int32 GetNumChapters() const { return Unlocked.Num(); }

	/** Broadcast when a bar is clicked, with the chapter's index. */
	FALKOnChapterSegmentClicked OnSegmentClicked;

	/** Width of the whole strip; bars shrink to fit many chapters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapter Strip")
	float MaxWidth = 880.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void Push();

	TSharedPtr<SALKChapterStrip> MyStrip;
	TArray<bool> Unlocked;
	int32 Current = 0;
	FLinearColor CurrentColor = FLinearColor::White;
	FLinearColor ReachedColor = FLinearColor::Gray;
	FLinearColor LockedColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.f);
};
