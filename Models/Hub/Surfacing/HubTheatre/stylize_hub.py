"""Stage Shift art pass for the current hub's native Substance Painter projects."""
from pathlib import Path
import json
import runpy

import substance_painter.layerstack as L
import substance_painter.resource as R
import substance_painter.textureset as T
from substance_painter.colormanagement import Color

ROOT = Path(__file__).resolve().parent
MASK_ROOT = ROOT/'Painter'/'Masks'
REGIONS = json.loads((MASK_ROOT/'Look.json').read_text('utf8'))
API = runpy.run_path(str(ROOT/'author_painter.py'))
C = T.ChannelType

# sRGB palette in the Painter document. Opacity leaves a controlled amount of the
# existing directional artwork under the new painted fields.
PAINT = {
    'FloorOakAmber': ('Honey plank', (.84,.64,.42), .77, .80),
    'FloorOakAged': ('Muted honey plank', (.78,.58,.38), .81, .78),
    'FloorOakBrown': ('Kraft brown plank', (.75,.54,.34), .82, .77),
    'FloorOakCopper': ('Warm tan plank', (.81,.59,.37), .78, .76),
    'FloorOakHoney': ('Light kraft plank', (.84,.65,.44), .80, .78),
    'WallSage': ('Painted scenery green', (.34,.41,.33), .83, .91),
    'WallSageDeep': ('Shadow sage boards', (.31,.37,.30), .84, .91),
    'Stiles': ('Chestnut framing and warm edges', (.64,.44,.29), .77, .60),
    'HoneyCard': ('Light kraft and honey trim', (.75,.56,.37), .81, .67),
    'ToastedCard': ('Warm brown framework', (.57,.39,.27), .83, .58),
    'OxideBlue': ('Slate blue panel', (.31,.40,.52), .78, .77),
    'IvoryScenery': ('Cream stage flat', (.76,.64,.47), .86, .62),
    'WineDeep': ('Deep wine cotton velvet', (.31,.09,.12), .86, .56),
    'WineFabric': ('Burgundy cotton velvet', (.51,.16,.19), .84, .58),
}
STYLE = {'Flat Shaded': .20, 'Hand Painted': 1.0}
STYLE_LAYER = 'Flat Shaded and Hand Painted'
BAKED_LIGHTING = .40
METALS = {'Surface refinement · Antique Brass', 'Surface refinement · Painted Iron'}
COLOR_MIX = {
    'PlankFloor': .54,
    'EntrySteps': .65,
    'CompanyRug': .42,
    'MusicCorner': .68,
    'DressingCorner': .72,
    'ForegroundDrapes': .75,
    'PortalValance': .76,
    'SelectorHardware': .62,
    'SelectionStand': .64,
    'SelectionPreview': .72,
    'WardrobeCostumes': .70,
    'CraftBench': .78,
}


def palette_pass(tex):
    atlas = tex.name().removeprefix('Hub_')
    stack = tex.get_stack()
    nodes = L.get_root_layer_nodes(stack)
    for node in nodes:
        if node.get_name().startswith('Hub palette · ') or node.get_name()==STYLE_LAYER:
            L.delete_node(node)
        elif node.get_name().startswith('Source finish'):
            opacity = .42 if atlas in ('PlankFloor','TimberShell','WardrobeRiser','EntrySteps') else .58
            node.set_opacity(opacity,C.Normal)
        elif node.get_name() in METALS:
            node.set_opacity(.24,C.Roughness)
            node.set_opacity(.05,C.Normal)
    top = L.InsertPosition.from_textureset_stack(stack)
    for region in REGIONS.get(atlas,[]):
        path = MASK_ROOT/'Look'/f'{atlas}_{region}.png'
        if region=='Corrugation':
            node = API['smart_material'](top,'Corrugated Cardboard Edge')
            node.set_name('Hub palette · Cut corrugated kraft')
            node.set_opacity(.76,C.BaseColor)
            node.set_opacity(.65,C.Normal)
            node.set_opacity(.77,C.Roughness)
            node.set_opacity(.46,C.Height)
            for channel in (C.Metallic,C.Emissive):
                node.set_opacity(0,channel)
        else:
            label,color,roughness,coverage = PAINT[region]
            node = L.insert_fill(top)
            node.set_name('Hub palette · '+label)
            node.set_source(C.BaseColor,Color(*color))
            node.set_source(C.Roughness,Color(roughness,0,0))
            node.set_opacity(coverage,C.BaseColor)
            node.set_opacity(.60,C.Roughness)
            for channel in (C.Normal,C.Height,C.Metallic,C.Emissive):
                node.set_opacity(0,channel)
        API['set_mask'](node,path)
    return stack


