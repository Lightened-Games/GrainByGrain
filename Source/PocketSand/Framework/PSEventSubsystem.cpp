// Pocket Sand - event bus.

#include "Framework/PSEventSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UPSEventSubsystem* UPSEventSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UPSEventSubsystem>() : nullptr;
}
