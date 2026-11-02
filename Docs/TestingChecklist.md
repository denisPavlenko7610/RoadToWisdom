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

- Full human artistic approval of Fab compositions remains required (NoAI). Real local captures and geometry-only review are recorded below; loading alone does not prove art quality.

- Multiplayer behavior of UT components in this project (single-player slice; UT

  domains are covered by their own automation in the UnrealTemplates repo).

- Any packaged (Shipping) build.

## M1.4 verification — 2026-10-25

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

## World Generation — 2026-10-25

- compiled: UE 5.8.4 RoadToWisdomEditor after source reorganization and SportsCar class removal. WorldEditorBuildFinal.log: Succeeded.
- editor-verified: pilot GPU generation (WorldPilotGPU.log), 1008 m, 16 components, 93 surface traces, 90 envelope checks, 6 buildings, no errors.
- editor-verified: full GPU generation and fresh reopen: 4032 m, 256 components / 64 proxies, 12.222 km roads, sampled maximum grade 6.19%, minimum radius 87.4 m, 9 POI / 18 buildings. 406 surface and 401 envelope checks, maximum gap 5.35 cm, no building overlap/foundation failures. Instance transforms persisted across regeneration and reopen. After descriptive actor-label changes the label-inclusive signature is 736512e9461518ae7a58485c75239351da7651dc06dee1b5d2075c54787de7a3.
- fixed regression: earlier -NullRHI generation kept correct collision but copied blank rendered heightmaps during Landscape proxy splitting. Both maps regenerated in the GPU editor. Generator rejects -NullRHI; VerifyWorld now checks rendered proxy bounds. It passed after a fresh main-map reopen. Earlier headless collision-only checks did not establish visual correctness.
- editor-verified: geometry-only capture confirmed terrain continuity and improved the extreme road cuts. Engine diagnostic materials/hiding restored. Production Landscape uses MI_RegionLandscape with rotated/multiple scales, macro tint, distance fading and slope tint. Fab originals unchanged.
- editor-verified: same-seed generation has no duplicate labels and preserves the manual VehicleStart. Marker height/heading deliberately aligned once to the revised road. Host preparation deterministic.
- editor-verified: Offroad spawned through existing VehicleAdv GameMode in PIE, simulates Chaos physics and traveled 136.5 m under throttle. Short straight drive only; not the entire route or manual keyboard acceptance. No vehicle Actor saved into the permanent map.
- editor-verified: PIE streaming at 2000 m range: far checkpoint unloaded 24 starting Landscape actors and loaded 25 different ones (StreamingPIEFinal.json).
- editor-verified: Asset Registry audit before SportsCar retirement. VehicleBasic placed pawn and VehicleAdv/TimeTrial spawn/respawn defaults use Offroad; shared materials/textures moved to Vehicles/Shared before deletion. SportsCar assets/native classes removed; base chassis/wheels kept. Blueprint classes loaded after rebuild/reopen.
- editor-verified: retired prototype deleted after zero package referencers; startup/game defaults point to OpenWorld. Original Fab and remaining template maps retained.
- manual-check-required: full route drive at speed and all branches, keyboard/camera acceptance, artistic approval of Fab building compositions, foreground/Standalone performance profiling. Background CSV was captured before rendered-heightmap repair; timings are not valid final-world FPS acceptance.
- manual-check-required: exhaustive TimeTrial/Offroad template gameplay regression.

Capture tooling encountered a native realtime-override ensure; the triggering call was removed. A Rider map switch during World Tick hit an engine assertion; generation now avoids reloading the active map, and pilot GPU generation ran in its own editor process. Final main-map reopen/validation succeeded. Legacy tests naming the retired prototype do not test this world. M3.1 is not implemented.

Evidence: Saved/WorldGeneration, WorldPilotGPU.log, LandscapeRepair.log, WorldAcceptance.log. Saved stays untracked. Final six editor captures are stored locally; protected Fab image data was not submitted to generative image tools.

