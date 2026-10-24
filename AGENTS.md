# AGENTS.md — RoadToWisdom / Quiet Road

Instructions for AI agents (and humans) working in this repository.

Read this file top-to-bottom before doing anything. When instructions conflict,

the order of precedence is:

1. This `AGENTS.md` and the linked UnrealTemplates documents (§2).

2. `Docs/Quiet_Road_GDD.md` — the approved game design.

3. Explicit user instructions given in the current session (they may override

   specifics, but conflicts must be stated openly, never silently resolved).

---

## 1. What this project is

**Quiet Road («Тихая дорога»)** — a small atmospheric exploration game built on the

UE 5.8 **Vehicle Template** (module `RoadToWisdom`, game class prefix `RoadToWisdom`).

The player drives a camper van, parks, gets out, solves small item-based tasks, and

returns. The first deliverable is **one working vertical slice** (GDD M0 → M1), not a

large pre-built architecture.

- Design source of truth: `Docs/Quiet_Road_GDD.md` (GDD). Follow it; do not

  invent features it excludes (§19 of the GDD lists what is out of scope).

- Engine: **Unreal Engine 5.8.4**, installed at

  `C:\Program Files\Epic Games\U_5.8\UE_5.8` (promoted build).

- Do not blindly port APIs from other engine versions; check the actual 5.8 headers

  installed with the engine when unsure.

## 2. Toolkit — UnrealTemplates (sibling repository, binding)

The reusable domain plugins live in the **sibling repository**

`C:\_DiskE\Projects\Unreal\UnrealTemplates` (referred to below as *UT*).

Its documentation is **binding instructions** for anyone integrating toolkit

systems here. Read the relevant document before touching a domain:

| Document (absolute path)                                                        | When to read |

|---------------------------------------------------------------------------------|--------------|

| `C:\_DiskE\Projects\Unreal\UnrealTemplates\AGENTS.md`                            | **Start here** — UT build/test commands, code conventions, UHT prefix rules, migration/CoreRedirect policy, kit distribution |

| `C:\_DiskE\Projects\Unreal\UnrealTemplates\README.md`                            | Overview of all domains, conventions, MCP setup |

| `C:\_DiskE\Projects\Unreal\UnrealTemplates\REUSE_GUIDE.md`                       | **Before any integration** — dependency closure table, ownership/save contracts, minimal API usage |

| `C:\_DiskE\Projects\Unreal\UnrealTemplates\ARCHITECTURE.md`                      | Before writing code that talks to domains — module boundaries, replication rules |

| `C:\_DiskE\Projects\Unreal\UnrealTemplates\KIT_CATALOG.md` / `PLUGIN_CATALOG.md` | When choosing what to integrate next — kit/plugin lookup |

| `C:\_DiskE\Projects\Unreal\UnrealTemplates\TEST_MATRIX.md`                       | Before claiming a domain "works" — what has actually been executed there |

| `C:\_DiskE\Projects\Unreal\UnrealTemplates\DEBUGGING.md`                         | When debugging domain behavior or log output |

| `C:\_DiskE\Projects\Unreal\UnrealTemplates\Plugins\<Plugin>\README.md`           | Before using a specific plugin's API |

Adopted integration set (GDD §16 "Adventure Kit"):

```

UTFoundation, UTInteraction, UTSave, UTItems, UTProgression, UTWorldMechanics,

UTCombat (required by UTWorldMechanics)

```

**The plugins are referenced, not copied.** `Plugins/UT*` in this repo are NTFS

directory junctions pointing at `C:\_DiskE\Projects\Unreal\UnrealTemplates\Plugins\*`

so both projects share one copy of the sources (the user develops both in parallel).

Consequences:

