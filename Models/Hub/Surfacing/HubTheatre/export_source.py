"""Export authored Hub_Theatre meshes, keeping module boundaries and source UVs."""
from pathlib import Path
from collections import defaultdict
import json
import math
import re

import bpy
import numpy as np
from mathutils import Matrix, Vector

HUB = Path(bpy.data.filepath).parent
OUT = HUB / 'Surfacing' / 'HubTheatre'
TEMP = Path(r'D:/Temp/StageShiftHubSource')
UV = 'HubSurfaceUV'
BATCHES = {
    'Architecture': ('PlankFloor', 'TimberShell', 'BalconyRail', 'EntrySteps', 'WardrobeRiser'),
    'Textiles': ('ForegroundDrapes', 'PortalValance', 'CompanyRug', 'SelectorDrapery', 'WardrobeCostumes'),
    'Workshop': ('Wardrobe', 'DressingCorner', 'CraftBench', 'WorkshopProps', 'SoundDesk', 'MusicCorner'),
    'Scenography': ('SelectionStand', 'SelectorHardware', 'ScenicGreens', 'FlyRig', 'SelectionPreview', 'CrimePreview', 'CastlePreview', 'WinterPreview', 'SeaPreview', 'SelectorSign'),
}
ALTERNATES = {'CrimePreview', 'CastlePreview', 'WinterPreview', 'SeaPreview'}
SPLIT = {'SelectorDrapery', 'SelectorHardware', 'WardrobeCostumes', 'WorkshopProps'}
ALIASES = {'': 'OakDark', 'Aged brass': 'Bronze', 'Solid brass': 'Brass', 'End grain': 'OakAged',
           'Oak · Amber': 'OakAmber', 'Oak · Carcass': 'OakCopper', 'Oak · Edge grain': 'OakDark',
           'Oak · Honey': 'OakHoney', 'Oak · Inner board': 'OakBrown', 'Oak · Toasted': 'OakAged'}
SCENE = bpy.context.scene
ANCHORS = {m['collection']: m['location'] for m in json.loads((HUB / 'Hub_Theatre.json').read_text(encoding='utf8'))['modules']}
SOURCE_COLS = {c.name: c for c in SCENE.collection.children_recursive}
SOURCES = defaultdict(list)
SLOTS = {}
for collection in SOURCE_COLS.values():
    for source in collection.objects:
        if source.type != 'MESH' or source.hide_render:
            continue
        atlas = source.get('HubSurfaceAtlas') or collection.name
        if any(atlas in batch for batch in BATCHES.values()):
            SOURCES[atlas].append(source)
            SLOTS[source.name] = list(source.get('HubSourceMaterialSlots', [slot.name for slot in source.material_slots]))


def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def vertex_bounds(world_objects):
    points = [obj.matrix_world @ Vector(corner) for obj in world_objects for corner in obj.bound_box]
    return [[min(v[i] for v in points) for i in range(3)], [max(v[i] for v in points) for i in range(3)]]


def atlas_role(slot):
    return ALIASES.get(slot, slot.removeprefix('MI_Hub_'))


def export(path, objects):
    path.parent.mkdir(parents=True, exist_ok=True)
    select(objects)
    assert bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'},
        axis_forward='-Y', axis_up='Z', apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
        mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False, path_mode='STRIP') == {'FINISHED'}


def copy_mesh(source, collection, keep):
    mesh = source.data.copy()
    obj = bpy.data.objects.new(source.name, mesh)
    collection.objects.link(obj)
    obj.matrix_world = source.matrix_world.copy()
    if keep:
        authored = np.empty(len(mesh.loops) * 2, np.float32)
        mesh.uv_layers[UV].uv.foreach_get('vector', authored)
        while len(mesh.uv_layers):
            mesh.uv_layers.remove(mesh.uv_layers[0])
        mesh.uv_layers.new(name=UV, do_init=False).uv.foreach_set('vector', authored)
    else:
        while len(mesh.uv_layers):
            mesh.uv_layers.remove(mesh.uv_layers[0])
        mesh.uv_layers.new(name=UV, do_init=False)
    return obj


