"""Export theatre modules from the open Blender file without changing its objects.

Usage: blender -b Menu_Theatre.blend --python Export_FBX.py -- ScenicMoon
Omit the argument after -- to export every module in the asset library.
"""

import json
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector


def export_module(module, source_root):
    collection = bpy.data.collections[module["collection"]]
    sources = [obj for obj in collection.objects if obj.type == "MESH"]
    if not sources:
        raise RuntimeError(f"{collection.name} has no mesh objects")

    if "Export Name" in collection:
        pivot = collection.instance_offset.copy()
    else:
        x, y, z = module["location"]
        pivot = Vector((x, -y, z))
    center_at_origin = Matrix.Translation(-pivot)
    temporary = bpy.data.collections.new("Temporary FBX export")
    bpy.context.scene.collection.children.link(temporary)
    copies = []
    combined = []
    try:
        for source in sources:
            copy = source.copy()
            temporary.objects.link(copy)
            copy.parent = None
            copy.matrix_world = center_at_origin @ source.matrix_world
            copies.append(copy)

        bpy.context.view_layer.update()
        depsgraph = bpy.context.evaluated_depsgraph_get()
        for copy in copies:
            evaluated = copy.evaluated_get(depsgraph)
            mesh = bpy.data.meshes.new_from_object(evaluated, depsgraph=depsgraph)
            baked = bpy.data.objects.new(copy.name + "_Baked", mesh)
            temporary.objects.link(baked)
            baked.matrix_world = copy.matrix_world
            combined.append(baked)

        bpy.ops.object.select_all(action="DESELECT")
        for obj in combined:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = combined[0]
        bpy.ops.object.join()
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        # The final asset and FBX filename stay stable on future reimports.
        path = source_root / "FBX" / (module["name"] + ".fbx")
        bpy.ops.export_scene.fbx(
            filepath=str(path),
            use_selection=True,
            object_types={"MESH"},
            axis_forward="-Y",
            axis_up="Z",
            apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_NONE",
            mesh_smooth_type="FACE",
            add_leaf_bones=False,
            bake_anim=False,
        )
    finally:
        for obj in list(temporary.objects):
            data = obj.data
            bpy.data.objects.remove(obj, do_unlink=True)
            if data and data.users == 0:
                bpy.data.meshes.remove(data)
        bpy.data.collections.remove(temporary)


def main():
    source_path = Path(bpy.data.filepath)
    source_root = source_path.parent
    library_scene = bpy.data.scenes.get("Asset Library")
    if library_scene is not None:
        entries = [
            {"name": collection["Export Name"], "collection": collection.name}
            for collection in library_scene.collection.children_recursive
            if "Export Name" in collection
        ]
    else:
        manifest = json.loads(source_path.with_suffix(".json").read_text(encoding="utf-8"))
        entries = manifest["modules"]
    module_names = set(sys.argv[sys.argv.index("--") + 1 :]) if "--" in sys.argv else set()
    modules = [entry for entry in entries if not module_names or entry["collection"] in module_names]
    if module_names - {entry["collection"] for entry in modules}:
        raise RuntimeError(f"Unknown module selection: {module_names}")
    (source_root / "FBX").mkdir(exist_ok=True)
    original_scene = bpy.context.window.scene
    try:
        if library_scene is not None:
            bpy.context.window.scene = library_scene
            bpy.context.view_layer.update()
        for module in modules:
            export_module(module, source_root)
    finally:
        bpy.context.window.scene = original_scene


if __name__ == "__main__":
    main()
