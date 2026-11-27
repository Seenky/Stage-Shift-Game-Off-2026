"""Create the Stage Shift surface benchmark with the game material master."""
import json
import math
from pathlib import Path
import runpy
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
SHOW=ROOT/'Models/Materials/Showcase'
SUBSYSTEM=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
WORLD=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ASSET=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.MaterialEditingLibrary
DEST='/Game/MATERIALS/Showcase'
MAP=DEST+'/LVL_SurfaceShowcase'
base=runpy.run_path(str(ROOT/'Models/Hub/Surfacing/Unreal/import_hub.py'))
CHANNELS = {
    'BaseColor': ('Base Color Map',unreal.TextureCompressionSettings.TC_DEFAULT,True),
    'Normal': ('Normal Map',unreal.TextureCompressionSettings.TC_NORMALMAP,False),
    'ORM': ('ORM Map',unreal.TextureCompressionSettings.TC_MASKS,False),
}
parent=unreal.load_asset('/Game/MATERIALS/PBR/M_Surface')
def import_preview():
    for name in json.loads((SHOW/'Samples.json').read_text('utf8')):
        mat_name='MI_Sample_'+name
        path=DEST+'/Materials/'+mat_name
        if ASSET.does_asset_exist(path):
            inst=unreal.load_asset(path)
        else:
            inst=TOOLS.create_asset(mat_name,DEST+'/Materials',unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(inst,parent)
        for kind,(parameter,compression,srgb) in CHANNELS.items():
            file=SHOW/'Textures'/f'Sample_{name}_{kind}.png'
            task=base['task'](file,DEST+'/Textures',True)
            TOOLS.import_asset_tasks([task])
            texture=next(t for t in task.get_objects() if isinstance(t,unreal.Texture2D))
            texture.set_editor_property('compression_settings',compression)
            texture.set_editor_property('srgb',srgb)
            LIB.set_material_instance_texture_parameter_value(inst,parameter,texture)
            ASSET.save_loaded_asset(texture,only_if_is_dirty=False)
        LIB.set_material_instance_static_switch_parameter_value(inst,'Use ORM',True)
        LIB.set_material_instance_scalar_parameter_value(inst,'Normal Strength',.78)
        LIB.update_material_instance(inst)
        ASSET.save_loaded_asset(inst,only_if_is_dirty=False)
    task=base['task'](SHOW/'StageShift_SurfaceShowcase.fbx',DEST+'/Meshes',True,base['fbx_options']())
    TOOLS.import_asset_tasks([task])
    fixture=next(m for m in task.get_objects() if isinstance(m,unreal.StaticMesh))
    for i,slot in enumerate(fixture.get_editor_property('static_materials')):
        name=str(slot.material_slot_name)
        material=unreal.load_asset(DEST+'/Materials/MI_'+name)
        assert material,name
        fixture.set_material(i,material)
    ASSET.save_loaded_asset(fixture,only_if_is_dirty=False)
    return fixture

def look_at_origin(position):
    x,y,z=-position.x,-position.y,-position.z
    return unreal.Rotator(pitch=math.degrees(math.atan2(z,math.hypot(x,y))),
                          yaw=math.degrees(math.atan2(y,x)),roll=0.0)

def build(fixture):
    world=WORLD.get_editor_world()
    current=world.get_path_name().split('.')[0]
    if current!=MAP:
        assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        if ASSET.does_asset_exist(MAP):
            assert SUBSYSTEM.load_level(MAP)
        else:
            assert SUBSYSTEM.new_level(MAP)
    actors={obj.get_actor_label():obj for obj in ACTORS.get_all_level_actors()}
    geometry=actors.get('Surface comparison geometry')
    if geometry is None:
        geometry=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
        geometry.set_actor_label('Surface comparison geometry')
    geometry.static_mesh_component.set_static_mesh(fixture)
    ground=actors.get('Neutral display board')
    if ground is None:
        ground=ACTORS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-13))
        ground.set_actor_label('Neutral display board')
        ground.set_actor_scale3d(unreal.Vector(8.1,7.0,.2))
        ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube.Cube'))
    for label,loc,power,temp in (
        ('Neutral broad key',(-500,-380,710),1950.,6500.),
        ('Neutral fill',(500,330,510),1300.,7500.)):
        pos=unreal.Vector(*loc)
        light=actors.get(label)
        if light is None:
            light=ACTORS.spawn_actor_from_class(unreal.RectLight,pos,look_at_origin(pos))
            light.set_actor_label(label)
        component=light.get_component_by_class(unreal.RectLightComponent)
        component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
        component.set_editor_property('attenuation_radius',3000.)
        component.set_editor_property('intensity',power)
        component.set_editor_property('source_width',540.)
        component.set_editor_property('source_height',470.)
        component.set_editor_property('use_temperature',True)
        component.set_editor_property('temperature',temp)
    cam_pos=unreal.Vector(0,540,1250)
    cam_rot=look_at_origin(cam_pos)
    camera=actors.get('Neutral material camera')
    if camera is None:
        camera=ACTORS.spawn_actor_from_class(unreal.CameraActor,cam_pos,cam_rot)
        camera.set_actor_label('Neutral material camera')
    cam=camera.get_component_by_class(unreal.CameraComponent)
    camera.set_actor_location(cam_pos,False,False)
    camera.set_actor_rotation(cam_rot,False)
    cam.set_editor_property('projection_mode',unreal.CameraProjectionMode.PERSPECTIVE)
    cam.set_editor_property('field_of_view',44.)
    exposure=cam.get_editor_property('post_process_settings')
    exposure.set_editor_property('override_auto_exposure_bias',True)
    exposure.set_editor_property('auto_exposure_bias',-1.3)
    cam.set_editor_property('post_process_settings',exposure)
    cam.set_editor_property('aspect_ratio',1.25)
    cam.set_editor_property('constrain_aspect_ratio',True)
    WORLD.set_level_viewport_camera_info(cam_pos,cam_rot)
    unreal.SystemLibrary.execute_console_command(WORLD.get_editor_world(),"viewmode lit")
    assert SUBSYSTEM.save_current_level()
    return camera

if __name__=='__main__':
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
    fixture=import_preview()
    camera=build(fixture)
    expected=SHOW/'MaterialShowcase_Unreal.png'
    unreal.AutomationLibrary.take_high_res_screenshot(1600,1280,expected.as_posix(),camera=camera,delay=1.5)
    print('Saved',MAP,fixture.get_path_name(),len(fixture.get_editor_property('static_materials')))


