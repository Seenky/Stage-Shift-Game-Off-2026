"""Publish the independently editable mesh modules from the existing hub atlases."""
from collections import defaultdict
from datetime import datetime
from pathlib import Path
import json
import runpy
import shutil

import unreal as u

root = Path(__file__).resolve().parent
project = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve()
out = project / 'Saved/StageShiftReview/HubModular'
library = '/Game/LVL/Hub/Theatre/HubTheatre/Meshes'
asset_folder = library + '/Modular'
map_path = '/Game/LVL/Hub/LVL_Hub'
rows = json.loads((root/'Modular/Manifest.json').read_text(encoding='utf8'))
groups = defaultdict(list)
for row in rows:
    groups[row['parent_mesh']].append(row)
original_import = runpy.run_path(str(root/'import_unreal.py'))
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assets = u.get_editor_subsystem(u.EditorAssetSubsystem)
mesh_editor = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
worlds = u.get_editor_subsystem(u.UnrealEditorSubsystem)

legacy_boxes = {
    'Wardrobe blocker', 'Rear wall', 'Wing wall -1', 'Wing wall 1', 'Central diorama',
    'Fitting booth', 'Vanity', 'Workbench', 'Sound console', 'Music props',
    'Rail bay left blocker', 'Rail bay right blocker',
}

def mesh_path(row):
    return asset_folder + '/' + row['asset']

def collision_for_asset(mesh, mode):
    body = mesh.get_editor_property('body_setup')
    if mode in ('surface','selector'):
        body.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    elif mode == 'box':
        body.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
        if mesh_editor and not mesh_editor.get_simple_collision_count(mesh):
            assert mesh_editor.add_simple_collisions(mesh,u.ScriptCollisionShapeType.BOX) >= 0,mesh

