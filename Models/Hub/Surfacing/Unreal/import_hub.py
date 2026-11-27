"""Install the saved Painter exports onto the existing runtime hub meshes."""
from pathlib import Path
import json
import shutil
import sys
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
SOURCE=ROOT/'Models/Hub/Surfacing'
BACKUP=ROOT/'Saved/Backups/VisualPipeline'
GAME='/Game/LVL/Hub/Theatre'
PBR='/Game/MATERIALS/PBR/M_Surface'
LIB=unreal.MaterialEditingLibrary
EDITOR=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
MESHES=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
info=json.loads((SOURCE/'Painter/Modules.json').read_text(encoding='utf8'))

def task(path,folder,replace=False,config=None):
    request=unreal.AssetImportTask()
    values={'filename':str(path),'destination_path':folder,'replace_existing':replace,
            'replace_existing_settings':False,'automated':True,'save':False}
    if config is not None:
        values['options']=config
    for key,value in values.items():
        request.set_editor_property(key,value)
    return request

def fbx_options():
    config=unreal.FbxImportUI()
    config.set_editor_property('automated_import_should_detect_type',False)
    config.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
    config.set_editor_property('import_mesh',True)
    config.set_editor_property('import_materials',False)
    config.set_editor_property('import_textures',False)
    mesh=config.get_editor_property('static_mesh_import_data')
    mesh.set_editor_property('combine_meshes',True)
    mesh.set_editor_property('generate_lightmap_u_vs',False)
    mesh.set_editor_property('auto_generate_collision',False)
    mesh.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return config

def read_mesh(mesh):
    bounds=mesh.get_bounds()
    collision=mesh.get_editor_property('body_setup')
    return {
      'bounds':list(bounds.origin.to_tuple())+list(bounds.box_extent.to_tuple()),
      'triangles':mesh.get_num_triangles(0),
      'slots':[str(s.material_slot_name) for s in mesh.get_editor_property('static_materials')],
      'collision':str(collision.get_editor_property('collision_trace_flag')) if collision else None,
      'nanite':mesh.get_editor_property('nanite_settings').get_editor_property('enabled')
    }

def store_backup(asset):
    relative=Path(asset.get_path_name().split('.')[0].removeprefix('/Game/')+'.uasset')
    source=ROOT/'Content'/relative
    destination=BACKUP/relative
    if not destination.exists() and source.exists():
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,destination)

def import_textures(module):
    channels={
      'BaseColor':('Base Color Map',unreal.TextureCompressionSettings.TC_DEFAULT,True),
      'Normal':('Normal Map',unreal.TextureCompressionSettings.TC_NORMALMAP,False),
      'ORM':('ORM Map',unreal.TextureCompressionSettings.TC_MASKS,False)
    }
    if set(info[module]['roles']) & {'LanternGlow','RimGlow','WindowLight'}:
        channels['Emissive']=('Emissive Map',unreal.TextureCompressionSettings.TC_DEFAULT,True)
    folder=GAME+'/Surfacing/Textures'
    result={}
    for key,(param,compression,srgb) in channels.items():
        file=SOURCE/'Textures'/f'Hub_{module}_{key}.png'
        request=task(file,folder,True)
        TOOLS.import_asset_tasks([request])
        obj=next((x for x in request.get_objects() if isinstance(x,unreal.Texture2D)),None)
        if obj is None:
            raise RuntimeError(f'Import did not produce a texture for {file}')
        obj.set_editor_property('compression_settings',compression)
        obj.set_editor_property('srgb',srgb)
        EDITOR.save_loaded_asset(obj,only_if_is_dirty=False)
        result[param]=obj
    return result

def atlas_material(module,maps):
    folder=GAME+'/Surfacing/Materials'
    name='MI_Hub_Painted_'+module
    inst=unreal.load_asset(folder+'/'+name) if EDITOR.does_asset_exist(folder+'/'+name) else None
    if inst is None:
        inst=TOOLS.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    parent=unreal.load_asset(PBR)
    LIB.set_material_instance_parent(inst,parent)
    for key,tex in maps.items():
        LIB.set_material_instance_texture_parameter_value(inst,key,tex)
        assert LIB.get_material_instance_texture_parameter_value(inst,key)==tex,(inst,key)
    LIB.set_material_instance_vector_parameter_value(inst,'Base Color Tint',unreal.LinearColor(1,1,1,1))
    LIB.set_material_instance_scalar_parameter_value(inst,'UV Scale',1.0)
    LIB.set_material_instance_scalar_parameter_value(inst,'Normal Strength',.75)
    LIB.set_material_instance_static_switch_parameter_value(inst,'Use ORM',True)
    LIB.set_material_instance_static_switch_parameter_value(inst,'Use Emissive','Emissive Map' in maps)
    LIB.set_material_instance_static_switch_parameter_value(inst,'Use Emissive Map','Emissive Map' in maps)
    if 'Emissive Map' in maps:
        LIB.set_material_instance_scalar_parameter_value(inst,'Emissive Multiplier',1.1)
    properties=inst.get_editor_property('base_property_overrides')
    properties.set_editor_property('override_two_sided',True)
    properties.set_editor_property('two_sided',True)
    inst.set_editor_property('base_property_overrides',properties)
    LIB.update_material_instance(inst)
    EDITOR.save_loaded_asset(inst,only_if_is_dirty=False)
    return inst

def import_module(module):
    name='SM_Hub_'+module
    mesh_path=GAME+'/Meshes/'+name
    original=unreal.load_asset(mesh_path)
    before=read_mesh(original)
    store_backup(original)
    file=SOURCE/'Runtime'/(name+'.fbx')
    request=task(file,GAME+'/Meshes',True,fbx_options())
    TOOLS.import_asset_tasks([request])
    mesh=next((o for o in request.get_objects() if isinstance(o,unreal.StaticMesh)),None)
    assert mesh and mesh.get_path_name().split('.')[0]==mesh_path,mesh_path
    after=read_mesh(mesh)
    # Unreal can discard a different number of degenerate triangles after the UV reimport.
    assert abs(before['triangles']-after['triangles'])<=max(1,int(before['triangles']*.02)),(module,before,after)
    assert all(abs(a-b)<.03 for a,b in zip(before['bounds'],after['bounds'])),(module,before,after)
    assert before['slots']==after['slots'],(module,before,after)
    assert before['collision']==after['collision'] and before['nanite']==after['nanite'],(module,before,after)
    inst=atlas_material(module,import_textures(module))
    for i in range(len(after['slots'])):
        mesh.set_material(i,inst)
    assert EDITOR.save_loaded_asset(mesh,only_if_is_dirty=False)
    return {'module':module,'material':inst.get_path_name(),'mesh':after}

if __name__=='__main__':
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
    results=[import_module(m) for m in sys.argv[1:]]
    print(json.dumps(results))



