
"""Run this text once per Blender session to show the Stage Shift viewport panel."""
import math
from pathlib import Path
from contextlib import contextmanager
import bpy
from mathutils import Matrix

RIG_NAME = "RIG_Character001"
CHECK_ACTION = "Character001 · Rig_Check"
MODULES = {'Body': ('Body', 'Face_Eye.L', 'Face_Eye.R', 'Face_Brow.L', 'Face_Brow.R', 'Face_Mouth'), 'Hat': ('Hat',), 'Outfit': ('Shirt', 'Vest', 'Legs', 'Boots')}
NAMES = tuple(dict.fromkeys(n for module in MODULES.values() for n in module))

def deform_bones(rig):
    return [bone for bone in rig.data.bones if bone.use_deform]


def capture_rest_meshes(rig, scale, names=NAMES):
    matrices = {}
    previous = rig.data.pose_position
    meshes = []
    rig.data.pose_position = "REST"
    try:
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        matrices = {name: bpy.context.scene.objects[name].matrix_world.copy() for name in names}
        for name in names:
            obj = bpy.context.scene.objects[name]
            if obj.data.shape_keys:
                if any(m.type != "ARMATURE" and m.show_viewport for m in obj.modifiers):
                    raise RuntimeError(f"{name}: a morph mesh has an unevaluated modifier")
                mesh = obj.data.copy()
                if mesh.shape_keys == obj.data.shape_keys:
                    raise RuntimeError(f"{name}: shape data was not copied")
                mesh.shape_keys.animation_data_clear()
                for key in list(mesh.shape_keys.key_blocks)[1:]:
                    key.value = 0.0
            else:
                mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(dg),
                    preserve_all_data_layers=True, depsgraph=dg)
            meshes.append((name, mesh, [g.name for g in obj.vertex_groups]))
            mesh.transform(Matrix.Scale(scale, 4) @ matrices[name], shape_keys=True)
            if not mesh.uv_layers or mesh.uv_layers[0].name != "UVMap":
                raise RuntimeError(f"{name}: export expects UVMap in slot 0")
            for extra_uv in list(mesh.uv_layers)[1:]:
                mesh.uv_layers.remove(extra_uv)
    except Exception:
        for _, mesh, _ in meshes:
            bpy.data.meshes.remove(mesh)
        raise
    finally:
        rig.data.pose_position = previous
        bpy.context.view_layer.update()
    return meshes

def bind_and_join(scene, skeleton, objects, module, meshes):
    allowed = {b.name for b in skeleton.data.bones}
    pieces = []
    for name, mesh, groups in meshes:
        ob = bpy.data.objects.new(name+"_UEPart", mesh)
        scene.collection.objects.link(ob)
        objects.append(ob)
        pieces.append(ob)
        for group_name in groups:
            ob.vertex_groups.get(group_name) or ob.vertex_groups.new(name=group_name)
        for group in tuple(ob.vertex_groups):
            if group.name not in allowed:
                ob.vertex_groups.remove(group)
        for v in mesh.vertices:
            if not v.groups or abs(sum(g.weight for g in v.groups)-1) > .0001:
                raise RuntimeError(f"{name}: evaluated vertex {v.index} has invalid skin weights")
    bpy.ops.object.select_all(action="DESELECT")
    active = pieces[0]
    for ob in pieces:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = active
    if len(pieces) > 1:
        retained = [ob for ob in objects if ob not in pieces[1:]]
        if bpy.ops.object.join() != {"FINISHED"}:
            raise RuntimeError(f"{module}: could not join module meshes")
        objects[:] = retained
    active.name = f"SK_Character001_{module}"
    active.data.name = active.name
    modifier = active.modifiers.new("Skin", "ARMATURE")
    modifier.object = skeleton
    modifier.use_deform_preserve_volume = False
    targets = {k.name for _, mesh, _ in meshes if mesh.shape_keys for k in list(mesh.shape_keys.key_blocks)[1:]}
    if targets and (not active.data.shape_keys or set(active.data.shape_keys.key_blocks.keys()) - {"Basis"} != targets):
        raise RuntimeError(f"{module}: joined mesh is missing facial morph targets")
    # Triangulate only export-copy n-gons: the source topology and UV remain untouched.
    if any(len(poly.vertices) > 4 for poly in active.data.polygons):
        import bmesh
        mesh = active.data
        before_count = len(mesh.vertices)
        shape_coordinates = {k.name: [tuple(v.co) for v in k.data] for k in mesh.shape_keys.key_blocks} if mesh.shape_keys else {}
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
        bm.to_mesh(mesh)
        bm.free()
        mesh.update()
        if len(mesh.vertices) != before_count:
            raise RuntimeError("Export triangulation changed vertex count")
        if shape_coordinates and any(shape_coordinates[k.name] != [tuple(v.co) for v in k.data] for k in mesh.shape_keys.key_blocks):
            raise RuntimeError("Export triangulation changed morph coordinates")
    return active

