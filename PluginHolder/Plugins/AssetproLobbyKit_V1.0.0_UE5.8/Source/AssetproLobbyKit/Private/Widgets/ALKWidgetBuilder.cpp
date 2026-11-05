// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKWidgetBuilder.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "Widgets/ALKAtmosphere.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKThumbnail.h"
#include "Components/BackgroundBlur.h"
#include "ALKTheme.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"
#include "Rendering/SlateRenderer.h"
#include "TextureResource.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

UTextBlock* FALKWidgetBuilder::MakeText(UWidgetTree& Tree, FName Name, const FText& Text)
{
	UTextBlock* Block = Make<UTextBlock>(Tree, Name);
	Block->SetText(Text);
	return Block;
}

void FALKWidgetBuilder::StyleText(UTextBlock* Text, const UALKTheme& Theme, EALKTextRole Role)
{
	StyleText(Text, Theme, Role, Theme.GetTextColor(Role));
}

void FALKWidgetBuilder::StyleText(UTextBlock* Text, const UALKTheme& Theme, EALKTextRole Role, const FLinearColor& Color)
{
	if (!Text)
	{
		return;
	}

	SetTextFont(Text, Theme.GetFont(Role));
	Text->SetColorAndOpacity(FSlateColor(Color));
	bool bUpper = false;
	if (Role == EALKTextRole::Title || Role == EALKTextRole::Heading)
	{
		bUpper = Theme.bUppercaseTitles;
	}
	else if (Role == EALKTextRole::Button)
	{
		bUpper = Theme.bUppercaseButtons;
	}
	Text->SetTextTransformPolicy(bUpper ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);

	if (Role == EALKTextRole::Title || Role == EALKTextRole::Heading)
	{
		Text->SetShadowColorAndOpacity(Theme.TitleShadowColor);
		Text->SetShadowOffset(Theme.TitleShadowOffset);
	}
}

namespace ALKWidgetBuilderPrivate
{
	/** A line box in the units of a font's size: what the kit's layouts were made for (a text face takes about 1.2). */
	static constexpr float LineBoxShare = 1.24f;

	/** The full line of a font and how much of it lies under the baseline, in pixels at 1080p. False before Slate is up. */
	static bool MeasureLine(const FSlateFontInfo& Font, float& OutHeight, float& OutDescent)
	{
		if (!FSlateApplication::IsInitialized() || !Font.HasValidFont() || Font.Size <= 0.f)
		{
			return false;
		}
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		OutHeight = static_cast<float>(Measure->GetMaxCharacterHeight(Font, 1.f));
		OutDescent = FMath::Abs(static_cast<float>(Measure->GetBaseline(Font, 1.f)));
		return OutHeight > 0.f;
	}
}

FMargin FALKWidgetBuilder::GetLineBoxTrim(const FSlateFontInfo& Font)
{
	float Height = 0.f;
	float Descent = 0.f;
	if (!ALKWidgetBuilderPrivate::MeasureLine(Font, Height, Descent))
	{
		return FMargin(0.f);
	}
	// Slate sizes fonts at 96 dots an inch.
	const float Em = Font.Size * (96.f / 72.f);
	const float Excess = FMath::RoundToFloat(Height - Em * ALKWidgetBuilderPrivate::LineBoxShare);
	if (Excess < 2.f)
	{
		return FMargin(0.f);
	}
	// Most of the spare room is over the capitals (it is there for accents, which may now reach out of the box: text is
	// not clipped to it); descenders keep theirs.
	const float Top = FMath::RoundToFloat(Excess * 0.62f);
	return FMargin(0.f, -Top, 0.f, -(Excess - Top));
}

void FALKWidgetBuilder::SetTextFont(UTextBlock* Text, const FSlateFontInfo& Font)
{
	if (Text)
	{
		Text->SetFont(Font);
		Text->SetMargin(GetLineBoxTrim(Font));
	}
}

float FALKWidgetBuilder::GetBaselineFromTop(const FSlateFontInfo& Font)
{
	float Height = 0.f;
	float Descent = 0.f;
	if (!ALKWidgetBuilderPrivate::MeasureLine(Font, Height, Descent))
	{
		return 0.f;
	}
	return Height - Descent + GetLineBoxTrim(Font).Top;
}

