"""Build the editable Painter stacks for the current Blender hub."""
from pathlib import Path
import json

import substance_painter.baking as baking
import substance_painter.export as export
import substance_painter.layerstack as layers
import substance_painter.project as project
import substance_painter.resource as resource
import substance_painter.textureset as texture
from substance_painter.colormanagement import Color

ROOT = Path(__file__).resolve().parent
LIBRARY = ROOT.parents[2]/'Materials'/'Painter'
PALETTE = json.loads((ROOT.parent/'Painter'/'Palette.json').read_text('utf8'))
SURFACES = json.loads((ROOT/'Surfaces.json').read_text('utf8'))
INPUT = Path(r'D:/Temp/StageShiftHubSource/PainterInputs')
C = texture.ChannelType


def project_path(batch):
    return ROOT/'Painter'/f'HubTheatre_{batch}.spp'


def open_batch(batch):
    target = project_path(batch)
    if project.is_open():
        if project.is_busy():
            raise RuntimeError('Painter is busy')
        if project.needs_saving():
            raise RuntimeError(f'Save current Painter project before switching: {project.file_path()}')
        project.close()
    if target.exists():
        project.open(target.as_posix())
    else:
        settings = project.Settings(default_texture_resolution=1024, normal_map_format=project.NormalMapFormat.OpenGL)
        project.create((ROOT/'Painter'/'Geometry'/f'Hub_{batch}.fbx').as_posix(), settings=settings)


def bitmap(path):
    return resource.import_project_resource(path.as_posix(),resource.Usage.TEXTURE).identifier()


def set_mask(group,path):
    group.add_mask(layers.MaskBackground.Black)
    mask = layers.insert_fill(layers.InsertPosition.inside_node(group,layers.NodeStack.Mask))
    mask.set_name(path.stem)
    mask.set_source(None,bitmap(path))


def smart_material(position, name):
    label = 'Stage Shift - '+name
    path = LIBRARY/'Smart Materials'/(label+'.spsm')
    local = resource.ResourceID.from_url('resource://your_assets/'+label)
    if not resource.Resource.retrieve(local):
        local = resource.import_project_resource(path.as_posix(),resource.Usage.SMART_MATERIAL).identifier()
    return layers.insert_smart_material(position,local)


def srgb(x):
    return 12.92*x if x<=.0031308 else 1.055*x**(1/2.4)-.055


def selector_title(rebuild=False):
    ts = next(s for s in texture.all_texture_sets() if s.name()=='Hub_SelectorHardware')
    stack = ts.get_stack()
    name = 'Printed lettering · Выбор постановки'
    existing = next((i for i in layers.get_root_layer_nodes(stack) if i.get_name()==name),None)
    if existing:
        if not rebuild:
            return
        layers.delete_node(existing)
    ink = layers.insert_fill(layers.InsertPosition.from_textureset_stack(stack))
    ink.set_name(name)
    ink.set_source(C.BaseColor,Color(.235,.12,.07))
    ink.set_source(C.Roughness,Color(.79,0,0))
    for channel in (C.Normal,C.Height,C.Metallic,C.Emissive):
        ink.set_opacity(0,channel)
    set_mask(ink,ROOT/'Painter'/'Masks'/'SelectorTitle.png')


def paint(name):
    settings = SURFACES[name]
    ts = next(t for t in texture.all_texture_sets() if t.name()=='Hub_'+name)
    stack = ts.get_stack()
    if any(x.get_name().startswith(('Source finish','Theatre palette')) for x in layers.get_root_layer_nodes(stack)):
        return 'present'
    ts.set_resolution(texture.Resolution(settings['size'],settings['size']))
    if not stack.has_channel(C.Emissive):
        stack.add_channel(C.Emissive,texture.ChannelFormat.RGB8)
    inputs = INPUT/name
    dest = layers.InsertPosition.from_textureset_stack(stack)
    if settings['baseline']:
        # Keep authored art and material separation from the current model.
        base = layers.insert_fill(dest)
        base.set_name('Source finish · original hub artwork and textures')
        for key,channel in [('BaseColor',C.BaseColor),('NormalGL',C.Normal),('Roughness',C.Roughness),('Metallic',C.Metallic),('Emission',C.Emissive)]:
            path = inputs/f'Source_{name}_{key}.png'
            if path.exists():
                base.set_source(channel,bitmap(path))
            else:
                base.set_opacity(0,channel)
        base.set_opacity(0,C.Height)
        for feature in settings['masks']:
            family = 'Warm Paper' if feature=='Paper Cream' else feature
            group = smart_material(dest,family)
            group.set_name('Surface refinement · '+family)
            group.set_opacity(.26 if family=='Theatre Velvet' else .15,C.Roughness)
            group.set_opacity(.18 if family=='Matte Carpet' else .10,C.Normal)
            group.set_opacity(.08 if family in ('Theatre Velvet','Matte Carpet') else .05,C.BaseColor)
            for channel in (C.Metallic,C.Emissive,C.Height):
                group.set_opacity(0,channel)
            set_mask(group,inputs/f'ID_{name}_{feature}.png')
        if name=='SelectorHardware':
            selector_title()
    else:
        for role in settings['masks']:
            style = PALETTE[role]
            label = style['material']
            if label:
                group = smart_material(dest,label)
                children = [child for child in group.sub_layers() if isinstance(child,layers.FillLayerNode)]
                fill = next((child for child in children if child.get_name()=='Material colour and response'),children[-1] if children else None)
            else:
                group = layers.insert_group(dest)
                fill = layers.insert_fill(layers.InsertPosition.inside_node(group,layers.NodeStack.Substack))
                fill.set_name('Soft painted glass')
            if fill is not None:
                fill.set_source(C.BaseColor,Color(*(srgb(x) for x in style['color'])))
                fill.set_source(C.Roughness,Color(style['roughness'],0,0))
                fill.set_source(C.Metallic,Color(style['metallic'],0,0))
                if not label:
                    fill.set_source(C.Emissive,Color(*(srgb(min(1,x*style['emissive'])) for x in style['color'])))
            group.set_name('Theatre palette · '+role)
            set_mask(group,inputs/f'ID_{name}_{role}.png')
    return len(settings['masks'])


def prepare_baker(name):
    tex = next(t for t in texture.all_texture_sets() if t.name()==name)
    settings = baking.BakingParameters.from_texture_set(tex)
    usage = type(settings.get_enabled_bakers()[0])
    settings.set_enabled_bakers([usage.Normal,usage.AO,usage.Curvature,usage.Position,usage.WorldSpaceNormal])


def save(batch):
    target = project_path(batch)
    if (project.file_path() or '').casefold()==target.as_posix().casefold():
        project.save()
    else:
        project.save_as(target.as_posix())


def export_batch(only=None):
    params = json.loads((LIBRARY/'Export Presets'/'Stage Shift Unreal ORM.json').read_text('utf8'))
    textures = ROOT/'Textures'
    textures.mkdir(exist_ok=True)
    params['exportPath'] = textures.as_posix()
    params['exportList'] = [{'rootPath': t.name()} for t in texture.all_texture_sets() if only is None or t.name()==only]
    return export.export_project_textures(params)
