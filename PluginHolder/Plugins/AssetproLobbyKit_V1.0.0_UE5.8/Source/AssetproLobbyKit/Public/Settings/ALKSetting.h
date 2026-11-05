// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "ALKTypes.h"
#include "ALKSetting.generated.h"

class ULocalPlayer;
class UEnhancedInputUserSettings;
class UEnhancedPlayerMappableKeyProfile;

UENUM(BlueprintType)
enum class EALKSettingType : uint8
{
	Options,
	Slider,
	KeyBinding,
	Action
};

/**
 * One entry in the settings screen. Settings are small view-model objects: they read and write their value through
 * getter / setter functions, and the setting rows display them. Create your own in C++ or define data-only ones with
 * Custom Settings in the menu config.
 */
UCLASS(Abstract, BlueprintType)
class ASSETPROLOBBYKIT_API UALKSetting : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	EALKSettingsCategory Category = EALKSettingsCategory::Gameplay;

	/** Group heading shown above the first setting of a section. */
	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	FText Section;

	/** Changes are saved on the spot (like Language), so they never count as unsaved changes waiting for Apply. */
	UPROPERTY(BlueprintReadWrite, Category = "Setting")
	bool bSavesImmediately = false;

	UFUNCTION(BlueprintPure, Category = "Setting")
	virtual EALKSettingType GetSettingType() const { return EALKSettingType::Options; }

	/** False when another setting makes this one irrelevant (e.g. resolution in windowed fullscreen). */
	UFUNCTION(BlueprintPure, Category = "Setting")
	bool IsEditable() const { return !EditCondition || EditCondition(); }

	UFUNCTION(BlueprintPure, Category = "Setting")
	virtual FText GetValueText() const { return FText::GetEmpty(); }

	UFUNCTION(BlueprintCallable, Category = "Setting")
	virtual void ResetToDefault() {}

	/** Tell listeners the value changed. */
	void NotifyChanged();

	/** Fired when any setting changes. Used by the settings screen to refresh every visible row. */
	static TMulticastDelegate<void(UALKSetting*)>& OnAnySettingChanged();

	TFunction<bool()> EditCondition;

	UALKSetting* Describe(FName InId, const FText& InName, const FText& InDescription, EALKSettingsCategory InCategory, const FText& InSection);
};

/** Pick one value from a list (window mode, quality levels, on/off...). */
UCLASS(BlueprintType)
class ASSETPROLOBBYKIT_API UALKSetting_Options : public UALKSetting
{
	GENERATED_BODY()

public:
	virtual EALKSettingType GetSettingType() const override { return EALKSettingType::Options; }
	virtual FText GetValueText() const override;
	virtual void ResetToDefault() override;

	UFUNCTION(BlueprintPure, Category = "Setting")
	int32 GetIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Setting")
	void SetIndex(int32 NewIndex);

	/** Moves to the previous (-1) or next (+1) option. */
	UFUNCTION(BlueprintCallable, Category = "Setting")
	void Cycle(int32 Direction);

	UFUNCTION(BlueprintPure, Category = "Setting")
	const TArray<FText>& GetOptions() const { return Options; }

	/** How many options the player can pick. Options past this are display-only states (e.g. "Custom"). */
	UFUNCTION(BlueprintPure, Category = "Setting")
	int32 GetSelectableCount() const { return SelectableCount > 0 ? FMath::Min(SelectableCount, Options.Num()) : Options.Num(); }

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	TArray<FText> Options;

	/** 0 = every option is selectable. */
	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	int32 SelectableCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	int32 DefaultIndex = 0;

	/** Wrap around at the ends when cycling. */
	UPROPERTY(BlueprintReadWrite, Category = "Setting")
	bool bWrap = false;

	/** An Off / On setting: its row shows a switch instead of the arrows. */
	UPROPERTY(BlueprintReadWrite, Category = "Setting")
	bool bIsToggle = false;

