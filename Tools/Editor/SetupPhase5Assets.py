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

def main():
    unreal.EditorLevelLibrary.new_level(MAP)
    spawn_cube("Phase5_Floor", (0, 0, -50), (80, 40, 0.5))
    for x, label in [(500, "5m"), (1000, "10m"), (2000, "20m"), (3000, "30m"), (5000, "50m")]:
        spawn_cube("Phase5_Cover_" + label, (x, 450, 100), (1, 3, 2))
        target_class = unreal.load_class(None, "/Script/ArenaDuel.ArenaDuelWeaponTarget")
        target = unreal.EditorLevelLibrary.spawn_actor_from_class(target_class, unreal.Vector(x, 0, 100), unreal.Rotator(0, 0, 0))
        target.set_actor_label("Phase5_Target_" + label)
    spawn_cube("Phase5_BulletImpactWall", (3500, -400, 200), (0.5, 20, 4))
    unreal.EditorLevelLibrary.save_current_level()
    print("PHASE5_SETUP DONE", MAP)

main()
