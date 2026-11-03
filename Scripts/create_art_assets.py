"""Run inside UE5.6 Editor with PythonScriptPlugin; never in the game runtime.

Creates the generated master material and imports original source textures.
Only this generator's tagged assets may be refreshed. Duplicate an asset before
hand-editing it if those edits must survive regeneration.
"""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[1]
TAG = 'AlphaGame.GeneratedBy'
GENERATOR = 'AlphaGame.Art.v1'
MATERIAL_PATH = '/Game/Art/Materials/M_AlphaSurface'
EFFECT_PATH = '/Game/Art/Materials/M_AlphaEffect'
TEXTURE_PATH = '/Game/Art/Textures'
ATLAS_SOURCE = ROOT / 'SourceArt/Anime/T_AnimeSurfaceAtlas.png'
ART_REVISION = 3
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def check_owned(path):
    if LIB.does_asset_exist(path):
        existing = LIB.load_asset(path)
        if LIB.get_metadata_tag(existing, TAG) != GENERATOR:
            raise RuntimeError(f'Refusing to overwrite an unowned art asset: {path}')
        return existing
    return None


def import_texture(filename, name, normal=False, color=False, source_folder="Surfaces"):
    path = f'{TEXTURE_PATH}/{name}'
    check_owned(path)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(ROOT / 'SourceArt' / source_folder / filename))
    task.set_editor_property('destination_path', TEXTURE_PATH)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = LIB.load_asset(path)
    if not texture:
        raise RuntimeError(f'Texture import failed: {filename}')
    texture.set_editor_property('srgb', color)
    texture.set_editor_property('compression_settings',
        unreal.TextureCompressionSettings.TC_NORMALMAP if normal else (
            unreal.TextureCompressionSettings.TC_DEFAULT if color else unreal.TextureCompressionSettings.TC_MASKS))
    LIB.set_metadata_tag(texture, TAG, GENERATOR)
    if not LIB.save_loaded_asset(texture, False):
        raise RuntimeError(f'Texture save failed: {path}')
    return texture


def expression(material, cls, x, y, **properties):
    node = EDIT.create_material_expression(material, cls, x, y)
    if not node:
        raise RuntimeError(f'Could not create material expression {cls}')
    for name, value in properties.items():
        node.set_editor_property(name, value)
    return node


def connect(source, output, target, input_name):
    if not EDIT.connect_material_expressions(source, output, target, input_name):
        raise RuntimeError(f'Material connection failed: {input_name}')


def property_link(node, output, prop):
    if not EDIT.connect_material_property(node, output, prop):
        raise RuntimeError(f'Material property connection failed: {prop}')


def create_material(path):
    material = check_owned(path)
    if material is None:
        folder, name = path.rsplit('/', 1)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError(f'Material creation failed: {path}')
    EDIT.delete_all_material_expressions(material)
    material.set_editor_property('used_with_instanced_static_meshes', True)
    return material


def save_material(material, path):
    EDIT.recompile_material(material)
    LIB.set_metadata_tag(material, TAG, GENERATOR)
    if not LIB.save_loaded_asset(material, False):
        raise RuntimeError(f'Material save failed: {path}')


