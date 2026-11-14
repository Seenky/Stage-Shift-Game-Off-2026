"""Export evaluated, static T-pose; no animation/pose changes are saved."""
import bpy,json
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[1];scene=bpy.data.scenes['Scene'];bpy.context.window.scene=scene
rig=bpy.data.objects['RIG_Character001']
for obj in [rig,bpy.data.objects['RIG_Hair001']]:
    if obj.animation_data:obj.animation_data.use_nla=False;obj.animation_data.action=None
    for pb in obj.pose.bones:pb.matrix_basis.identity()
for side in ['l','r']:
    rig['IK_arm_'+side]=0.;rig['IK_leg_'+side]=0.
bpy.context.view_layer.update()
for side in ['L','R']:
    for stem in ['upperarm','lowerarm','hand']:
        c=rig.pose.bones['CTRL_'+stem+'_FK.'+side];bpy.context.view_layer.update();sign=1 if c.head.x>0 else -1
        m=Vector((sign,0,0)).to_track_quat('Y','Z').to_matrix().to_4x4();m.translation=c.head;c.matrix=m;bpy.context.view_layer.update()
alignment={n:abs((rig.pose.bones[n].tail-rig.pose.bones[n].head).normalized().x) for n in ['upperarm_l','lowerarm_l','hand_l','upperarm_r','lowerarm_r','hand_r']}
assert min(alignment.values())>.9999,alignment
names=['Body','Boots','Hair','Hat','Legs','Shirt','Vest','Face_Brow.L','Face_Brow.R','Face_Eye.L','Face_Eye.R','Face_Mouth']
dg=bpy.context.evaluated_depsgraph_get();snap=[]
for name in names:
    ob=bpy.data.objects[name];mesh=bpy.data.meshes.new_from_object(ob.evaluated_get(dg),preserve_all_data_layers=True,depsgraph=dg);mesh.transform(ob.matrix_world)
    for uv in list(mesh.uv_layers)[1:]:mesh.uv_layers.remove(uv)
    snap.append((name,mesh));ob.name='_PainterSource_'+name
temp=bpy.data.scenes.new('Painter_Static_TPose');bpy.context.window.scene=temp;temp.unit_settings.system='METRIC';temp.unit_settings.scale_length=.01
for name,mesh in snap:
    o=bpy.data.objects.new(name,mesh);temp.collection.objects.link(o);o.select_set(True)
bpy.context.view_layer.objects.active=temp.objects['Body']
path=ROOT/'FBX/Character001_Painter_TPose.fbx'
bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},bake_anim=False,apply_unit_scale=True,global_scale=1,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',use_tspace=True,path_mode='STRIP')
print('PAINTER_TPOSE_EXPORTED',len(snap),json.dumps(alignment),flush=True)
