#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AGIComms.h"
#include "AGICoreCommunication.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAGIStateUpdate, const FAGIStateUpdateMessage&, StateUpdateMessage);

class AAGIPCGManager;
class UCognitiveCycleManager;
class UDeepTreeEchoCore;

/**
 * Relay between the cognitive core and the avatar.
 *
 * Every FAGIStateUpdateMessage broadcast through OnAGIStateUpdate comes from a
 * real producer:
 *   - ReceiveStateUpdate(): an external cognitive core (another process,
 *     network bridge, Blueprint) pushes a complete message. This is the only
 *     path that may carry PCG commands.
 *   - Per-tick snapshots of cognitive components found on CognitiveSourceActor
 *     (or on this actor, or on the first actor in the world that carries one):
 *     UCognitiveCycleManager supplies the three stream states, UDeepTreeEchoCore
 *     supplies nesting level, attention allocation and somatic markers.
 *
 * Nothing is synthesised here. When no source is present the actor broadcasts
 * nothing and logs a single operator-facing warning.
 */
UCLASS()
class DEEPTREEECHOAVATAR_API AAGICoreCommunication : public AActor
{
	GENERATED_BODY()
	
public:	
	AAGICoreCommunication();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintAssignable, Category = "AGI")
	FOnAGIStateUpdate OnAGIStateUpdate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI|PCG")
	AAGIPCGManager* PCGManager;

	/** Actor whose cognitive components feed the per-tick snapshot. Unset = this actor, then the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI|Source")
	AActor* CognitiveSourceActor;

	/** Read the bound cognitive components every SnapshotInterval seconds and broadcast their state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI|Source")
	bool bSnapshotCognitiveComponents;

	/** Seconds between component snapshots (0 = every tick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI|Source", meta = (ClampMin = "0.0"))
	float SnapshotInterval;

	/**
	 * Deliver a state update produced by the cognitive core. Broadcasts it to
	 * listeners and hands its PCG commands to PCGManager.
	 */
	UFUNCTION(BlueprintCallable, Category = "AGI")
	void ReceiveStateUpdate(const FAGIStateUpdateMessage& StateUpdateMessage);

	/** Re-run discovery of UCognitiveCycleManager / UDeepTreeEchoCore on the source actor. */
	UFUNCTION(BlueprintCallable, Category = "AGI|Source")
	void BindCognitiveSources();

	/** True when a cognitive component is bound for per-tick snapshots. */
	UFUNCTION(BlueprintPure, Category = "AGI|Source")
	bool HasCognitiveSource() const;

	/** The most recent message that was broadcast (empty until one arrives). */
	UFUNCTION(BlueprintPure, Category = "AGI")
	FAGIStateUpdateMessage GetLastStateUpdate() const { return LastStateUpdate; }

private:
	/** Fill OutMessage from the bound components. Returns false when nothing real could be read. */
	bool BuildMessageFromComponents(FAGIStateUpdateMessage& OutMessage) const;

	/** Broadcast and execute one real message. */
	void Dispatch(const FAGIStateUpdateMessage& Message);

	/** Normalised Shannon entropy (0-100) of a reservoir activation vector. */
	static float ComputeActivationEntropy(const TArray<float>& Activations);

	UPROPERTY()
	UCognitiveCycleManager* CycleManager;

	UPROPERTY()
	UDeepTreeEchoCore* EchoCore;

	FAGIStateUpdateMessage LastStateUpdate;

	float TimeAccumulator;
	float SnapshotAccumulator;
	bool bLoggedMissingSource;
};
