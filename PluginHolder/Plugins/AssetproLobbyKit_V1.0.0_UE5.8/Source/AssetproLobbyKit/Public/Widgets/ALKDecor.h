// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Styling/SlateBrush.h"
#include "ALKDecor.generated.h"

class SALKDecor;
class UTexture2D;

/** Small icons drawn in code (no textures), in any colour and size. */
UENUM(BlueprintType)
enum class EALKIcon : uint8
{
	None,
	/** A diamond in a diamond: checkpoints. */
	Checkpoint,
	/** An open padlock: something unlocked. */
	Unlock,
	/** A disc with a notch: saving and saved. */
	Save,
	/** A triangle with a bar: warnings. */
	Alert,
	/** A rounded square with an "i": information. */
	Info,
	/** Rising bars: objectives, signals. */
	Signal,
	/** A tick: ready, done, accepted. */
	Check,
	Cross,
	Plus,
	ChevronRight,
	ChevronLeft,
	ChevronDown,
	ChevronUp,
	/** The host or party leader. */
	Crown,
	/** A favourite, or the most valuable player. */
	Star,
	StarOutline,
	/** A closed padlock: password protected, locked. */
	Lock,
	Microphone,
	Speaker,
	SpeakerMuted,
	/** One person: a player, or an avatar placeholder. */
	User,
	/** Two people: a party, friends. */
	Users,
	Search,
	Refresh,
	/** Four connection bars; Level (0-4) lights them. */
	PingBars,
	/** A generic platform badge: put your platform's own icon in its place. */
	Platform,
	/** Spectating. */
	Eye,
	/** Report a player; objectives. */
	Flag,
	/** Quick match. */
	Bolt,
	/** A server: host a game. */
	Server,
	/** A list: browse games. */
	List,
	/** Join by code. */
	Hash,
	Play,
	Pause,
	/** Region. */
	Globe,
	Clock,
	Copy,
	/** A crosshair ring: the kill feed. */
	Target,
	/** A filled dot: presence, status. */
	Dot,
	/** Three dots: more options. */
	More
};

/** What a decor widget draws. */
enum class EALKDecorKind : uint8
{
	Rule,
	ValueBar,
	Spinner,
	Icon,
	Avatar,
	Switch
};

/** Everything a decor widget needs to paint itself. */
struct FALKDecorStyle
{
	EALKDecorKind Kind = EALKDecorKind::Rule;
	FLinearColor Color = FLinearColor::White;
	FLinearColor SecondaryColor = FLinearColor(1.f, 1.f, 1.f, 0.35f);
	FLinearColor TertiaryColor = FLinearColor::Transparent;
	FLinearColor QuaternaryColor = FLinearColor::Transparent;
	/** Avatar: the colour behind it (the presence dot is cut out with it). */
	FLinearColor BackColor = FLinearColor::Black;
	float Thickness = 1.f;
	/** Value bar: 0..1. Switch: where the knob is, 0 (off) to 1 (on). */
	float Value = 0.f;
	bool bKnob = false;
	/** Corner radius of the switch and its knob; below 0 for fully round. */
	float Radius = -1.f;
	EALKIcon Icon = EALKIcon::None;
	/** Ping bars: how many of the four are lit. */
	int32 Level = 4;
	/** Avatar: a picture drawn instead of the placeholder. */
	FSlateBrush Picture;
	FVector2D DesiredSize = FVector2D(32.0, 32.0);
};

