// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Settings/ALKSetting.h"
#include "AssetproLobbyKit.h"
#include "Widgets/ALKInputLabels.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Engine/LocalPlayer.h"
#include "GameplayTagContainer.h"
#include "Misc/EngineVersionComparison.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSetting)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

// ------------------------------------------------------------------------------------------------ UALKSetting

void UALKSetting::NotifyChanged()
{
	OnAnySettingChanged().Broadcast(this);
}

TMulticastDelegate<void(UALKSetting*)>& UALKSetting::OnAnySettingChanged()
{
	static TMulticastDelegate<void(UALKSetting*)> Delegate;
	return Delegate;
}

UALKSetting* UALKSetting::Describe(FName InId, const FText& InName, const FText& InDescription, EALKSettingsCategory InCategory, const FText& InSection)
{
	Id = InId;
	DisplayName = InName;
	Description = InDescription;
	Category = InCategory;
	Section = InSection;
	return this;
}

// ------------------------------------------------------------------------------------------------ Options

int32 UALKSetting_Options::GetIndex() const
{
	if (Options.Num() == 0)
	{
		return INDEX_NONE;
	}
	const int32 Index = Getter ? Getter() : DefaultIndex;
	return FMath::Clamp(Index, 0, Options.Num() - 1);
}

void UALKSetting_Options::SetIndex(int32 NewIndex)
{
	const int32 Selectable = GetSelectableCount();
	if (Selectable == 0 || !IsEditable())
	{
		return;
	}
	NewIndex = FMath::Clamp(NewIndex, 0, Selectable - 1);
	if (NewIndex != GetIndex())
	{
		if (Setter)
		{
			Setter(NewIndex);
		}
		NotifyChanged();
	}
}

void UALKSetting_Options::Cycle(int32 Direction)
{
	const int32 Selectable = GetSelectableCount();
	if (Selectable == 0)
	{
		return;
	}
	// From a display-only state (e.g. "Custom") step onto the nearest selectable option.
	const int32 Current = FMath::Min(GetIndex(), Selectable);
	int32 NewIndex = Current + FMath::Sign(Direction);
	if (bWrap)
	{
		NewIndex = (NewIndex + Selectable) % Selectable;
	}
	SetIndex(NewIndex);
}

FText UALKSetting_Options::GetValueText() const
{
	const int32 Index = GetIndex();
	return Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty();
}

void UALKSetting_Options::ResetToDefault()
{
	SetIndex(DefaultIndex);
}

// ------------------------------------------------------------------------------------------------ Slider

float UALKSetting_Slider::GetValue() const
{
	return FMath::Clamp(Getter ? Getter() : DefaultValue, MinValue, MaxValue);
}

void UALKSetting_Slider::SetValue(float NewValue)
{
	if (!IsEditable())
	{
		return;
	}

	if (StepSize > 0.f)
	{
		NewValue = MinValue + FMath::RoundToFloat((NewValue - MinValue) / StepSize) * StepSize;
	}
	NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);

	if (!FMath::IsNearlyEqual(NewValue, GetValue(), KINDA_SMALL_NUMBER))
	{
		if (Setter)
		{
			Setter(NewValue);
		}
		NotifyChanged();
	}
}

float UALKSetting_Slider::GetNormalizedValue() const
{
	const float Range = MaxValue - MinValue;
	return Range > 0.f ? (GetValue() - MinValue) / Range : 0.f;
}

void UALKSetting_Slider::SetNormalizedValue(float Normalized)
{
	SetValue(FMath::Lerp(MinValue, MaxValue, FMath::Clamp(Normalized, 0.f, 1.f)));
}

void UALKSetting_Slider::StepValue(int32 Direction)
{
	const float Step = StepSize > 0.f ? StepSize : (MaxValue - MinValue) / 20.f;
	SetValue(GetValue() + Step * FMath::Sign(Direction));
}