def toon_color(material, base, tint, emission):
    # Graphic three-band modulation over ordinary DefaultLit lighting. This is
    # not a post-process that quantizes actual scene shadows: physical lights
    # and shadows still work. Use a weaker amount on scenery than characters.
    amount = expression(material, unreal.MaterialExpressionScalarParameter, -900, 750,
                        parameter_name='AnimeShade', default_value=.35)
    normal = expression(material, unreal.MaterialExpressionPixelNormalWS, -900, 900)
    key = expression(material, unreal.MaterialExpressionConstant3Vector, -900, 1050,
                     constant=unreal.LinearColor(.36, -.48, .8, 0))
    dot = expression(material, unreal.MaterialExpressionDotProduct, -700, 900)
    connect(normal, '', dot, 'A')
    connect(key, '', dot, 'B')
    half = expression(material, unreal.MaterialExpressionMultiply, -500, 900, const_b=.5)
    connect(dot, '', half, 'A')
    bias = expression(material, unreal.MaterialExpressionAdd, -300, 900, const_b=.5)
    connect(half, '', bias, 'A')
    clamp = expression(material, unreal.MaterialExpressionSaturate, -100, 900)
    connect(bias, '', clamp, 'Input')
    levels = expression(material, unreal.MaterialExpressionMultiply, 100, 900, const_b=2.99)
    connect(clamp, '', levels, 'A')
    floor = expression(material, unreal.MaterialExpressionFloor, 300, 900)
    connect(levels, '', floor, 'Input')
    range_scale = expression(material, unreal.MaterialExpressionMultiply, 500, 900, const_b=.225)
    connect(floor, '', range_scale, 'A')
    bands = expression(material, unreal.MaterialExpressionAdd, 700, 900, const_b=.55)
    connect(range_scale, '', bands, 'A')
    shaded = expression(material, unreal.MaterialExpressionMultiply, 900, 700)
    connect(base, '', shaded, 'A')
    connect(bands, '', shaded, 'B')
    blend = expression(material, unreal.MaterialExpressionLinearInterpolate, 1100, 500)
    connect(base, '', blend, 'A')
    connect(shaded, '', blend, 'B')
    connect(amount, '', blend, 'Alpha')

    fresnel = expression(material, unreal.MaterialExpressionFresnel, -300, 1200,
                         exponent=5., base_reflect_fraction=0.)
    rim_strength = expression(material, unreal.MaterialExpressionScalarParameter, -300, 1350,
                              parameter_name='RimStrength', default_value=.12)
    rim = expression(material, unreal.MaterialExpressionMultiply, -50, 1200)
    connect(fresnel, '', rim, 'A')
    connect(rim_strength, '', rim, 'B')
    masked = expression(material, unreal.MaterialExpressionMultiply, 150, 1200)
    connect(rim, '', masked, 'A')
    connect(amount, '', masked, 'B')
    colored = expression(material, unreal.MaterialExpressionMultiply, 350, 1200)
    connect(masked, '', colored, 'A')
    connect(tint, '', colored, 'B')
    total_emission = expression(material, unreal.MaterialExpressionAdd, 1100, 1000)
    connect(emission, '', total_emission, 'A')
    connect(colored, '', total_emission, 'B')
    # Entry has no guaranteed sky capture. A restrained stylized ambient floor
    # keeps shadow-side colors readable; direct lighting/shadows still add shape.
    ambient_fill = expression(material, unreal.MaterialExpressionScalarParameter, 700, 1400,
                              parameter_name='AmbientFill', default_value=.16)
    ambient_color = expression(material, unreal.MaterialExpressionMultiply, 900, 1400)
    connect(base, '', ambient_color, 'A')
    connect(ambient_fill, '', ambient_color, 'B')
    readable_emission = expression(material, unreal.MaterialExpressionAdd, 1300, 1100)
    connect(total_emission, '', readable_emission, 'A')
    connect(ambient_color, '', readable_emission, 'B')
    return blend, readable_emission


