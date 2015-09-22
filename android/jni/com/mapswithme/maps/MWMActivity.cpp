#include "Framework.hpp"

#include "../core/jni_helper.hpp"

#include "../platform/Platform.hpp"

#include "../../../nv_event/nv_event.hpp"
#include "../../../../../base/logging.hpp"

#include "../../../../../map/country_status_display.hpp"
#define LONG_CLICK_LENGTH_SEC 1.0
#define SHORT_CLICK_LENGTH_SEC 0.1
////////////////////////////////////////////////////////////////////////////////////////////
extern "C"
{

  void CallClickListener(shared_ptr<jobject> obj, int x, int y, double seconds)
  {
    LOG(LDEBUG, ("seconds", seconds));
    JNIEnv * env = jni::GetEnv();
    jmethodID methodID = 0;
    if (seconds > LONG_CLICK_LENGTH_SEC)
    {
      methodID = jni::GetJavaMethodID(env, *obj.get(), "onLongClick", "(II)V");
    }
    else if(seconds <= SHORT_CLICK_LENGTH_SEC)
    {
      methodID = jni::GetJavaMethodID(env, *obj.get(), "onClick", "(II)V");
    }
    if (methodID != 0)
    {
      env->CallVoidMethod(*obj.get(), methodID, x, y);
    }
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_MWMActivity_addOnLongClickListener(JNIEnv * env, jobject thiz, jobject l)
  {
    return g_framework->AddClickListener(bind(&CallClickListener,jni::make_global_ref(l), _1, _2, _3));
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_MWMActivity_addOnClickListener(JNIEnv * env, jobject thiz, jobject l)
  {
    return g_framework->AddClickListener(bind(&CallClickListener,jni::make_global_ref(l), _1, _2, _3));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_removeOnLongClickListener(int h)
  {
    g_framework->RemoveClickListener(h);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_removeOnClickListener(int h)
  {
    g_framework->RemoveClickListener(h);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeOnLocationError(JNIEnv * env, jobject thiz,
      int errorCode)
  {
    g_framework->OnLocationError(errorCode);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeLocationUpdated(JNIEnv * env, jobject thiz,
      jlong time, jdouble lat, jdouble lon, jfloat accuracy)
  {
    g_framework->OnLocationUpdated(time, lat, lon, accuracy);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeCompassUpdated(JNIEnv * env, jobject thiz,
      jlong time, jdouble magneticNorth, jdouble trueNorth, jdouble accuracy)
  {
    g_framework->OnCompassUpdated(time, magneticNorth, trueNorth, accuracy);
  }

  JNIEXPORT jfloatArray JNICALL
  Java_com_mapswithme_maps_location_LocationService_nativeUpdateCompassSensor(
      JNIEnv * env, jobject thiz, jint ind, jfloatArray arr)
  {
    int const count = 3;

    // get Java array
    jfloat buffer[3];
    env->GetFloatArrayRegion(arr, 0, count, buffer);

    // get the result
    g_framework->UpdateCompassSensor(ind, buffer);

    // pass result back to Java
    jfloatArray ret = (jfloatArray)env->NewFloatArray(count);
    env->SetFloatArrayRegion(ret, 0, count, buffer);
    return ret;
  }

  void CallOnDownloadCountryClicked(shared_ptr<jobject> const & obj,
                                    jmethodID methodID)
  {
    JNIEnv * env = jni::GetEnv();
    env->CallVoidMethod(*obj.get(), methodID);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeConnectDownloadButton(JNIEnv * env, jobject thiz)
  {
    CountryStatusDisplay * display = g_framework->GetCountryStatusDisplay();

    jmethodID methodID = jni::GetJavaMethodID(env, thiz, "OnDownloadCountryClicked", "()V");

    display->setDownloadListener(bind(&CallOnDownloadCountryClicked,
                                       jni::make_global_ref(thiz),
                                       methodID));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeDownloadCountry(JNIEnv * env, jobject thiz)
  {
    g_framework->GetCountryStatusDisplay()->downloadCountry();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeSetString(JNIEnv * env, jobject thiz, jstring name, jstring value)
  {
    g_framework->AddString(jni::ToNativeString(env, name),
                           jni::ToNativeString(env, value));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeStorageConnected(JNIEnv * env, jobject thiz)
  {
    android::Platform::Instance().OnExternalStorageStatusChanged(true);
    g_framework->AddLocalMaps();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeStorageDisconnected(JNIEnv * env, jobject thiz)
  {
    android::Platform::Instance().OnExternalStorageStatusChanged(false);
    g_framework->RemoveLocalMaps();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeScale(JNIEnv * env, jobject thiz, jdouble k)
  {
    g_framework->Scale(static_cast<double>(k));
  }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_MWMActivity_nativeIsInChina(JNIEnv * env, jobject thiz,
                                                       jdouble lat, jdouble lon)
  {
    char const * arr[] = { "cn", "hk", "mo" };

    string const code = g_framework->GetCountryCode(lat, lon);
    LOG(LDEBUG, ("Current country ISO code = ", code));

    for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
      if (code == arr[i])
        return true;

    return false;
  }
} // extern "C"
