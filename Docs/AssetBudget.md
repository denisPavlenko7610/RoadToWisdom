# Asset budget

Measured 2026-11-02 after M2.3 terrain/acceptance changes.

- Content: 624.72 MiB.
- Tracked + pending source: 630.50 MiB.
- .git: 644.96 MiB before final commit.
- Largest asset: 16.25 MiB.
- Files >=50 MiB: 0; files >=100 MiB: 0; Git LFS pointers: 0.

Ordinary Git only. Required Landscape/World Partition External Actors are included. Their hash filenames are engine-owned and must not be renamed. Own assets use M_/MI_/SM_/SK_/SKEL_/PHYS_/ABP_/BP_/T_ prefixes and category folders. The original Fab content remains unchanged. Only the undamaged Utility Van and five 1K textures are imported; its damaged mesh/textures are excluded. SourceArt retains the clean OBJ and source textures for reproducible import.

Run `python Tools/CheckAssetBudget.py --json` for current values. Saved, Intermediate, Binaries and DDC remain ignored.

Wheel visibility correction (2026-11-02): budget check passed; Content 625.738 MiB, tracked + pending source 631.518 MiB. No files >=50 MiB or >=100 MiB, no LFS. One small wheel material instance reuses the existing five textures. Measurement includes pre-existing local world edits.

2026-11-02 input review: CheckAssetBudget.py passed. Tracked source total
631.72 MiB; Content 625.74 MiB; largest file 16.25 MiB. No file above 50 MiB
and no LFS files. This change adds no assets.


2026-11-03 persistence iteration: CheckAssetBudget.py passed with the isolated
Content/Tests/L_SaveSession.umap (about 27 KiB), reusing existing assets.
Content 625.766 MiB; tracked + pending source about 631.58 MiB; .git 665.70 MiB
before any commit. Largest file 16.25 MiB; no files >=50 MiB or >=100 MiB;
no Git LFS. Measurement includes pre-existing working-tree changes.
