"""UE5.6 Editor Python: import owned modular anime monster meshes, not skeletons.
Run after create_art_assets.py; FBX contains cm +X-front vertices, convert_scene is disabled.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]
REVISION = 1
GENERATOR = 'AlphaGame.Monsters.v1'
TAG = 'AlphaGame.GeneratedBy'
DESTINATION = '/Game/Art/Monsters/Generated'
PARTS = ('Body', 'Head', 'Hair', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg', 'Weapon')
LIB = unreal.EditorAssetLibrary


def source_fingerprint():
    digest = hashlib.sha256()
    sources = [Path(__file__), ROOT/'Scripts/generate_monster_meshes.py', ROOT/'SourceArt/Monsters/manifest.json']
    sources.extend(sorted((ROOT/'SourceArt/Monsters').glob('*.fbx')))
    for path in sources:
        digest.update(str(path.relative_to(ROOT)).encode());digest.update(path.read_bytes())
    return digest.hexdigest()


def main():
    LIB.make_directory(DESTINATION)
    manifest=json.loads((ROOT/'SourceArt/Monsters/manifest.json').read_text())
    expected_bounds={entry['name']:entry['bounds_cm'] for entry in manifest['meshes']}
    assets=[]
    for role in range(4):
        for part in PARTS+(('Shield',) if role==1 else ()):
            name=f'SM_AlphaMonster{role}_{part}'
            source=ROOT/'SourceArt/Monsters'/(name+'.fbx')
            if not source.is_file() or source.read_bytes().startswith(b'version https://git-lfs.github.com/spec/'):
                raise RuntimeError(f'Missing FBX source or LFS pointer: {source}; run git lfs pull')
            path=DESTINATION+'/'+name
            if LIB.does_asset_exist(path) and LIB.get_metadata_tag(LIB.load_asset(path),TAG)!=GENERATOR:
                raise RuntimeError('Refusing to replace unowned monster mesh: '+path)
            options=unreal.FbxImportUI()
            options.set_editor_property('import_mesh',True)
            options.set_editor_property('import_as_skeletal',False)
            options.set_editor_property('import_materials',False)
            options.set_editor_property('import_textures',False)
            options.set_editor_property('import_animations',False)
            options.set_editor_property('automated_import_should_detect_type',False)
            options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
            data=options.get_editor_property('static_mesh_import_data')
            data.set_editor_property('combine_meshes',True)
            data.set_editor_property('transform_vertex_to_absolute',True)
            data.set_editor_property('auto_generate_collision',False)
            data.set_editor_property('convert_scene',False)
            data.set_editor_property('convert_scene_unit',False)
            data.set_editor_property('force_front_x_axis',False)
            data.set_editor_property('import_uniform_scale',1.)
            data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
            task=unreal.AssetImportTask()
            for key,value in {'filename':str(source),'destination_path':DESTINATION,'destination_name':name,'automated':True,'replace_existing':True,'save':False,'options':options,'factory':unreal.FbxFactory()}.items():
                task.set_editor_property(key,value)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            mesh=LIB.load_asset(path)
            if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Expected StaticMesh at '+path)
            bounds=mesh.get_bounds()
            low,high=expected_bounds[name]
            expected_origin=[(a+b)*.5 for a,b in zip(low,high)]
            expected_extent=[(b-a)*.5 for a,b in zip(low,high)]
            actual_origin=[bounds.origin.x,bounds.origin.y,bounds.origin.z]
            actual_extent=[bounds.box_extent.x,bounds.box_extent.y,bounds.box_extent.z]
            if any(abs(a-b)>1. for a,b in zip(actual_origin+actual_extent,expected_origin+expected_extent)):
                raise RuntimeError(f'Monster mesh cm bounds/axis mismatch at {path}: origin={actual_origin}, extent={actual_extent}')
            slots={str(slot.get_editor_property('material_slot_name')) for slot in mesh.get_editor_property('static_materials')}
            if not slots or not slots.issubset({'Cloth','Metal','Accent','Skin','Glow','Ink','White'}):
                raise RuntimeError(f'Unexpected FBX material slots {slots} at {path}')
            LIB.set_metadata_tag(mesh,TAG,GENERATOR)
            if not LIB.save_loaded_asset(mesh,False):raise RuntimeError('Failed mesh save: '+path)
            assets.append(path)
    report={'revision':REVISION,'status':'saved','generator':GENERATOR,'source_fingerprint':source_fingerprint(),'assets':assets,'note':'Static meshes imported; not a skeletal rig, animation or rendered-quality verification.'}
    target=ROOT/'Saved/MonsterBuild.json';target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(report,indent=2)+'\n')
    unreal.log('Imported 33 modular anime monster meshes: '+str(target))


if __name__=='__main__':main()