## Utility Van integration

- compiled: RoadToWisdomEditor UE 5.8.4, UtilityVanBuild.log: Succeeded.
- editor-verified: commandlet imported only undamaged geometry (3308 triangles), five 1K textures, native Root + four wheel skeleton, rigid skin weights, chassis PhysicsAsset and compiled WheelController AnimBP. ImportUtilityVan.log succeeded.
- editor-verified: fresh main-map reopen resolves BP_UtilityVan_Pawn as GameMode default. All five reference-bone transforms match supplied wheel centers. Blueprint-created vehicle has a running ABP_UtilityVan instance.
- editor-verified: fresh reopen retained ExplorerStart at z6664.50 and VehicleStart at z6564.50 above actual terrain. VerifyWorld passes 406 road traces / 401 envelope checks and grounded-marker validation. actor.modify() is required before moving saved External Actors; generator now enforces it.
- manual-check-required: complete van route, physical keyboard/camera acceptance and foreground performance. Short scripted Chaos test is recorded separately in Saved/WorldGeneration/UtilityVanDrive.json; it does not prove full-route acceptance.
- tooling limitation: synchronous Interchange import through Rider's task callback caused a native TaskGraph recursion assertion. Import moved to a dedicated editor Python commandlet, which completes successfully.

- editor-verified: 13-second actual PIE UtilityVan drive accelerated to 3.11 m/s at 40% throttle, covered 11.4 m, steered briefly and stopped with brake/handbrake. Wheel contact, suspension offset and animated bone rotation changed during movement. Evidence UtilityVanDrive.json. This is a short smoke test, not full route or keyboard acceptance.

## Utility Van skeletal material regression — 2026-10-25

Previous PIE driving checks established movement, not correct shading. UtilityVanFinalReopen.log contained `missing usage flag SkeletalMesh` and engine fallback to Default Material. The editor preview had auto-enabled usage only in memory. Both persisted M_UtilityVan and MI_UtilityVan reported false before this fix.

- implemented: importer explicitly sets the base SkeletalMesh usage with UE 5.8 SetBaseMaterialUsage, inherits it on MI_UtilityVan, repairs all five slots even when SK_UtilityVan already exists, and rejects a missing usage/material assignment.
- editor-verified: fixed assets saved; all five skeletal slots, Blueprint component slots and five texture parameters resolve. Dedicated VerifyUtilityVanMaterials.py records editor/PIE checks in Saved/WorldGeneration/VanMaterials_*.json.
- editor-verified: repeat import commandlet completed with 0 errors / 0 warnings (VanMaterialImport.log). Fresh editor process loaded the persisted flags as true. Editor and actual PIE pawn checks passed (VanMaterials_Editor.json / VanMaterials_PIE.json).
- editor-verified: real PIE screenshot UtilityVan_Materials_PIE.png shows textured paint, window, lights and branding. Surrounding assets were hidden only in the temporary PIE world for isolated material inspection; PIE ended and the editor map was not modified. VanMaterialReload.log contains no missing SkeletalMesh usage/default material fallback warnings.
- editor-verified: imported static wheel-section bounds match all four skeleton wheel centers. No rig/UV/physics changes were needed for this material fix.

## M2.3 acceptance — 2026-11-02

Scope: existing 4032 m map, no new missions/UT systems, no replacement transport implementation. All evidence below was produced by real UE processes; Saved remains ignored.