def unwrap_alternate(objects):
    select(objects)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(62), island_margin=.004, correct_aspect=False)
    bpy.ops.uv.select_all(action='SELECT')
    bpy.ops.uv.average_islands_scale(scale_uv=False, shear=False)
    bpy.ops.uv.pack_islands(rotate=True, rotate_method='CARDINAL', margin_method='FRACTION', margin=.005,
                            shape_method='AABB', merge_overlap=False)
    bpy.ops.object.mode_set(mode='OBJECT')


def face_data(obj, slots):
    mesh = obj.data
    mesh.calc_loop_triangles()
    loop_ids = np.empty((len(mesh.loop_triangles), 3), np.int32)
    mesh.loop_triangles.foreach_get('loops', loop_ids.ravel())
    polys = np.empty(len(mesh.loop_triangles), np.int32)
    mesh.loop_triangles.foreach_get('polygon_index', polys)
    slot_ids = np.empty(len(mesh.polygons), np.int32)
    mesh.polygons.foreach_get('material_index', slot_ids)
    uv = np.empty((len(mesh.loops), 2), np.float32)
    mesh.uv_layers[UV].uv.foreach_get('vector', uv.ravel())
    roles = np.array([atlas_role(s) for s in slots] or ['OakDark'])
    return uv[loop_ids], roles[np.minimum(slot_ids[polys],len(roles)-1)]


def set_atlas(obj, atlas_mat):
    for poly in obj.data.polygons:
        poly.material_index = 0
    obj.data.materials.clear()
    obj.data.materials.append(atlas_mat)


def top_prop(obj):
    # Deliberate Blender parents define movable prop assemblies.
    parent = obj.parent
    last = obj
    while parent and parent.type == 'EMPTY' and parent.name not in {'ORIGIN · ' + obj.get('module_collection', '')}:
        # A common zone origin is not a single prop.
        if 'ORIGIN' in parent.name.upper():
            break
        last = parent
        parent = parent.parent
    return last


def name_for(atlas, label, index):
    bare = re.sub(r'[^A-Za-z0-9]+','_',label).strip('_')
    bare = re.sub(r'^(SM_Hub_)?(Wardrobe|SelectionStand|CraftBench)_?', '', bare)
    return f'{atlas}_{bare[:44]}_{index:02}'


def runtime_groups(atlas, src, copies):
    if atlas not in SPLIT:
        return [(atlas, copies, None)]
    groups = defaultdict(list)
    for source, copy in zip(src, copies):
        groups[top_prop(source).name].append(copy)
    # Selection trim/hardware and hanging clothes share their own movable mesh per authored group.
    return [(name_for(atlas, label, i), items, bpy.data.objects[label]) for i,(label,items) in enumerate(sorted(groups.items()),1)]


