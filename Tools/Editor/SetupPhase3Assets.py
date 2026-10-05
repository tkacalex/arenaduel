import unreal


ROOT = "/Game/ArenaDuel"
INPUT_PATH = ROOT + "/Input"
CHAR_PATH = ROOT + "/Characters"
GAME_PATH = ROOT + "/Game"
MAP_PATH = ROOT + "/Maps"
DEV_MATERIAL_PATH = ROOT + "/Development"


def log(message):
    print("PHASE3_SETUP", message)


def key(name):
    value = unreal.Key()
    value.set_editor_property("key_name", name)
    return value


def asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def load_or_create_data_asset(name, package_path, asset_class):
    object_path = package_path + "/" + name
    existing = unreal.load_object(None, object_path)
    if existing:
        log("REUSE " + object_path)
        return existing
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("supported_class", asset_class)
    created = asset_tools().create_asset(name, package_path, asset_class, factory)
    if not created:
        raise RuntimeError("Could not create " + object_path)
    log("CREATE " + object_path)
    return created


def load_or_create_blueprint(name, package_path, parent_class):
    object_path = package_path + "/" + name
    existing = unreal.load_object(None, object_path)
    if existing:
        log("REUSE " + object_path)
        return existing
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    created = asset_tools().create_asset(name, package_path, unreal.Blueprint, factory)
    if not created:
        raise RuntimeError("Could not create " + object_path)
    log("CREATE " + object_path)
    return created


def save(asset):
    if not unreal.EditorAssetLibrary.save_asset(asset.get_path_name(), only_if_is_dirty=False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def load_or_create_dev_material(name, color):
    object_path = DEV_MATERIAL_PATH + "/" + name
    material = unreal.load_object(None, object_path)
    if not material:
        factory = unreal.MaterialFactoryNew()
        material = asset_tools().create_asset(name, DEV_MATERIAL_PATH, unreal.Material, factory)
        if not material:
            raise RuntimeError("Could not create development material " + object_path)
        expression = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -300, 0)
        expression.set_editor_property("constant", unreal.LinearColor(color[0], color[1], color[2], 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(expression, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(material)
        save(material)
    return material


def compile_blueprint(blueprint):
    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    else:
        blueprint.compile()
    status = blueprint.get_editor_property("status")
    log("BLUEPRINT_STATUS " + blueprint.get_path_name() + " " + str(status))
    if "ERROR" in str(status).upper():
        raise RuntimeError("Blueprint compile failed for " + blueprint.get_path_name())
    save(blueprint)


def configure_input_actions():
    move = load_or_create_data_asset("IA_Move", INPUT_PATH, unreal.InputAction)
    look = load_or_create_data_asset("IA_Look", INPUT_PATH, unreal.InputAction)
    jump = load_or_create_data_asset("IA_Jump", INPUT_PATH, unreal.InputAction)
    sprint = load_or_create_data_asset("IA_Sprint", INPUT_PATH, unreal.InputAction)
    crouch = load_or_create_data_asset("IA_Crouch", INPUT_PATH, unreal.InputAction)
    move.set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
    look.set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
    jump.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    sprint.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    crouch.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    for action in [move, look, jump, sprint, crouch]:
        save(action)
    log("ACTION_TYPES " + str(move.get_editor_property("value_type")) + " " + str(look.get_editor_property("value_type")) + " " + str(jump.get_editor_property("value_type")) + " " + str(sprint.get_editor_property("value_type")) + " " + str(crouch.get_editor_property("value_type")))
    return move, look, jump, sprint, crouch


def modifier_swizzle(outer):
    modifier = unreal.new_object(unreal.InputModifierSwizzleAxis, outer)
    modifier.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return modifier


def modifier_negate(outer):
    modifier = unreal.new_object(unreal.InputModifierNegate, outer)
    modifier.set_editor_properties({"x": True, "y": True, "z": True})
    return modifier


def configure_mapping_context(move, look, jump, sprint, crouch):
    context = load_or_create_data_asset("IMC_Gameplay", INPUT_PATH, unreal.InputMappingContext)
    context.unmap_all()
    entries = []

    def add(action, key_name, modifiers=None):
        mapping = context.map_key(action, key(key_name))
        mapping.set_editor_property("modifiers", modifiers or [])
        entries.append(mapping)

    add(move, "D")
    add(move, "A", [modifier_negate(context)])
    add(move, "W", [modifier_swizzle(context)])
    add(move, "S", [modifier_swizzle(context), modifier_negate(context)])
    add(look, "Mouse2D")
    add(jump, "SpaceBar")
    add(sprint, "LeftShift")
    add(crouch, "LeftControl")

    context.set_editor_property("mappings", entries)
    mapping_data = context.get_editor_property("default_key_mappings")
    mapping_data.set_editor_property("mappings", entries)
    context.set_editor_property("default_key_mappings", mapping_data)
    save(context)
    log("MAPPING_COUNT " + str(len(entries)))
    for mapping in context.get_editor_property("mappings"):
        action = mapping.get_editor_property("action").get_name()
        key_name = str(mapping.get_editor_property("key").get_editor_property("key_name"))
        modifiers = [m.get_class().get_name() for m in mapping.get_editor_property("modifiers")]
        log("MAPPING " + action + " " + key_name + " " + str(modifiers))
    return context


def configure_blueprints(context, move, look, jump, sprint, crouch):
    character_class = unreal.load_class(None, "/Script/ArenaDuel.ArenaDuelCharacter")
    game_mode_class = unreal.load_class(None, "/Script/ArenaDuel.ArenaDuelGameMode")
    if not character_class or not game_mode_class:
        raise RuntimeError("Native ArenaDuel classes could not be loaded")
    character_bp = load_or_create_blueprint("BP_ArenaDuelCharacter", CHAR_PATH, character_class)
    character_generated = unreal.BlueprintEditorLibrary.generated_class(character_bp)
    character_cdo = unreal.get_default_object(character_generated)
    character_cdo.set_editor_properties({
        "default_mapping_context": context,
        "move_action": move,
        "look_action": look,
        "jump_action": jump,
        "sprint_action": sprint,
        "crouch_action": crouch,
    })
    compile_blueprint(character_bp)

    game_mode_bp = load_or_create_blueprint("BP_ArenaDuelGameMode", GAME_PATH, game_mode_class)
    game_mode_generated = unreal.BlueprintEditorLibrary.generated_class(game_mode_bp)
    game_mode_cdo = unreal.get_default_object(game_mode_generated)
    game_mode_cdo.set_editor_property("default_pawn_class", character_generated)
    compile_blueprint(game_mode_bp)
    return character_bp, game_mode_bp, character_generated, game_mode_generated


def create_test_map():
    object_path = MAP_PATH + "/L_Phase3Test"
    if not unreal.EditorAssetLibrary.does_asset_exist(object_path):
        unreal.EditorLevelLibrary.new_level(object_path)
        log("CREATE " + object_path)
    unreal.EditorLevelLibrary.load_level(object_path)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    floor = next((actor for actor in actors if isinstance(actor, unreal.StaticMeshActor)), None)
    if not floor:
        floor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -100), unreal.Rotator(0, 0, 0))
    floor.set_actor_scale3d(unreal.Vector(20, 20, 0.1))
    floor_component = floor.get_component_by_class(unreal.StaticMeshComponent)
    if not floor_component:
        raise RuntimeError("Phase 3 floor has no StaticMeshComponent")
    floor_component.set_static_mesh(unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube"))
    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    if len(starts) < 2:
        for location in [unreal.Vector(-500, 0, 100), unreal.Vector(500, 0, 100)]:
            unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, location, unreal.Rotator(0, 0, 0))
    if not any(isinstance(actor, unreal.DirectionalLight) for actor in unreal.EditorLevelLibrary.get_all_level_actors()):
        light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(-45, -35, 0))
        light.set_actor_rotation(unreal.Rotator(-45, -35, 0), False)
    unreal.EditorLevelLibrary.save_current_level()
    log("MAP_SAVED " + object_path)
    return object_path


