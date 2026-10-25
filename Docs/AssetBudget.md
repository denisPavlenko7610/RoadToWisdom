# Asset budget

Measured 2026-10-25 after the undamaged Utility Van import.

- Content: 625.69 MiB.
- Tracked + pending source: 631.41 MiB.
- .git: 654.40 MiB before final commit.
- Largest asset: 16.25 MiB.
- Files >=50 MiB: 0; files >=100 MiB: 0; Git LFS pointers: 0.

Ordinary Git only. Required Landscape/World Partition External Actors are included. Their hash filenames are engine-owned and must not be renamed. Own assets use M_/MI_/SM_/SK_/SKEL_/PHYS_/ABP_/BP_/T_ prefixes and category folders. The original Fab content remains unchanged. Only the undamaged Utility Van and five 1K textures are imported; its damaged mesh/textures are excluded. SourceArt retains the clean OBJ and source textures for reproducible import.

Run `python Tools/CheckAssetBudget.py --json` for current values. Saved, Intermediate, Binaries and DDC remain ignored.