void FALKWidgetBuilder::AlignBaselines(const TArray<UTextBlock*>& Texts)
{
	float Lowest = 0.f;
	for (const UTextBlock* Text : Texts)
	{
		if (Text)
		{
			Lowest = FMath::Max(Lowest, GetBaselineFromTop(Text->GetFont()));
		}
	}
	for (UTextBlock* Text : Texts)
	{
		if (UHorizontalBoxSlot* TextSlot = Text ? Cast<UHorizontalBoxSlot>(Text->Slot) : nullptr)
		{
			FMargin Padding = TextSlot->GetPadding();
			Padding.Top = Lowest - GetBaselineFromTop(Text->GetFont());
			TextSlot->SetPadding(Padding);
		}
	}
}

UVerticalBoxSlot* FALKWidgetBuilder::AddToVerticalBox(UVerticalBox* Box, UWidget* Child, const FMargin& Padding, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, float FillWeight)
{
	UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child);
	Slot->SetPadding(Padding);
	Slot->SetHorizontalAlignment(HAlign);
	Slot->SetVerticalAlignment(VAlign);
	if (FillWeight > 0.f)
	{
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = FillWeight;
		Slot->SetSize(Size);
	}
	else
	{
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	return Slot;
}

UHorizontalBoxSlot* FALKWidgetBuilder::AddToHorizontalBox(UHorizontalBox* Box, UWidget* Child, const FMargin& Padding, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, float FillWeight)
{
	UHorizontalBoxSlot* Slot = Box->AddChildToHorizontalBox(Child);
	Slot->SetPadding(Padding);
	Slot->SetHorizontalAlignment(HAlign);
	Slot->SetVerticalAlignment(VAlign);
	if (FillWeight > 0.f)
	{
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = FillWeight;
		Slot->SetSize(Size);
	}
	else
	{
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	return Slot;
}

UOverlaySlot* FALKWidgetBuilder::AddToOverlay(UOverlay* Overlay, UWidget* Child, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, const FMargin& Padding)
{
	UOverlaySlot* Slot = Overlay->AddChildToOverlay(Child);
	Slot->SetHorizontalAlignment(HAlign);
	Slot->SetVerticalAlignment(VAlign);
	Slot->SetPadding(Padding);
	return Slot;
}

USizeBox* FALKWidgetBuilder::WrapInSizeBox(UWidgetTree& Tree, UWidget* Child, float Width, float Height, bool bMinimum)
{
	USizeBox* Box = Make<USizeBox>(Tree);
	if (Width > 0.f)
	{
		bMinimum ? Box->SetMinDesiredWidth(Width) : Box->SetWidthOverride(Width);
	}
	if (Height > 0.f)
	{
		bMinimum ? Box->SetMinDesiredHeight(Height) : Box->SetHeightOverride(Height);
	}
	if (Child)
	{
		Box->AddChild(Child);
	}
	return Box;
}

UTexture2D* FALKWidgetBuilder::CreateGradientTexture(UObject* Outer, const FLinearColor& Top, const FLinearColor& Bottom)
{
	// Half-float linear pixels: dark gradients stretched over a full screen show visible banding at 8 bits.
	constexpr int32 Height = 512;
	UTexture2D* Texture = UTexture2D::CreateTransient(1, Height, PF_FloatRGBA);
	if (!Texture)
	{
		return nullptr;
	}

	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;

	if (FTexturePlatformData* PlatformData = Texture->GetPlatformData())
	{
		if (PlatformData->Mips.Num() > 0)
		{
			FTexture2DMipMap& Mip = PlatformData->Mips[0];
			FFloat16Color* Pixels = static_cast<FFloat16Color*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
			if (Pixels)
			{
				for (int32 Y = 0; Y < Height; ++Y)
				{
					// Smoothstep keeps the middle of the screen calm.
					const float T = FMath::SmoothStep(0.f, 1.f, static_cast<float>(Y) / static_cast<float>(Height - 1));
					Pixels[Y] = FFloat16Color(FMath::Lerp(Top, Bottom, T));
				}
			}
			Mip.BulkData.Unlock();
		}
	}

	Texture->UpdateResource();
	return Texture;
}

FSlateBrush FALKWidgetBuilder::MakeBackgroundBrush(const UALKTheme& Theme, UObject* Outer, TObjectPtr<UTexture2D>& InOutGradientTexture)
{
	if (Theme.bUseBackgroundBrush && Theme.BackgroundBrush.GetResourceObject())
	{
		return Theme.BackgroundBrush;
	}

	// Screens share one texture per colour pair; each screen's UPROPERTY keeps it alive.
	static TMap<uint32, TWeakObjectPtr<UTexture2D>> GradientCache;
	const uint32 Key = HashCombine(GetTypeHash(Theme.BackgroundTop), GetTypeHash(Theme.BackgroundBottom));
	if (TWeakObjectPtr<UTexture2D>* Cached = GradientCache.Find(Key); Cached && Cached->IsValid())
	{
		InOutGradientTexture = Cached->Get();
	}
	else
	{
		InOutGradientTexture = CreateGradientTexture(Outer, Theme.BackgroundTop, Theme.BackgroundBottom);
		GradientCache.Add(Key, InOutGradientTexture.Get());
	}

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	Brush.SetResourceObject(InOutGradientTexture);
	Brush.ImageSize = FVector2D(1.0, 512.0);
	return Brush;
}

namespace ALKWidgetBuilderPrivate
{
	/** Integer hash to [0, 1). */
	static float Hash(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = static_cast<uint32>(X) * 374761393u + static_cast<uint32>(Y) * 668265263u + static_cast<uint32>(Seed) * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= H >> 16;
		return static_cast<float>(H & 0xFFFFFFu) / static_cast<float>(0x1000000);
	}

	/** Value noise on a lattice that repeats every Period cells, so a texture made from it tiles without seams. */
	static UTexture2D* CreateTexture(int32 Width, int32 Height, bool bTile, TFunctionRef<FColor(int32, int32)> Pixel)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->SRGB = true;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = bTile ? TA_Wrap : TA_Clamp;
		Texture->AddressY = bTile ? TA_Wrap : TA_Clamp;
		Texture->NeverStream = true;
		Texture->LODGroup = TEXTUREGROUP_UI;

		if (FTexturePlatformData* PlatformData = Texture->GetPlatformData(); PlatformData && PlatformData->Mips.Num() > 0)
		{
			FTexture2DMipMap& Mip = PlatformData->Mips[0];
			if (FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE)))
			{
				for (int32 Y = 0; Y < Height; ++Y)
				{
					for (int32 X = 0; X < Width; ++X)
					{
						Pixels[Y * Width + X] = Pixel(X, Y);
					}
				}
			}
			Mip.BulkData.Unlock();
		}
		Texture->UpdateResource();
		return Texture;
	}

	static UTexture2D* CreateOverlayTexture(FALKWidgetBuilder::EOverlayTexture Type)
	{
		switch (Type)
		{
		case FALKWidgetBuilder::EOverlayTexture::Grain:
			// Fine per-pixel grain only, grey around the middle so it both lightens and darkens. No larger blotches: the
			// grain jumps to a new place many times a second, and anything bigger than a pixel or two reads as flicker.
			return CreateTexture(256, 256, true, [](int32 X, int32 Y)
			{
				const float Fine = (Hash(X, Y, 99) + Hash(X, Y, 131)) * 0.5f;
				const float Value = FMath::Clamp(0.5f + (Fine - 0.5f) * 1.6f, 0.f, 1.f);
				const uint8 Grey = static_cast<uint8>(FMath::RoundToInt(Value * 255.f));
				return FColor(Grey, Grey, Grey, 255);
			});

		case FALKWidgetBuilder::EOverlayTexture::Vignette:
			return CreateTexture(128, 128, false, [](int32 X, int32 Y)
			{
				const float DX = (X / 127.f - 0.5f) * 2.f;
				const float DY = (Y / 127.f - 0.5f) * 2.f;
				const float Distance = FMath::Sqrt((DX * DX + DY * DY) * 0.5f);
				const float Alpha = FMath::Pow(FMath::SmoothStep(0.3f, 1.f, Distance), 1.3f);
				return FColor(0, 0, 0, static_cast<uint8>(FMath::RoundToInt(Alpha * 255.f)));
			});

		case FALKWidgetBuilder::EOverlayTexture::Haze:
		default:
			return CreateTexture(128, 128, false, [](int32 X, int32 Y)
			{
				const float DX = (X + 0.5f) / 64.f - 1.f;
				const float DY = (Y + 0.5f) / 64.f - 1.f;
				const float Falloff = FMath::Clamp(1.f - FMath::Sqrt(DX * DX + DY * DY), 0.f, 1.f);
				const float Alpha = Falloff * Falloff * (3.f - 2.f * Falloff);
				// A little noise hides the steps between alpha levels once the glow is stretched over the screen.
				const float Dither = (Hash(X, Y, 57) - 0.5f) / 255.f;
				return FColor(255, 255, 255, static_cast<uint8>(FMath::Clamp(FMath::RoundToInt((Alpha + Dither) * 255.f), 0, 255)));
			});

		case FALKWidgetBuilder::EOverlayTexture::ScrimLeft:
		case FALKWidgetBuilder::EOverlayTexture::ScrimRight:
		case FALKWidgetBuilder::EOverlayTexture::ScrimCenter:
			return CreateTexture(256, 1, false, [Type](int32 X, int32 Y)
			{
				const float U = X / 255.f;
				float Amount = 0.f;
				switch (Type)
				{
				case FALKWidgetBuilder::EOverlayTexture::ScrimLeft: Amount = 1.f - U; break;
				case FALKWidgetBuilder::EOverlayTexture::ScrimRight: Amount = U; break;
				default: Amount = 1.f - FMath::Abs(U * 2.f - 1.f); break;
				}
				// White, so the theme's Menu Scrim colour paints it: dark themes darken, light themes brighten.
				const float Alpha = FMath::SmoothStep(0.f, 1.f, Amount);
				return FColor(255, 255, 255, static_cast<uint8>(FMath::RoundToInt(Alpha * 255.f)));
			});

		}
	}
}

