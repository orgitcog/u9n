#include "PhysicsDeformer.h"

UPhysicsDeformer::UPhysicsDeformer()
{
    ConstraintIterations = 3;
    bEnableCollisions = true;
    GroundPlaneHeight = 0.0f;
}

void UPhysicsDeformer::UpdatePhysicsSimulation(float DeltaTime)
{
    // Clamp delta time to prevent instability
    DeltaTime = FMath::Clamp(DeltaTime, 0.0f, 0.033f); // Max 30 FPS equivalent

    for (auto& Pair : PhysicsGroups)
    {
        FPhysicsGroup& Group = Pair.Value;

        // Update forces
        UpdateForces(Group, DeltaTime);

        // Integrate motion using Verlet integration
        IntegrateMotion(Group, DeltaTime);

        // Solve constraints multiple times for stability
        for (int32 i = 0; i < ConstraintIterations; i++)
        {
            SolveConstraints(Group);
        }

        // Apply collision detection
        if (bEnableCollisions)
        {
            ApplyCollisions(Group);
        }
    }
}

void UPhysicsDeformer::AddPhysicsGroup(const FName& GroupName, int32 ParticleCount)
{
    if (PhysicsGroups.Contains(GroupName))
    {
        UE_LOG(LogTemp, Warning, TEXT("Physics group already exists: %s"), *GroupName.ToString());
        return;
    }

    FPhysicsGroup NewGroup;
    NewGroup.GroupName = GroupName;
    NewGroup.Gravity = FVector(0.0f, 0.0f, -980.0f);
    NewGroup.Wind = FVector::ZeroVector;
    NewGroup.AirResistance = 0.1f;

    // Initialize particles
    for (int32 i = 0; i < ParticleCount; i++)
    {
        FPhysicsParticle Particle;
        Particle.Position = FVector::ZeroVector;
        Particle.Velocity = FVector::ZeroVector;
        Particle.Force = FVector::ZeroVector;
        Particle.Mass = 1.0f;
        Particle.Damping = 0.9f;
        Particle.bFixed = (i == 0); // First particle is typically fixed (root)

        NewGroup.Particles.Add(Particle);
    }

    // Create chain constraints
    for (int32 i = 0; i < ParticleCount - 1; i++)
    {
        FPhysicsConstraint Constraint;
        Constraint.ParticleIndexA = i;
        Constraint.ParticleIndexB = i + 1;
        Constraint.RestLength = 10.0f;
        Constraint.Stiffness = 0.8f;

        NewGroup.Constraints.Add(Constraint);
    }

    PhysicsGroups.Add(GroupName, NewGroup);

    UE_LOG(LogTemp, Log, TEXT("Physics group created: %s with %d particles"), *GroupName.ToString(), ParticleCount);
}

void UPhysicsDeformer::SetGravity(const FName& GroupName, const FVector& NewGravity)
{
    if (FPhysicsGroup* Group = PhysicsGroups.Find(GroupName))
    {
        Group->Gravity = NewGravity;
    }
}

void UPhysicsDeformer::SetWind(const FName& GroupName, const FVector& NewWind)
{
    if (FPhysicsGroup* Group = PhysicsGroups.Find(GroupName))
    {
        Group->Wind = NewWind;
    }
}

void UPhysicsDeformer::AddConstraint(const FName& GroupName, int32 ParticleA, int32 ParticleB, float RestLength, float Stiffness)
{
    if (FPhysicsGroup* Group = PhysicsGroups.Find(GroupName))
    {
        if (ParticleA >= 0 && ParticleA < Group->Particles.Num() &&
            ParticleB >= 0 && ParticleB < Group->Particles.Num())
        {
            FPhysicsConstraint Constraint;
            Constraint.ParticleIndexA = ParticleA;
            Constraint.ParticleIndexB = ParticleB;
            Constraint.RestLength = RestLength;
            Constraint.Stiffness = Stiffness;

            Group->Constraints.Add(Constraint);
        }
    }
}

FVector UPhysicsDeformer::GetParticlePosition(const FName& GroupName, int32 ParticleIndex) const
{
    if (const FPhysicsGroup* Group = PhysicsGroups.Find(GroupName))
    {
        if (ParticleIndex >= 0 && ParticleIndex < Group->Particles.Num())
        {
            return Group->Particles[ParticleIndex].Position;
        }
    }

    return FVector::ZeroVector;
}

