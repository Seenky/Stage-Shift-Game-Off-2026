"""Install current Hub_Theatre Blender exports and their Painter textures into Unreal."""
from pathlib import Path
import json
import shutil

import unreal

ROOT = Path(__file__).resolve().parent
GAME = '/Game/LVL/Hub/Theatre/HubTheatre'
MAP = '/Game/LVL/Hub/LVL_Hub'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDITOR = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
MATERIALS = unreal.MaterialEditingLibrary
STATIC_MESHES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MANIFEST = json.loads((ROOT/'Manifest.json').read_text('utf8'))
SURFACES = json.loads((ROOT/'Surfaces.json').read_text('utf8'))
CASSETTES = {'SelectionPreview','CrimePreview','CastlePreview','WinterPreview','SeaPreview'}
WALKABLE_GEOMETRY = {'PlankFloor','WardrobeRiser','EntrySteps','BalconyRail'}


def import_asset(source, folder, options=None, refresh=False):
    path = folder+'/'+source.stem
    if EDITOR.does_asset_exist(path) and not refresh:
        return unreal.load_asset(path)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = folder
    task.automated = True
    task.save = False
    task.replace_existing = refresh
    if options:
        task.options = options
    TOOLS.import_asset_tasks([task])
    found = task.get_objects()
    if len(found)!=1:
        raise RuntimeError(f'Could not import {source}: {found}')
    return found[0]


def fbx_options():
    options=unreal.FbxImportUI()
    options.automated_import_should_detect_type=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh=True
    options.import_materials=False
    options.import_textures=False
    settings=options.static_mesh_import_data
    settings.combine_meshes=True
    settings.generate_lightmap_u_vs=False
    settings.auto_generate_collision=False
    settings.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    settings.build_nanite=False
    return options


def expected_bounds(source):
    lo,hi=source['bounds']
    return (lo[0],-hi[1],lo[2],hi[0],-lo[1],hi[2])


def geometry(entry,refresh=False):
    name=entry['mesh']
    file=ROOT/'FBX'/f'SM_Hub_{name}.fbx'
    asset=import_asset(file,GAME+'/Meshes',fbx_options(),refresh=refresh)
    if not isinstance(asset,unreal.StaticMesh):
        raise RuntimeError(f'Expected Static Mesh from {file}')
    bounds=asset.get_bounds()
    lo=bounds.origin-bounds.box_extent
    hi=bounds.origin+bounds.box_extent
    actual=list(lo.to_tuple())+list(hi.to_tuple())
    expected=expected_bounds(entry)
    # Blender's transformed object boxes can overestimate rotated source geometry.
    inside = all(actual[i] >= expected[i]-.5 and actual[i+3] <= expected[i+3]+.5 for i in range(3))
    comparable = all(actual[i+3]-actual[i] >= .55*(expected[i+3]-expected[i]) for i in range(3))
    if not inside or not comparable:
        raise RuntimeError(f'Check FBX axes and units for {name}: {actual} vs {expected}')
    triangles=asset.get_num_triangles(0)
    if not 0 < triangles <= entry['triangles']:
        raise RuntimeError(f'Mesh {name} has {triangles} triangles, expected at most {entry["triangles"]}')
    if name in WALKABLE_GEOMETRY:
        body=asset.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assert EDITOR.save_loaded_asset(asset,only_if_is_dirty=False)
    return asset


