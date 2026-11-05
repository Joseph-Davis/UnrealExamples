// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKPaint.h"
#include "Widgets/SCompoundWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKTitleText)

/** Fades its text block in while moving it up a few pixels. */
class SALKTextIntro : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SALKTextIntro) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot[InArgs._Content.Widget];
	}

	void PlayIntro(float Duration)
	{
		IntroStart = FALKEffects::Now();
		IntroDuration = FMath::Max(0.05f, Duration);
		Invalidate(EInvalidateWidgetReason::Paint);
		if (!bAnimating)
		{
			bAnimating = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SALKTextIntro::Animate));
		}
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const float IntroTime = static_cast<float>(FALKEffects::Now() - IntroStart);
		if (IntroStart < 0.0 || IntroTime >= IntroDuration)
		{
			return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
		}
		const float T = FMath::Clamp(IntroTime / IntroDuration, 0.f, 1.f);
		const float Eased = 1.f - FMath::Square(1.f - T);
		const float Rise = FALKEffects::AreReduced() ? 0.f : (1.f - Eased) * 12.f;
		const FWidgetStyle FadedStyle = FWidgetStyle(InWidgetStyle).BlendColorAndOpacityTint(FLinearColor(1.f, 1.f, 1.f, Eased));
		const FGeometry Geometry = AllottedGeometry.MakeChild(FVector2f(AllottedGeometry.GetLocalSize()), FSlateLayoutTransform(FVector2f(0.f, Rise)));
		return SCompoundWidget::OnPaint(Args, Geometry, MyCullingRect, OutDrawElements, LayerId, FadedStyle, bParentEnabled);
	}

private:
	EActiveTimerReturnType Animate(double InCurrentTime, float InDeltaTime)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
		const bool bBusy = IntroStart >= 0.0 && FALKEffects::Now() - IntroStart < IntroDuration + 0.05;
		bAnimating = bBusy;
		return bBusy ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}

	double IntroStart = -1.0;
	float IntroDuration = 0.3f;
	bool bAnimating = false;
};

void UALKTitleText::PlayIntro(float Duration)
{
	if (MyIntro.IsValid())
	{
		MyIntro->PlayIntro(Duration);
	}
}

FString UALKTitleText::GetDisplayString() const
{
	const FString Raw = GetText().ToString();
	switch (GetTextTransformPolicy())
	{
	case ETextTransformPolicy::ToUpper: return Raw.ToUpper();
	case ETextTransformPolicy::ToLower: return Raw.ToLower();
	default: return Raw;
	}
}

TSharedRef<SWidget> UALKTitleText::RebuildWidget()
{
	const TSharedRef<SWidget> TextWidget = Super::RebuildWidget();
	MyIntro = SNew(SALKTextIntro)[TextWidget];
	return MyIntro.ToSharedRef();
}

void UALKTitleText::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyIntro.Reset();
}
