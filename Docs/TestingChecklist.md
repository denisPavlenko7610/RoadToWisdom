# Testing Checklist — Quiet Road vertical slice

Status vocabulary (per AGENTS.md): `implemented` → `compiled` → `editor-verified` →
`manual-check-required`. Everything below is honestly labeled as of 2026-10-24.

## Automated checks (executed in this session)

| Check | Command / how | Result |
|---|---|---|
| C++ editor target builds | `Build.bat RoadToWisdomEditor Win64 Development` | **compiled** — Result: Succeeded |
| Plugin closure compiles | same build (all 7 UT modules linked) | **compiled** |
| Prototype map exists and loads in the editor | `Tools/Editor/VerifyPrototype.py` (`-ExecutePythonScript`) | **editor-verified** — `[VERIFY-RESULT] PASSED`: van BP, fuse box, lever, world item (Item.Fuse ×1), mission manager with wired refs, lights, player start |
| Input assets resolve at CDO construction | same run, log checked | **editor-verified** — 0 «Failed to find» CDO errors |
| Game-mode boot of the slice map | `-game -NullRHI -ExecCmds="quit"` on `L_QuietRoad_Prototype` | **editor-verified** — clean exit 0, no ensures/exceptions (after the UFUNCTION fix below) |
| Content budget | `python Tools/CheckAssetBudget.py` | **passed** — 97.0 MiB / 250 MiB |

## Bugs found and fixed by the automated runs

- `ARoadToWisdomMissionManager::HandleFuseInstalled` / `HandleLeverSwitched` were not
  `UFUNCTION()`-marked → ensure on dynamic delegate bind, caught by the `-game` smoke
  run, fixed and re-verified clean.

## PIE scenarios (GDD §5.4) — `manual-check-required`

Not executed in this session (no human in PIE; Unreal MCP server was down because the
editor was closed). Checklist to run in the editor:

1. **Driving → parked → exit → on foot → enter → driving.**
   Start on foot near the van → F to enter → drive → stop → F to exit → walk → F to
   re-enter. Van must not be recreated (same actor instance; handbrake released on
   enter).
2. **Exit while moving is forbidden** — hint «Остановите фургон, чтобы выйти», no pawn
   switch.
3. **Exit blocked by an obstacle** — block the driver door (spawn a cube next to it):
   alternate offsets tried; fully surrounded → «Вокруг фургона нет места для выхода».
4. **Rapid repeated F** — no second character spawned, possession stays valid.
5. **Mission progression does not duplicate** — interacting with the fuse box twice
   (second time without the fuse / after install) must not consume items or re-fire
   `AdvanceStage`; lever event after restore is stage-guarded.
6. **Save/load roundtrip** — complete fuse install, quit PIE, relaunch: stage restored
   (objective shows «Включить рубильник»), fuse mesh visible on the box, inventory
   matches, van at saved spot, no re-reward.
7. **Fresh game** — delete the save slot (`Saved/SaveGames/QuietRoad01.sav`): starts
   on foot at the player start, stage 0 objective.
8. **Packaged/editor entry map** — editor opens `VehicleBasic`; the slice map applies
   `RoadToWisdomQuietRoadGameMode` via `GameModeMapOverrides`; template maps still
   spawn their own game mode (no regression).

## Pre-existing data issue (manual)

4 `VehicleBasic` external-actor files are tracked but missing on disk (initial
commit). Restore or commit the deletion — see `Docs/AssetBudget.md`.

## Not run / not covered

- Rendered visual quality (M2 concern; NullRHI and headless runs prove loading only).
- Multiplayer behavior of UT components in this project (single-player slice; UT
  domains are covered by their own automation in the UnrealTemplates repo).
- Any packaged (Shipping) build.
