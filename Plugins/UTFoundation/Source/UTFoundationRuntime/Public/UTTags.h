// UT Templates - native gameplay tags owned by the library.
#pragma once

#include "NativeGameplayTags.h"

/**
 * Tags the UT library itself broadcasts or consumes. Declare them once here instead of
 * scattering RequestGameplayTag(TEXT("...")) strings through the code; game-specific
 * tags stay in the game.
 */
namespace UTTags
{
/** Cross-domain death notice (payload: FUTActorDiedMessage). Broadcast by UT health implementations. */
extern UTFOUNDATIONRUNTIME_API FNativeGameplayTag TAG_UT_Message_ActorDied;

/** Checkpoint adopted as respawn point (payload: FUTCheckpointReachedMessage). */
extern UTFOUNDATIONRUNTIME_API FNativeGameplayTag TAG_UT_Message_CheckpointReached;

/** Standard physical damage type for demo/example content. */
extern UTFOUNDATIONRUNTIME_API FNativeGameplayTag TAG_UT_Damage_Type_Kinetic;

/** Standard fire damage type for demo/example content. */
extern UTFOUNDATIONRUNTIME_API FNativeGameplayTag TAG_UT_Damage_Type_Fire;

/** Standard melee damage type for demo/example content. */
extern UTFOUNDATIONRUNTIME_API FNativeGameplayTag TAG_UT_Damage_Type_Melee;

/** Standard explosion damage type for demo/example content. */
extern UTFOUNDATIONRUNTIME_API FNativeGameplayTag TAG_UT_Damage_Type_Explosion;
} // namespace UTTags