def material(atlas,refresh=False,preserve_overrides=False):
    name='MI_Hub_'+atlas
    folder=GAME+'/Materials'
    path=folder+'/'+name
    exists=EDITOR.does_asset_exist(path)
    instance=unreal.load_asset(path) if exists else TOOLS.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    MATERIALS.set_material_instance_parent(instance,unreal.load_asset('/Game/MATERIALS/PBR/M_Surface'))
    channels={
        'BaseColor':('Base Color Map',unreal.TextureCompressionSettings.TC_DEFAULT,True),
        'Normal':('Normal Map',unreal.TextureCompressionSettings.TC_NORMALMAP,False),
        'ORM':('ORM Map',unreal.TextureCompressionSettings.TC_MASKS,False),
    }
    emissive=SURFACES[atlas]['emissive']
    if emissive:
        channels['Emissive']=('Emissive Map',unreal.TextureCompressionSettings.TC_DEFAULT,True)
    for suffix,(parameter,compression,srgb) in channels.items():
        source=ROOT/'Textures'/f'Hub_{atlas}_{suffix}.png'
        assert source.is_file(),source
        texture=import_asset(source,GAME+'/Textures',refresh=refresh)
        assert isinstance(texture,unreal.Texture2D),texture
        texture.set_editor_property('compression_settings',compression)
        texture.set_editor_property('srgb',srgb)
        texture.set_editor_property('never_stream',False)
        if refresh:
            size=SURFACES[atlas]['size']
            limit=texture.get_editor_property('max_texture_size')
            if 0 < limit < size:
                texture.set_editor_property('max_texture_size',size)
        assert EDITOR.save_loaded_asset(texture,only_if_is_dirty=False)
        MATERIALS.set_material_instance_texture_parameter_value(instance,parameter,texture)
        assert MATERIALS.get_material_instance_texture_parameter_value(instance,parameter)==texture
    for parameter,value in [('Use ORM',True),('Use Emissive',emissive),('Use Emissive Map',emissive)]:
        MATERIALS.set_material_instance_static_switch_parameter_value(instance,parameter,value)
        assert bool(MATERIALS.get_material_instance_static_switch_parameter_value(instance,parameter))==value,parameter
    if not (exists and preserve_overrides):
        MATERIALS.set_material_instance_scalar_parameter_value(instance,'Normal Strength',.65)
        MATERIALS.set_material_instance_scalar_parameter_value(instance,'Emissive Multiplier',.80)
        props=instance.get_editor_property('base_property_overrides')
        props.set_editor_property('override_two_sided',True)
        props.set_editor_property('two_sided',True)
        instance.set_editor_property('base_property_overrides',props)
    MATERIALS.update_material_instance(instance)
    assert EDITOR.save_loaded_asset(instance,only_if_is_dirty=False)
    return instance


def install_revision(previous_manifest):
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    previous={row['mesh']:row for row in previous_manifest['meshes']}
    current={row['mesh']:row for row in MANIFEST['meshes']}
    assert previous.keys()-current.keys()=={'WardrobeRiser'}
    assert current.keys()-previous.keys()=={'SelectorSign'}

    instances={atlas:material(atlas,refresh=True,preserve_overrides=True) for atlas in MANIFEST['sets']}
    print('HUB_MATERIALS',len(instances),flush=True)
    refreshed=[]
    meshes={}
    for name,entry in current.items():
        old=previous.get(name)
        # Moving a component's pivot in the scene leaves the local FBX geometry intact.
        changed=old is None or any(old[key]!=entry[key] for key in ('bounds','triangles','atlas','source_objects'))
        mesh=geometry(entry,refresh=changed)
        assert len(mesh.get_editor_property('static_materials'))==1,name
        instance=instances[entry['atlas']]
        if mesh.get_material(0)!=instance:
            mesh.set_material(0,instance)
            assert EDITOR.save_loaded_asset(mesh,only_if_is_dirty=False),name
        if changed:
            refreshed.append(name)
        meshes[name]=mesh

    station=update_map(meshes)
    assert station.get_editor_property('selection_camera')
    removed=remove_obsolete_collision()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return {'sets':len(instances),'reimported_meshes':refreshed,'meshes':len(meshes),
            'removed_collision':removed,'map':MAP}


def imported_assets():
    inst={}
    meshes={}
    for entry in MANIFEST['meshes']:
        name,atlas=entry['mesh'],entry['atlas']
        mesh=unreal.load_asset(GAME+'/Meshes/SM_Hub_'+name)
        assert isinstance(mesh,unreal.StaticMesh),name
        if atlas not in inst:
            inst[atlas]=material(atlas)
        assert len(mesh.get_editor_property('static_materials'))==1,name
        mesh.set_material(0,inst[atlas])
        assert EDITOR.save_loaded_asset(mesh,only_if_is_dirty=False)
        meshes[name]=mesh
    return meshes


