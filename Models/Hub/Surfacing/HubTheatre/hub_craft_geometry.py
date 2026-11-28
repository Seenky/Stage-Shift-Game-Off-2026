"""Correct the authored hub components and prepare the revised Painter geometry."""
from pathlib import Path
import json
import runpy
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
PANEL = 'Inset wood panel with switch sockets'


def front_plane(panel):
    normal_matrix = panel.matrix_world.to_3x3().inverted().transposed()
    front = max((p for p in panel.data.polygons if (normal_matrix @ p.normal).normalized().x < -.6),key=lambda p:p.area)
    return (normal_matrix @ front.normal).normalized(), panel.matrix_world @ front.center


def move(obj, delta):
    world = obj.matrix_world.copy()
    world.translation += delta
    obj.matrix_world = world


def crown_uv():
    sign=bpy.data.objects['Inset muted green scenic panel']
    layer=sign.data.uv_layers['HubSurfaceUV'].uv
    transform=sign.matrix_world
    normals=transform.to_3x3().inverted().transposed()
    for face in sign.data.polygons:
        direction=(normals@face.normal).normalized().x
        for i in face.loop_indices:
            point=transform@sign.data.vertices[sign.data.loops[i].vertex_index].co
            if abs(direction)>.60:
                bottom=.58 if direction<0 else .12
                layer[i].vector=(.5-point.y*.00222,bottom+(point.z-242.)*.00222)
            else:
                u,v=layer[i].vector
                layer[i].vector=(.05+.90*u,.38+.11*v)
    sign['HubSurfaceAtlas']='SelectorSign'
    sign.data.update()


def settle_components():
    assert bpy.context.mode=='OBJECT'
    collection = bpy.data.collections['SelectionStand']
    normal, surface = front_plane(bpy.data.objects[PANEL])
    receipt = {}
    for side in ('left','right'):
        housing = collection.objects[f'Selector · {side} paddle housing']
        rocker = collection.objects[f'Selector · {side} pressable paddle']
        bezel = next(o for o in housing.children if o.name.startswith('Stepped antique brass bezel'))
        plate = next(o for o in rocker.children if o.name.startswith('Rounded cast rocker plate'))
        move(housing,normal*3.36)
        move(rocker,normal*2.05)
        bpy.context.view_layer.update()
        offsets = [min(normal.dot(o.matrix_world @ v.co-surface) for v in o.data.vertices) for o in (bezel,plate)]
        assert offsets[0] >= -.3 and offsets[1] >= 1.,(side,offsets)
        receipt[side+'_control_min_clearance_cm'] = offsets
    bell = collection.objects['Selector · service bell']
    move(bell,Vector((-11.4,0,0)))
    bpy.context.view_layer.update()
    shelf = collection.objects['Wooden bell shelf']
    xs = [(shelf.matrix_world @ v.co).x for v in shelf.data.vertices]
    low,high = min(xs),max(xs)
    local = shelf.matrix_world.inverted()
    for v in shelf.data.vertices:
        world = shelf.matrix_world @ v.co
        # Extend the free end underneath the bell's foot while keeping the wall attachment.
        world.x -= 7.4*max(0.,1.-1.7*(world.x-low)/(high-low))
        v.co = local @ world
    shelf.data.update()
    foot = collection.objects['Blackened brass foot']
    clearance = min(normal.dot(foot.matrix_world @ v.co-surface) for v in foot.data.vertices)
    assert clearance > .45,clearance
    receipt['bell_clearance_cm'] = clearance
    for name,distance in (('DressingCorner',25.8),('Wardrobe',26.2)):
        group = bpy.data.collections[name]
        for obj in group.objects:
            if obj.parent not in group.objects.values():
                move(obj,Vector((0,0,-distance)))
    riser=bpy.data.collections['WardrobeRiser']
    assert len(riser.objects)==3
    for obj in list(riser.objects):
        bpy.data.objects.remove(obj,do_unlink=True)
    bpy.data.collections.remove(riser)
    sprigs=[o for o in collection.objects if o.name.startswith(('Stamped brass foliage stem','Pressed acanthus leaf'))]
    assert len(sprigs)==32
    for obj in sprigs:
        bpy.data.objects.remove(obj,do_unlink=True)
    crown_uv()
    bpy.context.view_layer.update()
    receipt['removed_riser_meshes']=3
    receipt['painted_sprig_source_meshes']=len(sprigs)
    return receipt


def painter_meshes():
    api=runpy.run_path(str(ROOT/'export_source.py'))
    scene=bpy.context.scene
    for batch, atlases in api['BATCHES'].items():
        staging=bpy.data.collections.new('Temporary Painter geometry')
        scene.collection.children.link(staging)
        copies=[]
        for i,atlas in enumerate(atlases):
            if atlas not in api['SOURCES']:
                continue
            slot=bpy.data.materials.new('Hub_'+atlas)
            for original in sorted(api['SOURCES'][atlas],key=lambda o:o.name):
                copy=api['copy_mesh'](original,staging,True)
                api['set_atlas'](copy,slot)
                copy.location.x+=4500*i
                copies.append(copy)
        bpy.context.view_layer.update()
        api['export'](ROOT/'Painter'/'Geometry'/f'Hub_{batch}.fbx',copies)
        for copy in copies:
            mesh=copy.data
            bpy.data.objects.remove(copy,do_unlink=True)
            bpy.data.meshes.remove(mesh)
        bpy.data.collections.remove(staging)


if __name__ == '__main__':
    work=Path(sys.argv[sys.argv.index('--')+1])
    record=settle_components()
    (work/'geometry-receipt.json').write_text(json.dumps(record,indent=2),'utf8')
    painter_meshes()
