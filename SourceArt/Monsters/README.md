# Original anime monster source meshes

`References/AnimeMonsterTurnarounds.png` is the original generated design sheet. `References/ModeledMonstersPreview.png` is a Blender Workbench render of the exported FBX meshes reimported into Blender, **not an Unreal screenshot**. The preview puts the shield and boss hammer to one side to expose torso details; the FBX origins retain the gameplay attachment contract.

The 33 source FBX meshes cover four silhouettes:

- 0: red iron oni swordsman, horned mask, overlapping small breast scales and layered shoulder armor, curved narrow sword.
- 1: amber heavy sentinel, broad riveted plate armor and an independent chamfered shield with a circular ward seal.
- 2: indigo hooded gun puppet, ivory bird beak, split long coat, energy backpack and banded firearm.
- 3: furnace oni general, large horns, glowing caged chest core, twin furnace exhausts and a chamfered rectangular forge hammer.

Each has Body, Head, Hair (helmet/hood), LeftArm, RightArm, LeftLeg, RightLeg and Weapon. Role 1 also has Shield. Total 27,534 triangles before engine processing. Source coordinates are centimeters, +X front and +Z up, with identity FBX node transforms. Arm pivots are (0, ±24, 34), leg pivots (0, ±12, -29); Weapon is relative to the existing weapon root; Shield uses BodyRoot (34, -20, 0). Boss source remains normal-sized and runtime applies its existing 1.35 scale.

```bash
blender -b --python Scripts/generate_monster_meshes.py -- --preview
```

The preview path asserts every reimported mesh retains identity node transforms and matches source local bounds within 0.001cm. `Scripts/Editor/import_monster_meshes.py` imports to `/Game/Art/Monsters/Generated`, protects non-owned assets, and verifies imported bounds within 1cm. Semantic material slots are Cloth, Metal, Accent, Skin, Glow, Ink and White, matching the character material pipeline.

These are original modular low-poly static meshes derived from the design target, not a hand-sculpted production asset, skinned rig, cloth simulation or finished animation set. Existing combat poses animate the component pivots; elbows and knees are not independently rigged. UE import, material compilation, actual attack silhouettes and packaged rendering require engine validation. No external model or franchise assets are included.