def update_map(meshes):
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    project=Path(unreal.Paths.project_dir()).resolve()
    backup=project/'Saved'/'Backups'/'HubTheatre'/ 'LVL_Hub.umap'
    if not backup.exists():
        backup.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(project/'Content'/'LVL'/'Hub'/'LVL_Hub.umap',backup)
    actors=ACTORS.get_all_level_actors()
    station=next(a for a in actors if a.get_class().get_name()=='StageSelectionStation')
    previous={}
    for actor in actors:
        if not isinstance(actor,unreal.StaticMeshActor):
            continue
        old=actor.static_mesh_component.static_mesh
        if old and old.get_name().startswith('SM_Hub_'):
            previous[old.get_name().removeprefix('SM_Hub_')]=actor
    for entry in MANIFEST['meshes']:
        name,atlas=entry['mesh'],entry['atlas']
        if name in CASSETTES or name=='SelectionStand':
            continue
        pivot=unreal.Vector(*entry['pivot'])
        actor=previous.get(name)
        if actor is None:
            actor=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,pivot,unreal.Rotator())
            actor.set_actor_label(name)
            actor.set_folder_path(unreal.Name('Hub/Art/'+atlas))
        actor.modify()
        component=actor.static_mesh_component
        if component.static_mesh!=meshes[name]:
            assert component.set_static_mesh(meshes[name]),name
        actor.set_actor_location(pivot,False,False)
        actor.set_actor_rotation(unreal.Rotator(),False)
        component.set_collision_profile_name('BlockAll' if name in WALKABLE_GEOMETRY else 'NoCollision')
    for name in ('SceneryRails','WardrobeRiser'):
        if name in previous and name not in meshes:
            assert ACTORS.destroy_actor(previous[name])
    station.modify()
    components={c.get_name():c for c in station.get_components_by_class(unreal.StaticMeshComponent)}
    components['Display'].set_static_mesh(meshes['SelectionStand'])
    options=list(station.get_editor_property('options'))
    assert len(options)==3
    keys=('SelectionPreview','WinterPreview','SeaPreview')
    for option,key in zip(options,keys):
        option.set_editor_property('preview_mesh',meshes[key])
    station.set_editor_property('options',options)
    components['PreviewA'].set_static_mesh(meshes['SelectionPreview'])
    print('HUB_MAP_STAGE',station.get_actor_label(),[option.get_editor_property('preview_mesh').get_name() for option in options])
    return station


def remove_obsolete_collision():
    # These boxes belonged to the earlier rectangular floor and covered the new stair opening.
    removed = []
    for actor in ACTORS.get_all_level_actors():
        label = actor.get_actor_label()
        if not isinstance(actor,unreal.StaticMeshActor):
            continue
        mesh = actor.static_mesh_component.static_mesh
        if mesh and mesh.get_name()=='Cube' and (label.startswith('Floor ') or
                label in {'Stairs edge','Balcony rail','Dressing riser surface'}):
            assert ACTORS.destroy_actor(actor)
            removed.append(label)
    return removed


def refine_level():
    removed = remove_obsolete_collision()
    for actor in ACTORS.get_all_level_actors():
        label = actor.get_actor_label()
        if isinstance(actor,unreal.SpotLight) and 'proscenium' in label:
            index = int(label[-1])-1
            light = actor.get_component_by_class(unreal.SpotLightComponent)
            actor.modify()
            light.set_intensity((1650.,2700.,2700.,1650.)[index])
            light.set_outer_cone_angle(21.)
            light.set_inner_cone_angle(11.)
            location = actor.get_actor_location()
            target = unreal.Vector(365.,(-86.,-28.,28.,86.)[index],112.)
            actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(location,target),False)
        elif label=='Miniature room practical':
            actor.modify()
            actor.set_actor_location(unreal.Vector(359., -8., 207.),False,False)
            component = actor.get_component_by_class(unreal.PointLightComponent)
            component.set_intensity(265.)
            component.set_attenuation_radius(175.)
            component.set_source_radius(30.)
        elif label=='Curtain fill':
            actor.modify()
            position=unreal.Vector(-761.,12.,424.)
            actor.set_actor_location(position,False,False)
            actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position,unreal.Vector(-493.,0.,272.)),False)
            component=actor.get_component_by_class(unreal.RectLightComponent)
            component.set_intensity(3850.)
            component.set_attenuation_radius(690.)
        elif label in {'Dressing fill','Sound fill'}:
            component=actor.get_component_by_class(unreal.RectLightComponent)
            actor.modify()
            component.set_intensity(5900. if label=='Dressing fill' else 5450.)
    return removed
