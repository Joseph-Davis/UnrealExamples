// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "ALKTitleText.generated.h"

class SALKTextIntro;

/**
 * A text block for titles and button labels. When Play Intro is called it fades in while rising a few pixels (with
 * Reduce Effects on it only fades). Everything else works like a Text block.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKTitleText : public UTextBlock
{
	GENERATED_BODY()

public:
	/** Fades the text in while it rises a few pixels. Screens play it for their titles when they open. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Title")
	void PlayIntro(float Duration = 0.3f);

	/** The text as shown (after the transform policy, such as upper case). */
	FString GetDisplayString() const;

	ETextJustify::Type GetTitleJustification() const { return Justification; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SALKTextIntro> MyIntro;
};
