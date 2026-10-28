# Half-Life 1 — Modern-Graphics Remaster

## Creative target

Preserve the original 1998 Half-Life campaign: Black Mesa, enemy behaviors,
weapons, scripted events, story and level progression. Modernize the visuals
substantially beyond Half-Life 2 (2004).

**Target quality:** realistic high-detail 2K/4K PBR materials, detailed original
lab props, redesigned character models and animation, modern indoor lighting,
reflections, volumetrics, environmental FX and faithful level layout.

This is the **design target**, not a claim that the game is presently playable
in a modern engine.

## Recommended platforms

- **Build and validate on PC first.** Unreal Engine 5 is a plausible
  high-end rendering target; its Lumen lighting, Nanite meshes and PBR
  workflows suit the look. Unreal recommends FBX for static-mesh imports.
  This repo currently exports OBJ as a portable intermediate; import in
  Blender, clean geometry and export FBX for Unreal.
- **Later:** review Xbox Series X|S as a separate licensed console
  deployment target once a full PC vertical slice runs. Xbox Dev Mode by
  itself is not proof an arbitrary Unreal game can be deployed.
- **Original Xbox / Xbox 360:** do not expect Unreal 5 Lumen or Nanite.
  A lower-fidelity original-Xbox graphical upgrade is a distinct project.
- **Half-Life 2 original Xbox image:** not a new engine or automatic asset
  upgrade for Half-Life 1. The linked Google Drive ISO was identified by
  metadata but raw download failed, so its assets have not been used.

Useful technical docs:
- https://dev.epicgames.com/documentation/unreal-engine/lumen-technical-details-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/importing-static-meshes-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/designing-visuals-rendering-and-graphics-with-unreal-engine

## Actual implementation and test status

The repository already has tools/half_life1_xbox.py for XISO extraction and
tools/half_life1_graphics.py for early original-Xbox detail overlays.

**New modern-engine preparation tools:**

- tools/half_life1_modern_scene.py reads GoldSrc BSP version 30 worldspawn
  geometry from the user's existing Half-LifeX ISO and writes:
  hl1_c0a0_world.obj, hl1_c0a0_world.mtl, material_manifest.json.
  It preserves original geometric proportions and texture coordinates.
  It only exports worldspawn; other models may contain moving doors, lifts
  and other gameplay-linked geometry and are **not yet reconstructed**.
- tools/half_life1_pbr_starter.py generates 512px (configurable) original
  base-color, normal, roughness, and metallic maps for six procedural
  material presets. It updates the OBJ MTL with preview diffuse maps and
  populates the material manifest's PBR slots. These are **starter
  placeholders**, not high-quality final photoreal art or Source assets.
- tests/test_half_life1_modern_scene.py and
  tests/test_half_life1_pbr_starter.py use synthetic fixtures.

**Verified with Half-LifeX Beta 1.4 c0a0 map:**

- 3,047 worldspawn BSP faces
- 7,833 exported triangles, 46 material groups, zero invalid faces skipped
- 12 additional brush models intentionally excluded from static world
- OBJ/MTL and PBR PNG starter assets generated, verified structurally
- Seven synthetic tests for the new tools passed locally

This is **a renderable static scene asset**, *not* a playable rebuilt game.
Lighting, physics, NPCs, scripting, weapons, encounters, save/load and level
transitions are not implemented by these tools.

## Windows PowerShell workflow

From the checkout root; Python 3.10+ needed.

    py -3 -m pip install pillow numpy
    py -3 -m tools.half_life1_modern_scene "C:\path\to\Half-LifeX_Test_v2.iso" --map c0a0 -o "local\hl1-modern\c0a0"
    py -3 -m tools.half_life1_pbr_starter "local\hl1-modern\c0a0" --resolution 512
    py -3 -m unittest tests.test_half_life1_modern_scene tests.test_half_life1_pbr_starter

Outputs remain inside the ignored local directory:

    local/hl1-modern/c0a0/
      hl1_c0a0_world.obj
      hl1_c0a0_world.mtl
      material_manifest.json
      materials/concrete_basecolor.png
      materials/concrete_normal.png
      materials/concrete_roughness.png
      materials/concrete_metallic.png
      ... six presets, four channels each

Import the OBJ into Blender (File > Import > Wavefront OBJ). Review its
orientation, scale and mapped material textures. The file uses Y-up **meters**
where 1 GoldSrc source unit was treated as 0.0254 m. For Unreal (centimeters),
convert via Blender to FBX at verified scale; rebuild normals where
necessary, author collision and lightmap UVs, and connect all material PBR
channels in the engine manually. Shader response and gameplay are unverified.

## Development milestones

1. Recover and validate entire static world across the campaign.
2. Export moving brush model geometry separately and restore entity links.
3. Author properly licensed modern PBR assets and high-detail environment
   props while preserving recognizable Black Mesa architecture.
4. Establish an Unreal map prototype with lighting, shadows, post effects,
   reflection captures and collision.
5. Port or recreate original scripted sequences, NPC AI, interactions,
   weapons and physics; keep the original story and level order.
6. Ship a fully interactive **first Black Mesa chapter** as a desktop
   vertical slice with debug logs and performance profiling.
7. Repeat for the full campaign, then evaluate technically supported
   console hardware, access and packaging.

## Copyright and local-only game files

Only source tools, docs and synthetic test fixtures belong on GitHub.
Keep retail Xbox ISO, executable, original game BSPs, PACK files,
Valve art, models, sound, Source assets and Xbox SDKs local.
The repository root .gitignore excludes local/.
