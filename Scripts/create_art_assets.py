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


def main():
    LIB.make_directory('/Game/Art/Materials')
    LIB.make_directory(TEXTURE_PATH)
    detail = import_texture('T_ForgeDetail.png', 'T_ForgeDetail')
    normal = import_texture('T_ForgeNormal_N.png', 'T_ForgeNormal_N', True)
    material = check_owned(MATERIAL_PATH)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_AlphaSurface', '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError('Master material creation failed')
    EDIT.delete_all_material_expressions(material)
    material.set_editor_property('used_with_instanced_static_meshes', True)
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
    property_link(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    property_link(metallic, '', unreal.MaterialProperty.MP_METALLIC)
    property_link(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    property_link(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    property_link(normal_sample, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    EDIT.recompile_material(material)
    LIB.set_metadata_tag(material, TAG, GENERATOR)
    if not LIB.save_loaded_asset(material, False):
        raise RuntimeError('Master material save failed')
    report = {
        'generator': GENERATOR, 'material': MATERIAL_PATH,
        'textures': [TEXTURE_PATH + '/T_ForgeDetail', TEXTURE_PATH + '/T_ForgeNormal_N'],
        'parameters': ['Tint', 'Metallic', 'Roughness', 'Glow'],
        'status': 'saved', 'note': 'Asset save is not a rendered visual or packaging verification.',
    }
    report_path = Path(os.environ.get('ALPHA_ART_REPORT', str(ROOT / 'Saved/ArtBuild.json')))
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    unreal.log(f'AlphaGame art assets saved; report: {report_path}')


if __name__ == '__main__':
    main()
