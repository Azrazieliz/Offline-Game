#include "World/OGTraversalFramework.h"

const FName FOGTraversalCapabilityIds::Sprint(TEXT("traversal.sprint"));
const FName FOGTraversalCapabilityIds::Climb(TEXT("traversal.climb"));
const FName FOGTraversalCapabilityIds::Swim(TEXT("traversal.swim"));
const FName FOGTraversalCapabilityIds::Dive(TEXT("traversal.dive"));
const FName FOGTraversalCapabilityIds::Flight(TEXT("traversal.flight"));
const FName FOGTraversalCapabilityIds::Mount(TEXT("traversal.mount"));
const FName FOGTraversalCapabilityIds::Vehicle(TEXT("traversal.vehicle"));
const FName FOGTraversalCapabilityIds::Teleport(TEXT("traversal.teleport"));
const FName FOGTraversalCapabilityIds::Phase(TEXT("traversal.phase"));
const FName FOGTraversalCapabilityIds::Tunnel(TEXT("traversal.tunnel"));
const FName FOGTraversalCapabilityIds::DimensionalTravel(TEXT("traversal.dimensional"));
const FName FOGTraversalCapabilityIds::PressureResistance(TEXT("environment.pressure_resistance"));
const FName FOGTraversalCapabilityIds::UnderwaterAdaptation(TEXT("environment.underwater_adaptation"));
const FName FOGTraversalCapabilityIds::VacuumSurvival(TEXT("environment.vacuum_survival"));
const FName FOGTraversalCapabilityIds::HeatResistance(TEXT("environment.heat_resistance"));
const FName FOGTraversalCapabilityIds::ColdResistance(TEXT("environment.cold_resistance"));
const FName FOGTraversalCapabilityIds::RadiationResistance(TEXT("environment.radiation_resistance"));
const FName FOGTraversalCapabilityIds::DimensionalStability(TEXT("environment.dimensional_stability"));
const FName FOGTraversalCapabilityIds::Authority(TEXT("world.authority"));
const FName FOGTraversalCapabilityIds::Coordinates(TEXT("world.coordinates"));
const FName FOGTraversalCapabilityIds::SafeTransport(TEXT("world.safe_transport"));
const FName FOGTraversalCapabilityIds::PhysicalForce(TEXT("world.physical_force"));

UOGTraversalCapabilityComponent::UOGTraversalCapabilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // Baseline mortal traversal. Exotic routes remain content-granted.
    Capabilities = {
        FOGTraversalCapabilityIds::Sprint,
        FOGTraversalCapabilityIds::Climb,
        FOGTraversalCapabilityIds::Swim,
        FOGTraversalCapabilityIds::Dive,
        FOGTraversalCapabilityIds::Mount,
        FOGTraversalCapabilityIds::Vehicle
    };
}

void UOGTraversalCapabilityComponent::SetTraversalMode(EOGTraversalMode NewMode)
{
    CurrentMode = NewMode;
}

bool UOGTraversalCapabilityComponent::HasCapability(FName CapabilityId) const
{
    return !CapabilityId.IsNone() && Capabilities.Contains(CapabilityId);
}

void UOGTraversalCapabilityComponent::GrantCapability(FName CapabilityId)
{
    if (!CapabilityId.IsNone())
    {
        Capabilities.AddUnique(CapabilityId);
    }
}

void UOGTraversalCapabilityComponent::RevokeCapability(FName CapabilityId)
{
    Capabilities.Remove(CapabilityId);
}

bool UOGTraversalCapabilityComponent::SatisfiesRequirement(
    const FOGTraversalRequirement& Requirement) const
{
    for (const FName Capability : Requirement.RequiredAll)
    {
        if (!HasCapability(Capability))
        {
            return false;
        }
    }

    if (Requirement.RequiredAny.IsEmpty())
    {
        return true;
    }

    for (const FName Capability : Requirement.RequiredAny)
    {
        if (HasCapability(Capability))
        {
            return true;
        }
    }

    return false;
}

FName UOGTraversalCapabilityComponent::RequiredCapabilityForMode(
    EOGTraversalMode Mode)
{
    switch (Mode)
    {
        case EOGTraversalMode::Climbing:
            return FOGTraversalCapabilityIds::Climb;
        case EOGTraversalMode::Swimming:
            return FOGTraversalCapabilityIds::Swim;
        case EOGTraversalMode::Diving:
            return FOGTraversalCapabilityIds::Dive;
        case EOGTraversalMode::Flying:
            return FOGTraversalCapabilityIds::Flight;
        case EOGTraversalMode::Mounted:
            return FOGTraversalCapabilityIds::Mount;
        case EOGTraversalMode::Vehicle:
            return FOGTraversalCapabilityIds::Vehicle;
        default:
            return NAME_None;
    }
}