- compiled: M23BuildFinal.log, UE 5.8.4 Editor Development succeeded. Clean checkout initially failed linking the committed route driver's Json calls. RoadToWisdom.Build.cs now includes Json; clean build succeeded after correction. Shared Toolkit junction DLLs require closing all editor processes before rebuilding; they cannot be concurrently relinked.
- editor-verified: clean-copy Asset Registry / Blueprint audit, five maps loaded (VehicleBasic, Offroad, TimeTrial, pilot, main), 31 Blueprint parent/generated classes compiled, no Blueprint compile errors. It exposed stale T_GridChecker_A streaming metadata in MI_TrackEdge and MI_TrackEdge_Inst. Their shaders use no textures; a guarded native editor repair cleared obsolete streaming data. Fresh process reports zero missing Game dependencies. Templates' appearance/parents were preserved; exhaustive TimeTrial racing gameplay is not claimed.
- editor-verified: all seven branches driven physically in PIE: ValleyLoop 3161.7 m, FarmSpur 389.4 m, SouthSpur 214.0 m, CabinSpur 969.2 m, DepotSpur 939.7 m, OverlookSpur 526.8 m, RidgeLink 1079.1 m. All completed, no stalls/vehicle-envelope departures; maximum lateral offset 1.052 m. M23Branches.json. The earlier Main acceptance covered 4922.1 m at average 31.99 km/h. The new editor test driver was calibrated before acceptance; initial excessive lookahead cut corners and was rejected, not counted as a map defect.
- editor-verified: 41-point fast streaming-source sweep (teleports, separate from Chaos driving) always found actual Landscape collision, minimum 27 loaded terrain actors. Across endpoints 23 terrain actors unloaded and 24 different ones loaded. M23FastStreaming.json. This is not proof of 110 m/s physical vehicle handling.
- editor-verified: two identical host preparations; eight roads form one connected network; each of nine POI pads intersects a road. M23DeterminismConnectivity.json.
- fixed: geometry-only visual inspection found abrupt earthwork transitions. PrepareWorld blends overlapping shoulders continuously while preserving the exact validated road/camper corridor. Road arrays before/after are identical. Adjacent-grid jumps >3 m reduced 361 -> 188; maximum 5.62 -> 4.14 m. New terrain SHA256 d60118bac0522136e89754d533fb7bfc64d8242861b2cc8575b719b15de45da8.
- editor-verified: final terrain generated in GPU editor and passes 406 surface traces / 401 vehicle-envelope checks, all building foundations/overlaps and rendered bounds. Final instances: Grass 22132, Shrubs 9919, RockSmall 3624, RockLarge 651. Final label/transform signature e691ebde4644455ee19145cdd02f70f64b2d76c4b39b45bb36b3ffbce1134460.
- editor-verified: final generator repeated twice in isolated M23Clean copy, exact signature and no duplicate labels, manual sentinel retained, zero geometric failures. M23FinalRegeneration.log / M23Regeneration.json. All production manual objects remained outside generator ownership. The first fixture run contained a sentinel from an interrupted test; the fixture now removes only its own tagged sentinel before testing.
- editor-verified: eight real local editor captures, updated road-level cameras and added large settlement / overlook. Geometry-only comparison uses engine materials with protected Fab actors hidden, then restores original scene materials. Saved/Screenshots.json and GeometryDiagnosticRestored.txt prove capture/cleanup. Full artistic review of protected Fab images remains manual because the listing disallows AI usage.
- measured: foreground Standalone 1920x1080, render scale 100%, dynamic resolution off, Ryzen 5800X / RTX 5070 Ti / D3D12. Verified foreground capture: 2400 frames, frame median 13.33 ms, p95 13.56 ms, maximum 70.75 ms; GPU median 5.89 ms. Default effective frame cap 75. Startup streaming component registration caused rare spikes. A test registration granularity of 1 did not materially help, so no config change was retained.
- measured: longer Standalone run (6000 frames) excluded first 2250 frames (~30 s initial streaming). Remaining 3750 frames: frame median 13.33 ms, p95 13.60 ms, p99 13.95 ms, maximum 34.11 ms; GPU median 6.12 ms. Foreground activation for this repeat was interrupted by user Escape, so it is not labelled a verified foreground capture. M23StandaloneSteady.json. Measurements precede the shoulder-only terrain adjustment; final map checks are appended separately. No universal FPS / hitch-free claim and no unmeasured HLOD optimization.
- editor tooling: Landscape creation stalled in a rendering-enabled commandlet. Owned stalled process was stopped; full generation ran successfully in a regular GPU editor. -ExecutePythonScript callbacks require EditorPythonScripting.set_keep_python_script_alive(True); otherwise the editor exits after the synchronous script.