def stylize(tex):
    stack = palette_pass(tex)
    matching = next(r.identifier() for r in R.search('stylization') if r.identifier().name=='stylization/stylization')
    layer = L.insert_paint(L.InsertPosition.from_textureset_stack(stack))
    layer.set_name(STYLE_LAYER)
    for channel in (C.BaseColor,C.Roughness,C.Normal,C.Height,C.Metallic):
        try:
            layer.set_blending_mode(L.BlendingMode.Passthrough,channel)
        except ValueError:
            pass
    # Painter applies the bottom effect first, matching the source projects.
    for name, intensity in STYLE.items():
        effect = L.insert_filter_effect(L.InsertPosition.inside_node(layer,L.NodeStack.Content),matching)
        effect.set_name(name)
        source = effect.get_source()
        source.apply_preset(name)
        source.set_parameters({'stylized_intensity':intensity,'baked_lighting_intensity':BAKED_LIGHTING})
    protect_detail(tex,layer)
    return verify(tex)


def protect_detail(tex,layer=None):
    atlas = tex.name().removeprefix('Hub_')
    nodes = L.get_root_layer_nodes(tex.get_stack())
    if layer is None:
        layer = next(node for node in nodes if node.get_name()==STYLE_LAYER)
    layer.set_opacity(COLOR_MIX.get(atlas,1.),C.BaseColor)
    if atlas in {'PlankFloor','EntrySteps','CompanyRug','MusicCorner','DressingCorner','WardrobeCostumes',
                 'ForegroundDrapes','PortalValance','SelectorDrapery'}:
        layer.set_opacity(.52,C.Normal)
    if atlas=='PlankFloor':
        color_by_label = {label: color for key,(label,color,_,_) in PAINT.items() if key.startswith('Floor')}
        for node in nodes:
            label = node.get_name().removeprefix('Hub palette · ')
            if label in color_by_label:
                node.set_source(C.BaseColor,Color(*color_by_label[label]))
    if 'Corrugation' in REGIONS.get(atlas,[]):
        edge = next((node for node in nodes if node.get_name()=='Hub palette · Kraft liners'),None)
        if edge is None:
            edge = L.insert_fill(L.InsertPosition.above_node(layer))
            edge.set_name('Hub palette · Kraft liners')
            edge.set_source(C.BaseColor,Color(.82,.65,.43))
            edge.set_source(C.Roughness,Color(.88,0,0))
            edge.set_opacity(.42,C.BaseColor)
            edge.set_opacity(.34,C.Roughness)
            for channel in (C.Normal,C.Height,C.Metallic,C.Emissive):
                edge.set_opacity(0,channel)
            API['set_mask'](edge,MASK_ROOT/'Look'/f'{atlas}_Corrugation.png')


def verify(tex):
    stack = tex.get_stack()
    layer = next(n for n in L.get_root_layer_nodes(stack) if n.get_name()==STYLE_LAYER)
    assert [e.get_name() for e in layer.content_effects()]==['Hand Painted','Flat Shaded']
    data = {}
    for effect in layer.content_effects():
        name = effect.get_name()
        values = effect.get_source().get_parameters()
        assert abs(values['stylized_intensity']-STYLE[name])<.001, tex.name()
        assert abs(values['baked_lighting_intensity']-BAKED_LIGHTING)<.001, tex.name()
        data[name] = {'intensity':STYLE[name],'baked_lighting':BAKED_LIGHTING}
    data['mask_count'] = len(REGIONS.get(tex.name().removeprefix('Hub_'),[]))
    return data
