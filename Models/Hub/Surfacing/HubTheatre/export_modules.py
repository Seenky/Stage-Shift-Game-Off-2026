"""Export movable Hub modules using the current Painter atlases and source pivots."""
from collections import defaultdict
from pathlib import Path
import json
import re
import runpy

import bpy
from mathutils import Matrix, Vector

root = Path(__file__).resolve().parent
output = root / 'Modular'
api = runpy.run_path(str(root / 'export_source.py'))
manifest = json.loads((root / 'Manifest.json').read_text(encoding='utf8'))
prior = {entry['mesh']: entry for entry in manifest['meshes']}
unchanged = api['ALTERNATES'] | {'CompanyRug','SelectionPreview','SelectorSign','SelectorDrapery','SelectorHardware','WardrobeCostumes'}
unique = set()

def clean(label):
    return re.sub(r'[^A-Za-z0-9]+','_',label).strip('_')

def center(obj):
    return obj.matrix_world @ (sum((Vector(v) for v in obj.bound_box), Vector())/8.)

def original_groups(atlas, sources):
    if atlas not in api['SPLIT']:
        return [(atlas,sources)]
    grouped = defaultdict(list)
    for obj in sources:
        grouped[api['top_prop'](obj).name].append(obj)
    return [(api['name_for'](atlas,key,i),rows) for i,(key,rows) in enumerate(sorted(grouped.items()),1)]

def assembly(atlas,obj,flyspots):
    raw = obj.name.removeprefix('SM_Hub_'+atlas+'_').removeprefix('SM_Hub_').replace('_',' ')
    parent = obj.parent
    is_prop = parent and parent.type=='EMPTY' and 'ORIGIN' not in parent.name.upper()
    if atlas in {'PortalValance','ForegroundDrapes'}:
        point=center(obj)
        side='Left' if point.y>0 else 'Right'
        if not is_prop and atlas=='PortalValance':
            return side+' proscenium curtain',None
        if atlas=='ForegroundDrapes':
            if 'Curtain Portal' in raw or 'Curtain Tie End' in raw:
                return side+' overhead curtain',None
            if 'Balcony' in raw or 'Rail Tie' in raw:
                return side+' balcony drape',None
            return side+' entrance curtain',None
    if atlas=='FlyRig' and not is_prop:
        point=center(obj)
        if raw.startswith(('Spot hanger','Fly spotlight suspension')):
            match=min(flyspots,key=lambda part:abs(part.matrix_world.translation.y-point.y))
            return match.name,match
        if raw.startswith('Coiled hemp'):
            return ('Left' if point.y>0 else 'Right')+' coiled hemp',None
        index=min(range(4),key=lambda i:abs((-466.,-300.,300.,461.)[i]-point.y))
        return f'Pulley assembly {index+1:02}',None
    if not is_prop:
        if atlas=='CraftBench' and ('lamp' in raw.lower()):
            return 'Workbench task lamp',None
        if atlas=='SoundDesk' and ('hemp coil' in raw.lower() or 'Copper hook' in raw):
            return 'Sound desk rope coil',None
        if atlas=='DressingCorner' and any(tag in raw for tag in ('vanity mat','fabric edge')):
            return 'Vanity mat',None
        if atlas=='Wardrobe' and raw.startswith(('Braided tie','Tie tassel','Tassel')):
            return 'Fitting booth curtain tie',None
    return (parent.name,parent) if is_prop else (raw,obj)

