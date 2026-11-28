"""Paint the die-cut cardboard props and sew the rug artwork in Painter."""
from pathlib import Path
import json
import runpy

from substance_painter.colormanagement import Color

ROOT=Path(__file__).resolve().parent
API=runpy.run_path(str(ROOT/'stylize_hub.py'))
AUTHOR=API['API']
L, C = API['L'], API['C']
MASKS=ROOT/'Painter'/'Masks'/'Craft'
LAYERS=json.loads((MASKS/'Layers.json').read_text('utf8'))
# Local colour, roughness, opacity and (where helpful) relief by cardstock layer.
SURFACES={
    'QuietTimber': ('Clean honey boards',(.77,.57,.375),.84,.72,0.),
    'HoneyPaper': ('Warm kraft faces',(.79,.62,.435),.88,.94,0.),
    'PanelKraft': ('Pressed kraft console',(.67,.49,.34),.87,.98,0.),
    'ToastedPaper': ('Toasted kraft laminate',(.58,.40,.255),.87,.93,0.),
    'SageCard': ('Painted sage card inserts',(.38,.42,.30),.88,.92,0.),
    'KraftCut': ('Open fluted core shadow',(.31,.184,.095),.91,.96,0.),
    'RugGround': ('Clean burgundy felt',(.445,.154,.18),.93,.97,0.),
    'RugFineWeave': ('Muted woven nap',(.52,.205,.238),.93,.20,.016),
    'RugBorderStitches': ('Hand-sewn floor rug border',(.83,.56,.28),.90,.94,.053),
    'BrushGrain': ('Long grain brush accents',(.83,.64,.445),.83,.29,0.),
    'FlutedPaper': ('Folded kraft flute',(.79,.56,.315),.85,.92,.068),
    'PaperLiners': ('Cream paper liners',(.91,.73,.49),.87,.93,.085),
    'GoldApplique': ('Ochre wool comedy appliqué',(.78,.51,.25),.89,.98,.045),
    'PlumApplique': ('Rose linen tragedy appliqué',(.546,.29,.36),.90,.98,.045),
    'MaskStitches': ('Short cream running stitches',(.94,.72,.455),.88,.93,.085),
}
UNDER_STYLE=('QuietTimber','HoneyPaper','PanelKraft','ToastedPaper','SageCard','KraftCut')
OVER_STYLE=('BrushGrain','FlutedPaper','PaperLiners','RugGround','RugFineWeave','RugBorderStitches','GoldApplique','PlumApplique','MaskStitches')


def stock_sprigs(stack):
    resource=API['R']
    stock=next(r.identifier() for r in resource.search('ornament') if r.identifier().name=='ornament_leaf/ornament_leaf')
    stamp=[('Stem',-.007,0.,1.9,51.,5.),('Leaf 1',-.029,-.004,9.2,9.2,54.),
           ('Leaf 2',-.020,-.007,9.4,9.4,-43.),('Leaf 3',-.009,-.001,9.,9.,53.),
           ('Leaf 4',.005,-.006,9.5,9.5,-43.),('Leaf 5',.018,.003,9.8,9.8,48.),
           ('Tip',.042,.006,10.5,10.5,14.)]
    for number,(u,v,direction) in enumerate(((.179,.659,1),(.326,.694,1),(.674,.694,-1),(.821,.659,-1)),1):
        group=L.insert_group(L.InsertPosition.from_textureset_stack(stack))
        group.set_name(f'Cardboard pass · Stencilled sprig {number}')
        for label,dx,dy,sx,sy,angle in stamp:
            layer=L.insert_fill(L.InsertPosition.inside_node(group,L.NodeStack.Substack))
            layer.set_name(label)
            layer.set_source(C.BaseColor,Color(.79,.58,.34))
            layer.set_source(C.Roughness,Color(.88,0.,0.))
            for chan in (C.Normal,C.Metallic,C.Height,C.Emissive):
                layer.set_opacity(0.,chan)
            layer.add_mask(L.MaskBackground.Black)
            mask=L.insert_fill(L.InsertPosition.inside_node(layer,L.NodeStack.Mask))
            mask.set_name('Painter stock alpha · Ornament Leaf')
            mask.set_source(None,stock)
            mask.set_projection_mode(L.ProjectionMode.UV)
            uv=mask.get_projection_parameters()
            uv.uv_wrapping_mode=L.UVWrapMode.RepeatNone
            uv.uv_transformation.scale=[sx,sy]
            uv.uv_transformation.offset=[u+dx*direction-.5,v+dy-.5]
            uv.uv_transformation.rotation=angle if direction>0 else 180.-angle
            mask.set_projection_parameters(uv)


