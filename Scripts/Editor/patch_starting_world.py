import unreal

MAP_PATH = "/Game/Maps/StartingWorld"

def log(msg):
    unreal.log("[OfflineGame StartingWorld Patch] " + msg)


def tag_world_presentation(actor):
    if actor is None:
        return
    tags = list(actor.get_editor_property("tags") or [])
    tag = unreal.Name("OG.WorldPresentation")
    if tag not in tags:
        tags.append(tag)
        actor.set_editor_property("tags", tags)


level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level(MAP_PATH):
    raise RuntimeError("Failed to load StartingWorld.")

world = unreal.EditorLevelLibrary.get_editor_world()
if world is None:
    raise RuntimeError("StartingWorld editor world unavailable.")

actors = unreal.EditorLevelLibrary.get_all_level_actors()

for actor in actors:
    label = actor.get_actor_label()

    if label in (
        "Starting Region - Deterministic Presentation",
        "Cold Late-Afternoon Key",
        "Washed Ambient Sky",
        "Starting World Atmosphere",
        "Mobile Sky Dome",
        "Lowland Melancholic Haze",
    ):
        tag_world_presentation(actor)

    if isinstance(actor, unreal.PlayerStart):
        actor.set_actor_rotation(unreal.Rotator(-10.0, 35.0, 0.0), False)
        log("Updated PlayerStart rotation.")

    elif isinstance(actor, unreal.DirectionalLight):
        comp = actor.get_component_by_class(unreal.DirectionalLightComponent)
        if comp:
            comp.set_mobility(unreal.ComponentMobility.MOVABLE)
            comp.set_editor_property("intensity", 5.2)
            comp.set_editor_property("light_color", unreal.Color(214, 220, 214, 255))
            comp.set_editor_property("atmosphere_sun_light", True)
            comp.set_editor_property("cast_shadows", True)
            log("Directional light set Movable.")

    elif isinstance(actor, unreal.SkyLight):
        comp = actor.get_component_by_class(unreal.SkyLightComponent)
        if comp:
            comp.set_mobility(unreal.ComponentMobility.MOVABLE)
            comp.set_editor_property("intensity", 0.72)
            comp.set_editor_property("real_time_capture", True)
            log("Skylight set Movable.")

    elif isinstance(actor, unreal.ExponentialHeightFog):
        comp = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
        if comp:
            comp.set_editor_property("fog_density", 0.018)
            comp.set_editor_property("fog_height_falloff", 0.22)
            comp.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.34, 0.39, 0.39, 1.0))
            comp.set_editor_property("enable_volumetric_fog", False)
            log("Fog properties updated for UE 5.8.")


mobile_sky = None
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.get_actor_label() == "Mobile Sky Dome":
        mobile_sky = actor
        break

if mobile_sky is None:
    mobile_sky = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0))
    if mobile_sky is None:
        raise RuntimeError("Failed to spawn Mobile Sky Dome.")
    mobile_sky.set_actor_label("Mobile Sky Dome")

tag_world_presentation(mobile_sky)
mobile_sky.set_actor_scale3d(unreal.Vector(180.0, 180.0, 180.0))
mobile_sky_comp = mobile_sky.static_mesh_component
mobile_sky_mesh = unreal.load_asset("/Engine/EngineSky/SM_SkySphere.SM_SkySphere")
mobile_sky_material = unreal.load_asset(
    "/Engine/EngineSky/SkyAtmosphere/SkyAtmosphere_MaterialSkyDome."
    "SkyAtmosphere_MaterialSkyDome")
if not mobile_sky_comp or not mobile_sky_mesh or not mobile_sky_material:
    raise RuntimeError("Required engine mobile sky assets are unavailable.")
mobile_sky_comp.set_static_mesh(mobile_sky_mesh)
mobile_sky_comp.set_material(0, mobile_sky_material)
mobile_sky_comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
try:
    mobile_sky_comp.set_editor_property("cast_shadow", False)
except Exception:
    pass
log("Mobile SkyAtmosphere-compatible sky dome confirmed.")

game_mode = unreal.load_class(None, "/Script/OfflineGame.OGWorldPresentationGameMode")
settings = world.get_world_settings()
if game_mode and settings:
    settings.set_editor_property("default_game_mode", game_mode)
    log("WorldSettings game mode confirmed.")

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Failed to save patched StartingWorld.")

log("StartingWorld patched and saved successfully.")
