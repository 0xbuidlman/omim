#pragma once

#include <jni.h>

#include "../../../../../std/string.hpp"
#include "../../../../../std/shared_ptr.hpp"
#include "../../../../../geometry/point2d.hpp"
#include "../../../../../map/framework.hpp"

namespace jni
{
  /// @name Some examples of signature:
  /// "()V" - void function returning void;
  /// "(Ljava/lang/String;)V" - String function returning void;
  /// "com/mapswithme/maps/MapStorage$Index" - class MapStorage.Index
  //@{
  jmethodID GetJavaMethodID(JNIEnv * env, jobject obj, char const * fn, char const * sig);

  //jobject CreateJavaObject(JNIEnv * env, char const * klass, char const * sig, ...);
  //@}

  JNIEnv * GetEnv();
  JavaVM * GetJVM();

  string ToNativeString(JNIEnv * env, jstring str);
  inline string ToNativeString(jstring str)
  {
    return ToNativeString(GetEnv(), str);
  }

  jstring ToJavaString(JNIEnv * env, char const * s);
  inline jstring ToJavaString(JNIEnv * env, string const & s)
  {
    return ToJavaString(env, s.c_str());
  }

  string DescribeException();

  shared_ptr<jobject> make_global_ref(jobject obj);

  jobject GetNewParcelablePointD(JNIEnv * env, m2::PointD const & point);

  jobject GetNewPoint(JNIEnv * env, m2::PointD const & point);
  jobject GetNewPoint(JNIEnv * env, m2::PointI const & point);
  jobject GetNewAddressInfo(JNIEnv * env, Framework::AddressInfo const & adInfo, m2::PointD const & px);
}