UTexture2D* FALKWidgetBuilder::GetOverlayTexture(EOverlayTexture Type)
{
	// Generated once and shared; each atmosphere's UPROPERTY keeps the texture alive while it is shown.
	static TWeakObjectPtr<UTexture2D> Cache[static_cast<int32>(EOverlayTexture::Count)];
	TWeakObjectPtr<UTexture2D>& Cached = Cache[static_cast<int32>(Type)];
	if (!Cached.IsValid())
	{
		Cached = ALKWidgetBuilderPrivate::CreateOverlayTexture(Type);
	}
	return Cached.Get();
}

FSlateBrush FALKWidgetBuilder::MakeTextureBrush(UTexture2D* Texture, const FLinearColor& Tint, bool bTile)
{
#if WITH_EDITOR
	// Editor builds compile textures in the background and report a placeholder size until they are done; pictures
	// are sized (and kept in proportion) from the real one.
	if (Texture)
	{
		FTextureCompilingManager::Get().FinishCompilation({ Texture });
	}
#endif
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = bTile ? ESlateBrushTileType::Both : ESlateBrushTileType::NoTile;
	Brush.TintColor = FSlateColor(Tint);
	if (Texture)
	{
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
	}
	return Brush;
}

UImage* FALKWidgetBuilder::AddBackgroundLayers(UWidgetTree& Tree, UOverlay* Root, FName BaseName, bool bWithPicture)
{
	UBackgroundBlur* Blur = Make<UBackgroundBlur>(Tree, TEXT("GameBlur"));
	Blur->SetVisibility(ESlateVisibility::Collapsed);
	AddToOverlay(Root, Blur);
	UImage* Dim = Make<UImage>(Tree, TEXT("DimImage"));
	Dim->SetVisibility(ESlateVisibility::Collapsed);
	AddToOverlay(Root, Dim);

	UImage* Base = Make<UImage>(Tree, BaseName);
	AddToOverlay(Root, Base);
	if (bWithPicture)
	{
		UScaleBox* PictureScale = Make<UScaleBox>(Tree, TEXT("BackgroundPictureScale"));
		PictureScale->SetStretch(EStretch::ScaleToFill);
		UImage* Picture = Make<UImage>(Tree, TEXT("BackgroundPicture"));
		Picture->SetVisibility(ESlateVisibility::Collapsed);
		PictureScale->AddChild(Picture);
		AddToOverlay(Root, PictureScale);
	}
	UALKAtmosphere* Atmosphere = Make<UALKAtmosphere>(Tree, TEXT("Atmosphere"));
	AddToOverlay(Root, Atmosphere);
	UBackgroundBlur* SceneBlur = Make<UBackgroundBlur>(Tree, TEXT("SceneBlur"));
	SceneBlur->SetVisibility(ESlateVisibility::Collapsed);
	AddToOverlay(Root, SceneBlur);
	UImage* SceneDim = Make<UImage>(Tree, TEXT("SceneDim"));
	SceneDim->SetVisibility(ESlateVisibility::Collapsed);
	AddToOverlay(Root, SceneDim);
	return Base;
}

