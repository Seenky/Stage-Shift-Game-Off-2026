import bpy,json,traceback
from pathlib import Path
BASE=Path(__file__).resolve().parents[1]
scene=bpy.data.scenes['Scene'];bpy.context.window.scene=scene
rig=bpy.data.objects['RIG_Character001']
path=BASE/'Tools/Export_UE_FBX.py';scope={'__name__':'stage_shift_export_current','__file__':str(path)};exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),scope)
meshes=scope['export_meshes'](rig,BASE/'FBX')
print('CURRENT_MESHES',len(meshes),flush=True)
animation=rig.animation_data
tracks=[(t.name,next((s.action for s in t.strips if s.action),None)) for t in animation.nla_tracks]
animation.use_nla=False;animation.action=None
animation_files=[]
for label,action in tracks:
    if action is None:continue
    animation.action=None
    for pb in rig.pose.bones:pb.matrix_basis.identity()
    animation.action=action
    if hasattr(animation,'action_slot') and len(action.slots):animation.action_slot=action.slots[0]
    name='AN_Character001_'+bpy.path.clean_name(label)
    p=scope['export_animation'](rig,BASE/'FBX/Animations',name);animation_files.append(str(p.relative_to(BASE)))
    print('ANIMATION_EXPORTED',label,len(animation_files),flush=True)
(BASE/'FBX/Export_manifest.json').write_text(json.dumps({'meshes':[str(p.relative_to(BASE)) for p in meshes],'animations':animation_files,'ue_scale':rig['UE_Scale'],'native_actions_preserved_in':'Character001.blend','painter_mesh':'FBX/Character001_Painter_TPose.fbx'},indent=2))
assert len(meshes)==5
assert len(animation_files)==len(tracks)
print('CURRENT_EXPORT_COMPLETE',len(meshes),len(animation_files),flush=True)
