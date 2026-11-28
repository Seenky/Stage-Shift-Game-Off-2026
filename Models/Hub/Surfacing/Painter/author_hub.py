"""Author the hub atlases inside Substance Painter. Run one batch at a time."""
import json
from pathlib import Path

import substance_painter.project as project
import substance_painter.textureset as texture_set
import substance_painter.layerstack as layers
import substance_painter.resource as resources
import substance_painter.export as painter_export
from substance_painter.colormanagement import Color

ROOT = Path(__file__).resolve().parents[4]
PAINTER = ROOT / 'Models/Hub/Surfacing/Painter'
LIBRARY = ROOT / 'Models/Materials/Painter'
SURFACE_ROLES = json.loads((PAINTER / 'Palette.json').read_text(encoding='utf-8'))
MODULES = json.loads((PAINTER / 'Modules.json').read_text(encoding='utf-8'))
CHANNEL = texture_set.ChannelType

def create_batch(batch):
    if project.is_open():
        if project.needs_saving():
            raise RuntimeError(f'Save the current Painter project before changing batches: {project.file_path()}')
        project.close()
    file = PAINTER / f'Hub_{batch}.spp'
    if file.is_file():
        project.open(file.as_posix())
        return
    mesh = PAINTER / 'Geometry' / f'Hub_{batch}.fbx'
    settings = project.Settings(default_texture_resolution=1024)
    project.create(mesh.as_posix(), settings=settings)

def tint_to_srgb(linear):
    return [12.92*x if x <= .0031308 else 1.055*(x**(1/2.4))-.055 for x in linear]

def module_style(module, role):
    style = dict(SURFACE_ROLES[role])
    if module == 'ScenicGreens' and role == 'Canvas':
        style.update(material='Corrugated Cardboard Edge',color=[.47,.28,.13],roughness=.84)
    if module == 'SoundDesk' and role == 'Ink':
        style.update(color=[.045,.082,.128],roughness=.69,metallic=.16)
    return style

def add_role(stack, module, role):
    style=module_style(module, role)
    position=layers.InsertPosition.from_textureset_stack(stack)
    material=style['material']
    if material:
        label='Stage Shift - '+material
        packaged=LIBRARY/'Smart Materials'/(label+'.spsm')
        local=resources.ResourceID.from_url('resource://your_assets/'+label)
        if not resources.Resource.retrieve(local):
            local=resources.import_project_resource(packaged.as_posix(),resources.Usage.SMART_MATERIAL).identifier()
        group=layers.insert_smart_material(position,local)
        source=next((c for c in group.sub_layers() if isinstance(c,layers.FillLayerNode) and c.get_name()=='Material colour and response'),None)
        if source is None:
            source=next((c for c in reversed(group.sub_layers()) if isinstance(c,layers.FillLayerNode)),None)
        if source:
            source.set_source(CHANNEL.BaseColor, Color(*tint_to_srgb(style['color'])))
            source.set_source(CHANNEL.Roughness, Color(style['roughness'],0.,0.))
            source.set_source(CHANNEL.Metallic, Color(style['metallic'],0.,0.))
        # Dampen noisy grazing effects for the rug and architectural-scale timber.
        for child in group.sub_layers():
            if child.get_name()=='High-frequency finish normal':
                if role.startswith('Rug'):
                    child.set_opacity(.44,CHANNEL.Normal)
                elif 'Wood' in material or 'Oak' in material:
                    child.set_opacity(.37,CHANNEL.Normal)
            if child.get_name()=='Close-range fibres and dry finish' and role.startswith('Rug'):
                child.set_opacity(.50,CHANNEL.BaseColor)
    else:
        group=layers.insert_group(position)
        bulb=layers.insert_fill(layers.InsertPosition.inside_node(group,layers.NodeStack.Substack))
        bulb.set_name('Warm diffused lamp')
        bulb.set_source(CHANNEL.BaseColor, Color(*tint_to_srgb(style['color'])))
        bulb.set_source(CHANNEL.Roughness, Color(style['roughness'],0.,0.))
        bulb.set_source(CHANNEL.Emissive, Color(*(min(1.0,v*style['emissive']) for v in tint_to_srgb(style['color']))))
    group.set_name(role+' | '+(material or 'Warm lamp'))
    group.add_mask(layers.MaskBackground.Black)
    mask=layers.insert_fill(layers.InsertPosition.inside_node(group,layers.NodeStack.Mask))
    mask.set_name('Surface ID · '+role)
    mask_file=PAINTER/'Masks'/module/f'ID_{module}_{role}.png'
    bitmap=resources.import_project_resource(mask_file.as_posix(),resources.Usage.TEXTURE).identifier()
    mask.set_source(None,bitmap)

def paint_set(module):
    wanted='Hub_'+module
    tex=next(ts for ts in texture_set.all_texture_sets() if ts.name()==wanted)
    stack=tex.get_stack()
    for existing in list(layers.get_root_layer_nodes(stack)):
        layers.delete_node(existing)
    for ch in (CHANNEL.Emissive,):
        if not stack.has_channel(ch):
            stack.add_channel(ch,texture_set.ChannelFormat.RGB8)
    roles=MODULES[module]['roles']
    for role in roles:
        add_role(stack,module,role)
    return len(roles)

def save_batch(batch):
    file=PAINTER/f'Hub_{batch}.spp'
    if project.file_path()==file.as_posix():
        project.save()
    else:
        project.save_as(file.as_posix())

def export_batch(batch):
    template=json.loads((LIBRARY/'Export Presets'/'Stage Shift Unreal ORM.json').read_text(encoding='utf-8'))
    out=PAINTER.parent/'Textures'
    out.mkdir(parents=True,exist_ok=True)
    template['exportPath']=out.as_posix()
    template['exportList']=[{'rootPath': ts.name()} for ts in texture_set.all_texture_sets()]
    return painter_export.export_project_textures(template)


