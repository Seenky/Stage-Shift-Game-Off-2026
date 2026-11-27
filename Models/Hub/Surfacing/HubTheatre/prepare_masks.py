"""Prepare Painter material ID masks and split the Blender look development maps."""
from pathlib import Path
import json

import numpy as np
from PIL import Image, ImageDraw
from scipy.ndimage import distance_transform_edt

ROOT = Path(__file__).resolve().parent
WORK = Path(r'D:/Temp/StageShiftHubSource')
PACKED = WORK/'Packed'
MAPS = WORK/'PainterInputs'
FAMILIES = {
    'Aged Wood Umber': {'Beam','BackWood','OakDark','Walnut','OakAmber','OakAged','OakBrown','OakCopper','OakHoney'},
    'Theatre Velvet': {'Wine','Velvet','VelvetDark'},
    'Matte Carpet': {'RugRed','RugDye','RugPurple'},
    'Antique Brass': {'Brass','Bronze'},
    'Plain Canvas': {'Canvas','BlueFabric','PinkFabric','Storm','Indigo'},
    'Paper Cream': {'Paper','PaperLight'},
    'Painted Iron': {'Slate','Ink','Silver'},
}
HIGH_DETAIL = {'PlankFloor','TimberShell','PortalValance','ForegroundDrapes','SelectionStand','CraftBench','WorkshopProps','Wardrobe','SoundDesk'}


def material_masks(name, size, baseline):
    archive = np.load(WORK/(name+'-uv.npz'))
    uv = archive['uv']*[size,-size]+[-.5,size-.5]
    role = archive['role']
    labels = {name:i for i,name in enumerate(sorted(set(role)),1)}
    ids = np.asarray([labels[x] for x in role],np.uint8)
    bitmap = Image.new('L',(size,size))
    draw = ImageDraw.Draw(bitmap)
    for triangle,value in zip(uv,ids):
        draw.polygon([tuple(p) for p in triangle],fill=int(value))
    raw = np.array(bitmap)
    assert raw.any(),name
    nearest = distance_transform_edt(raw==0,return_distances=False,return_indices=True)
    expanded = raw[tuple(nearest)]
    assert np.all(np.isfinite(uv)) and (uv>=-1).all() and (uv<=size+1).all(),name
    if baseline:
        groups = {label: [labels[x] for x in roles if x in labels] for label,roles in FAMILIES.items()}
    else:
        groups = {label: [i] for label,i in labels.items()}
    folder = MAPS/name
    folder.mkdir(parents=True,exist_ok=True)
    made = []
    for label,numbers in groups.items():
        if not numbers:
            continue
        mask = np.isin(expanded,numbers)
        if not mask.any():
            continue
        Image.fromarray((mask*255).astype(np.uint8)).save(folder/f'ID_{name}_{label}.png')
        made.append(label)
    return made


def prepare_baseline(name, size):
    color = Image.open(PACKED/f'T_Hub_{name}_BaseColor.png').convert('RGB')
    normal = Image.open(PACKED/f'T_Hub_{name}_Normal_GL.png').convert('RGB')
    packed = Image.open(PACKED/f'T_Hub_{name}_ORM.png').convert('RGBA')
    folder = MAPS/name
    for key,image in [('BaseColor',color),('NormalGL',normal),('Roughness',packed.getchannel('G')),('Metallic',packed.getchannel('B'))]:
        image.resize((size,size),Image.Resampling.LANCZOS).save(folder/f'Source_{name}_{key}.png')
    strength = np.asarray(packed.getchannel('A'),dtype=np.float32)/255
    if strength.max()>0:
        color_at_emitter = np.asarray(color,dtype=np.float32)*np.power(strength[:,:,None],1/2.2)
        Image.fromarray(np.rint(color_at_emitter).astype(np.uint8)).resize((size,size),Image.Resampling.LANCZOS).save(folder/f'Source_{name}_Emission.png')
        return True
    return False


def main():
    sets = json.loads((ROOT/'Manifest.json').read_text(encoding='utf8'))['sets']
    result = {}
    for name in sets:
        resolution = 2048 if name in HIGH_DETAIL else 1024
        baseline = (PACKED/f'T_Hub_{name}_BaseColor.png').is_file()
        groups = material_masks(name,resolution,baseline)
        emissive = prepare_baseline(name,resolution) if baseline else bool(set(groups)&{'LanternGlow','RimGlow','WindowLight'})
        result[name] = {'size':resolution,'baseline':baseline,'masks':groups,'emissive':emissive}
        print(name,resolution,len(groups),emissive,flush=True)
    (ROOT/'Surfaces.json').write_text(json.dumps(result,indent=2),encoding='utf8')


if __name__ == '__main__':
    main()