def source_2k(node,atlas):
    folder=AUTHOR['INPUT']/atlas
    for label,channel in [('BaseColor',C.BaseColor),('NormalGL',C.Normal),('Roughness',C.Roughness),('Metallic',C.Metallic),('Emission',C.Emissive)]:
        path=folder/f'Source_{atlas}_{label}.png'
        if path.is_file():
            node.set_source(channel,AUTHOR['bitmap'](path))


def paint(tex):
    name=tex.name()
    atlas=name.removeprefix('Hub_')
    stack=tex.get_stack()
    tex.set_resolution(API['T'].Resolution(2048,2048))
    if not stack.has_channel(C.Emissive):
        stack.add_channel(C.Emissive,API['T'].ChannelFormat.RGB8)
    nodes=L.get_root_layer_nodes(stack)
    style=next((node for node in nodes if node.get_name()==API['STYLE_LAYER']),None)
    if style is None and atlas=='SelectorSign':
        base=L.insert_fill(L.InsertPosition.from_textureset_stack(stack))
        base.set_name('Sage paint over pressed card')
        base.set_source(C.BaseColor,Color(.34,.41,.32))
        base.set_source(C.Roughness,Color(.90,0.,0.))
        base.set_source(C.Metallic,Color(0.,0.,0.))
        base.set_opacity(0.,C.Emissive)
        API['stylize'](tex)
        nodes=L.get_root_layer_nodes(stack)
        style=next(node for node in nodes if node.get_name()==API['STYLE_LAYER'])
    assert style is not None,atlas
    style.set_visible(True)
    for chan in (C.BaseColor,C.Roughness,C.Normal,C.Height,C.Metallic):
        try:
            style.set_blending_mode(L.BlendingMode.Passthrough,chan)
        except ValueError:
            pass
        style.set_opacity(1.,chan)
    for effect in style.content_effects():
        value=API['STYLE'][effect.get_name()]
        effect.set_visible(True)
        effect.get_source().set_parameters({'stylized_intensity':value,'baked_lighting_intensity':.40})
    assert style.is_visible() and all(e.is_visible() for e in style.content_effects())
    assert abs(style.get_opacity(C.BaseColor)-1.)<.001
    areas=LAYERS.get(atlas,[])
    for node in nodes:
        label=node.get_name()
        if label.startswith('Cardboard pass · '):
            L.delete_node(node)
        elif areas and label in {'Hub palette · Cut corrugated kraft','Hub palette · Kraft liners'}:
            node.set_visible(False)
        elif label.startswith('Source finish'):
            source_2k(node,atlas)
            if atlas=='CompanyRug':
                node.set_opacity(0.,C.Normal)
            elif areas and atlas not in {'ScenicGreens','MusicCorner'}:
                node.set_opacity(.13,C.Normal)
    for key in UNDER_STYLE+OVER_STYLE:
        if key not in areas:
            continue
        position=L.InsertPosition.below_node(style) if key in UNDER_STYLE else L.InsertPosition.from_textureset_stack(stack)
        layer=L.insert_fill(position)
        label,color,roughness,opacity,height=SURFACES[key]
        layer.set_name('Cardboard pass · '+label)
        layer.set_source(C.BaseColor,Color(*color))
        layer.set_source(C.Roughness,Color(roughness,0.,0.))
        layer.set_opacity(opacity,C.BaseColor)
        for chan in (C.Normal,C.Metallic,C.Emissive):
            layer.set_opacity(0.,chan)
        if height:
            layer.set_source(C.Height,Color(height,0.,0.))
        else:
            layer.set_opacity(0.,C.Height)
        AUTHOR['set_mask'](layer,MASKS/f'{atlas}_{key}.png')
    if atlas in {'PlankFloor','SelectionStand'}:
        glaze=L.insert_fill(L.InsertPosition.from_textureset_stack(stack))
        glaze.set_name('Cardboard pass · Transparent warm pigment')
        glaze.set_source(C.BaseColor,Color(.815,.625,.435))
        glaze.set_opacity(.34,C.BaseColor)
        for chan in (C.Normal,C.Roughness,C.Metallic,C.Height,C.Emissive):
            glaze.set_opacity(0.,chan)
        key='QuietTimber' if atlas=='PlankFloor' else 'HoneyPaper'
        AUTHOR['set_mask'](glaze,MASKS/f'{atlas}_{key}.png')
    if atlas=='SelectorSign':
        stock_sprigs(stack)
    result=API['verify'](tex)
    result['filter_layer_opacity']=style.get_opacity(C.BaseColor)
    result['paint_layers']=len(areas)
    return result
