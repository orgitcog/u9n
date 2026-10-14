"""Run inside Unreal Editor 5.3's Python console (not system Python).

exec(open(r'/absolute/path/import_dte_avatar.py').read())

Set DTE_AVATAR_BUNDLE to the extracted bundle root before running, or edit
BUNDLE below. Requires Python Editor Script Plugin and Editor Scripting Utilities.
No asset is overwritten: use an empty destination folder for the first import.
The runtime plugin is installed separately under the project's Plugins directory.
"""
import os
from pathlib import Path
import unreal

BUNDLE = Path(os.environ.get('DTE_AVATAR_BUNDLE', str(Path.home() / 'DTEAvatarUE')))
DESTINATION = '/Game/DeepTreeEcho/AvatarPrototype'
FBX = BUNDLE / 'Assets' / 'DTE_Avatar_Prototype.fbx'


def import_avatar():
    if not FBX.is_file():
        raise RuntimeError('Avatar FBX not found: {}. Set DTE_AVATAR_BUNDLE.'.format(FBX))
    if unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        existing = unreal.EditorAssetLibrary.list_assets(DESTINATION, recursive=True)
        if existing:
            raise RuntimeError('Destination contains assets; choose a new DESTINATION to avoid overwrites.')
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.automated_import_should_detect_type = False
    options.import_animations = True
    options.import_materials = True
    options.import_textures = True
    options.create_physics_asset = True
    # Custom skeleton: do not assign Manny/MetaHuman here.
    options.skeletal_mesh_import_data.import_morph_targets = False
    options.skeletal_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    options.skeletal_mesh_import_data.import_uniform_scale = 1.0
    options.skeletal_mesh_import_data.convert_scene = True
    options.skeletal_mesh_import_data.convert_scene_unit = True
    task = unreal.AssetImportTask()
    task.filename = str(FBX)
    task.destination_path = DESTINATION
    task.automated = True
    task.replace_existing = False
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = list(task.imported_object_paths)
    if not imported:
        raise RuntimeError('FBX importer created no assets. Inspect the Unreal Output Log.')
    skeletal = []
    for path in imported:
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.SkeletalMesh):
            skeletal.append(path)
    if not skeletal:
        raise RuntimeError('No SkeletalMesh was created. Check the skeleton and skin weights in Blender.')
    unreal.log('DTE import complete: {}'.format(imported))
    unreal.log('Open SkeletalMesh in Persona; inspect height, normals, skinning and both shoulders/knees.')
    unreal.log('Assign DTEAvatarAnimInstance through an Animation Blueprint after enabling DTEAvatarBridge.')
    unreal.log('This importer has not been executed against a native UE editor in the generation sandbox.')
    return imported


if __name__ == '__main__':
    import_avatar()