def run():
    print('Units', SCENE.unit_settings.system, SCENE.unit_settings.scale_length, flush=True)
    # Blender source is authored directly in centimetres.
    SCENE.unit_settings.system = 'METRIC'
    SCENE.unit_settings.scale_length = .01
    TEMP.joinpath('Packed').mkdir(parents=True, exist_ok=True)
    for image in bpy.data.images:
        if image.name.startswith('T_Hub_') and image.packed_file:
            (TEMP/'Packed'/(image.name+'.png')).write_bytes(image.packed_file.data)
    meshes = []
    sets = {}
    for batch, atlases in BATCHES.items():
        collection = bpy.data.collections.new('Hub export · '+batch)
        SCENE.collection.children.link(collection)
        batch_objects = []
        for i, atlas in enumerate(atlases):
            src = sorted(SOURCES[atlas], key=lambda obj: obj.name)
            if not src and atlas=='WardrobeRiser':
                continue
            assert src, atlas
            complete_uv = all(UV in obj.data.uv_layers for obj in src)
            copies = [copy_mesh(o, collection, complete_uv) for o in src]
            if not complete_uv:
                unwrap_alternate(copies)
            bpy.context.view_layer.update()
            samples = [face_data(copy, SLOTS[source.name]) for source, copy in zip(src,copies)]
            UVs = np.concatenate([uv for uv,_ in samples])
            roles = np.concatenate([role for _,role in samples])
            np.savez_compressed(TEMP/(atlas+'-uv.npz'), uv=UVs, role=roles)
            m = bpy.data.materials.new('Hub_'+atlas)
            for copy in copies:
                set_atlas(copy,m)
            origin = ANCHORS.get(atlas, ANCHORS['SelectionStand'] if 'Preview' in atlas else [0,0,0])
            sets[atlas] = {'batch':batch,'roles':sorted(set(roles)),'triangles':len(roles), 'bounds':vertex_bounds(copies), 'source_objects':len(copies)}
            for name, group, prop in runtime_groups(atlas,src,copies):
                low, high = vertex_bounds(group)
                if prop:
                    seed=prop.matrix_world.translation
                    if not all(low[k]-15 <= seed[k] <= high[k]+15 for k in range(3)):
                        seed=Vector(((low[0]+high[0])*.5,(low[1]+high[1])*.5,low[2]))
                    pivot=list(seed)
                elif atlas=='SelectorSign':
                    pivot=[(low[0]+high[0])*.5,(low[1]+high[1])*.5,low[2]]
                else:
                    pivot=[origin[0],-origin[1],origin[2]]
                matrices = [o.matrix_world.copy() for o in group]
                transform = Matrix.Translation(-Vector(pivot))
                if atlas in ALTERNATES:
                    # Unplaced library cassettes sit at the origin. Fit them into the
                    # aperture using the placed source's floor and centre as guides.
                    main_low, main_high = sets['SelectionPreview']['bounds']
                    fit = min(1., *(0.985*(b-a)/(d-c) for a,b,c,d in zip(main_low,main_high,low,high)))
                    entrance = Vector(((main_low[0]+main_high[0])*.5, 0, main_low[2]))
                    source_floor = Vector(((low[0]+high[0])*.5, (low[1]+high[1])*.5, low[2]))
                    transform = Matrix.Translation(entrance-Vector(pivot)) @ Matrix.Scale(fit,4) @ Matrix.Translation(-source_floor)
                for o in group:
                    o.matrix_world = transform @ o.matrix_world
                bpy.context.view_layer.update()
                local_bounds=vertex_bounds(group)
                export(OUT/'FBX'/('SM_Hub_'+name+'.fbx'), group)
                for o, transform in zip(group,matrices):
                    o.matrix_world=transform
                meshes.append({'mesh':name,'atlas':atlas,'pivot':[pivot[0],-pivot[1],pivot[2]],
                    'bounds':local_bounds,'source_objects':len(group),
                    'triangles':sum(len(o.data.loop_triangles) for o in group)})
            dims=np.asarray(sets[atlas]['bounds'])
            print(atlas, len(src), len(roles), len(set(roles)), dims.round(2).tolist(), flush=True)
            # Keep each separately baked texture set away from neighbouring sets.
            for obj in copies:
                obj.location.x += 4500*i
            batch_objects.extend(copies)
        export(OUT/'Painter'/'Geometry'/('Hub_'+batch+'.fbx'),batch_objects)
        for obj in batch_objects:
            bpy.data.objects.remove(obj,do_unlink=True)
        bpy.data.collections.remove(collection)
        (OUT/'Manifest.json').write_text(json.dumps({'source':'Hub_Theatre.blend','sets':sets,'meshes':meshes},indent=2,ensure_ascii=False),encoding='utf8')


if __name__ == '__main__':
    run()
