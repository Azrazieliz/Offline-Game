#pragma once

/** Delivers Android media-volume notifications through UE's existing receiver on the game thread. */
namespace OGAndroidMediaVolumeBridge
{
    void Start();
    void Stop();
}
