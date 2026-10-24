// UT Templates - UTWorldMechanicsRuntime module.
#include "UTCollectibleLog.h"
#include "UTPickupSpawnerLog.h"
#include "UTRespawnLog.h"
#include "UTCheckpointLog.h"
#include "UTMovingPlatformLog.h"
#include "UTJumpPadLog.h"
#include "UTTeleporterLog.h"
#include "UTTriggerVolumeLog.h"
#include "UTPressurePlateLog.h"
#include "UTLeverLog.h"
#include "UTDoorLog.h"
#include "Modules/ModuleManager.h"
DEFINE_LOG_CATEGORY(LogUTDoor);
DEFINE_LOG_CATEGORY(LogUTLever);
DEFINE_LOG_CATEGORY(LogUTPressurePlate);
DEFINE_LOG_CATEGORY(LogUTTriggerVolume);
DEFINE_LOG_CATEGORY(LogUTTeleporter);
DEFINE_LOG_CATEGORY(LogUTJumpPad);
DEFINE_LOG_CATEGORY(LogUTMovingPlatform);
DEFINE_LOG_CATEGORY(LogUTCheckpoint);
DEFINE_LOG_CATEGORY(LogUTRespawn);
DEFINE_LOG_CATEGORY(LogUTPickupSpawner);
DEFINE_LOG_CATEGORY(LogUTCollectible);
IMPLEMENT_MODULE(FDefaultModuleImpl, UTWorldMechanicsRuntime);