def create_effect_material():
    material = create_material(EFFECT_PATH)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    tint = expression(material, unreal.MaterialExpressionVectorParameter, -600, -100,
                      parameter_name='Tint', default_value=unreal.LinearColor(1, .65, .15, 1))
    glow = expression(material, unreal.MaterialExpressionScalarParameter, -600, 100,
                      parameter_name='Glow', default_value=1.)
    # A base 1 keeps the solid bright-color silhouettes readable even with
    # Bloom disabled and during glow decay. No transparent overlap stacking.
    gain = expression(material, unreal.MaterialExpressionAdd, -350, 100, const_b=1.)
    connect(glow, '', gain, 'A')
    emission = expression(material, unreal.MaterialExpressionMultiply, -100, -100)
    connect(tint, '', emission, 'A')
    connect(gain, '', emission, 'B')
    property_link(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    save_material(material, EFFECT_PATH)
    validate_parameters(material, EFFECT_PATH, vector=('Tint',), scalar=('Glow',))


def source_fingerprint():
    paths = [Path(__file__), ROOT / 'SourceArt/Surfaces/T_ForgeDetail.png',
             ROOT / 'SourceArt/Surfaces/T_ForgeNormal_N.png', ATLAS_SOURCE]
    digest = hashlib.sha256()
    for path in paths:
        if not path.is_file():
            raise RuntimeError(f'Required source art is missing: {path}')
        digest.update(path.relative_to(ROOT).as_posix().encode('utf-8'))
        digest.update(path.read_bytes())
    return digest.hexdigest()


def validate_parameters(material, path, vector=(), scalar=(), texture=()):
    # A saved package alone does not establish that runtime parameters exist.
    actual = {
        'vector': set(map(str, EDIT.get_vector_parameter_names(material))),
        'scalar': set(map(str, EDIT.get_scalar_parameter_names(material))),
        'texture': set(map(str, EDIT.get_texture_parameter_names(material))),
    }
    for kind, expected in (('vector', vector), ('scalar', scalar), ('texture', texture)):
        missing = set(expected) - actual[kind]
        if missing:
            raise RuntimeError(f'{path} missing {kind} parameters: {sorted(missing)}')


def atlas_color(material, tint, atlas):
    # Crop safely INSIDE a 2x2 atlas tile; source corners do not bleed across cells.
    # 0 wood, 1 ivory stone, 2 teal ceramic, 3 crimson cloth. Characters default
    # TextureStrength=0, so skin/armor never inherit a wood color by accident.
    uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -1600, -700)
    repeat = expression(material, unreal.MaterialExpressionFrac, -1400, -700)
    connect(uv, '', repeat, 'Input')
    inset = expression(material, unreal.MaterialExpressionMultiply, -1200, -700, const_b=.46)
    connect(repeat, '', inset, 'A')
    margin = expression(material, unreal.MaterialExpressionAdd, -1000, -700, const_b=.02)
    connect(inset, '', margin, 'A')
    style = expression(material, unreal.MaterialExpressionScalarParameter, -1600, -450,
                       parameter_name='SurfaceStyle', default_value=0.)
    two = expression(material, unreal.MaterialExpressionConstant, -1600, -300, r=2.)
    column = expression(material, unreal.MaterialExpressionFmod, -1400, -450)
    connect(style, '', column, 'A')
    connect(two, '', column, 'B')
    half = expression(material, unreal.MaterialExpressionMultiply, -1400, -250, const_b=.5)
    connect(style, '', half, 'A')
    row = expression(material, unreal.MaterialExpressionFloor, -1200, -250)
    connect(half, '', row, 'Input')
    cell = expression(material, unreal.MaterialExpressionAppendVector, -1000, -450)
    connect(column, '', cell, 'A')
    connect(row, '', cell, 'B')
    cell_scale = expression(material, unreal.MaterialExpressionMultiply, -800, -450, const_b=.5)
    connect(cell, '', cell_scale, 'A')
    coordinates = expression(material, unreal.MaterialExpressionAdd, -600, -650)
    connect(margin, '', coordinates, 'A')
    connect(cell_scale, '', coordinates, 'B')
    sample = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, -650,
                        parameter_name='SurfaceAtlas', texture=atlas,
                        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(coordinates, '', sample, 'UVs')
    strength = expression(material, unreal.MaterialExpressionScalarParameter, -400, -400,
                          parameter_name='TextureStrength', default_value=0.)
    blend = expression(material, unreal.MaterialExpressionLinearInterpolate, -200, -450)
    connect(tint, 'RGB', blend, 'A')
    connect(sample, 'RGB', blend, 'B')
    connect(strength, '', blend, 'Alpha')
    return blend


def flat_color_outputs(material, base, emission, tint, metallic, roughness):
    # Diagnostic isolation: no diffuse/specular lighting contribution, while
    # retaining DefaultLit for the normal mode. Flat color intentionally ignores
    # textures, glow, bands and rim so the runtime Tint can be checked directly.
    flat = expression(material, unreal.MaterialExpressionScalarParameter, 1400, -650,
                      parameter_name='FlatColor', default_value=0.)
    black = expression(material, unreal.MaterialExpressionConstant, 1400, -450, r=0.)
    matte = expression(material, unreal.MaterialExpressionConstant, 1400, -250, r=1.)
    base_result = expression(material, unreal.MaterialExpressionLinearInterpolate, 1650, -650)
    connect(base, '', base_result, 'A')
    connect(black, '', base_result, 'B')
    connect(flat, '', base_result, 'Alpha')
    emission_result = expression(material, unreal.MaterialExpressionLinearInterpolate, 1650, -450)
    connect(emission, '', emission_result, 'A')
    connect(tint, 'RGB', emission_result, 'B')
    connect(flat, '', emission_result, 'Alpha')
    metal_result = expression(material, unreal.MaterialExpressionLinearInterpolate, 1650, -250)
    connect(metallic, '', metal_result, 'A')
    connect(black, '', metal_result, 'B')
    connect(flat, '', metal_result, 'Alpha')
    rough_result = expression(material, unreal.MaterialExpressionLinearInterpolate, 1650, -50)
    connect(roughness, '', rough_result, 'A')
    connect(matte, '', rough_result, 'B')
    connect(flat, '', rough_result, 'Alpha')
    specular_result = expression(material, unreal.MaterialExpressionLinearInterpolate, 1650, 150, const_a=.5)
    connect(black, '', specular_result, 'B')
    connect(flat, '', specular_result, 'Alpha')
    return base_result, emission_result, metal_result, rough_result, specular_result


