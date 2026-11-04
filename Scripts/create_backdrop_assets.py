"""UE5.6 Editor Python: import original three-row anime vistas, create unlit materials.

Rows: dock dawn, forge valley, canal sunset. Only generator-owned assets are replaced.
An asset save report does not prove shader compilation or in-game composition.
"""
import hashlib
import json
import os
from pathlib import Path
import struct

import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceArt/Backdrops/T_ChapterPanoramas.png'
REVISION = 1
BACKDROP_REVISION = REVISION
GENERATOR = 'AlphaGame.Backdrops.v1'
TAG = 'AlphaGame.GeneratedBy'
ASSET_PATH = '/Game/Art/Backdrops'
TEXTURE_PATH = ASSET_PATH + '/T_ChapterPanoramas'
MATERIAL_PATHS = tuple(ASSET_PATH + f'/M_Chapter{i}' for i in range(3))
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def source_fingerprint():
    digest = hashlib.sha256()
    for path in (Path(__file__), SOURCE):
        digest.update(str(path.relative_to(ROOT)).encode('utf-8'))
        digest.update(path.read_bytes())
    return digest.hexdigest()


def owned(path):
    if not LIB.does_asset_exist(path):
        return None
    asset = LIB.load_asset(path)
    if LIB.get_metadata_tag(asset, TAG) != GENERATOR:
        raise RuntimeError(f'Refusing to overwrite unowned backdrop asset: {path}')
    return asset


def node(material, cls, x, y, **properties):
    result = EDIT.create_material_expression(material, cls, x, y)
    if not result:
        raise RuntimeError(f'Could not create {cls}')
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def wire(source, target, target_pin):
    if not EDIT.connect_material_expressions(source, '', target, target_pin):
        raise RuntimeError(f'Could not connect backdrop material pin {target_pin}')


def save(asset):
    LIB.set_metadata_tag(asset, TAG, GENERATOR)
    if not LIB.save_loaded_asset(asset, False):
        raise RuntimeError(f'Could not save backdrop asset: {asset.get_path_name()}')


def main():
    png = SOURCE.read_bytes()
    if png[:8] != b'\x89PNG\r\n\x1a\n':
        raise RuntimeError('Backdrop source must be the actual PNG, not a Git LFS pointer.')
    width, height = struct.unpack('>II', png[16:24])
    if width < 512 or height < 512:
        raise RuntimeError('Backdrop atlas must contain three detailed horizontal panorama rows.')
    LIB.make_directory(ASSET_PATH)
    owned(TEXTURE_PATH)
    task = unreal.AssetImportTask()
    for key, value in {'filename': str(SOURCE), 'destination_path': ASSET_PATH,
                       'destination_name': 'T_ChapterPanoramas', 'automated': True,
                       'replace_existing': True, 'save': False}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = LIB.load_asset(TEXTURE_PATH)
    if not texture or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Backdrop Texture2D import failed')
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    # Three unrelated paintings share an atlas. Mipmapping would blend their borders.
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    save(texture)
    for chapter, path in enumerate(MATERIAL_PATHS):
        material = owned(path)
        if material is None:
            material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                path.rsplit('/', 1)[1], ASSET_PATH, unreal.Material, unreal.MaterialFactoryNew())
        if not material or not isinstance(material, unreal.Material):
            raise RuntimeError(f'Backdrop material creation failed: {path}')
        EDIT.delete_all_material_expressions(material)
        material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property('two_sided', True)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
        uv = node(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
        scale = node(material, unreal.MaterialExpressionConstant2Vector, -900, 150,
                     r=1., g=1. / 3. - 2. / height)
        mul = node(material, unreal.MaterialExpressionMultiply, -650, 0)
        wire(uv, mul, 'A')
        wire(scale, mul, 'B')
        offset = node(material, unreal.MaterialExpressionConstant2Vector, -650, 200,
                      r=0., g=chapter / 3. + 1. / height)
        add = node(material, unreal.MaterialExpressionAdd, -400, 0)
        wire(mul, add, 'A')
        wire(offset, add, 'B')
        sample = node(material, unreal.MaterialExpressionTextureSample, -150, 0,
                      texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        wire(add, sample, 'Coordinates')
        if not EDIT.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError(f'Could not connect backdrop emissive: {path}')
        EDIT.recompile_material(material)
        save(material)
    report = {'generator': GENERATOR, 'revision': REVISION, 'status': 'saved',
              'source_fingerprint': source_fingerprint(), 'assets': [TEXTURE_PATH, *MATERIAL_PATHS],
              'source_size': [width, height], 'row_order': ['dock', 'forge', 'canal'],
              'note': 'Requires UE shader, composition, atlas orientation and packaged-render verification.'}
    report_path = Path(os.environ.get('ALPHA_BACKDROP_REPORT', str(ROOT / 'Saved/BackdropBuild.json')))
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    unreal.log(f'AlphaGame painted vistas saved: {report_path}')


if __name__ == '__main__':
    main()
