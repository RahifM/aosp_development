/*
 * Copyright 2015, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaSync-JNI"
#include <utils/Log.h>

#include "android_media_MediaSync.h"

#include "android_media_AudioTrack.h"
#include "android_media_SyncSettings.h"
#include "android_runtime/AndroidRuntime.h"
#include "android_runtime/android_view_Surface.h"
#include "jni.h"
#include "JNIHelp.h"

#include <gui/Surface.h>

#include <media/AudioTrack.h>
#include <media/stagefright/MediaClock.h>
#include <media/stagefright/MediaSync.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AString.h>

#include <nativehelper/ScopedLocalRef.h>

namespace android {

struct fields_t {
    jfieldID context;
    jfieldID mediaTimestampMediaTimeUsID;
    jfieldID mediaTimestampNanoTimeID;
    jfieldID mediaTimestampClockRateID;
};

static fields_t gFields;
static SyncSettings::fields_t gSyncSettingsFields;

////////////////////////////////////////////////////////////////////////////////

JMediaSync::JMediaSync() {
    mSync = MediaSync::create();
}

JMediaSync::~JMediaSync() {
}

status_t JMediaSync::configureSurface(const sp<IGraphicBufferProducer> &bufferProducer) {
    return mSync->configureSurface(bufferProducer);
}

status_t JMediaSync::configureAudioTrack(
        const sp<AudioTrack> &audioTrack,
        int32_t nativeSampleRateInHz) {
    return mSync->configureAudioTrack(audioTrack, nativeSampleRateInHz);
}

status_t JMediaSync::createInputSurface(
        sp<IGraphicBufferProducer>* bufferProducer) {
    return mSync->createInputSurface(bufferProducer);
}

status_t JMediaSync::setPlaybackRate(float rate) {
    return mSync->setPlaybackRate(rate);
}

sp<const MediaClock> JMediaSync::getMediaClock() {
    return mSync->getMediaClock();
}

status_t JMediaSync::updateQueuedAudioData(
        int sizeInBytes, int64_t presentationTimeUs) {
    return mSync->updateQueuedAudioData(sizeInBytes, presentationTimeUs);
}

status_t JMediaSync::getPlayTimeForPendingAudioFrames(int64_t *outTimeUs) {
    return mSync->getPlayTimeForPendingAudioFrames(outTimeUs);
}

}  // namespace android

////////////////////////////////////////////////////////////////////////////////

using namespace android;

static sp<JMediaSync> setMediaSync(JNIEnv *env, jobject thiz, const sp<JMediaSync> &sync) {
    sp<JMediaSync> old = (JMediaSync *)env->GetLongField(thiz, gFields.context);
    if (sync != NULL) {
        sync->incStrong(thiz);
    }
    if (old != NULL) {
        old->decStrong(thiz);
    }

    env->SetLongField(thiz, gFields.context, (jlong)sync.get());

    return old;
}

static sp<JMediaSync> getMediaSync(JNIEnv *env, jobject thiz) {
    return (JMediaSync *)env->GetLongField(thiz, gFields.context);
}

static void android_media_MediaSync_release(JNIEnv *env, jobject thiz) {
    setMediaSync(env, thiz, NULL);
}

static void throwExceptionAsNecessary(
        JNIEnv *env, status_t err, const char *msg = NULL) {
    switch (err) {
        case NO_ERROR:
            break;

        case BAD_VALUE:
            jniThrowException(env, "java/lang/IllegalArgumentException", msg);
            break;

        case NO_INIT:
        case INVALID_OPERATION:
        default:
            if (err > 0) {
                break;
            }
            AString msgWithErrorCode(msg);
            msgWithErrorCode.append(" error:");
            msgWithErrorCode.append(err);
            jniThrowException(env, "java/lang/IllegalStateException", msgWithErrorCode.c_str());
            break;
    }
}

static void android_media_MediaSync_native_configureSurface(
        JNIEnv *env, jobject thiz, jobject jsurface) {
    ALOGV("android_media_MediaSync_configureSurface");

    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return;
    }

    sp<IGraphicBufferProducer> bufferProducer;
    if (jsurface != NULL) {
        sp<Surface> surface(android_view_Surface_getSurface(env, jsurface));
        if (surface != NULL) {
            bufferProducer = surface->getIGraphicBufferProducer();
        } else {
            throwExceptionAsNecessary(env, BAD_VALUE, "The surface has been released");
            return;
        }
    }

    status_t err = sync->configureSurface(bufferProducer);

    if (err == INVALID_OPERATION) {
        throwExceptionAsNecessary(
                env, INVALID_OPERATION, "Surface has already been configured");
    } if (err != NO_ERROR) {
        AString msg("Failed to connect to surface with error ");
        msg.append(err);
        throwExceptionAsNecessary(env, BAD_VALUE, msg.c_str());
    }
}

static void android_media_MediaSync_native_configureAudioTrack(
        JNIEnv *env, jobject thiz, jobject jaudioTrack, jint nativeSampleRateInHz) {
    ALOGV("android_media_MediaSync_configureAudioTrack");

    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return;
    }

    sp<AudioTrack> audioTrack;
    if (jaudioTrack != NULL) {
        audioTrack = android_media_AudioTrack_getAudioTrack(env, jaudioTrack);
        if (audioTrack == NULL) {
            throwExceptionAsNecessary(env, BAD_VALUE, "The audio track has been released");
            return;
        }
    }

    status_t err = sync->configureAudioTrack(audioTrack, nativeSampleRateInHz);

    if (err == INVALID_OPERATION) {
        throwExceptionAsNecessary(
                env, INVALID_OPERATION, "Audio track has already been configured");
    } if (err != NO_ERROR) {
        AString msg("Failed to configure audio track with error ");
        msg.append(err);
        throwExceptionAsNecessary(env, BAD_VALUE, msg.c_str());
    }
}

static jobject android_media_MediaSync_createInputSurface(
        JNIEnv* env, jobject thiz) {
    ALOGV("android_media_MediaSync_createInputSurface");

    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return NULL;
    }

    // Tell the MediaSync that we want to use a Surface as input.
    sp<IGraphicBufferProducer> bufferProducer;
    status_t err = sync->createInputSurface(&bufferProducer);
    if (err != NO_ERROR) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return NULL;
    }

    // Wrap the IGBP in a Java-language Surface.
    return android_view_Surface_createFromIGraphicBufferProducer(env,
            bufferProducer);
}

static void android_media_MediaSync_native_updateQueuedAudioData(
        JNIEnv *env, jobject thiz, jint sizeInBytes, jlong presentationTimeUs) {
    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return;
    }

    status_t err = sync->updateQueuedAudioData(sizeInBytes, presentationTimeUs);
    if (err != NO_ERROR) {
        throwExceptionAsNecessary(env, err);
        return;
    }
}

static jboolean android_media_MediaSync_native_getTimestamp(
        JNIEnv *env, jobject thiz, jobject timestamp) {
    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return JNI_FALSE;
    }

    sp<const MediaClock> mediaClock = sync->getMediaClock();
    if (mediaClock == NULL) {
        return JNI_FALSE;
    }

    int64_t nowUs = ALooper::GetNowUs();
    int64_t mediaUs = 0;
    if (mediaClock->getMediaTime(nowUs, &mediaUs) != OK) {
        return JNI_FALSE;
    }

    env->SetLongField(timestamp, gFields.mediaTimestampMediaTimeUsID,
            (jlong)mediaUs);
    env->SetLongField(timestamp, gFields.mediaTimestampNanoTimeID,
            (jlong)(nowUs * 1000));
    env->SetFloatField(timestamp, gFields.mediaTimestampClockRateID,
            (jfloat)mediaClock->getPlaybackRate());
    return JNI_TRUE;
}

static jlong android_media_MediaSync_native_getPlayTimeForPendingAudioFrames(
        JNIEnv *env, jobject thiz) {
    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
    }

    int64_t playTimeUs = 0;
    status_t err = sync->getPlayTimeForPendingAudioFrames(&playTimeUs);
    if (err != NO_ERROR) {
        throwExceptionAsNecessary(env, err);
    }
    return (jlong)playTimeUs;
}

static void
android_media_MediaSync_setSyncSettings(JNIEnv *env, jobject thiz, jobject settings)
{
    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return;
    }

    SyncSettings scs;
    scs.fillFromJobject(env, gSyncSettingsFields, settings);
    ALOGV("setSyncSettings: %d:%d %d:%d %d:%f %d:%f",
            scs.syncSourceSet, scs.syncSource,
            scs.audioAdjustModeSet, scs.audioAdjustMode,
            scs.toleranceSet, scs.tolerance,
            scs.frameRateSet, scs.frameRate);

    // TODO: pass sync settings to mediasync when it supports it
}

static jobject
android_media_MediaSync_getSyncSettings(JNIEnv *env, jobject thiz)
{
    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return NULL;
    }

    SyncSettings scs;
    scs.syncSource = 0; // SYNC_SOURCE_DEFAULT
    scs.audioAdjustMode = 0; // AUDIO_ADJUST_MODE_DEFAULT
    scs.tolerance = 0.f;
    scs.frameRate = 0.f;

    // TODO: get this from mediaplayer when it supports it
    // process_media_player_call(
    //        env, thiz, mp->getSyncSettings(&scs), NULL, NULL);
    ALOGV("getSyncSettings: %d %d %f %f",
            scs.syncSource, scs.audioAdjustMode, scs.tolerance, scs.frameRate);

    scs.syncSourceSet = true;
    scs.audioAdjustModeSet = true;
    scs.toleranceSet = true;
    scs.frameRateSet = false;

    return scs.asJobject(env, gSyncSettingsFields);
}

static void android_media_MediaSync_native_init(JNIEnv *env) {
    ScopedLocalRef<jclass> clazz(env, env->FindClass("android/media/MediaSync"));
    CHECK(clazz.get() != NULL);

    gFields.context = env->GetFieldID(clazz.get(), "mNativeContext", "J");
    CHECK(gFields.context != NULL);

    clazz.reset(env->FindClass("android/media/MediaTimestamp"));
    CHECK(clazz.get() != NULL);

    gFields.mediaTimestampMediaTimeUsID =
        env->GetFieldID(clazz.get(), "mediaTimeUs", "J");
    CHECK(gFields.mediaTimestampMediaTimeUsID != NULL);

    gFields.mediaTimestampNanoTimeID =
        env->GetFieldID(clazz.get(), "nanoTime", "J");
    CHECK(gFields.mediaTimestampNanoTimeID != NULL);

    gFields.mediaTimestampClockRateID =
        env->GetFieldID(clazz.get(), "clockRate", "F");
    CHECK(gFields.mediaTimestampClockRateID != NULL);

    gSyncSettingsFields.init(env);
}

static void android_media_MediaSync_native_setup(JNIEnv *env, jobject thiz) {
    sp<JMediaSync> sync = new JMediaSync();

    setMediaSync(env, thiz, sync);
}

static void android_media_MediaSync_native_setPlaybackRate(
        JNIEnv *env, jobject thiz, jfloat rate) {
    sp<JMediaSync> sync = getMediaSync(env, thiz);
    if (sync == NULL) {
        throwExceptionAsNecessary(env, INVALID_OPERATION);
        return;
    }

    status_t err = sync->setPlaybackRate(rate);
    if (err != NO_ERROR) {
        throwExceptionAsNecessary(env, err);
        return;
    }
}

static void android_media_MediaSync_native_finalize(JNIEnv *env, jobject thiz) {
    android_media_MediaSync_release(env, thiz);
}

static JNINativeMethod gMethods[] = {
    { "native_configureSurface",
      "(Landroid/view/Surface;)V",
      (void *)android_media_MediaSync_native_configureSurface },

    { "native_configureAudioTrack",
      "(Landroid/media/AudioTrack;I)V",
      (void *)android_media_MediaSync_native_configureAudioTrack },

    { "createInputSurface", "()Landroid/view/Surface;",
      (void *)android_media_MediaSync_createInputSurface },

    { "native_updateQueuedAudioData",
      "(IJ)V",
      (void *)android_media_MediaSync_native_updateQueuedAudioData },

    { "native_getTimestamp",
      "(Landroid/media/MediaTimestamp;)Z",
      (void *)android_media_MediaSync_native_getTimestamp },

    { "native_getPlayTimeForPendingAudioFrames",
      "()J",
      (void *)android_media_MediaSync_native_getPlayTimeForPendingAudioFrames },

    { "native_init", "()V", (void *)android_media_MediaSync_native_init },

    { "native_setup", "()V", (void *)android_media_MediaSync_native_setup },

    { "native_release", "()V", (void *)android_media_MediaSync_release },

    { "native_setPlaybackRate", "(F)V", (void *)android_media_MediaSync_native_setPlaybackRate },

    { "setSyncSettings", "(Landroid/media/SyncSettings;)V", (void *)android_media_MediaSync_setSyncSettings},

    { "getSyncSettings", "()Landroid/media/SyncSettings;", (void *)android_media_MediaSync_getSyncSettings},

    { "native_finalize", "()V", (void *)android_media_MediaSync_native_finalize },
};

int register_android_media_MediaSync(JNIEnv *env) {
    return AndroidRuntime::registerNativeMethods(
                   env, "android/media/MediaSync", gMethods, NELEM(gMethods));
}
