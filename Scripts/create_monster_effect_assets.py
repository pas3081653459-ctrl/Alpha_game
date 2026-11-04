"""UE5.6 Editor: import original transparent anime VFX atlas and masked cards.

Only generator-owned assets are replaced. Saved assets do not prove shader/render quality.
"""
import hashlib
import json
from pathlib import Path
import struct
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceArt/MonsterEffects/T_MonsterEffects.png'
REVISION = 1
GENERATOR = 'AlphaGame.MonsterEffects.v1'
TAG = 'AlphaGame.GeneratedBy'
DESTINATION = '/Game/Art/MonsterEffects'
NAMES = ('Slash', 'Impact', 'Fire', 'Smoke')
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def source_fingerprint():
    digest = hashlib.sha256(Path(__file__).read_bytes())
    digest.update(SOURCE.read_bytes())
    return digest.hexdigest()


def owned(path):
    if not LIB.does_asset_exist(path):
        return None
    asset = LIB.load_asset(path)
    if LIB.get_metadata_tag(asset, TAG) != GENERATOR:
        raise RuntimeError(f'Refusing to overwrite unowned monster effect asset: {path}')
    return asset


def node(material, cls, x, y, **properties):
    value = EDIT.create_material_expression(material, cls, x, y)
    if not value:
        raise RuntimeError(f'Cannot create material node {cls}')
    for key, item in properties.items():
        value.set_editor_property(key, item)
    return value


def wire(source, output, target, pin):
    if not EDIT.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f'Cannot connect monster effect pin {pin}')


def link(source, output, prop):
    if not EDIT.connect_material_property(source, output, prop):
        raise RuntimeError(f'Cannot connect monster effect material property {prop}')


def save(asset):
    LIB.set_metadata_tag(asset, TAG, GENERATOR)
    if not LIB.save_loaded_asset(asset, False):
        raise RuntimeError(f'Cannot save {asset.get_path_name()}')


def main():
    png = SOURCE.read_bytes()
    if png[:8] != b'\x89PNG\r\n\x1a\n':
        raise RuntimeError('VFX source PNG missing or still an LFS pointer')
    width, height = struct.unpack('>II', png[16:24])
    if width != height or width % 2 or png[25] != 6:
        raise RuntimeError('Monster VFX atlas must be an even square RGBA PNG')
    LIB.make_directory(DESTINATION)
    texture_path = DESTINATION + '/T_MonsterEffects'
    owned(texture_path)
    task = unreal.AssetImportTask()
    for key, value in {'filename': str(SOURCE), 'destination_path': DESTINATION,
                       'destination_name': 'T_MonsterEffects', 'automated': True,
                       'replace_existing': True, 'save': False}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = LIB.load_asset(texture_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError('Monster VFX atlas failed to import')
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    save(texture)
    paths = [texture_path]
    for cell, name in enumerate(NAMES):
        path = DESTINATION + '/M_' + name
        material = owned(path)
        if material is None:
            material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                'M_' + name, DESTINATION, unreal.Material, unreal.MaterialFactoryNew())
        if not isinstance(material, unreal.Material):
            raise RuntimeError(f'Monster VFX material failed: {path}')
        EDIT.delete_all_material_expressions(material)
        material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property('two_sided', True)
        material.set_editor_property('opacity_mask_clip_value', .12)
        uv = node(material, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
        scale = node(material, unreal.MaterialExpressionConstant2Vector, -1000, 160,
                     r=.5 - 2. / width, g=.5 - 2. / height)
        mul = node(material, unreal.MaterialExpressionMultiply, -800, 0)
        wire(uv, '', mul, 'A'); wire(scale, '', mul, 'B')
        offset = node(material, unreal.MaterialExpressionConstant2Vector, -800, 160,
                      r=(cell % 2) * .5 + 1. / width, g=(cell // 2) * .5 + 1. / height)
        add = node(material, unreal.MaterialExpressionAdd, -600, 0)
        wire(mul, '', add, 'A'); wire(offset, '', add, 'B')
        sample = node(material, unreal.MaterialExpressionTextureSample, -400, 0,
                      texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        wire(add, '', sample, 'Coordinates')
        fade = node(material, unreal.MaterialExpressionScalarParameter, -400, 300,
                    parameter_name='Fade', default_value=1.)
        mask = node(material, unreal.MaterialExpressionMultiply, -100, 250)
        wire(sample, 'A', mask, 'A'); wire(fade, '', mask, 'B')
        link(mask, '', unreal.MaterialProperty.MP_OPACITY_MASK)
        glow = node(material, unreal.MaterialExpressionScalarParameter, -400, 450,
                    parameter_name='Glow', default_value=.25)
        gain = node(material, unreal.MaterialExpressionAdd, -100, 450, const_b=1.)
        wire(glow, '', gain, 'A')
        emission = node(material, unreal.MaterialExpressionMultiply, 100, 0)
        wire(sample, 'RGB', emission, 'A'); wire(gain, '', emission, 'B')
        link(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        EDIT.recompile_material(material)
        save(material)
        paths.append(path)
    report = {'status': 'saved', 'revision': REVISION, 'source_fingerprint': source_fingerprint(),
              'assets': paths, 'note': 'Masked atlas import only; shader and gameplay composition need UE verification.'}
    target = ROOT / 'Saved/MonsterEffectsBuild.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, indent=2) + '\n')
    unreal.log(f'Monster VFX atlas saved: {target}')


if __name__ == '__main__':
    main()
