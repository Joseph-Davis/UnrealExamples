// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKChapterStrip.h"
#include "Widgets/ALKPaint.h"
#include "ALKTheme.h"
#include "Brushes/SlateColorBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKChapterStrip)

/** Draws the bars and turns clicks into chapter indices. */
class SALKChapterStrip : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SALKChapterStrip) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
	}

	void SetData(const TArray<bool>& InUnlocked, int32 InCurrent, float InMaxWidth, const FLinearColor& InCurrentColor, const FLinearColor& InReached, const FLinearColor& InLocked)
	{
		Unlocked = InUnlocked;
		Current = InCurrent;
		MaxWidth = InMaxWidth;
		CurrentColor = InCurrentColor;
		ReachedColor = InReached;
		LockedColor = InLocked;
		Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}

	TFunction<void(int32)> OnClicked;

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(GetStripWidth(), Height);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		// Rounded bars: the shown chapter lit and taller, reached ones filled, locked ones outlined.
		const float Bar = GetBarWidth();
		const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
		for (int32 Index = 0; Index < Unlocked.Num(); ++Index)
		{
			const bool bCurrent = Index == Current;
			const float BarHeight = bCurrent ? 10.f : (Index == HoveredIndex ? 8.f : 6.f);
			FLinearColor Color = bCurrent ? CurrentColor : (Unlocked[Index] ? ReachedColor : LockedColor);
			if (Index == HoveredIndex && !bCurrent)
			{
				Color = FMath::Lerp(Color, CurrentColor, 0.45f);
			}
			const float X = Index * (Bar + Gap);
			const float Y = (Height - BarHeight) * 0.5f;
			const FVector2f Min(X, Y);
			const FVector2f Max(X + Bar, Y + BarHeight);
			if (bCurrent || Unlocked[Index])
			{
				FALKPaint::RoundedBox(OutDrawElements, LayerId, AllottedGeometry, Min, Max, BarHeight * 0.5f, Color * Tint);
			}
			else
			{
				FALKPaint::RoundedBox(OutDrawElements, LayerId, AllottedGeometry, Min, Max, BarHeight * 0.5f, FLinearColor::Transparent, Color * Tint, 1.f);
			}
		}
		return LayerId + 1;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const int32 Index = IndexAt(MyGeometry, MouseEvent);
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Index != INDEX_NONE)
		{
			if (OnClicked)
			{
				OnClicked(Index);
			}
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const int32 Index = IndexAt(MyGeometry, MouseEvent);
		if (Index != HoveredIndex)
		{
			HoveredIndex = Index;
			Invalidate(EInvalidateWidgetReason::Paint);
		}
		return FReply::Unhandled();
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		HoveredIndex = INDEX_NONE;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		return HoveredIndex != INDEX_NONE ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
	}

private:
	float GetBarWidth() const
	{
		const int32 Count = FMath::Max(1, Unlocked.Num());
		return FMath::Clamp((MaxWidth - (Count - 1) * Gap) / Count, 4.f, 44.f);
	}

	float GetStripWidth() const
	{
		const int32 Count = Unlocked.Num();
		return Count > 0 ? Count * GetBarWidth() + (Count - 1) * Gap : 0.f;
	}

	int32 IndexAt(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
	{
		const FVector2f Local = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
		const float Step = GetBarWidth() + Gap;
		if (Local.X < 0.f || Local.Y < -6.f || Local.Y > Height + 6.f || Step <= 0.f)
		{
			return INDEX_NONE;
		}
		const int32 Index = FMath::FloorToInt(Local.X / Step);
		return Unlocked.IsValidIndex(Index) ? Index : INDEX_NONE;
	}

	static constexpr float Gap = 8.f;
	static constexpr float Height = 18.f;

	TArray<bool> Unlocked;
	int32 Current = 0;
	int32 HoveredIndex = INDEX_NONE;
	float MaxWidth = 880.f;
	FLinearColor CurrentColor = FLinearColor::White;
	FLinearColor ReachedColor = FLinearColor::Gray;
	FLinearColor LockedColor = FLinearColor::Gray;
	FSlateColorBrush FlatBrush = FSlateColorBrush(FLinearColor::White);
};

TSharedRef<SWidget> UALKChapterStrip::RebuildWidget()
{
	MyStrip = SNew(SALKChapterStrip);
	MyStrip->OnClicked = [WeakThis = TWeakObjectPtr<UALKChapterStrip>(this)](int32 Index)
	{
		if (UALKChapterStrip* Strip = WeakThis.Get())
		{
			Strip->OnSegmentClicked.Broadcast(Index);
		}
	};
	Push();
	return MyStrip.ToSharedRef();
}

void UALKChapterStrip::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyStrip.Reset();
}

void UALKChapterStrip::SetChapters(const TArray<bool>& InUnlocked)
{
	Unlocked = InUnlocked;
	Push();
}

void UALKChapterStrip::SetCurrent(int32 InIndex)
{
	Current = InIndex;
	Push();
}

void UALKChapterStrip::ApplyTheme(const UALKTheme& Theme)
{
	CurrentColor = Theme.Accent;
	ReachedColor = Theme.TextSecondary.CopyWithNewOpacity(Theme.TextSecondary.A * 0.7f);
	LockedColor = Theme.TextMuted.CopyWithNewOpacity(Theme.TextMuted.A * 0.8f);
	Push();
}

void UALKChapterStrip::Push()
{
	if (MyStrip.IsValid())
	{
		MyStrip->SetData(Unlocked, Current, MaxWidth, CurrentColor, ReachedColor, LockedColor);
	}
}
