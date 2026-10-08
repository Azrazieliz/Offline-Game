#include "Runtime/OGAndroidMediaVolumeBridge.h"
#include "CoreMinimal.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Containers/Ticker.h"
#include "OfflineGame.h"
#include <atomic>

namespace
{
    // Broadcasts may arrive before game initialization or while backgrounded.
    // The current media-volume state is all UE needs when the game thread resumes.
    std::atomic<int32> PendingVolume{-1};
    FTSTicker::FDelegateHandle VolumeTicker;

    bool DeliverVolume(float)
    {
        check(IsInGameThread());
        const int32 Volume = PendingVolume.exchange(-1, std::memory_order_acq_rel);
        if (Volume < 0) return true;
        JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (!Env)
        {
            int32 Empty = -1;
            PendingVolume.compare_exchange_strong(Empty, Volume);
            return true;
        }
        jclass Receiver = FAndroidApplication::FindJavaClass("com/epicgames/unreal/VolumeReceiver");
        jmethodID Callback = Receiver ? Env->GetStaticMethodID(Receiver, "volumeChanged", "(I)V") : nullptr;
        if (Callback)
            Env->CallStaticVoidMethod(Receiver, Callback, static_cast<jint>(Volume));
        else
            UE_LOG(LogOfflineGame, Error, TEXT("Android media-volume receiver callback is unavailable."));
        if (Env->ExceptionCheck())
        {
            Env->ExceptionClear();
            UE_LOG(LogOfflineGame, Error, TEXT("Android media-volume receiver callback raised an exception."));
        }
        if (Receiver) Env->DeleteLocalRef(Receiver);
        return true;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_epicgames_unreal_GameActivity_nativeOGMediaVolumeChanged(JNIEnv*, jclass, jint Volume)
{
    // Never call UE's FVolumeReceiver::volumeChanged/FAppTime from the UI thread.
    PendingVolume.store(FMath::Max(0, static_cast<int32>(Volume)), std::memory_order_release);
}
#endif

void OGAndroidMediaVolumeBridge::Start()
{
#if PLATFORM_ANDROID
    if (!VolumeTicker.IsValid())
        VolumeTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&DeliverVolume));
#endif
}

void OGAndroidMediaVolumeBridge::Stop()
{
#if PLATFORM_ANDROID
    if (VolumeTicker.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(VolumeTicker);
        VolumeTicker.Reset();
    }
#endif
}