- `Plugins/UT*` is gitignored here; after a fresh clone run

  `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LinkToolkitPlugins.ps1`

  (recreates the junctions, idempotent) before opening the editor or building.

  Equivalent alternative: the UT repo's official installer

  `Tools/Install-UTKit.ps1 -Kit Adventure -Project <this repo> -Link`

  (resolves the closure itself; `Kits/Adventure.json` lists the same six plugins —

  `UTCombat` is pulled in as UTWorldMechanics' declared `.uplugin` dependency).

- Never modify files under `Plugins/UT*` (from either project) to bend behavior —

  a suspected bug is fixed in the UT repository and benefits both projects; note the

  bug in `Docs/ToolkitIntegrationMatrix.md` in the meantime.

- Building this project writes UT plugin `Binaries/`/`Intermediate/` into the

  UnrealTemplates folders through the junctions — that is expected and gitignored there.

- **Never guess a UT API.** Open the actual header under

  `Plugins/UT*/Source/*/Public/` in this repo (or the UT repo) and read it.

- Do not enable `UTExamples`, `UTShowcase`, `UTIntegrations` (GAS / StateTree /

  SmartObjects adapters) in production modules or maps. They exist only in the UT

  repo for its own validation.

- Game modules may depend on UT modules; UT modules must never depend on the game.

## 3. Unreal MCP

The engine plugin `ModelContextProtocol` (enabled in `RoadToWisdom.uproject`) starts

an HTTP MCP server at `http://127.0.0.1:8000/mcp` when the UE editor is running

(settings in `Config/DefaultEditorPerProjectUserSettings.ini`). The project root has

`.mcp.json` with the same server so MCP-capable clients pick it up automatically.

- If the editor is not running, the MCP server is **down** — do not claim editor

  verification in that state.

- Asset/map creation is automated via headless editor Python scripts under

  `Tools/Editor/` (run with `UnrealEditor-Cmd.exe -run=pythonscript`); see

  `Docs/HowToRun.md`.

## 4. Verification honesty ladder

Label every result with the strongest honest claim it supports:

- `implemented` — code exists.

- `compiled` — the target built with UnrealBuildTool, error-free.

- `editor-verified` — the editor/headless editor actually loaded the map/asset and

  reported no errors (log evidence saved).

- `manual-check-required` — needs a human in PIE/game; never report this as done.

Never claim the project builds, a scene opens, or PIE works if it did not actually

happen in this session. See also GDD §20 (Definition of Done).

## 5. Code and content rules

- **C++ for game rules**, Blueprints for configuration/presentation, asset

  references, maps and UI glue (GDD §15.3).

- No new frameworks: no GAS/StateTree/SmartObjects, no ECS, no quest platform, no

  networking framework, no god-manager singletons. Build only what the current

  slice needs; add folders/classes only when needed (GDD §3 layout is a guide,

  not a scaffold to pre-create).

- Configurable values go in `UPROPERTY(EditAnywhere)` / data assets, not magic

  numbers. Event-driven over Tick where per-frame updates are not needed.

- Keep the stock Vehicle Template working: no renames of template classes/assets

  without updating the class redirects in `Config/DefaultEngine.ini` and checking

  `.uasset` references (task rule §0.9).

- Current user-authorized scope: editor-generated 4032 m open world using the installed Nimikko Wild West Town. New gameplay/camper work is deferred. SportsCar was explicitly retired after dependency audit; temporary transport is the existing Offroad pawn. Shared chassis/wheels/materials remain. Main map: `/Game/QuietRoadWorld/Maps/L_QuietRoad_OpenWorld`; old prototype map retired.
- Current content policy supersedes the initial 250 MiB slice budget: ordinary Git only, **no Git LFS**, every tracked file strictly below 100 MiB, preferably below 50 MiB. Preserve required World Partition External Actors. Original Fab assets remain unchanged; do not add packs or upload protected assets to AI generation services. Run `python Tools/CheckAssetBudget.py` before commits.
- `.gitignore` already excludes `Binaries/`, `Intermediate/`, `Saved/`,

  `DerivedDataCache/`, IDE files. Do not weaken it to sneak in build output.

## 6. Git

- Commit per meaningful stage; **commit messages in English**.

- No destructive git operations (history rewrite, hard resets of shared work,

  force pushes) without explicit user permission.

- Never commit the engine, engine binaries, or third-party content that cannot be

  legally redistributed.

## 7. Documentation to keep updated

When you add or change gameplay, update the matching doc in the same change:

- `Docs/HowToRun.md` — build/run instructions, what is automated vs manual.

- `Docs/ToolkitIntegrationMatrix.md` — real API touchpoints per plugin, status and risks.

- `Docs/AssetBudget.md` — content size measurement.

- `Docs/TestingChecklist.md` — GDD §5.4 scenarios, honestly marked pass/fail/not-run.

- `Docs/Architecture.md` — 1–2 pages max; who owns what and why.