FText UALKSetting_Slider::GetValueText() const
{
	const float Value = GetValue();
	FNumberFormattingOptions Options;
	Options.UseGrouping = false;

	switch (Format)
	{
	case EALKValueFormat::Percent:
		Options.MaximumFractionalDigits = 0;
		return FText::AsPercent(Value, &Options);
	case EALKValueFormat::Integer:
		Options.MaximumFractionalDigits = 0;
		return FText::Format(LOCTEXT("ValueWithSuffix", "{0}{1}"), FText::AsNumber(FMath::RoundToInt(Value), &Options), Suffix);
	case EALKValueFormat::OneDecimal:
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 1;
		return FText::Format(LOCTEXT("ValueWithSuffix", "{0}{1}"), FText::AsNumber(Value, &Options), Suffix);
	case EALKValueFormat::TwoDecimals:
	default:
		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;
		return FText::Format(LOCTEXT("ValueWithSuffix", "{0}{1}"), FText::AsNumber(Value, &Options), Suffix);
	}
}

void UALKSetting_Slider::ResetToDefault()
{
	SetValue(DefaultValue);
}

// ------------------------------------------------------------------------------------------------ Key binding

namespace ALKKeyBindingPrivate
{
	static bool IsGamepadMapping(const FPlayerKeyMapping& Mapping)
	{
		const FKey& DefaultKey = Mapping.GetDefaultKey();
		return DefaultKey.IsValid() ? DefaultKey.IsGamepadKey() : Mapping.GetCurrentKey().IsGamepadKey();
	}

	/** Mappings of one device type in a row, ordered by slot. */
	static TArray<const FPlayerKeyMapping*> GetDeviceMappings(const UEnhancedInputUserSettings* Settings, FName MappingName, bool bGamepad)
	{
		TArray<const FPlayerKeyMapping*> Result;
		if (!Settings)
		{
			return Result;
		}
		const UEnhancedPlayerMappableKeyProfile* Profile = UALKSetting_KeyBinding::GetKeyProfile(Settings);
		if (!Profile)
		{
			return Result;
		}
		if (const FKeyMappingRow* Row = Profile->GetPlayerMappingRows().Find(MappingName))
		{
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (IsGamepadMapping(Mapping) == bGamepad)
				{
					Result.Add(&Mapping);
				}
			}
		}
		Result.Sort([](const FPlayerKeyMapping& A, const FPlayerKeyMapping& B)
		{
			return static_cast<uint8>(A.GetSlot()) < static_cast<uint8>(B.GetSlot());
		});
		return Result;
	}

	/** The slot to write for a slot index: the existing mapping's slot, or the first slot unused in the row. */
	static EPlayerMappableKeySlot ResolveSlot(const UEnhancedInputUserSettings* Settings, FName MappingName, bool bGamepad, int32 SlotIndex)
	{
		const TArray<const FPlayerKeyMapping*> DeviceMappings = GetDeviceMappings(Settings, MappingName, bGamepad);
		if (DeviceMappings.IsValidIndex(SlotIndex))
		{
			return DeviceMappings[SlotIndex]->GetSlot();
		}

		TSet<EPlayerMappableKeySlot> UsedSlots;
		if (const UEnhancedPlayerMappableKeyProfile* Profile = UALKSetting_KeyBinding::GetKeyProfile(Settings))
		{
			if (const FKeyMappingRow* Row = Profile->GetPlayerMappingRows().Find(MappingName))
			{
				for (const FPlayerKeyMapping& Mapping : Row->Mappings)
				{
					UsedSlots.Add(Mapping.GetSlot());
				}
			}
		}
		for (uint8 Slot = static_cast<uint8>(EPlayerMappableKeySlot::First); Slot <= static_cast<uint8>(EPlayerMappableKeySlot::Seventh); ++Slot)
		{
			if (!UsedSlots.Contains(static_cast<EPlayerMappableKeySlot>(Slot)))
			{
				return static_cast<EPlayerMappableKeySlot>(Slot);
			}
		}
		return EPlayerMappableKeySlot::Second;
	}
}

UEnhancedInputUserSettings* UALKSetting_KeyBinding::GetInputUserSettings(const ULocalPlayer* LocalPlayer)
{
	if (LocalPlayer)
	{
		if (const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			return InputSubsystem->GetUserSettings();
		}
	}
	return nullptr;
}