	TFunction<int32()> Getter;
	TFunction<void(int32)> Setter;
};

/** A numeric value with a range (volumes, sensitivity, FOV...). */
UCLASS(BlueprintType)
class ASSETPROLOBBYKIT_API UALKSetting_Slider : public UALKSetting
{
	GENERATED_BODY()

public:
	virtual EALKSettingType GetSettingType() const override { return EALKSettingType::Slider; }
	virtual FText GetValueText() const override;
	virtual void ResetToDefault() override;

	UFUNCTION(BlueprintPure, Category = "Setting")
	float GetValue() const;

	UFUNCTION(BlueprintCallable, Category = "Setting")
	void SetValue(float NewValue);

	UFUNCTION(BlueprintPure, Category = "Setting")
	float GetNormalizedValue() const;

	UFUNCTION(BlueprintCallable, Category = "Setting")
	void SetNormalizedValue(float Normalized);

	/** Moves by one step in Direction (-1 / +1). */
	UFUNCTION(BlueprintCallable, Category = "Setting")
	void StepValue(int32 Direction);

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	float MinValue = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	float MaxValue = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	float StepSize = 0.05f;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	float DefaultValue = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	EALKValueFormat Format = EALKValueFormat::Percent;

	/** Optional text appended to Integer / decimal values, e.g. a degree sign. */
	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	FText Suffix;

	TFunction<float()> Getter;
	TFunction<void(float)> Setter;
};

/** One rebindable Enhanced Input action (a player mappable key row). */
UCLASS(BlueprintType)
class ASSETPROLOBBYKIT_API UALKSetting_KeyBinding : public UALKSetting
{
	GENERATED_BODY()

public:
	static constexpr int32 NumSlots = 2;

	virtual EALKSettingType GetSettingType() const override { return EALKSettingType::KeyBinding; }
	virtual FText GetValueText() const override;
	virtual void ResetToDefault() override;

	/** Keyboard rows have a primary and secondary slot; gamepad rows have one. */
	UFUNCTION(BlueprintPure, Category = "Setting")
	int32 GetNumSlots() const { return bGamepad ? 1 : NumSlots; }

	UFUNCTION(BlueprintPure, Category = "Setting")
	FKey GetKey(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Setting")
	FText GetKeyText(int32 SlotIndex) const;

	/** Binds NewKey to a slot. Any other action using NewKey loses it, so keys never double up. */
	UFUNCTION(BlueprintCallable, Category = "Setting")
	bool SetKey(int32 SlotIndex, FKey NewKey);

	UFUNCTION(BlueprintCallable, Category = "Setting")
	void ClearKey(int32 SlotIndex);

	/** Is this row for gamepad keys (true) or keyboard and mouse (false)? */
	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	bool bGamepad = false;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	FName MappingName;

	TWeakObjectPtr<ULocalPlayer> OwningPlayer;

	static UEnhancedInputUserSettings* GetInputUserSettings(const ULocalPlayer* LocalPlayer);

	/** The key profile the player is using (UE 5.6 renamed GetCurrentKeyProfile to GetActiveKeyProfile). */
	static UEnhancedPlayerMappableKeyProfile* GetKeyProfile(const UEnhancedInputUserSettings* Settings);

private:
	void SaveInputSettings() const;
};

/** A button that runs something (auto-detect quality, reset bindings...). */
UCLASS(BlueprintType)
class ASSETPROLOBBYKIT_API UALKSetting_Action : public UALKSetting
{
	GENERATED_BODY()

public:
	virtual EALKSettingType GetSettingType() const override { return EALKSettingType::Action; }
	virtual FText GetValueText() const override { return ActionText; }

	UFUNCTION(BlueprintCallable, Category = "Setting")
	void Execute();

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	FText ActionText;

	/** Ask for confirmation before running. */
	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	bool bRequiresConfirmation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Setting")
	FText ConfirmMessage;

	TFunction<void()> Action;
};
