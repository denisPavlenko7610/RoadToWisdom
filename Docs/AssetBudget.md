# Asset Budget — RoadToWisdom

Measured with `python Tools/CheckAssetBudget.py` (2026-10-24, after M1 stabilization + M2 visual reference).

## Totals

| Metric | Value | Target |
|---|---|---|
| Tracked game content (git ls-files, Content/Config/Source/Docs/Tools) | **97.3 MiB**, 341 files | ≤ 250 MiB (first slice), ≤ 500 MiB extended |
| `Content/QuietRoad/` (slice content) | ~200 KB (input assets, item definition, map + M2 material palette) | — |
| `Plugins/` (versioned) | **0 B** — UT plugins are junctions, gitignored | — |
| Files > 50 MiB | none | none without explicit reason |
| Git LFS | not used (0 files, `.git` = 65 MB) | re-evaluate before large packs |

## Largest tracked files

1. `Content/__ExternalActors__/Variant_OffRoad/Maps/Lvl_Offroad/B/9M/…uasset` — 16.25 MiB (template offroad map external actor)
2. `Content/Vehicles/OffroadCar/SKM_Offroad.uasset` — 8.49 MiB
3. `Content/Vehicles/OffroadCar/SM_Offroad_Body.uasset` — 2.70 MiB
4. Many 2.2–2.4 MiB offroad-map external actors.

The template Offroad variant (`Content/Variant_OffRoad/` + `Content/__ExternalActors__/Variant_OffRoad/`)
and `Content/Vehicles/OffroadCar/` are the bulk of the budget. If the budget gets tight,
deleting the unused Offroad variant (map + external actors) is the single biggest lever
(~40+ MiB) — but only after the template regression checks no longer need it.

## Known data issue (pre-existing, needs user decision)

4 tracked files are missing on disk — `Content/__ExternalActors__/VehicleTemplate/Maps/VehicleBasic/`
(4 hashed `.uasset`s, committed in the initial commit). Either restore them
(`git checkout -- Content/__ExternalActors__/VehicleTemplate/`) or confirm deletion and
commit it. Until then `CheckAssetBudget.py` prints a warning.

## Rules in force

- Run `python Tools/CheckAssetBudget.py` before large content commits (fails on budget
  breach or any file > 50 MiB).
- No Marketplace/Fab packs, Megascans or 4K/8K texture imports without an explicit
  reason and a matrix update.
- Frequent re-saving of `.umap` grows history; keep map edits batched.
