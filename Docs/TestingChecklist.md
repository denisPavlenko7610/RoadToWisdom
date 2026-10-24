# Testing Checklist — Quiet Road vertical slice

Status vocabulary (per AGENTS.md): `implemented` → `compiled` → `editor-verified` →
`manual-check-required`. Everything below is honestly labeled as of 2026-10-24.

## Automated checks (executed in this session)

| Check | Command / how | Result |
|---|---|---|
| C++ editor target builds | `Build.bat RoadToWisdomEditor Win64 Development` | **compiled** — Result: Succeeded |
| Plugin closure compiles | same build (all 7 UT modules linked) | **compiled** |
| Prototype map exists and loads in the editor | `Tools/Editor/VerifyPrototype.py` (`-ExecutePythonScript`) | **editor-verified** — `[VERIFY-RESULT] PASSED`: van BP (single `QuietRoad.Van` tag), fuse box, lever, world item (Item.Fuse ×1), mission manager with wired refs, radio panel, decor cluster (110/110/130/45 HISM instances after reload), SkyAtmosphere, lights, player start |
| M2 decor instance persistence | generator reload check + verify | **editor-verified** — HISM instances serialize into the umap only via direct `PerInstanceSMData` writes (see the toolkit matrix note) |
| Per-map GameMode binding | `-game` run log | **editor-verified** — `LogLoad: Game class is 'RoadToWisdomQuietRoadGameMode'` (binding lives in WorldSettings; the ini `GameModeMapOverrides` did not apply) |
| Real rendered look of the slice | `-game -windowed` + `HighResShot` | **editor-verified** — screenshot: on-foot third-person start, one van, tinted ground/road, trees/bushes/rocks visible (Saved/Screenshots/WindowsEditor) |
| Input assets resolve at CDO construction | same run, log checked | **editor-verified** — 0 «Failed to find» CDO errors |
| Game-mode boot of the slice map | `-game -NullRHI -ExecCmds="quit"` on `L_QuietRoad_Prototype` | **editor-verified** — clean exit 0, no ensures/exceptions (after the UFUNCTION fix below) |
| Content budget | `python Tools/CheckAssetBudget.py` | **passed** — 97.0 MiB / 250 MiB |

## M1.3 regression gate (GDD v0.3 §28) — automated evidence

| Gate | Evidence | Status |
|---|---|---|
| Van tag ambiguity blocks registration (no arbitrary pick) | `RegisterVanFromWorld`: >1 match → error log, VanPawn stays null, F disabled | **implemented + compiled**; live 2-van map test = manual |
| Quest item cannot respawn after load | `ARoadToWisdomPlacedPickup` records its stable id into the save coordinator on pickup; "Pickups" section restores by destroying matching placed actors (REUSE_GUIDE: project persists collection state) | **implemented + compiled**; full pickup→save→load cycle = PIE manual |
| Official HISM API | `SetInstances` uses `ClearInstances`/`AddInstance`; two same-seed generator runs → identical 110/110/130/45, reload check identical, no duplicates | **editor-verified** |
| Real scene look | `-game` screenshots `M2_Start.png` (van+road+forest) and `M2_Relay.png` (mast, fuse box, lever, crate) in Saved/Screenshots/WindowsEditor | **editor-verified** |

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

## M1.4 verification � 2026-10-25

`compiled`: UE 5.8.4 Editor Development, Saved/Logs/M14_Build.log.
`editor-verified`: QuietRoad.Session.Persistence passed in five actual PIE sessions;
report Saved/Automation/M14/index.json, log Saved/Logs/M14_Tests.log.
Covers pre-pickup, full/range rejection, partial stack restoration, collected fuse,
installed fuse, completed mission, missing Inventory section, enter/exit and on-foot IMC.
Pickups section is now version 2 (remaining payloads plus collected IDs); older saves
are rejected as a whole, with no automatic migration. The normal slot is preserved;
-QuietRoadAutomation selects only QuietRoad_Automation for regression tests.
UTSave's LoadFromSlot validates/replaces its in-memory object atomically; SaveToSlot
uses engine SaveGameToSlot, with no extra power-loss-safe write guarantee. World-state
application is sequential: this slice prevalidates the sole fuse-box schema and identity.
Rendered driving, physical keyboard tap/hold, and art acceptance still require checks.
