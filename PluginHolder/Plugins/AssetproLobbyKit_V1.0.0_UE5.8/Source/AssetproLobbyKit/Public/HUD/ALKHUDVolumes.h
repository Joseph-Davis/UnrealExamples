// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Widgets/ALKDecor.h"
#include "ALKHUDVolumes.generated.h"

class UBoxComponent;
class UInputAction;
class APlayerController;
struct FInputActionInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnInteracted, APlayerController*, PlayerController);

/** Base of the HUD volumes: a box that notices the local player's pawn entering and leaving. */
UCLASS(Abstract, NotBlueprintable)
class ASSETPROLOBBYKIT_API AALKHUDVolumeBase : public AActor
{
	GENERATED_BODY()

public:
	AALKHUDVolumeBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Volume")
	TObjectPtr<UBoxComponent> Box;

protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	/** The local player's controller when Actor is the pawn it controls, else null. */
	static APlayerController* GetLocalController(AActor* Actor);

	virtual void OnPlayerEntered(APlayerController* PlayerController) {}
	virtual void OnPlayerLeft(APlayerController* PlayerController) {}
};

/**
 * Shows an interaction prompt ("[E] OPEN") while the player's pawn is inside the box, and fires On Interacted when the
 * player presses the key. With an Input Action the prompt shows the key it is bound to (following rebinding and the
 * gamepad); without one it listens for Keyboard Key and Gamepad Key. Drop it on a door, a terminal, a lever.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKInteractionVolume : public AALKHUDVolumeBase
{
	GENERATED_BODY()

public:
	/** The input the player presses. Leave empty to use the two keys below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UInputAction> Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FKey KeyboardKey = EKeys::E;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FKey GamepadKey = EKeys::Gamepad_FaceButton_Left;

	/** What pressing does: "Open", "Talk", "Pick Up". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText ActionText;

	/** Optional small line over the prompt naming the thing ("Maintenance door"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText SubjectText;

	/** Where the prompt points, from the volume's origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (MakeEditWidget = true))
	FVector PromptOffset = FVector(0.0, 0.0, 120.0);

	/** Works once, then the prompt goes for good (a door that stays open). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bOnlyOnce = false;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FALKOnInteracted OnInteracted;

	/** Turns the volume on or off (off hides its prompt). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void SetInteractionEnabled(bool bInEnabled);

protected:
	virtual void OnPlayerEntered(APlayerController* PlayerController) override;
	virtual void OnPlayerLeft(APlayerController* PlayerController) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Interact();
	void HandleAction(const FInputActionInstance& Instance);
	void ShowPrompt();

	TWeakObjectPtr<APlayerController> Player;
	bool bEnabled = true;
	bool bBound = false;
};

/** One line of a subtitle trigger. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKSubtitleLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle")
	FText Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle", meta = (MultiLine = true))
	FText Line;

	/** Seconds on screen; 0 picks a time from the line's length. The next line follows it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitle", meta = (ClampMin = "0"))
	float Duration = 0.f;
};

/** Plays its subtitle lines one after the other when the player's pawn first enters the box. */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKSubtitleTrigger : public AALKHUDVolumeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitles")
	TArray<FALKSubtitleLine> Lines;

	/** Plays every time the player enters, not only the first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subtitles")
	bool bRepeat = false;

	/** Plays the lines now, wherever the player is. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|HUD")
	void PlayLines();

protected:
	virtual void OnPlayerEntered(APlayerController* PlayerController) override;

private:
	void PlayNext();

	FTimerHandle NextLineTimer;
	int32 NextLine = 0;
	bool bPlayed = false;
};

/** Shows a notification ("Checkpoint / Progress saved" by default) and the saving indicator when the player first enters. */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKCheckpointVolume : public AALKHUDVolumeBase
{
	GENERATED_BODY()

public:
	/** Leave both empty for the translated "Checkpoint / Progress saved". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	FText Detail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	EALKIcon Icon = EALKIcon::Checkpoint;

	/** Autosaves when reached (with the menu config's Autosave At Checkpoints on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	bool bAutosave = true;

	/** Fired when the player reaches the checkpoint (after the autosave has started). */
	UPROPERTY(BlueprintAssignable, Category = "Checkpoint")
	FALKOnInteracted OnReached;

protected:
	virtual void OnPlayerEntered(APlayerController* PlayerController) override;

private:
	bool bReached = false;
};
