"""Apply finished Painter maps to the editable Blender hub for DCC review."""
from pathlib import Path
import json
import runpy
import sys

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parent
SETTINGS = json.loads((ROOT/'Surfaces.json').read_text('utf8'))
UV = 'HubSurfaceUV'


def load_map(atlas, channel, srgb):
    path = ROOT/'Textures'/f'Hub_{atlas}_{channel}.png'
    image = bpy.data.images.load(str(path), check_existing=False)
    image.name = f'Painter · {atlas} · {channel}'
    image.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    image.pack()
    image.filepath = '//Surfacing/HubTheatre/Textures/'+path.name
    return image


def material(atlas):
    name = 'MI_Hub_Baked_'+atlas
    original = bpy.data.materials.get(name)
    result = original or bpy.data.materials.new(name)
    result.use_nodes = True
    nodes = result.node_tree.nodes
    links = result.node_tree.links
    nodes.clear()

    def node(kind, label, x, y):
        n = nodes.new(kind)
        n.label = label
        n.location = (x,y)
        return n

    uv = node('ShaderNodeUVMap','Painter atlas',-1100,40)
    uv.uv_map = UV
    shader = node('ShaderNodeBsdfPrincipled','Painter PBR',-80,410)
    soft = {'ForegroundDrapes','PortalValance','SelectorDrapery','WardrobeCostumes','CompanyRug'}
    shader.inputs['Specular IOR Level'].default_value = .23 if atlas in soft else .34
    output = node('ShaderNodeOutputMaterial','Surface',240,410)
    links.new(shader.outputs['BSDF'],output.inputs['Surface'])
    maps = {}
    for suffix,srgb,y in [('BaseColor',True,410),('Normal',False,90),('ORM',False,-245),('Emissive',True,-575)]:
        if suffix=='Emissive' and not SETTINGS[atlas]['emissive']:
            continue
        tex = node('ShaderNodeTexImage',suffix,-870,y)
        tex.image = load_map(atlas,suffix,srgb)
        tex.extension = 'EXTEND'
        links.new(uv.outputs['UV'],tex.inputs['Vector'])
        maps[suffix] = tex
    links.new(maps['BaseColor'].outputs['Color'],shader.inputs['Base Color'])
    channels = node('ShaderNodeSeparateColor','ORM channels',-565,-245)
    links.new(maps['ORM'].outputs['Color'],channels.inputs[0])
    links.new(channels.outputs['Green'],shader.inputs['Roughness'])
    links.new(channels.outputs['Blue'],shader.inputs['Metallic'])
    split = node('ShaderNodeSeparateColor','DirectX normal',-610,105)
    invert = node('ShaderNodeMath','OpenGL green',-425,-60)
    invert.operation = 'SUBTRACT'
    invert.inputs[0].default_value = 1.
    combined = node('ShaderNodeCombineColor','Blender tangent normal',-415,120)
    normal = node('ShaderNodeNormalMap','Tangent-space normal',-260,130)
    normal.uv_map = UV
    normal.inputs['Strength'].default_value = .65
    links.new(maps['Normal'].outputs['Color'],split.inputs[0])
    links.new(split.outputs['Green'],invert.inputs[1])
    links.new(split.outputs['Red'],combined.inputs['Red'])
    links.new(invert.outputs[0],combined.inputs['Green'])
    links.new(split.outputs['Blue'],combined.inputs['Blue'])
    links.new(combined.outputs[0],normal.inputs['Color'])
    links.new(normal.outputs['Normal'],shader.inputs['Normal'])
    if 'Emissive' in maps:
        links.new(maps['Emissive'].outputs['Color'],shader.inputs['Emission Color'])
        shader.inputs['Emission Strength'].default_value = .8
    nodes.active = maps['BaseColor']
    result['SurfaceMaps']='Substance Painter · BaseColor · Normal DX · ORM · optional Emissive'
    return result


def run():
    export = runpy.run_path(str(ROOT/'export_source.py'))
    scene = bpy.context.scene
    selection = [obj.name for obj in bpy.context.selected_objects]
    active = bpy.context.view_layer.objects.active
    for atlas,sources in export['SOURCES'].items():
        if atlas in export['ALTERNATES'] and any(UV not in obj.data.uv_layers for obj in sources):
            collection = bpy.data.collections.new('Temporary UV transfer')
            scene.collection.children.link(collection)
            originals = sorted(sources,key=lambda item:item.name)
            copies = [export['copy_mesh'](o,collection,False) for o in originals]
            export['unwrap_alternate'](copies)
            for original,clone in zip(originals,copies):
                source = original.data
                if source.users>1:
                    original.data=source.copy()
                    source=original.data
                new = source.uv_layers.get(UV) or source.uv_layers.new(name=UV,do_init=False)
                coords = np.empty(len(source.loops)*2,np.float32)
                clone.data.uv_layers[UV].uv.foreach_get('vector',coords)
                new.uv.foreach_set('vector',coords)
                new.active_render=True
                source.uv_layers.active=new
                old = list(original.get('HubSourceMaterialSlots', [slot.name for slot in original.material_slots]))
                original['HubSourceMaterialSlots']=old
                original['HubSurfaceAtlas']=atlas
                clone_mesh=clone.data
                bpy.data.objects.remove(clone,do_unlink=True)
                bpy.data.meshes.remove(clone_mesh)
            bpy.data.collections.remove(collection)
        styled = material(atlas)
        for target in sources:
            if not target.data.materials:
                target.data.materials.append(styled)
            for index in range(len(target.data.materials)):
                target.data.materials[index] = styled
    retired = bpy.data.materials.get('MI_Hub_Baked_WardrobeRiser')
    if retired and not retired.users:
        bpy.data.materials.remove(retired)
    # Drop only imported preview images that lost every node user.
    for image in list(bpy.data.images):
        if not image.users and image.name.startswith(('T_Hub_','Painter · ')):
            bpy.data.images.remove(image)
    for name in selection:
        obj = bpy.context.view_layer.objects.get(name)
        if obj:
            obj.select_set(True)
    bpy.context.view_layer.objects.active = active


if __name__ == '__main__':
    work=Path(sys.argv[sys.argv.index('--')+1])
    run()
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(work/'Hub_Theatre.blend'),copy=True,relative_remap=False)
    scene=bpy.context.scene
    scene.render.resolution_percentage=65
    scene.render.filepath=str(work/'Hub-Painter-Blender.png')
    bpy.ops.render.render(write_still=True)