Remaining human acceptance: protected Fab art/composition approval from eight local frames, physical keyboard comfort and complete TimeTrial gameplay. These are not replaced by automated scene checks. No new mission, inventory, save or survival systems were added.

Final M2.3 delivery check: a new clone of commit 85bb904 (no copied Binaries/Intermediate/Saved) rebuilt successfully in 70.3 s after linking the documented external Toolkit. Fresh preparation reproduced manifest ae48f1f49a53d901ad00993776f08081d4efc297aa240b0659e3897cc67ed69c. M23FinalCleanLoad.log: five maps, 31 Blueprint classes, 1801 assets, zero missing Game dependencies, zero errors/warnings; loaded final terrain has 256 components and signature e691ebde4644455ee19145cdd02f70f64b2d76c4b39b45bb36b3ffbce1134460. Surface/envelope checks 406/401 passed; maximum sampled road gap 5.36 cm. The actual clean-clone map contains no acceptance sentinel; that fixture was confined to the separate regeneration copy.

Final saved-terrain Standalone repeat (M23FinalStandalone.log / M23FinalStandalone.json): 1920x1080, 100% render scale, dynamic resolution off, 2400 frames; after the first 300, median 13.33 ms, p95 13.58 ms, p99 13.84 ms, maximum 66.96 ms; GPU median 6.53 ms. This repeat did not force/verify foreground activation. Startup streaming still has rare registration spikes; no configuration or visual quality was weakened to hide them.

## Utility Van wheel rotation — 2026-11-02

The initial current-session check already had a valid native VehicleAnimationInstance, Blueprint animation mode and live bone rotation. No replacement physics or manual Tick wheel simulation was necessary. The request was verified against actual movement rather than assuming a running AnimBP proved visible wheels.

- editor-verified: 13-second PIE test checks all four physical rotation angles and the four corresponding bone quaternions separately. All four rotate; front steering exceeds 3 degrees. Actual mesh rendered in 129/130 sampled frames. Suspension displacement alone can no longer make this check pass.
- editor-verified: native FBX export of the delivered skeletal mesh contains 134 nonzero bound vertices for each wheel bone (UV/normal seams included). Wheel geometry is bound to its own bones, not Root.
- implemented: ImportUtilityVan.py and FixVanWheelAnim.py explicitly save AnimationBlueprint mode, ABP_UtilityVan and AlwaysTickPoseAndRefreshBones. Import asserts the binding survives Blueprint compilation. The repair script only configures animation; chassis/wheels/handling remain unchanged.
- editor-verified: two real side-view PIE images show the user-supplied van during spin/steer. World render actors were hidden only in the temporary PIE session, retaining real collision; ending PIE restores the ordinary map. No protected Fab renders were sent for image analysis.

Evidence: VanWheelAnimation.json, VanSkinWeights.json, VanWheelSpin_0.png / _1.png and VanWheels.log. Final fresh-reopen check is recorded after execution.

Fresh-reopen wheel check: after the updated importer completed with 0 errors / 0 warnings, a new editor process loaded the saved AnimationBlueprint mode, ABP_UtilityVan and AlwaysTickPoseAndRefreshBones. The 13-second wheel-specific PIE test passed again for all four spin/physics pairs and front steering (VanWheelsReopen.log / VanWheelAnimation.json). Material/skeletal usage audit also passed. RuntimeHashSet metadata was queried directly: LH cells 128/256/512 m, each LoadingRange 2000 m and BlockOnSlowStreaming=true.
