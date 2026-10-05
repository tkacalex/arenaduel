import unreal

MAP = "/Game/ArenaDuel/Maps/L_Phase5GunRange"

def spawn_cube(label, location, scale, material=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(0, 0, 0))
    actor.set_actor_label(label)
    mesh = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if material:
        actor.static_mesh_component.set_material(0, material)
    return actor

def spawn_actor(class_path, label, location, rotation=(0, 0, 0)):
    actor_class = unreal.load_class(None, class_path)
    actor_rotation = unreal.Rotator()
    actor_rotation.pitch = rotation[0]
    actor_rotation.yaw = rotation[1]
    actor_rotation.roll = rotation[2]
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, unreal.Vector(*location), actor_rotation)
    actor.set_actor_label(label)
    return actor

def main():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        unreal.EditorLoadingAndSavingUtils.load_map(MAP)
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            unreal.EditorLevelLibrary.destroy_actor(actor)
    else:
        unreal.EditorLevelLibrary.new_level(MAP)
    spawn_cube("Phase5_Floor", (0, 0, -50), (80, 40, 0.5))
    for x, label in [(500, "5m"), (1000, "10m"), (2000, "20m"), (3000, "30m"), (5000, "50m")]:
        spawn_cube("Phase5_Cover_" + label, (x, 450, 100), (1, 3, 2))
        target_class = unreal.load_class(None, "/Script/ArenaDuel.ArenaDuelWeaponTarget")
        target = unreal.EditorLevelLibrary.spawn_actor_from_class(target_class, unreal.Vector(x, 0, 100), unreal.Rotator(0, 0, 0))
        target.set_actor_label("Phase5_Target_" + label)
    spawn_cube("Phase5_BulletImpactWall", (3500, -400, 200), (0.5, 20, 4))
    spawn_cube("Phase5_NorthSafetyWall", (0, 3000, 250), (80, 0.25, 2.5))
    spawn_cube("Phase5_SouthSafetyWall", (0, -3000, 250), (80, 0.25, 2.5))
    spawn_cube("Phase5_WestSafetyWall", (-8000, 0, 250), (0.25, 30, 2.5))
    spawn_cube("Phase5_EastSafetyWall", (8000, 0, 250), (0.25, 30, 2.5))
    spawn_actor("/Script/Engine.PlayerStart", "Phase5_PlayerStart", (-2500, 0, 100), (0, 0, 0))
    spawn_actor("/Script/Engine.DirectionalLight", "Phase5_DirectionalLight", (0, 0, 1000), (-45, 0, 0))
    spawn_actor("/Script/Engine.SkyLight", "Phase5_SkyLight", (0, 0, 500), (0, 0, 0))
    for x, label in [(500, "Phase5_Label_5m"), (1000, "Phase5_Label_10m"), (2000, "Phase5_Label_20m"), (3000, "Phase5_Label_30m"), (5000, "Phase5_Label_50m")]:
        label_actor = spawn_actor("/Script/Engine.TextRenderActor", label, (x, 350, 20), (0, 90, 0))
        label_actor.text_render.set_text(unreal.Text(label.replace("Phase5_Label_", "")))
        label_actor.text_render.set_world_size(48.0)
    unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(), MAP)
    print("PHASE5_SETUP DONE", MAP)

main()