void FALKWidgetBuilder::StyleBackgroundLayers(UWidgetTree& Tree, const UALKTheme& Theme, UObject* Outer, TObjectPtr<UTexture2D>& InOutGradientTexture,
	TArray<TObjectPtr<UTexture2D>>& InOutOverlayTextures, float Opacity, FName BaseName, bool bOverGame, bool bDimScene, bool bKeyArt)
{
	// The lowest layer shown (the game dim over the game, the base otherwise) takes clicks on empty space, so they reach
	// the screen, which swallows them, instead of the game viewport behind the menu. Every layer above lets clicks through.
	if (UBackgroundBlur* Blur = Cast<UBackgroundBlur>(Tree.FindWidget(TEXT("GameBlur"))))
	{
		Blur->SetBlurStrength(Theme.PauseBlur);
		Blur->SetVisibility(bOverGame && Theme.PauseBlur > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UImage* Dim = Cast<UImage>(Tree.FindWidget(TEXT("DimImage"))))
	{
		Dim->SetBrush(UALKTheme::MakeSolidBrush(Theme.GameDim));
		Dim->SetVisibility(bOverGame ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (UImage* Base = Cast<UImage>(Tree.FindWidget(BaseName)))
	{
		Base->SetBrush(MakeBackgroundBrush(Theme, Outer, InOutGradientTexture));
		Base->SetRenderOpacity(Opacity);
		Base->SetVisibility(bOverGame ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// Key art from the menu config replaces the theme's backdrop on the screens that ask for it (the main menu); the
	// scrim and the vignette still go over it.
	InOutOverlayTextures.Reset();
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	UTexture2D* KeyArt = bKeyArt && !bOverGame && Config ? Config->MainMenuBackgroundImage.LoadSynchronous() : nullptr;
	UImage* Picture = Cast<UImage>(Tree.FindWidget(TEXT("BackgroundPicture")));
	if (Picture)
	{
		if (KeyArt)
		{
			InOutOverlayTextures.Add(KeyArt);
			Picture->SetBrush(MakeTextureBrush(KeyArt));
			Picture->SetRenderOpacity(Opacity);
		}
		Picture->SetVisibility(KeyArt ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (UALKAtmosphere* Atmosphere = Cast<UALKAtmosphere>(Tree.FindWidget(TEXT("Atmosphere"))))
	{
		const bool bCustomBrush = Theme.bUseBackgroundBrush && Theme.BackgroundBrush.GetResourceObject();
		Atmosphere->SetDrawScene(!bOverGame && !bCustomBrush && !(KeyArt && Picture));
		Atmosphere->SetSceneOpacity(Opacity);
		// The scrim is for menus on the open backdrop; pages dim the whole scene and the pause menu sits on a panel.
		Atmosphere->SetScrimOpacity(bOverGame || bDimScene ? 0.f : 1.f);
	}
	// Pages step the scene back: out of focus (Page Blur) and darker (Page Dim).
	if (UBackgroundBlur* SceneBlur = Cast<UBackgroundBlur>(Tree.FindWidget(TEXT("SceneBlur"))))
	{
		SceneBlur->SetBlurStrength(Theme.PageBlur);
		SceneBlur->SetVisibility(bDimScene && !bOverGame && Theme.PageBlur > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UImage* SceneDim = Cast<UImage>(Tree.FindWidget(TEXT("SceneDim"))))
	{
		SceneDim->SetBrush(UALKTheme::MakeSolidBrush(Theme.PageDim));
		SceneDim->SetVisibility(bDimScene && !bOverGame && Theme.PageDim.A > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UALKIconWidget* FALKWidgetBuilder::MakeIcon(UWidgetTree& Tree, FName Name, EALKIcon Icon, float Size)
{
	UALKIconWidget* Widget = Make<UALKIconWidget>(Tree, Name);
	Widget->Icon = Icon;
	Widget->IconSize = Size;
	return Widget;
}

UALKFrame* FALKWidgetBuilder::MakeChip(UWidgetTree& Tree, FName Name, const FText& Text, EALKIcon Icon)
{
	UALKFrame* Chip = Make<UALKFrame>(Tree, Name);
	Chip->SetPadding(FMargin(10.f, 3.f, 10.f, 4.f));
	Chip->SetHorizontalAlignment(HAlign_Center);
	Chip->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox* Row = Make<UHorizontalBox>(Tree);
	Chip->SetContent(Row);
	UALKIconWidget* IconWidget = MakeIcon(Tree, NAME_None, Icon, 15.f);
	IconWidget->SetVisibility(Icon == EALKIcon::None ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	AddToHorizontalBox(Row, IconWidget, FMargin(0.f, 0.f, 6.f, 0.f), HAlign_Left, VAlign_Center);
	AddToHorizontalBox(Row, MakeText(Tree, NAME_None, Text), FMargin(), HAlign_Left, VAlign_Center);
	return Chip;
}

void FALKWidgetBuilder::StyleChip(UALKFrame* Chip, const UALKTheme& Theme, const FLinearColor& Fill, const FLinearColor& Outline, const FLinearColor& TextColor)
{
	if (!Chip)
	{
		return;
	}
	Chip->SetFrameStyle(Theme.MakeChipFrame(Fill, Outline));
	if (const UHorizontalBox* Row = Cast<UHorizontalBox>(Chip->GetContent()))
	{
		for (UWidget* Child : Row->GetAllChildren())
		{
			if (UALKIconWidget* IconWidget = Cast<UALKIconWidget>(Child))
			{
				IconWidget->SetIcon(IconWidget->Icon, TextColor);
			}
			else if (UTextBlock* Text = Cast<UTextBlock>(Child))
			{
				StyleText(Text, Theme, EALKTextRole::Data, TextColor);
			}
		}
	}
}

void FALKWidgetBuilder::SetChipText(UALKFrame* Chip, const FText& Text)
{
	if (const UHorizontalBox* Row = Chip ? Cast<UHorizontalBox>(Chip->GetContent()) : nullptr)
	{
		for (UWidget* Child : Row->GetAllChildren())
		{
			if (UTextBlock* TextBlock = Cast<UTextBlock>(Child))
			{
				TextBlock->SetText(Text);
			}
		}
	}
}

UALKThumbnail* FALKWidgetBuilder::AddPanelBand(UWidgetTree& Tree, UALKFrame* Panel, UWidget* Content, const FMargin& ContentPadding, float Height, FName Name)
{
	if (!Panel || !Content)
	{
		return nullptr;
	}
	Panel->SetPadding(FMargin(0.f));
	UOverlay* Layers = Make<UOverlay>(Tree);
	Panel->SetContent(Layers);
	UALKThumbnail* Band = Make<UALKThumbnail>(Tree, Name);
	Band->ThumbnailSize = FVector2D(320.0, Height);
	Band->SetVisibility(ESlateVisibility::Collapsed);
	AddToOverlay(Layers, Band, HAlign_Fill, VAlign_Top);
	AddToOverlay(Layers, Content, HAlign_Fill, VAlign_Fill, ContentPadding);
	return Band;
}

void FALKWidgetBuilder::StylePanelBand(UALKThumbnail* Band, const UALKTheme& Theme)
{
	if (!Band)
	{
		return;
	}
	// Inside the panel's outline, with the panel's corners.
	const float Inset = Theme.PanelStyle == EALKPanelStyle::Flat ? 0.f : Theme.OutlineWidth;
	if (UOverlaySlot* BandSlot = Cast<UOverlaySlot>(Band->Slot))
	{
		BandSlot->SetPadding(FMargin(Inset, Inset, Inset, 0.f));
	}
	Band->SetCornerRadius(FMath::Max(0.f, Theme.CornerRadius - Inset));
	// Skins with the veil let the picture melt away completely; the others show it as a strip with a hard lower edge.
	const bool bSoft = Theme.PictureVeil > 0.f;
	Band->SetTint(FLinearColor(1.f, 1.f, 1.f, bSoft ? 0.7f : 1.f));
	Band->SetVeil(Theme.Panel.CopyWithNewOpacity(bSoft ? 1.f : 0.f));
}

void FALKWidgetBuilder::SetPanelBandPicture(UALKThumbnail* Band, UTexture2D* Picture)
{
	if (Band)
	{
		if (Picture)
		{
			Band->SetPicture(Picture);
		}
		Band->SetVisibility(Picture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

namespace ALKWidgetBuilderPrivate
{
	/** The disc over an empty state's line (the first child of the column both sit in), or null for a line on its own. */
	static UALKFrame* FindEmptyStateDisc(const UTextBlock* Line)
	{
		const UVerticalBox* Column = Line ? Cast<UVerticalBox>(Line->GetParent()) : nullptr;
		UALKFrame* Disc = Column && Column->GetChildrenCount() > 0 ? Cast<UALKFrame>(Column->GetChildAt(0)) : nullptr;
		return Disc && Cast<UALKIconWidget>(Disc->GetContent()) ? Disc : nullptr;
	}
}

UALKFrame* FALKWidgetBuilder::MakeIconDisc(UWidgetTree& Tree, FName Name, EALKIcon Icon, float Size)
{
	// The icon with enough padding round it to make the disc Size across.
	const float IconSize = FMath::RoundToFloat(Size * 0.46f);
	UALKFrame* Disc = Make<UALKFrame>(Tree, Name);
	Disc->SetPadding(FMargin((Size - IconSize) * 0.5f));
	Disc->SetHorizontalAlignment(HAlign_Center);
	Disc->SetVerticalAlignment(VAlign_Center);
	Disc->SetContent(MakeIcon(Tree, NAME_None, Icon, IconSize));
	Disc->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Disc;
}

void FALKWidgetBuilder::StyleIconDisc(UALKFrame* Disc, const UALKTheme& Theme)
{
	if (!Disc)
	{
		return;
	}
	// A quiet disc in the raised colour with the icon in the muted text colour: it says "nothing here", not "look here".
	// Round on skins with round corners (the frame keeps the radius to half its size), a small square on the sharp ones.
	FALKFrameStyle DiscStyle = Theme.MakeChipFrame(Theme.PanelRaised, Theme.PanelOutline);
	DiscStyle.Radius = Theme.CornerRadius * 4.f;
	DiscStyle.Gradient = Theme.PanelGradient;
	DiscStyle.TopEdge = Theme.PanelTopEdge;
	Disc->SetFrameStyle(DiscStyle);
	if (UALKIconWidget* Icon = Cast<UALKIconWidget>(Disc->GetContent()))
	{
		Icon->SetIcon(Icon->Icon, Theme.TextMuted);
	}
}

void FALKWidgetBuilder::SetIconDiscIcon(UALKFrame* Disc, EALKIcon Icon)
{
	if (UALKIconWidget* IconWidget = Disc ? Cast<UALKIconWidget>(Disc->GetContent()) : nullptr; IconWidget && IconWidget->Icon != Icon)
	{
		IconWidget->SetIcon(Icon, IconWidget->Color);
	}
}

UVerticalBox* FALKWidgetBuilder::MakeEmptyState(UWidgetTree& Tree, FName Name, EALKIcon Icon, UTextBlock* Line, float DiscSize)
{
	UVerticalBox* Column = Make<UVerticalBox>(Tree, Name);
	AddToVerticalBox(Column, MakeIconDisc(Tree, NAME_None, Icon, DiscSize), FMargin(0.f, 0.f, 0.f, FMath::RoundToFloat(DiscSize * 0.25f)), HAlign_Center);
	if (Line)
	{
		Line->SetJustification(ETextJustify::Center);
		// Fill, so a long line wraps inside the space the state gets.
		AddToVerticalBox(Column, Line, FMargin(), HAlign_Fill);
	}
	Column->SetVisibility(Line ? Line->GetVisibility() : ESlateVisibility::HitTestInvisible);
	return Column;
}

void FALKWidgetBuilder::StyleEmptyState(UTextBlock* Line, const UALKTheme& Theme)
{
	StyleIconDisc(ALKWidgetBuilderPrivate::FindEmptyStateDisc(Line), Theme);
}

void FALKWidgetBuilder::SetEmptyStateIcon(UTextBlock* Line, EALKIcon Icon)
{
	SetIconDiscIcon(ALKWidgetBuilderPrivate::FindEmptyStateDisc(Line), Icon);
}

void FALKWidgetBuilder::SetEmptyStateShown(UTextBlock* Line, bool bShown)
{
	if (!Line)
	{
		return;
	}
	const ESlateVisibility Wanted = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	UWidget* Target = Line;
	if (ALKWidgetBuilderPrivate::FindEmptyStateDisc(Line))
	{
		// The whole state shows and hides; its line stays ready inside it.
		Target = Line->GetParent();
		if (Line->GetVisibility() != ESlateVisibility::HitTestInvisible)
		{
			Line->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	if (Target->GetVisibility() != Wanted)
	{
		Target->SetVisibility(Wanted);
	}
}

UTexture2D* FALKWidgetBuilder::GetPlaceholderArt(FName Id)
{
	static TMap<FName, TWeakObjectPtr<UTexture2D>> Cache;
	if (const TWeakObjectPtr<UTexture2D>* Found = Cache.Find(Id); Found && Found->IsValid())
	{
		return Found->Get();
	}

	// The same picture for the same Id on every run and every machine.
	const int32 Seed = static_cast<int32>(FCrc::StrCrc32(*Id.ToString().ToLower()) & 0x7fffffff);
	struct FPalette
	{
		const TCHAR* Top;
		const TCHAR* Low;
		const TCHAR* Dark;
	};
	static const FPalette Palettes[] = {
		{ TEXT("27445C"), TEXT("E8A87C"), TEXT("1B2733") }, { TEXT("3A2C5A"), TEXT("F08A5D"), TEXT("1E1830") }, { TEXT("244B4A"), TEXT("A8D8B9"), TEXT("14282A") },
		{ TEXT("2B3A67"), TEXT("9DB4E8"), TEXT("161D33") }, { TEXT("5A3A2E"), TEXT("F2D0A4"), TEXT("2A1C17") }, { TEXT("36454F"), TEXT("C9D6DF"), TEXT("1C2429") }
	};
	const FPalette& Palette = Palettes[Seed % UE_ARRAY_COUNT(Palettes)];
	const FLinearColor Top = FLinearColor::FromSRGBColor(FColor::FromHex(Palette.Top));
	const FLinearColor Low = FLinearColor::FromSRGBColor(FColor::FromHex(Palette.Low));
	const FLinearColor Dark = FLinearColor::FromSRGBColor(FColor::FromHex(Palette.Dark));

	constexpr int32 Width = 320;
	constexpr int32 Height = 180;
	const float Horizon = Height * 0.62f;
	FRandomStream Random(Seed);
	const float SunX = Random.FRandRange(0.2f, 0.8f) * Width;
	const float SunRadius = Height * 0.13f;
	// Two rows of blocks along the horizon, the far row lighter.
	struct FBlock
	{
		float Left;
		float Right;
		float TopY;
		int32 Row;
	};
	TArray<FBlock> Blocks;
	for (int32 Row = 0; Row < 2; ++Row)
	{
		float X = -Random.FRandRange(0.f, 20.f);
		while (X < Width)
		{
			const float BlockWidth = Random.FRandRange(0.05f, 0.14f) * Width;
			const float BlockHeight = Random.FRandRange(0.12f, 0.42f - 0.12f * Row) * Height;
			Blocks.Add({ X, X + BlockWidth, Horizon - BlockHeight, Row });
			X += BlockWidth + Random.FRandRange(-0.01f, 0.03f) * Width;
		}
	}

	UTexture2D* Texture = ALKWidgetBuilderPrivate::CreateTexture(Width, Height, false, [&](int32 X, int32 Y)
	{
		FLinearColor Color;
		if (Y < Horizon)
		{
			Color = FMath::Lerp(Top, FMath::Lerp(Low, Top, 0.15f), static_cast<float>(Y) / Height);
			const float SunDistance = FVector2f(X - SunX, Y - (Horizon - SunRadius * 0.9f)).Size();
			Color = FMath::Lerp(Color, FMath::Lerp(Low, FLinearColor::White, 0.5f), 0.85f * FMath::Clamp(SunRadius - SunDistance + 0.5f, 0.f, 1.f));
			for (const FBlock& Block : Blocks)
			{
				if (X >= Block.Left && X < Block.Right && Y >= Block.TopY)
				{
					Color = FMath::Lerp(Low, Dark, Block.Row == 0 ? 0.45f : 0.8f);
				}
			}
		}
		else
		{
			// The floor: dark, with faint lines running to the horizon and across.
			Color = Dark;
			const float U = (Y - Horizon) / (Height - Horizon);
			const float Spacing = Width * (0.03f + 0.17f * U);
			const float Along = FMath::Abs(FMath::Frac((X - Width * 0.5f) / Spacing + 100.5f) - 0.5f) * Spacing;
			float Line = FMath::Clamp(1.f - Along, 0.f, 1.f);
			for (int32 Step = 1; Step <= 5; ++Step)
			{
				const float LineY = Horizon + (Height - Horizon) * FMath::Square(Step / 5.f);
				Line = FMath::Max(Line, FMath::Clamp(1.f - FMath::Abs(Y - LineY), 0.f, 1.f));
			}
			Color = FMath::Lerp(Color, Low, 0.16f * Line);
		}
		Color.A = 1.f;
		return Color.ToFColor(true);
	});
	Cache.Add(Id, Texture);
	return Texture;
}