def collision(atlas,label):
    l=label.casefold()
    if atlas=='PlankFloor':
        return 'surface'
    if atlas=='TimberShell':
        return 'surface' if l.startswith(('side wall','wall board','wing plank','wing post','rear upright','timber angled')) else 'none'
    if atlas=='EntrySteps':
        return 'surface'
    if atlas=='BalconyRail':
        return 'none' if 'caged lantern' in l else 'surface'
    if atlas=='SelectionStand':
        if l.endswith('stepped podium'):
            return 'selector'
        if 'timber wing' in l:
            return 'selector'
    if atlas=='Wardrobe' and any(x in l for x in ('costume rack','framed canopy','cedar accessories box','travel case · wardrobe','wardrobe · plum hatbox','fitting booth')):
        return 'surface' if 'canopy' in l else 'box' if any(x in l for x in ('rack','box','case')) else 'none'
    if atlas=='DressingCorner' and (l.endswith('vanity · table') or l.startswith('stool')):
        return 'box'
    if atlas=='CraftBench' and l in {'sm_hub_workbench','worktop','open shelf','backboard'}:
        return 'box' if l in {'sm_hub_workbench','worktop'} else 'none'
    if atlas=='WorkshopProps' and (l=='sm_hub_workbench' or 'open canvas chest' in l or 'open offcut crate' in l):
        return 'box'
    if atlas=='SoundDesk' and (l.endswith('timber housing') or l.startswith(('stool','rolled scenery plans','scroll crate','travel case · sounddesk'))):
        return 'box'
    if atlas=='MusicCorner' and (l.startswith(('stool','instrument trunk','drum','music stand · open'))):
        return 'box'
    return 'none'

def run():
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene.unit_settings.scale_length=.01
    temporary=bpy.data.collections.new('Modular hub export')
    bpy.context.scene.collection.children.link(temporary)
    rows=[]
    for atlas,sources in api['SOURCES'].items():
        if atlas in unchanged:
            continue
        assert atlas in manifest['sets'],atlas
        ordered=sorted(sources,key=lambda obj:obj.name)
        flyspots=sorted({o.parent for o in ordered if o.parent and o.parent.name.startswith('Fly spot ·')},key=lambda o:o.name)
        atlas_mat=bpy.data.materials.new('Hub_'+atlas)
        for previous,parts in original_groups(atlas,ordered):
            original=prior[previous]
            assert len(parts)==original['source_objects'],(atlas,previous,len(parts))
            if len(parts)==1:
                continue
            groups=defaultdict(list)
            anchors={}
            for obj in parts:
                name, anchor=assembly(atlas,obj,flyspots)
                groups[name].append(obj)
                anchors[name]=anchor
            if len(groups)<=1:
                continue
            total=0
            for label,parts_in_group in sorted(groups.items()):
                group=[api['copy_mesh'](o,temporary,True) for o in parts_in_group]
                for copied in group:
                    api['set_atlas'](copied,atlas_mat)
                    copied.data.calc_loop_triangles()
                bpy.context.view_layer.update()
                low,high=api['vertex_bounds'](group)
                seed=anchors[label].matrix_world.translation if anchors[label] else None
                pivot=Vector(((low[0]+high[0])*.5,(low[1]+high[1])*.5,low[2]))
                if seed is not None and all(low[i]-8.<=seed[i]<=high[i]+8. for i in range(3)):
                    pivot=seed.copy()
                station=atlas=='SelectionStand' and label.endswith('stepped podium')
                if station:
                    x,y,z=original['pivot']
                    pivot=Vector((x,-y,z))
                matrix=Matrix.Translation(-pivot)
                for item in group:
                    item.matrix_world=matrix @ item.matrix_world
                bpy.context.view_layer.update()
                slug=clean(label)
                name=f'SM_Hub_{atlas}_{slug}'[:99]
                assert name not in unique,name
                unique.add(name)
                triangles=sum(len(o.data.loop_triangles) for o in group)
                local_bounds=api['vertex_bounds'](group)
                api['export'](output/'FBX'/f'{name}.fbx',group)
                rows.append(dict(asset=name,parent_mesh=previous,atlas=atlas,label=label if not station else 'Selector podium',
                                 pivot=[pivot.x,-pivot.y,pivot.z],old_pivot=original['pivot'],bounds=local_bounds,triangles=triangles,
                                 station_base=station,collision=collision(atlas,label)))
                total+=triangles
                for item in group:
                    bpy.data.objects.remove(item,do_unlink=True)
            assert total==original['triangles'],(previous,total,original['triangles'])
    output.mkdir(exist_ok=True)
    (output/'Manifest.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('HUB_MODULES',len(rows), len({row['parent_mesh'] for row in rows}),sum(row['triangles'] for row in rows),flush=True)

if __name__=='__main__':
    run()
