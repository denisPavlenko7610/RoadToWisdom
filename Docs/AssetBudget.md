# Asset budget

Current policy: ordinary Git, no LFS, every file <100 MiB; 50 MiB is a warning threshold. Earlier aggregate 250/500 MiB goals are superseded by the approved large-world task and already imported Fab pack.

Measured 2026-10-25 with `python Tools/CheckAssetBudget.py --json` (includes pending source assets): Content 631.78 MiB; tracked source files 585.70 MiB before staging; all tracked/pending source files 632.26 MiB; .git 535.40 MiB before the world commit. No file reaches 50 MiB or 100 MiB. No LFS filter attributes. Largest file is existing Vehicle Template Offroad External Actor 16.25 MiB; largest Fab texture 12.45 MiB. Actual totals may increase slightly when documentation/screenshots are staged.

Fab audit: 242 files / 486.1 MiB, 76 static meshes, 7 Blueprints, 6 materials, 46 material instances, 101 textures, 2 maps. Original content is preserved; the new world references buildings/shrub/sand textures and uses project-owned collision derivatives. World Partition external packages keep Landscape data split across small source assets.

Do not add Saved/Intermediate/Binaries/DDC. Main map, own meshes/materials and required External Actors/Objects must be tracked. Keep package licensing restrictions and repository access in mind; do not publish raw Fab assets for unrestricted standalone distribution.
