# Original anime character mesh sources

`References/AnimeHeroTurnarounds.png` is the generated design reference. `References/ModeledHeroesPreview.png` is an **offline Blender Workbench render of the exported FBX meshes reimported into Blender**, not an Unreal screenshot or evidence of engine import quality.

The 21 original FBX parts cover three heroes: red split-coat swordsman, teal/white sleeveless fist fighter, and cream/gold long-coat lantern caster. Each uses a torso, face/head, hair, two arms and two legs. Separate material slots preserve clothing, skin, eye whites, iris/accent and hair colors. Curved clothing overlays conform to the torso, and sculpted profile rings replace the previous engine cubes. Total: 29,932 triangles across all three heroes before engine build processing.

Rebuild on a machine with Blender 4.3 or newer:

```bash
blender -b --python Scripts/generate_character_meshes.py -- --preview
```

Sources use centimeters with +X facing, +Z up, and limb-local pivots. Exported FBX nodes have identity translation/rotation/scale and centimeter scene units. The preview path asserts reimported local bounds match the source within 0.001 cm without compensating transforms. The UE import script independently compares each imported mesh's origin and extent with the manifest within 1 cm; it refuses to report success for a 100x scale or axis mismatch.

In UE5.6 Editor Python, run `Scripts/Editor/import_character_meshes.py` (the standard art-build entry point also invokes it). Assets live under `/Game/Art/Characters/Generated`. The importer only replaces assets marked as owned by its generator. Runtime maps semantic material slot names and keeps existing cosmetic body/limb animation pivots. Missing generated parts trigger a complete prototype fallback, rather than a partially assembled hero.

These are modeled static mesh parts, not skinned skeletal characters. Elbows/knees are shaped but do not have independent animated joints; long garments follow the torso rather than cloth simulation. Weapons and enemy silhouettes still use the existing procedural visuals. Further work is required for hand-authored sculpting, retopology, production face textures, skin weighting, skeletal attack animation and UE rendering acceptance. The reference illustration is a design target; this mesh revision is a low-poly modeling foundation.