/** A thin line across its width: separators under headers and between rows. Stretch it horizontally. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKRule : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FLinearColor Color = FLinearColor(1.f, 1.f, 1.f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (ClampMin = "0.5"))
	float Thickness = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetRuleColor(const FLinearColor& InColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	TSharedPtr<SALKDecor> MyDecor;
};

/** A rounded track filled from the left to show a value from 0 to 1: sliders (with a knob), progress, hold timers. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKValueBar : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value Bar", meta = (ClampMin = "0", ClampMax = "1"))
	float Value = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value Bar")
	FLinearColor FillColor = FLinearColor::White;

	/** Colour of the empty part of the track. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value Bar")
	FLinearColor TrackColor = FLinearColor(1.f, 1.f, 1.f, 0.25f);

	/** A round handle at the value (sliders). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value Bar")
	bool bShowKnob = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value Bar", meta = (EditCondition = "bShowKnob"))
	FLinearColor KnobColor = FLinearColor::White;

	/** Height of the track in pixels (the widget itself is as tall as the knob). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value Bar", meta = (ClampMin = "1"))
	float TrackHeight = 6.f;

	/** Size the bar asks for (a stretched slot overrides it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value Bar")
	FVector2D BarSize = FVector2D(300.0, 20.0);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetValue(float InValue);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetColors(const FLinearColor& InFill, const FLinearColor& InTrack);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetKnobColor(const FLinearColor& InKnob);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	void Push();
	TSharedPtr<SALKDecor> MyDecor;
};

/** A ring of dots turning round: saving, searching, connecting. It keeps turning while the game is paused. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKSpinner : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spinner")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spinner", meta = (ClampMin = "8"))
	float Diameter = 30.f;

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetSpinnerColor(const FLinearColor& InColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	TSharedPtr<SALKDecor> MyDecor;
};

/** A small icon drawn in code (see EALKIcon), in any colour and size. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKIconWidget : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	EALKIcon Icon = EALKIcon::Check;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon", meta = (ClampMin = "8"))
	float IconSize = 24.f;

	/** Ping Bars: how many of the four bars are lit (0-4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon", meta = (ClampMin = "0", ClampMax = "4"))
	int32 Level = 4;

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetIcon(EALKIcon InIcon, const FLinearColor& InColor);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetLevel(int32 InLevel);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	TSharedPtr<SALKDecor> MyDecor;
};

/**
 * A round player picture. Without a picture it shows one of the menu config's Default Avatars, picked by the player's
 * name (always the same for that name); when that list is empty, or in streamer mode, it draws a placeholder: a disc in
 * a colour picked from the name with a person on it. A ring shows who is talking or the team; a small dot shows presence.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKAvatar : public UWidget
{
	GENERATED_BODY()

public:
	/** Picks the placeholder's colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
	FString PlayerName;

	/** The player's picture (from your platform or profile service). Empty shows the placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
	TObjectPtr<UTexture2D> Picture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar", meta = (ClampMin = "12"))
	float Diameter = 36.f;

	/** Ring around the picture (talking, team colour). Alpha 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
	FLinearColor RingColor = FLinearColor::Transparent;

	/** Presence dot at the lower right. Alpha 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
	FLinearColor StatusColor = FLinearColor::Transparent;

	/** Colour behind the avatar: the placeholder blends towards it and the presence dot is cut out with it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
	FLinearColor BackColor = FLinearColor(0.02f, 0.02f, 0.03f, 1.f);

	/** Hides who the player is (streamer mode): a grey placeholder, whatever the name or picture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
	bool bAnonymous = false;

	/** Without a picture, show one of the menu config's Default Avatars for the name instead of the drawn placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar")
	bool bUseDefaultAvatars = true;

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetPlayer(const FString& InPlayerName, UTexture2D* InPicture = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetRingColor(const FLinearColor& InColor);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetStatusColor(const FLinearColor& InColor);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetBackColor(const FLinearColor& InColor);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetAnonymous(bool bInAnonymous);

	/** The placeholder colour for a name: one of eight calm colours, always the same for the same name. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Decor")
	static FLinearColor GetPlaceholderColor(const FString& InPlayerName);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	TSharedPtr<SALKDecor> MyDecor;

	/** The default avatar picked for the name, kept alive while it is shown. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultPicture;
};

/** An on/off switch: a pill with a knob that slides across. It only shows a state; its row or button handles input. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKSwitch : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	bool bOn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	FLinearColor OnColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	FLinearColor OffColor = FLinearColor(1.f, 1.f, 1.f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	FLinearColor KnobOnColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	FLinearColor KnobOffColor = FLinearColor::White;

	/** Corner radius; below 0 for a fully round pill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	float CornerRadius = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch")
	FVector2D SwitchSize = FVector2D(44.0, 24.0);

	/** Turns the switch on or off; the knob slides unless bInstant (or Reduce Effects) is set. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetOn(bool bInOn, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Decor")
	void SetColors(const FLinearColor& InOn, const FLinearColor& InOff, const FLinearColor& InKnobOn, const FLinearColor& InKnobOff);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	void Push(bool bInstant);
	TSharedPtr<SALKDecor> MyDecor;
};