void UPhysicsDeformer::SetParticlePosition(const FName& GroupName, int32 ParticleIndex, const FVector& Position)
{
    if (FPhysicsGroup* Group = PhysicsGroups.Find(GroupName))
    {
        if (ParticleIndex >= 0 && ParticleIndex < Group->Particles.Num())
        {
            Group->Particles[ParticleIndex].Position = Position;
            Group->Particles[ParticleIndex].Velocity = FVector::ZeroVector;
        }
    }
}

void UPhysicsDeformer::ResetPhysics()
{
    for (auto& Pair : PhysicsGroups)
    {
        FPhysicsGroup& Group = Pair.Value;

        for (FPhysicsParticle& Particle : Group.Particles)
        {
            Particle.Velocity = FVector::ZeroVector;
            Particle.Force = FVector::ZeroVector;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Physics simulation reset"));
}

void UPhysicsDeformer::UpdateForces(FPhysicsGroup& Group, float DeltaTime)
{
    for (FPhysicsParticle& Particle : Group.Particles)
    {
        if (Particle.bFixed)
            continue;

        // Reset forces
        Particle.Force = FVector::ZeroVector;

        // Apply gravity
        Particle.Force += Group.Gravity * Particle.Mass;

        // Apply wind force
        if (!Group.Wind.IsNearlyZero())
        {
            FVector WindForce = Group.Wind * (1.0f - Group.AirResistance);
            Particle.Force += WindForce;
        }

        // Apply air resistance (drag)
        FVector DragForce = -Particle.Velocity * Group.AirResistance * Particle.Mass;
        Particle.Force += DragForce;
    }
}

void UPhysicsDeformer::IntegrateMotion(FPhysicsGroup& Group, float DeltaTime)
{
    for (FPhysicsParticle& Particle : Group.Particles)
    {
        if (Particle.bFixed)
            continue;

        // Verlet integration
        FVector Acceleration = Particle.Force / Particle.Mass;
        
        // Update velocity
        Particle.Velocity += Acceleration * DeltaTime;
        
        // Apply damping
        Particle.Velocity *= Particle.Damping;

        // Update position
        Particle.Position += Particle.Velocity * DeltaTime;
    }
}

void UPhysicsDeformer::SolveConstraints(FPhysicsGroup& Group)
{
    for (const FPhysicsConstraint& Constraint : Group.Constraints)
    {
        int32 IndexA = Constraint.ParticleIndexA;
        int32 IndexB = Constraint.ParticleIndexB;

        if (IndexA < 0 || IndexA >= Group.Particles.Num() ||
            IndexB < 0 || IndexB >= Group.Particles.Num())
            continue;

        FPhysicsParticle& ParticleA = Group.Particles[IndexA];
        FPhysicsParticle& ParticleB = Group.Particles[IndexB];

        // Calculate distance between particles
        FVector Delta = ParticleB.Position - ParticleA.Position;
        float CurrentLength = Delta.Size();

        if (CurrentLength < KINDA_SMALL_NUMBER)
            continue;

        // Calculate correction
        float Difference = (CurrentLength - Constraint.RestLength) / CurrentLength;
        FVector Correction = Delta * Difference * Constraint.Stiffness * 0.5f;

        // Apply correction
        if (!ParticleA.bFixed)
        {
            ParticleA.Position += Correction;
        }

        if (!ParticleB.bFixed)
        {
            ParticleB.Position -= Correction;
        }
    }
}

void UPhysicsDeformer::ApplyCollisions(FPhysicsGroup& Group)
{
    for (FPhysicsParticle& Particle : Group.Particles)
    {
        if (Particle.bFixed)
            continue;

        // Ground plane collision
        if (Particle.Position.Z < GroundPlaneHeight)
        {
            Particle.Position.Z = GroundPlaneHeight;
            
            // Reflect velocity with energy loss
            Particle.Velocity.Z = -Particle.Velocity.Z * 0.5f;
            
            // Apply friction
            Particle.Velocity.X *= 0.8f;
            Particle.Velocity.Y *= 0.8f;
        }
    }
}
