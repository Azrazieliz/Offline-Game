import unreal

MAP_PATH = "/Game/Maps/StartingWorld"

def log(msg):
    unreal.log("[OfflineGame StartingWorld] " + msg)

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    if not unreal.EditorAssetLibrary.delete_asset(MAP_PATH):
        raise RuntimeError("Could not replace existing StartingWorld map asset.")

if not level.new_level(MAP_PATH):
    raise RuntimeError("Failed to create StartingWorld level.")

world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    raise RuntimeError("Editor world is unavailable after level creation.")

def spawn(script_name, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), label=None):
    cls = unreal.load_class(None, "/Script/OfflineGame." + script_name)
    if cls is None:
        raise RuntimeError("Missing class /Script/OfflineGame." + script_name)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls,
        unreal.Vector(*location),
        unreal.Rotator(*rotation))
    if actor is None:
        raise RuntimeError("Failed to spawn " + script_name)
    if label:
        actor.set_actor_label(label)
    return actor

def tag_world_presentation(actor):
    if actor is None:
        return
    tags = list(actor.get_editor_property("tags") or [])
    tag = unreal.Name("OG.WorldPresentation")
    if tag not in tags:
        tags.append(tag)
        actor.set_editor_property("tags", tags)


def spawn_engine(cls, location=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0), label=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        cls,
        unreal.Vector(*location),
        unreal.Rotator(*rotation))
    if actor is None:
        raise RuntimeError("Failed to spawn " + str(cls))
    if label:
        actor.set_actor_label(label)
    return actor

region_actor = spawn(
    "OGStartingRegionGenerator",
    (0.0, 0.0, 0.0),
    (0.0, 0.0, 0.0),
    "Starting Region - Deterministic Presentation")
tag_world_presentation(region_actor)

spawn_engine(
    unreal.PlayerStart,
    (0.0, 0.0, 350.0),
    (-10.0, 35.0, 0.0),
    "World Mode Player Start")

sun = spawn_engine(
    unreal.DirectionalLight,
    (0.0, 0.0, 4000.0),
    (-38.0, -28.0, 0.0),
    "Cold Late-Afternoon Key")
tag_world_presentation(sun)
sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
if sun_comp:
    try:
        sun_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception as exc:
        log("DirectionalLight mobility skipped: %s" % exc)
    for name, value in [
        ("intensity", 5.2),
        ("light_color", unreal.Color(214, 220, 214, 255)),
        ("atmosphere_sun_light", True),
        ("cast_shadows", True),
    ]:
        try:
            sun_comp.set_editor_property(name, value)
        except Exception as exc:
            log("DirectionalLight property %s skipped: %s" % (name, exc))

sky = spawn_engine(
    unreal.SkyLight,
    (0.0, 0.0, 1800.0),
    (0.0, 0.0, 0.0),
    "Washed Ambient Sky")
tag_world_presentation(sky)
sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
if sky_comp:
    try:
        sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception as exc:
        log("SkyLight mobility skipped: %s" % exc)
    for name, value in [
        ("intensity", 0.72),
        ("real_time_capture", True),
    ]:
        try:
            sky_comp.set_editor_property(name, value)
        except Exception as exc:
            log("SkyLight property %s skipped: %s" % (name, exc))

atmosphere = spawn_engine(
    unreal.SkyAtmosphere,
    (0.0, 0.0, 0.0),
    (0.0, 0.0, 0.0),
    "Starting World Atmosphere")
tag_world_presentation(atmosphere)


sky_dome = spawn_engine(
    unreal.StaticMeshActor,
    (0.0, 0.0, 0.0),
    (0.0, 0.0, 0.0),
    "Mobile Sky Dome")
tag_world_presentation(sky_dome)
sky_dome.set_actor_scale3d(unreal.Vector(180.0, 180.0, 180.0))
sky_dome_comp = sky_dome.static_mesh_component
sky_mesh = unreal.load_asset("/Engine/EngineSky/SM_SkySphere.SM_SkySphere")
sky_material = unreal.load_asset(
    "/Engine/EngineSky/SkyAtmosphere/SkyAtmosphere_MaterialSkyDome."
    "SkyAtmosphere_MaterialSkyDome")
if sky_dome_comp and sky_mesh and sky_material:
    sky_dome_comp.set_static_mesh(sky_mesh)
    sky_dome_comp.set_material(0, sky_material)
    sky_dome_comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    try:
        sky_dome_comp.set_editor_property("cast_shadow", False)
    except Exception:
        pass
    log("Mobile SkyAtmosphere-compatible sky dome configured.")
else:
    raise RuntimeError("Required engine mobile sky assets are unavailable.")

fog = spawn_engine(
    unreal.ExponentialHeightFog,
    (0.0, 0.0, 150.0),
    (0.0, 0.0, 0.0),
    "Lowland Melancholic Haze")
tag_world_presentation(fog)
fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
if fog_comp:
    for name, value in [
        ("fog_density", 0.018),
        ("fog_height_falloff", 0.22),
        ("fog_inscattering_luminance", unreal.LinearColor(0.34, 0.39, 0.39, 1.0)),
        ("enable_volumetric_fog", False),
    ]:
        try:
            fog_comp.set_editor_property(name, value)
        except Exception as exc:
            log("Fog property %s skipped: %s" % (name, exc))

game_mode = unreal.load_class(None, "/Script/OfflineGame.OGWorldPresentationGameMode")
settings = world.get_world_settings()
if game_mode and settings:
    set_ok = False
    for prop in ("default_game_mode", "default_game_mode_class"):
        try:
            settings.set_editor_property(prop, game_mode)
            set_ok = True
            log("WorldSettings game mode set through " + prop)
            break
        except Exception:
            pass
    if not set_ok:
        log("WorldSettings override unavailable; project GlobalDefaultGameMode will be authoritative.")

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Failed to save StartingWorld level.")

unreal.EditorAssetLibrary.save_directory("/Game/Maps", only_if_is_dirty=False, recursive=True)
log("StartingWorld created and saved successfully.")
