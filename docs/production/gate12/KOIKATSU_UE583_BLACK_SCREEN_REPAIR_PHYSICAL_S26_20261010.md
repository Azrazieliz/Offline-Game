# Gate 12 — physical S26 Ultra black-screen repair: experimental Koikatsu fixture

**2026-10-10 · Engineering fixture only · NOT Gate 11 ARTIFACT_READY / NOT full DEVICE_VALIDATED.**

## Starting defect: actual user-recorded failure

The user-provided 45.58-second Android video `814050.mp4` (SHA256 `17cfce6ffda408724ba92730bf92b2007a4910a8c0fa29d5e49cdc20d2197865`) shows Unreal splash followed by a persistent black screen and partial white loading shape. Earlier native S26 screenshots confirm this. The previous `install PASS` / `launch PASS` must not be equated with working runtime.

We pulled a **real Unreal log from the S26 Ultra app data**, `Saved/Gate12Koikatsu/device_log_from_user_video_20261010.log` (133,961 bytes): the correct QA map loaded in ~0.04 sec, runtime `GameModeBase` initialized, but `LogGameMode: FindPlayerStart: PATHS NOT DEFINED or NO PLAYERSTART with positive rating` appeared.

A live UE 5.8.3 Python commandlet inspection of the saved real QA map proved that the camera was accidentally pointing at `pitch=-45, yaw=180, roll=174` (wrong Python Rotator positional argument order), with **zero PlayerStarts**. Four actual skeletal actors were placed at x=0, 260, 520, 780 cm as a split diagnostic layout.

## Non-destructive applied fix

1. In isolated worktree `D:\UnrealProjects\OfflineGame_Gate12_Koikatsu_20261010`, corrected camera orientation using **explicit named Unreal.Rotator fields**, added real PlayerStart, front key light, and corrected directional-light orientation.
2. Saved and verified the actual map. First corrected build physically displayed a body and **separate** face/hair meshes on S26 — evidence `Saved/Gate12Koikatsu/s26_fixed_camera_10s.png`, SHA256 `0F15FFD0695FB0D1D0EFBB7CF3D9CCEDBB924CE5405DC6CA1A0F490FBEC0BE38`.
3. Moved **four real imported skeletal components** to the same origin with reference-pose dimensions ~154 cm body, 31 cm face, 34 cm hair, 129 cm outfit. Preserved their actual .uasset material, texture and morph data. Changed camera and PlayerStart to (0,190,108) cm facing yaw -90°.
4. Android ASTC full recook **569/569** / engine result 0; BuildCookRun and Gradle successful / AutomationTool ExitCode 0. Installed only updated separate fixture package `com.azrazieliz.gate12fixture` through `adb install -r`, ADB Success; original `com.azrazieliz.OfflineGame` untouched.
5. Real S26 Ultra **assembled-character screenshot** `Saved/Gate12Koikatsu/s26_assembled_real_character.png`, SHA256 `F706DE4231A5B99D1CFC130ED69ED6AF25DEE2793C78D89A180FD86F33318D26`. Shows the body, face and hair geometrically aligned in the *actual Android runtime*, **not** a generated image. The old black-screen obstruction is corrected.
6. Real 8-second Android `screenrecord` saved as `Saved/Gate12Koikatsu/s26_assembled_motion_check_8sec.mp4`, SHA256 `81EA09EB01FADBE9C8723604AC0CD9C0F288DD2630DA49B5E064D0812164E43B`, size 3,432,918 bytes. App remained foreground and alive throughout. Device PSS ~577,653 KB, RSS ~730,008 KB during observation. This is a **static scene**, not proof of skeletal animation.
7. Final assembled Android APK `Saved/Gate12Koikatsu/IsolatedAndroidPackage/Android_ASTC/OfflineGame-arm64.apk`, size **168,040,957 bytes**, SHA256 **`364711F0075F6EB6CE0F28E5ECE2098F4F572CEBF1E4E4E2082D356A7092B8C7`**. Previous black-screen APK and camera-fixed-but-unassembled APK separately preserved inside `Saved/Gate12Koikatsu`; exact older hashes in earlier reports.
8. At end, force-stopped **only** experimental app, restored ChatGPT to foreground, verified fixture remains installed, existing original game remains installed, zero Gate12 UE/Blender processes. Frozen original checkout is clean on commit/tag `3f54f07cacbf193e160d0be15a7be5a7bc730c3b`. Existing `Saved/OfflineGame/WorldState.db` SHA256 unchanged `4DBB961D45823763EBCE7E16DC62B1102B32B8FFD46003AB61AB05F0EF83554A`.

## Independent acceptance

| Check | Verdict |
|---|---|
| UE 5.8.3 four real split skeletal .uassets imported | **PASS** (engineering subset only) |
| Native skeletons / static model bounds | **PASS** (basic import/reference pose; 124 and 126 bone differences unresolved) |
| Facial morph-target existence in Unreal | **PASS** (3 morphs per relevant face mesh) |
| Material translucent blend-mode existence | **PASS** (source/material property, not visual fidelity) |
| Correct camera orientation and spawn, saved map | **PASS** |
| Physical S26 Ultra ASTC cook / APK / install | **PASS** |
| Physical S26 Ultra static body/face/hair renderer | **PASS** (screen-confirmed) |
| Material textures and clothing fidelity | **FAIL** (predominantly white with incomplete clothing appearance) |
| Animated skeletal skin deformation | **NOT TESTED** (static scene) |
| Facial expression playback | **NOT TESTED** |
| Full Koikatsu original vs reduced morphs/texture parity | **NOT TESTED / NOT ACCEPTED** |
| Performance frame-times / FPS, game controls or production UX | **NOT TESTED** |
| Genuine production content & Gate11 approval | **NOT AUTHORIZED** |
| Frozen Foundation, existing game and player data preservation | **PASS** |

**Important:** This rectifies the specific device black-screen plus visibly detached test meshes. It does **not** certify a complete gameplay system or production-ready character. No PR #16 merge or frozen-Foundation redesign occurred.