def main():
    LIB.make_directory('/Game/Art/Materials')
    LIB.make_directory(TEXTURE_PATH)
    detail = import_texture('T_ForgeDetail.png', 'T_ForgeDetail')
    normal = import_texture('T_ForgeNormal_N.png', 'T_ForgeNormal_N', True)
    atlas = import_texture('T_AnimeSurfaceAtlas.png', 'T_AnimeSurfaceAtlas', color=True, source_folder='Anime')
    material = create_material(MATERIAL_PATH)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    tint = expression(material, unreal.MaterialExpressionVectorParameter, -700, -250,
                      parameter_name='Tint', default_value=unreal.LinearColor(.1, .2, .25, 1))
    metallic = expression(material, unreal.MaterialExpressionScalarParameter, -400, 100,
                          parameter_name='Metallic', default_value=.5)
    roughness = expression(material, unreal.MaterialExpressionScalarParameter, -400, 200,
                           parameter_name='Roughness', default_value=.65)
    glow = expression(material, unreal.MaterialExpressionScalarParameter, -700, -100,
                      parameter_name='Glow', default_value=0.)
    detail_sample = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 350,
                               parameter_name='DetailTexture', texture=detail,
                               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    # Restrained 0.82..1 surface modulation keeps combat colors recognizable.
    detail_scale = expression(material, unreal.MaterialExpressionMultiply, -450, 350, const_b=.18)
    connect(detail_sample, 'R', detail_scale, 'A')
    detail_bias = expression(material, unreal.MaterialExpressionAdd, -250, 350, const_b=.82)
    connect(detail_scale, '', detail_bias, 'A')
    base = expression(material, unreal.MaterialExpressionMultiply, 0, -200)
    textured_tint = atlas_color(material, tint, atlas)
    connect(textured_tint, '', base, 'A')
    connect(detail_bias, '', base, 'B')
    emission = expression(material, unreal.MaterialExpressionMultiply, -100, -50)
    connect(tint, '', emission, 'A')
    connect(glow, '', emission, 'B')
    normal_sample = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -350, 500,
                               parameter_name='NormalTexture', texture=normal,
                               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    base, emission = toon_color(material, base, tint, emission)
    base, emission, metallic, roughness, specular = flat_color_outputs(material, base, emission, tint, metallic, roughness)
    property_link(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    property_link(metallic, '', unreal.MaterialProperty.MP_METALLIC)
    property_link(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    property_link(specular, '', unreal.MaterialProperty.MP_SPECULAR)
    property_link(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    property_link(normal_sample, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    save_material(material, MATERIAL_PATH)
    validate_parameters(material, MATERIAL_PATH, vector=('Tint',),
                        scalar=('Metallic', 'Roughness', 'Glow', 'AnimeShade', 'AmbientFill', 'SurfaceStyle', 'TextureStrength', 'FlatColor'),
                        texture=('SurfaceAtlas', 'DetailTexture', 'NormalTexture'))
    create_effect_material()
    report = {
        'generator': GENERATOR, 'material': MATERIAL_PATH, 'effects_material': EFFECT_PATH,
        'textures': [TEXTURE_PATH + '/T_ForgeDetail', TEXTURE_PATH + '/T_ForgeNormal_N', TEXTURE_PATH + '/T_AnimeSurfaceAtlas'],
        'parameters': ['Tint', 'Metallic', 'Roughness', 'Glow', 'AnimeShade', 'RimStrength', 'AmbientFill', 'SurfaceStyle', 'TextureStrength', 'FlatColor'],
        'style': 'DefaultLit graphic bands and rim; separate opaque unlit energy effects',
        'revision': ART_REVISION, 'source_fingerprint': source_fingerprint(),
        'parameter_contract': 'checked',
        'status': 'saved', 'note': 'Asset save is not a rendered visual or packaging verification.',
    }
    report_path = Path(os.environ.get('ALPHA_ART_REPORT', str(ROOT / 'Saved/ArtBuild.json')))
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    unreal.log(f'AlphaGame art assets saved; report: {report_path}')


if __name__ == '__main__':
    main()