def import_meshes():
    out.mkdir(parents=True,exist_ok=True)
    result=[]
    for parent,children in groups.items():
        source = u.load_asset(library+'/SM_Hub_'+parent)
        assert source and len(source.static_materials)==1,parent
        material=source.get_material(0)
        assert material and isinstance(material,u.MaterialInstanceConstant),source
        triangles=0
        for row in children:
            file=root/'Modular'/'FBX'/(row['asset']+'.fbx')
            obj=original_import['import_asset'](file,asset_folder,original_import['fbx_options']())
            assert obj.get_path_name().split('.')[0]==mesh_path(row),obj
            assert len(obj.static_materials)==1,row['asset']
            count=obj.get_num_triangles(0)
            assert 0<count<=row['triangles'],(row['asset'],count,row['triangles'])
            triangles+=count
            bounds=obj.get_bounds()
            lo=list((bounds.origin-bounds.box_extent).to_tuple())
            hi=list((bounds.origin+bounds.box_extent).to_tuple())
            old_lo,old_hi=row['bounds']
            lower=(old_lo[0],-old_hi[1],old_lo[2])
            upper=(old_hi[0],-old_lo[1],old_hi[2])
            assert all(lo[i]>=lower[i]-.1 and hi[i]<=upper[i]+.1 for i in range(3)),(row['asset'],lo,hi,lower,upper)
            obj.set_material(0,material)
            collision_for_asset(obj,row['collision'])
            assert obj.get_material(0)==material
            assert assets.save_loaded_asset(obj,only_if_is_dirty=False),obj
            result.append(obj.get_path_name())
        assert abs(triangles-source.get_num_triangles(0))<=max(2,source.get_num_triangles(0)*.003),(parent,triangles,source.get_num_triangles(0))
        print('Hub modular meshes',parent,len(children),triangles,flush=True)
    (out/'mesh-import.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    print('HUB_MODULAR_ASSETS',len(result))

def assign_collision(component, row):
    mode=row['collision']
    component.set_collision_profile_name('NoCollision' if mode=='none' else 'BlockAll')
    if mode == 'selector':
        component.set_collision_response_to_channel(u.CollisionChannel.ECC_VISIBILITY,u.CollisionResponseType.ECR_IGNORE)

def replace_level():
    out.mkdir(parents=True,exist_ok=True)
    assert not worlds.get_game_world()
    assert worlds.get_editor_world().get_name()=='LVL_Hub'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    registry=u.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([asset_folder],True)
    loaded = {r['asset']:u.load_asset(mesh_path(r)) for r in rows}
    assert all(loaded.values()),[k for k,v in loaded.items() if not v]
    for row in rows:
        if row['collision']=='box':
            mesh=loaded[row['asset']]
            collision_for_asset(mesh,'box')
            assert mesh_editor.get_simple_collision_count(mesh)==1,mesh
            assert assets.save_loaded_asset(mesh,only_if_is_dirty=False),mesh
    existing=actors.get_all_level_actors()
    station,= [a for a in existing if isinstance(a,u.StageSelectionStation)]
    display=station.get_component_by_class(u.StaticMeshComponent)
    assert display.get_name()=='Display' and display.static_mesh.get_name()=='SM_Hub_SelectionStand'
    old={}
    blockers=[]
    for actor in existing:
        if not isinstance(actor,u.StaticMeshActor):
            continue
        component=actor.static_mesh_component
        mesh=component.static_mesh
        if not mesh:
            continue
        if mesh.get_name()=='Cube' and actor.get_actor_label() in legacy_boxes:
            blockers.append(actor)
        leaf=mesh.get_name().removeprefix('SM_Hub_')
        if leaf in groups:
            assert leaf not in old,leaf
            old[leaf]=actor
    assert {*old,'SelectionStand'}==groups.keys(),(set(old),groups.keys())
    assert {a.get_actor_label() for a in blockers}==legacy_boxes
    backup=project/'Saved/Backups/HubTheatre'/('BeforeModular-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    shutil.copy2(project/'Content/LVL/Hub/LVL_Hub.umap',backup/'LVL_Hub.umap')

    output={}
    with u.ScopedEditorTransaction('Separate Hub meshes and fit collision'):
        for parent, entries in groups.items():
            previous=station if parent=='SelectionStand' else old[parent]
            original_component=display if previous==station else previous.static_mesh_component
            transform=previous.get_actor_transform()
            scale=previous.get_actor_scale3d()
            rotation=previous.get_actor_rotation()
            folder=str(previous.get_folder_path())+'/'+parent.replace('WorkshopProps_','')
            overrides=original_component.get_editor_property('override_materials')
            for row in entries:
                mesh=loaded[row['asset']]
                delta=u.Vector(*(a-b for a,b in zip(row['pivot'],row['old_pivot'])))
                location=u.MathLibrary.transform_location(transform,delta)
                if row['station_base']:
                    assert delta.length()<.001,delta
                    station.modify()
                    display.modify()
                    assert display.set_static_mesh(mesh)
                    assign_collision(display,row)
                    output[row['asset']]=station.get_actor_label()
                    continue
                actor=actors.spawn_actor_from_class(u.StaticMeshActor,location,rotation)
                assert actor
                actor.set_actor_label(row['label'])
                actor.set_folder_path(u.Name(folder))
                actor.set_actor_scale3d(scale)
                current=actor.static_mesh_component
                assert current.set_static_mesh(mesh)
                for i,material in enumerate(overrides):
                    if material:
                        current.set_material(i,material)
                current.set_mobility(original_component.mobility)
                for prop in ('cast_shadow','receives_decals'):
                    current.set_editor_property(prop,original_component.get_editor_property(prop))
                assign_collision(current,row)
                assert not actor.get_attach_parent_actor()
                output[row['asset']]=actor.get_actor_label()
        for original in (*old.values(),*blockers):
            assert actors.destroy_actor(original)
    assert len(output)==len(rows)
    assert level.save_current_level()
    (out/'installed.json').write_text(json.dumps({'map':map_path,'modules':output,'removed_boxes':sorted(legacy_boxes),'backup':str(backup)},ensure_ascii=False,indent=2),encoding='utf8')
    print('HUB_MODULAR_MAP',len(output),len(blockers),str(backup))

if __name__=='__main__':
    import_meshes()
