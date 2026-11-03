"""Run inside UE5.6 Editor with PythonScriptPlugin; never in the game runtime.

Creates the generated master material and imports original source textures.
Only this generator's tagged assets may be refreshed. Duplicate an asset before
hand-editing it if those edits must survive regeneration.
"""
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
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def check_owned(path):
    if LIB.does_asset_exist(path):
        existing = LIB.load_asset(path)
        if LIB.get_metadata_tag(existing, TAG) != GENERATOR:
            raise RuntimeError(f'Refusing to overwrite an unowned art asset: {path}')
        return existing
    return None


def import_texture(filename, name, normal=False):
    path = f'{TEXTURE_PATH}/{name}'
    check_owned(path)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(ROOT / 'SourceArt/Surfaces' / filename))
    task.set_editor_property('destination_path', TEXTURE_PATH)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = LIB.load_asset(path)
    if not texture:
        raise RuntimeError(f'Texture import failed: {filename}')
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('compression_settings',
        unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS)
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
    return blend, total_emission


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


def main():
    LIB.make_directory('/Game/Art/Materials')
    LIB.make_directory(TEXTURE_PATH)
    detail = import_texture('T_ForgeDetail.png', 'T_ForgeDetail')
    normal = import_texture('T_ForgeNormal_N.png', 'T_ForgeNormal_N', True)
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
    connect(tint, '', base, 'A')
    connect(detail_bias, '', base, 'B')
    emission = expression(material, unreal.MaterialExpressionMultiply, -100, -50)
    connect(tint, '', emission, 'A')
    connect(glow, '', emission, 'B')
    normal_sample = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -350, 500,
                               parameter_name='NormalTexture', texture=normal,
                               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    base, emission = toon_color(material, base, tint, emission)
    property_link(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    property_link(metallic, '', unreal.MaterialProperty.MP_METALLIC)
    property_link(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    property_link(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    property_link(normal_sample, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    save_material(material, MATERIAL_PATH)
    create_effect_material()
    report = {
        'generator': GENERATOR, 'material': MATERIAL_PATH, 'effects_material': EFFECT_PATH,
        'textures': [TEXTURE_PATH + '/T_ForgeDetail', TEXTURE_PATH + '/T_ForgeNormal_N'],
        'parameters': ['Tint', 'Metallic', 'Roughness', 'Glow', 'AnimeShade', 'RimStrength'],
        'style': 'DefaultLit graphic bands and rim; separate opaque unlit energy effects',
        'status': 'saved', 'note': 'Asset save is not a rendered visual or packaging verification.',
    }
    report_path = Path(os.environ.get('ALPHA_ART_REPORT', str(ROOT / 'Saved/ArtBuild.json')))
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    unreal.log(f'AlphaGame art assets saved; report: {report_path}')


if __name__ == '__main__':
    main()
