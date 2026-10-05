import unreal


def out(label, value):
    print("PHASE3_VALIDATE", label, value)


def key_name(mapping):
    return str(mapping.get_editor_property("key").get_editor_property("key_name"))


def main():
    expected = [
        "/Game/ArenaDuel/Input/IA_Move",
        "/Game/ArenaDuel/Input/IA_Look",
        "/Game/ArenaDuel/Input/IA_Jump",
        "/Game/ArenaDuel/Input/IMC_Gameplay",
        "/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter",
        "/Game/ArenaDuel/Game/BP_ArenaDuelGameMode",
        "/Game/ArenaDuel/Maps/L_Phase3Test",
    ]
    for path in expected:
        obj = unreal.load_object(None, path)
        if not obj:
            raise RuntimeError("Missing asset " + path)
        out("ASSET", path + " " + obj.get_class().get_name())

    move = unreal.load_object(None, expected[0])
    look = unreal.load_object(None, expected[1])
    jump = unreal.load_object(None, expected[2])
    out("VALUE_TYPES", str(move.get_editor_property("value_type")) + " " + str(look.get_editor_property("value_type")) + " " + str(jump.get_editor_property("value_type")))

    context = unreal.load_object(None, expected[3])
    mappings = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    out("MAPPING_COUNT", len(mappings))
    for mapping in mappings:
        action = mapping.get_editor_property("action").get_name()
        mods = mapping.get_editor_property("modifiers")
        details = []
        for modifier in mods:
            name = modifier.get_class().get_name()
            if name == "InputModifierSwizzleAxis":
                details.append(name + ":" + str(modifier.get_editor_property("order")))
            elif name == "InputModifierNegate":
                details.append(name + ":" + str((modifier.get_editor_property("x"), modifier.get_editor_property("y"), modifier.get_editor_property("z"))))
            else:
                details.append(name)
        out("MAPPING", action + " " + key_name(mapping) + " " + str(details))

    char_bp = unreal.load_object(None, expected[4])
    char_class = unreal.BlueprintEditorLibrary.generated_class(char_bp)
    char_cdo = unreal.get_default_object(char_class)
    out("CHARACTER_DEFAULTS", str((
        char_cdo.get_editor_property("default_mapping_context").get_path_name(),
        char_cdo.get_editor_property("move_action").get_path_name(),
        char_cdo.get_editor_property("look_action").get_path_name(),
        char_cdo.get_editor_property("jump_action").get_path_name(),
    )))
    native_character = unreal.load_class(None, "/Script/ArenaDuel.ArenaDuelCharacter")
    parent_fn = getattr(unreal.BlueprintEditorLibrary, "get_blueprint_parent_class", None)
    if parent_fn:
        out("CHARACTER_PARENT", parent_fn(char_bp).get_path_name())
    else:
        out("CHARACTER_PARENT", "native class supplied to BlueprintFactory: " + native_character.get_path_name())

    game_bp = unreal.load_object(None, expected[5])
    game_class = unreal.BlueprintEditorLibrary.generated_class(game_bp)
    game_cdo = unreal.get_default_object(game_class)
    pawn_class = game_cdo.get_editor_property("default_pawn_class")
    native_game_mode = unreal.load_class(None, "/Script/ArenaDuel.ArenaDuelGameMode")
    if parent_fn:
        out("GAMEMODE_PARENT", parent_fn(game_bp).get_path_name())
    else:
        out("GAMEMODE_PARENT", "native class supplied to BlueprintFactory: " + native_game_mode.get_path_name())
    out("GAMEMODE_DEFAULT_PAWN", pawn_class.get_path_name() if pawn_class else "None")

    unreal.EditorLevelLibrary.load_level(expected[6])
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    floors = [a for a in actors if isinstance(a, unreal.StaticMeshActor) and a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property("static_mesh")]
    lights = [a for a in actors if isinstance(a, unreal.DirectionalLight)]
    out("MAP_ACTORS", str((len(starts), len(floors), len(lights))))
    if len(starts) < 2 or not floors or not lights:
        raise RuntimeError("Phase 3 test map contents are incomplete")
    out("RESULT", "PASS")


main()
