# How To Run — RoadToWisdom / Quiet Road

Environment used for all verification in this repo: **UE 5.8.4** at
`C:\Program Files\Epic Games\U_5.8\UE_5.8`, Windows, Visual Studio toolchain.

## 0. One-time setup after a fresh clone

```powershell
# Recreate Plugins/UT* junctions into ..\..\UnrealTemplates (idempotent)
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LinkToolkitPlugins.ps1
```

The UT plugins are **not** stored in this repository; `Plugins/UT*` are NTFS junctions
into the sibling `UnrealTemplates` repo. If that repo lives elsewhere:
`-ToolkitRoot 'D:\path\UnrealTemplates\Plugins'`.

## 1. Build (C++ editor target)

```bat
"C:\Program Files\Epic Games\U_5.8\UE_5.8\Engine\Build\BatchFiles\Build.bat" ^
  RoadToWisdomEditor Win64 Development ^
  -project="C:\_DiskE\Projects\Unreal\RoadToWisdom\RoadToWisdom.uproject" -WaitMutex
```

Or open `RoadToWisdom.uproject` (the IDE sln/slnx files are gitignored; regenerate
with "Generate Visual Studio project files" or just let the launcher open it).

## 2. Open and play

- Editor: open `RoadToWisdom.uproject`, load `/Game/QuietRoad/Maps/L_QuietRoad_Prototype`, press Play.
  The map's GameMode is bound via `GameModeMapOverrides` in `DefaultEngine.ini`
  (`RoadToWisdomQuietRoadGameMode`): you start on foot near the parked van.
- Standalone game smoke (headless, used by automation):
  ```bat
  UnrealEditor-Cmd.exe RoadToWisdom.uproject /Game/QuietRoad/Maps/L_QuietRoad_Prototype -game -NullRHI -ExecCmds="quit"
  ```

## 3. What is automated vs manual

Automated (headless editor, full editor boot via `UnrealEditor-Cmd -ExecutePythonScript`):

- `Tools/Editor/BuildQuietRoadPrototype.py` — input actions, mapping contexts, the
  `Item.Fuse` data asset, the whole prototype map (idempotent), the WorldSettings
  GameMode binding and the M2 decor cluster carrier.
- `Tools/Editor/GenerateM2Decor.py` — deterministic M2 decor (seed/density/exclusions)
  into the map's HISM cluster + tinted material palette. Explicitly loads the target
  map first: the editor silently restores a different current world otherwise.
- `Tools/Editor/SetMapGameMode.py`, `Tools/Editor/TakeM2Screenshot.py` — helpers;
  for real screenshots run the game windowed with `-ExecCmds="HighResShot 1400x800"`.
- `Tools/Editor/VerifyPrototype.py` — loads the map, checks the actor set, mission
  references, the fuse world item payload, input assets, the decor cluster instance
  counts and SkyAtmosphere; prints `[VERIFY-RESULT] PASSED/FAILED`.


Manual (needs a human in the editor):

- PIE playthrough of the mission loop (drive → exit → fuse → lever → return) —
  see `Docs/TestingChecklist.md`.
- Unreal MCP: the editor plugin starts an HTTP MCP server at
  `http://127.0.0.1:8000/mcp` (settings in `DefaultEditorPerProjectUserSettings.ini`;
  project-root `.mcp.json` registers the server for MCP clients). It is only up while
  the editor is running.

## 4. Content budget

```bat
python Tools/CheckAssetBudget.py
```

Fails when tracked content exceeds 250 MiB or any single file exceeds 50 MiB.
Details: `Docs/AssetBudget.md`.

## 5. Commit / git conventions

English commit messages, one meaningful stage per commit, no history rewrites.
`.gitignore` covers `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`,
IDE files and `Plugins/UT*/`.

## Focused save regression

Run UnrealEditor-Cmd.exe RoadToWisdom.uproject -unattended -NullRHI
-QuietRoadAutomation -ExecCmds="Automation RunTests QuietRoad.Session"
-TestExit="Automation Test Queue Empty" -ReportExportPath="<project>/Saved/Automation/M14".
Check exported test results (not just process exit). The test uses a separate slot.
Pickups schema v2 deliberately rejects legacy saves; start a new game by moving the
normal QuietRoad01.sav out of Saved/SaveGames before launching a fresh session.