def release_meshes(meshes):
    for _,mesh,_ in meshes:
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)



@contextmanager
def export_scene(rig):
    context = bpy.context
    window = context.window
    source = window.scene
    before_mode = rig.mode
    if before_mode != "OBJECT":
        if before_mode != "POSE" or context.view_layer.objects.active != rig:
            raise RuntimeError("Switch to Object Mode or the Character001 rig in Pose Mode.")
        bpy.ops.object.mode_set(mode="OBJECT")
    temp = bpy.data.scenes.new("Stage Shift FBX · Temporary")
    temp.unit_settings.system = "METRIC"
    temp.unit_settings.scale_length = .01
    temp.render.fps = source.render.fps
    temp.render.fps_base = source.render.fps_base
    data = bpy.data.armatures.new("Character001_UE")
    objects = []
    actions = []
    skeleton = bpy.data.objects.new("Armature", data)
    objects.append(skeleton)
    try:
        if skeleton.name != "Armature":
            raise RuntimeError("An object is already named Armature. Rename it before exporting.")
        window.scene = temp
        temp.collection.objects.link(skeleton)
        skeleton.select_set(True)
        context.view_layer.objects.active = skeleton
        bpy.ops.object.mode_set(mode="EDIT")
        scale = float(rig["UE_Scale"])
        try:
            for source_bone in deform_bones(rig):
                b = data.edit_bones.new(source_bone.name)
                m = source_bone.matrix_local.copy()
                m.translation *= scale
                b.length = source_bone.length * scale
                b.matrix = m
                if source_bone.parent:
                    b.parent = data.edit_bones[source_bone.parent.name]
        finally:
            bpy.ops.object.mode_set(mode="OBJECT")
        yield temp, skeleton, objects, actions
    finally:
        window.scene = source
        for obj in reversed(objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        bpy.data.armatures.remove(data)
        for a in actions:
            bpy.data.actions.remove(a, do_unlink=True)
        bpy.data.scenes.remove(temp)
        if before_mode == "POSE":
            context.view_layer.objects.active = rig
            bpy.ops.object.mode_set(mode="POSE")

def write_fbx(path, animated=False, with_shapes=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    result = bpy.ops.export_scene.fbx(
        filepath=str(path), use_selection=True, object_types={"ARMATURE"} if animated and not with_shapes else {"ARMATURE", "MESH"},
        global_scale=1, apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE",
        axis_forward="-Y", axis_up="Z", use_space_transform=True, bake_space_transform=False,
        armature_nodetype="NULL", add_leaf_bones=False, use_armature_deform_only=True,
        primary_bone_axis="Y", secondary_bone_axis="X",
        use_mesh_modifiers=False, mesh_smooth_type="FACE", use_tspace=not animated, path_mode="STRIP",
        use_custom_props=False, bake_anim=animated, bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True, bake_anim_step=1, bake_anim_simplify_factor=0)
    if result != {"FINISHED"}:
        raise RuntimeError(f"FBX export failed: {path.name}")
    return path

def output_directory(rig):
    return Path(bpy.path.abspath(rig["FBX_Path"]))



def export_hair(rig, output):
    hair = bpy.data.objects["Hair"]
    hair_rig = hair.find_armature()
    if hair_rig is None or hair_rig == rig:
        raise RuntimeError("Hair requires an independent armature")
    mesh = hair.data.copy()
    scale = float(rig["UE_Scale"])
    mesh.transform(Matrix.Scale(scale, 4) @ hair_rig.matrix_world.inverted() @ hair.matrix_world)
    meshes = [("Hair", mesh, [g.name for g in hair.vertex_groups])]
    try:
        with export_scene(hair_rig) as (scene, skeleton, objects, _):
            module = bind_and_join(scene, skeleton, objects, "Hair", meshes)
            skeleton.select_set(True)
            module.select_set(True)
            return write_fbx(Path(output) / "Modules" / "SK_Character001_Hair.fbx")
    finally:
        release_meshes(meshes)

def export_meshes(rig, output=None):
    output = Path(output) if output is not None else output_directory(rig)
    exported = []
    for module, names in (*MODULES.items(), ("FullBody", NAMES + ("Hair",))):
        meshes = capture_rest_meshes(rig, float(rig["UE_Scale"]), names)
        if module == "FullBody":
            import bmesh
            patched = []
            for part_name, mesh, groups in meshes:
                if part_name == "Hair":
                    bm = bmesh.new(); bm.from_mesh(mesh)
                    layer = bm.verts.layers.deform.verify()
                    for v in bm.verts:
                        v[layer].clear(); v[layer][0] = 1.0
                    bm.to_mesh(mesh); bm.free(); groups = ["head"]
                    temp_group_owner = bpy.data.objects.new("_FullBodyHairGroups", mesh)
                    temp_group_owner.vertex_groups[0].name = "head"
                    for group in list(temp_group_owner.vertex_groups)[1:]: temp_group_owner.vertex_groups.remove(group)
                    bpy.data.objects.remove(temp_group_owner)
                patched.append((part_name, mesh, groups))
            meshes = patched
        try:
            with export_scene(rig) as (scene, skeleton, objects, _):
                module_obj = bind_and_join(scene, skeleton, objects, module, meshes)
                skeleton.select_set(True)
                module_obj.select_set(True)
                folder = output if module == "FullBody" else output / "Modules"
                exported.append(write_fbx(folder / f"SK_Character001_{module}.fbx"))
        finally:
            release_meshes(meshes)
    exported.append(export_hair(rig, output))
    return exported

def capture_shape_weights(frames):
    scene = bpy.context.scene
    targets = {}
    for name in MODULES["Body"]:
        ob = scene.objects[name]
        if ob.data.shape_keys:
            for key in list(ob.data.shape_keys.key_blocks)[1:]:
                targets.setdefault(key.name, key)
    if not targets:
        return {}
    start, sub = scene.frame_current, scene.frame_subframe
    values = {name: [] for name in targets}
    try:
        for frame in frames:
            scene.frame_set(frame)
            for name, key in targets.items():
                values[name].append(float(key.value))
    finally:
        scene.frame_set(start, subframe=sub)
    return {name: arr for name, arr in values.items() if any(v > 1e-6 for v in arr)}


def capture_animation(rig, frames):
    scene = bpy.context.scene
    start, sub = scene.frame_current, scene.frame_subframe
    previous_pose_position = rig.data.pose_position
    names = [b.name for b in deform_bones(rig)]
    scale = float(rig["UE_Scale"])
    result = []
    try:
        rig.data.pose_position = "POSE"
        bpy.context.view_layer.update()
        for f in frames:
            scene.frame_set(f)
            ev = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
            sample = {n: ev.pose.bones[n].matrix.copy() for n in names}
            for m in sample.values():
                m.translation *= scale
            result.append(sample)
    finally:
        rig.data.pose_position = previous_pose_position
        scene.frame_set(start, subframe=sub)
        bpy.context.view_layer.update()
    return result


def export_animation(rig, output=None, filename=None):
    animation = rig.animation_data
    action = animation.action if animation else None
    if action is None:
        raise RuntimeError("Choose an active Action in the Dope Sheet before exporting.")
    start, end = map(float, action.frame_range)
    if end < start:
        raise RuntimeError("The active Action has no frame range.")
    frames = range(math.floor(start), math.ceil(end)+1)
    samples = capture_animation(rig, frames)
    weights = capture_shape_weights(frames)
    meshes = capture_rest_meshes(rig, float(rig["UE_Scale"]), MODULES["Body"]) if weights else []
    output = Path(output) if output is not None else output_directory(rig)/"Animations"
    stem = filename or ("AN_Character001_"+bpy.path.clean_name(action.name))
    try:
        with export_scene(rig) as (scene, skeleton, objects, actions):
            scene.frame_start, scene.frame_end = 1, len(samples)
            if weights:
                body = bind_and_join(scene, skeleton, objects, "Body", meshes)
                for name, values in weights.items():
                    k = body.data.shape_keys.key_blocks[name]
                    for f, value in enumerate(values, 1):
                        k.value = value
                        k.keyframe_insert(data_path="value",frame=f)
                body.select_set(True)
                skeleton.select_set(True)
            temp = bpy.data.actions.new(stem)
            actions.append(temp)
            skeleton.animation_data_create().action = temp
            previous = {}
            for f, sample in enumerate(samples, 1):
                for b in skeleton.pose.bones:
                    pose = sample[b.name]
                    if b.parent:
                        local = b.bone.convert_local_to_pose(
                            pose, b.bone.matrix_local, parent_matrix=sample[b.parent.name],
                            parent_matrix_local=b.parent.bone.matrix_local, invert=True)
                    else:
                        local = b.bone.convert_local_to_pose(pose, b.bone.matrix_local, invert=True)
                    loc, rot, scale = local.decompose()
                    if b.name in previous:
                        rot.make_compatible(previous[b.name])
                    previous[b.name] = rot.copy()
                    b.rotation_mode = "QUATERNION"
                    b.location, b.rotation_quaternion, b.scale = loc, rot, scale
                    for channel in ("location", "rotation_quaternion", "scale"):
                        b.keyframe_insert(channel, frame=f, group=b.name)
            scene.frame_set(1)
            return write_fbx(output/(stem+".fbx"), animated=True, with_shapes=bool(weights))
    finally:
        release_meshes(meshes)



def match_switch(rig, limb, side, to_ik, key=False):
    types = ("upperarm", "lowerarm", "hand") if limb == "arm" else ("thigh", "calf", "foot", "ball")
    names = [f"{name}_{side}" for name in types]
    fk = [f"CTRL_{n}_FK.{side.upper()}" for n in types]
    ctrl_target = f"CTRL_{types[2]}_IK.{side.upper()}"
    ctrl_pole = f"CTRL_{'elbow' if limb=='arm' else 'knee'}_Pole.{side.upper()}"
    mode_property = f"IK_{limb}_{side}"
    frame = bpy.context.scene.frame_current
    bpy.context.view_layer.update()
    pb = rig.pose.bones
    matrices = [pb[n].matrix.copy() for n in names]
    update = bpy.context.view_layer.update
    if to_ik:
        pb[ctrl_target].matrix = matrices[2]
        update()
        a, b, c = [m.translation for m in matrices[:3]]
        line = c-a
        if line.length_squared < 1e-6:
            raise RuntimeError("Cannot place a pole on a zero-length limb.")
        off = b-a-line*((b-a).dot(line)/line.length_squared)
        if off.length < .04:
            d = pb[ctrl_pole].head-a
            off = d-line*(d.dot(line)/line.length_squared)
        p = pb[ctrl_pole].matrix.copy()
        p.translation = b+off.normalized()*(58 if limb=="arm" else 77)
        pb[ctrl_pole].matrix = p
        adjusted = [ctrl_target, ctrl_pole]
        update()
        if limb == "leg":
            toe = f"CTRL_{types[3]}_IK.{side.upper()}"
            pb[toe].matrix = matrices[3]
            adjusted.append(toe)
            update()
    else:
        for n, matrix in zip(fk, matrices):
            pb[n].matrix = matrix
            update()
        adjusted = fk
    rig[mode_property] = float(to_ik)
    rig.update_tag()
    update()
    if key:
        for n in adjusted:
            for channel in ("location", "rotation_quaternion" if pb[n].rotation_mode == "QUATERNION" else "rotation_euler"):
                pb[n].keyframe_insert(channel, frame=frame, group=n)
        path=f'["{mode_property}"]'
        rig.keyframe_insert(path, frame=frame, group="IK / FK")
        action=rig.animation_data.action
        if action:
            slot=rig.animation_data.action_slot
            for layer in action.layers:
                for strip in layer.strips:
                    bag=strip.channelbag(slot)
                    if bag:
                        curve=bag.fcurves.find(path)
                        if curve:
                            for point in curve.keyframe_points:
                                point.interpolation="CONSTANT"
    for collection in ["03 · Arms FK", "04 · Legs IK", "05 · Arms IK", "06 · Legs FK"]:
        rig.data.collections[collection].is_visible = True
    rig.data.update_tag()
    update()


def active_rig():
    rig = bpy.context.scene.objects.get(RIG_NAME)
    if not rig:
        raise RuntimeError("RIG_Character001 is missing from this scene.")
    return rig

class STAGESHIFT_OT_mesh_fbx(bpy.types.Operator):
    bl_idname = "stageshift.export_mesh_fbx"
    bl_label = "Export skeletal meshes"
    def execute(self, context):
        try:
            files = export_meshes(active_rig())
        except Exception as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        self.report({"INFO"}, f"Exported {len(files)} FBX files to {files[0].parent}")
        return {"FINISHED"}

class STAGESHIFT_OT_action_fbx(bpy.types.Operator):
    bl_idname = "stageshift.export_action_fbx"
    bl_label = "Export active Action"
    def execute(self, context):
        try:
            file = export_animation(active_rig())
        except Exception as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        self.report({"INFO"}, f"Exported {file.name}")
        return {"FINISHED"}

class STAGESHIFT_OT_match_ik_fk(bpy.types.Operator):
    bl_idname = "stageshift.match_ik_fk"
    bl_label = "Match and switch IK / FK"
    bl_options = {"UNDO"}
    limb: bpy.props.StringProperty()
    side: bpy.props.StringProperty()
    to_ik: bpy.props.BoolProperty()
    def execute(self, context):
        try:
            match_switch(active_rig(),self.limb,self.side,self.to_ik,context.scene.tool_settings.use_keyframe_insert_auto)
        except Exception as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        return {"FINISHED"}

class STAGESHIFT_PT_rig(bpy.types.Panel):
    bl_label = "Character001 Rig"
    bl_idname = "STAGESHIFT_PT_rig"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Stage Shift"
    @classmethod
    def poll(cls, context):
        return context.scene.objects.get(RIG_NAME) is not None
    def draw(self, context):
        rig = active_rig()
        layout = self.layout
        for limb in ("arm","leg"):
            row=layout.row(align=True)
            for side in ("l","r"):
                to_ik=rig[f"IK_{limb}_{side}"]<.5
                op=row.operator("stageshift.match_ik_fk",text=f"{limb.title()} {side.upper()} → {'IK' if to_ik else 'FK'}")
                op.limb,op.side,op.to_ik=limb,side,to_ik
        layout.prop(rig.data.collections["07 · Face"], "is_visible", text="Face controls")
        layout.separator()
        layout.prop(rig,'["UE_Scale"]',text="UE scale")
        layout.prop(rig,'["FBX_Path"]',text="FBX folder")
        layout.operator("stageshift.export_mesh_fbx")
        layout.operator("stageshift.export_action_fbx")


class STAGESHIFT_PT_face_expressions(bpy.types.Panel):
    bl_label = "Face Expressions"
    bl_idname = "STAGESHIFT_PT_face_expressions"
    bl_parent_id = "STAGESHIFT_PT_rig"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Stage Shift"
    bl_options = {"DEFAULT_CLOSED"}
    def draw(self, context):
        rig = active_rig()
        for name, label in (("Happy","Joy"),("Sad","Sadness"),("Angry","Anger"),
                            ("Surprise","Surprise"),("Fear","Fear"),("Doubt","Doubt"),("Blink","Blink")):
            self.layout.prop(rig,f'["Face_{name}"]',text=label,slider=True)
        self.layout.separator()
        row = self.layout.row(align=True)
        for side, side_label in (("L", "Left"), ("R", "Right")):
            col = row.column(align=True)
            col.label(text=side_label)
            for target, label in (("Eye_Blink", "Blink"), ("Eye_Squint", "Squint"),
                                  ("Eye_Wide", "Wide"), ("Brow_Raise", "Raise Brow"),
                                  ("Brow_Lower", "Lower Brow")):
                col.prop(rig, f'["{target}_{side}"]', text=label, slider=True)






        # Independent face morph controls (v1)
        self.layout.separator()
        box = self.layout.box()
        box.label(text="Mouth only")
        for emotion in ("Happy", "Sad", "Angry", "Surprise", "Fear", "Doubt"):
            box.prop(rig, f'["Mouth_{emotion}"]', text=emotion, slider=True)
        for name in ("Open", "Wide", "Narrow", "Up", "Down", "Left", "Right",
                     "Rotate_CW", "Rotate_CCW", "CornerUp_L", "CornerUp_R",
                     "CornerDown_L", "CornerDown_R"):
            box.prop(rig, f'["Mouth_{name}"]', text=name.replace("_", " "), slider=True)
        for feature in ("Brow", "Eye"):
            box = self.layout.box()
            box.label(text=f"{feature}s independently")
            row = box.row(align=True)
            for side in ("L", "R"):
                col = row.column(align=True)
                col.label(text=side)
                for emotion in ("Happy", "Sad", "Angry", "Surprise", "Fear", "Doubt"):
                    col.prop(rig, f'["{feature}_{emotion}_{side}"]', text=emotion, slider=True)
                if feature == "Brow":
                    for tilt in ("In", "Out"):
                        col.prop(rig, f'["Brow_Rotate{tilt}_{side}"]', text=f"Rotate {tilt}", slider=True)


CLASSES=(STAGESHIFT_OT_mesh_fbx, STAGESHIFT_OT_action_fbx, STAGESHIFT_OT_match_ik_fk, STAGESHIFT_PT_rig, STAGESHIFT_PT_face_expressions)
def register():
    for cls in reversed(CLASSES):
        previous=cls if cls.is_registered else None
        if previous is None:
            if issubclass(cls,bpy.types.Operator):
                ns,cmd=cls.bl_idname.split(".")
                previous=bpy.types.Operator.bl_rna_get_subclass_py(f"{ns.upper()}_OT_{cmd}")
            else:
                previous=bpy.types.Panel.bl_rna_get_subclass_py(cls.bl_idname)
        if previous is not None:
            bpy.utils.unregister_class(previous)
    for cls in CLASSES:
        bpy.utils.register_class(cls)

if __name__=="__main__":
    register()
    print("Stage Shift rig panel enabled in Viewport N > Stage Shift.")