UEnhancedPlayerMappableKeyProfile* UALKSetting_KeyBinding::GetKeyProfile(const UEnhancedInputUserSettings* Settings)
{
	if (!Settings)
	{
		return nullptr;
	}
#if UE_VERSION_OLDER_THAN(5, 6, 0)
	return Settings->GetCurrentKeyProfile();
#else
	return Settings->GetActiveKeyProfile();
#endif
}

FKey UALKSetting_KeyBinding::GetKey(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= GetNumSlots())
	{
		return EKeys::Invalid;
	}
	const TArray<const FPlayerKeyMapping*> Mappings = ALKKeyBindingPrivate::GetDeviceMappings(GetInputUserSettings(OwningPlayer.Get()), MappingName, bGamepad);
	return Mappings.IsValidIndex(SlotIndex) ? Mappings[SlotIndex]->GetCurrentKey() : EKeys::Invalid;
}

FText UALKSetting_KeyBinding::GetKeyText(int32 SlotIndex) const
{
	return FALKInputLabels::GetKeyLabel(GetKey(SlotIndex), OwningPlayer.Get());
}

FText UALKSetting_KeyBinding::GetValueText() const
{
	return GetKeyText(0);
}

bool UALKSetting_KeyBinding::SetKey(int32 SlotIndex, FKey NewKey)
{
	UEnhancedInputUserSettings* Settings = GetInputUserSettings(OwningPlayer.Get());
	if (!GetKeyProfile(Settings) || SlotIndex < 0 || SlotIndex >= GetNumSlots())
	{
		return false;
	}

	const EPlayerMappableKeySlot TargetSlot = ALKKeyBindingPrivate::ResolveSlot(Settings, MappingName, bGamepad, SlotIndex);

	// Take the key away from any other action (or other slot) that uses it.
	if (NewKey.IsValid())
	{
		TArray<TPair<FName, EPlayerMappableKeySlot>> Conflicts;
		for (const TPair<FName, FKeyMappingRow>& Row : GetKeyProfile(Settings)->GetPlayerMappingRows())
		{
			for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
			{
				const bool bSameSlot = Row.Key == MappingName && Mapping.GetSlot() == TargetSlot;
				if (!bSameSlot && Mapping.GetCurrentKey() == NewKey)
				{
					Conflicts.Emplace(Row.Key, Mapping.GetSlot());
				}
			}
		}
		for (const TPair<FName, EPlayerMappableKeySlot>& Conflict : Conflicts)
		{
			FMapPlayerKeyArgs ClearArgs;
			ClearArgs.MappingName = Conflict.Key;
			ClearArgs.Slot = Conflict.Value;
			ClearArgs.NewKey = EKeys::Invalid;
			FGameplayTagContainer IgnoredFailure;
			Settings->MapPlayerKey(ClearArgs, IgnoredFailure);
		}
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = TargetSlot;
	Args.NewKey = NewKey;
	FGameplayTagContainer FailureReason;
	Settings->MapPlayerKey(Args, FailureReason);

	if (!FailureReason.IsEmpty())
	{
		UE_LOG(LogALK, Warning, TEXT("Could not bind %s to %s: %s"), *NewKey.ToString(), *MappingName.ToString(), *FailureReason.ToString());
	}

	SaveInputSettings();
	NotifyChanged();
	return FailureReason.IsEmpty();
}

void UALKSetting_KeyBinding::ClearKey(int32 SlotIndex)
{
	SetKey(SlotIndex, EKeys::Invalid);
}

void UALKSetting_KeyBinding::ResetToDefault()
{
	UEnhancedInputUserSettings* Settings = GetInputUserSettings(OwningPlayer.Get());
	if (!Settings)
	{
		return;
	}

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	FGameplayTagContainer FailureReason;
	Settings->ResetAllPlayerKeysInRow(Args, FailureReason);
	SaveInputSettings();
	NotifyChanged();
}

void UALKSetting_KeyBinding::SaveInputSettings() const
{
	if (UEnhancedInputUserSettings* Settings = GetInputUserSettings(OwningPlayer.Get()))
	{
		Settings->ApplySettings();
		Settings->AsyncSaveSettings();
	}
}

// ------------------------------------------------------------------------------------------------ Action

void UALKSetting_Action::Execute()
{
	if (Action && IsEditable())
	{
		Action();
		NotifyChanged();
	}
}

#undef LOCTEXT_NAMESPACE