def create_phase4_map():
    object_path = MAP_PATH + "/L_Phase4MovementTest"
    if not unreal.EditorAssetLibrary.does_asset_exist(object_path):
        unreal.EditorLevelLibrary.new_level(object_path)
    current_world = unreal.EditorLevelLibrary.get_editor_world()
    current_package = current_world.get_outermost().get_name() if current_world else ""
    if current_package != object_path:
        unreal.EditorLevelLibrary.load_level(object_path)

    floor_material = load_or_create_dev_material("M_Phase4Floor", (0.08, 0.10, 0.14))
    wall_material = load_or_create_dev_material("M_Phase4Wall", (0.12, 0.28, 0.45))
    obstacle_material = load_or_create_dev_material("M_Phase4Obstacle", (0.75, 0.25, 0.08))
    mantle_material = load_or_create_dev_material("M_Phase4Mantle", (0.38, 0.16, 0.55))
    safety_material = load_or_create_dev_material("M_Phase4Safety", (0.18, 0.18, 0.20))

    def ensure_cube(label, location, scale, material=None):
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        actor = next((candidate for candidate in actors if candidate.get_actor_label() == label), None)
        if not actor:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(0, 0, 0))
            actor.set_actor_label(label)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        component.set_static_mesh(unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube"))
        actor.set_actor_location(location, False, False)
        actor.set_actor_scale3d(scale)
        if material:
            component.set_material(0, material)
        return actor

    ensure_cube("Phase4_LongSprintLane", unreal.Vector(0, 0, -100), unreal.Vector(80, 30, 0.1), floor_material)
    ensure_cube("Phase4_SlideObstacle", unreal.Vector(700, 0, 25), unreal.Vector(1.2, 4, 1.25), obstacle_material)
    ensure_cube("Phase4_VaultObstacle", unreal.Vector(1100, 0, 50), unreal.Vector(1.0, 4, 2.0), obstacle_material)
    ensure_cube("Phase4_MantleLedge", unreal.Vector(1500, 0, 120), unreal.Vector(1.0, 4, 3.0), mantle_material)
    ensure_cube("Phase4_LeftWall", unreal.Vector(800, 450, 180), unreal.Vector(25, 0.4, 3.0), wall_material)
    ensure_cube("Phase4_RightWall", unreal.Vector(800, -450, 180), unreal.Vector(25, 0.4, 3.0), wall_material)
    ensure_cube("Phase4_NorthSafetyWall", unreal.Vector(0, 2400, 250), unreal.Vector(80, 0.4, 5.0), safety_material)
    ensure_cube("Phase4_SouthSafetyWall", unreal.Vector(0, -2400, 250), unreal.Vector(80, 0.4, 5.0), safety_material)
    ensure_cube("Phase4_WestSafetyWall", unreal.Vector(-3900, 0, 250), unreal.Vector(0.4, 30, 5.0), safety_material)
    ensure_cube("Phase4_EastSafetyWall", unreal.Vector(3900, 0, 250), unreal.Vector(0.4, 30, 5.0), safety_material)

    def ensure_actor(label, actor_class, location, rotation):
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        actor = next((candidate for candidate in actors if candidate.get_actor_label() == label and isinstance(candidate, actor_class)), None)
        if not actor:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
            actor.set_actor_label(label)
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)
        return actor

    def ensure_label(label, text, location, rotation=unreal.Rotator(0, 180, 0), scale=2.0):
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        actor = next((candidate for candidate in actors if candidate.get_actor_label() == label and isinstance(candidate, unreal.TextRenderActor)), None)
        if not actor:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TextRenderActor, location, rotation)
            actor.set_actor_label(label)
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        component.set_editor_property("text", text)
        component.set_editor_property("text_render_color", unreal.Color(255, 235, 120, 255))
        try:
            component.set_editor_property("horizontal_alignment", 1)
        except Exception:
            pass
        return actor

    directional = ensure_actor("Phase4_DirectionalLight", unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(-45, -35, 0))
    directional_component = directional.get_component_by_class(unreal.DirectionalLightComponent)
    directional_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    directional_component.set_editor_property("intensity", 8.0)

    sky_light = ensure_actor("Phase4_SkyLight", unreal.SkyLight, unreal.Vector(0, 0, 300), unreal.Rotator(0, 0, 0))
    sky_component = sky_light.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    if hasattr(sky_component, "set_editor_property"):
        try:
            sky_component.set_editor_property("real_time_capture", True)
        except Exception:
            pass

    ensure_actor("Phase4_SkyAtmosphere", unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    starts = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(actor, unreal.PlayerStart)]
    if len(starts) < 2:
        for location in [unreal.Vector(-2500, 0, 100), unreal.Vector(-2500, 300, 100)]:
            unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, location, unreal.Rotator(0, 0, 0))
    else:
        starts[0].set_actor_location(unreal.Vector(-2500, 0, 100), False, False)
        starts[0].set_actor_rotation(unreal.Rotator(0, 0, 0), False)
        starts[1].set_actor_location(unreal.Vector(-2500, 300, 100), False, False)
        starts[1].set_actor_rotation(unreal.Rotator(0, 0, 0), False)

    ensure_label("Phase4_LabelSprint", "SPRINT  SHIFT + W", unreal.Vector(-1900, 0, 220))
    ensure_label("Phase4_LabelSlide", "SLIDE  CTRL WHILE SPRINTING", unreal.Vector(500, 0, 220), scale=1.5)
    ensure_label("Phase4_LabelVault", "VAULT  JUMP AT LOW OBSTACLE", unreal.Vector(1050, 0, 260), scale=1.5)
    ensure_label("Phase4_LabelMantle", "MANTLE  JUMP AT HIGH LEDGE", unreal.Vector(1450, 0, 380), scale=1.5)
    ensure_label("Phase4_LabelWallRun", "WALL RUN  SPRINT + JUMP ALONG WALL", unreal.Vector(800, 650, 260), scale=1.5)
    ensure_label("Phase4_LabelWallJump", "WALL JUMP  SPACE DURING WALL RUN", unreal.Vector(800, 800, 220), scale=1.5)
    unreal.EditorLevelLibrary.save_current_level()
    log("MAP_SAVED " + object_path)
    return object_path


def main():
    move, look, jump, sprint, crouch = configure_input_actions()
    context = configure_mapping_context(move, look, jump, sprint, crouch)
    character_bp, game_mode_bp, _, game_mode_class = configure_blueprints(context, move, look, jump, sprint, crouch)
    map_path = create_test_map()
    phase4_map_path = create_phase4_map()
    log("DONE " + character_bp.get_path_name() + " " + game_mode_bp.get_path_name() + " " + map_path + " " + phase4_map_path)


main()
