///////////////////////////////////////////////////////////////////////////////////
// DownloadUI
///////////////////////////////////////////////////////////////////////////////////

#include "Framework.hpp"

#include "../core/jni_helper.hpp"


extern "C"
{
  class IndexBinding
  {
  private:
    shared_ptr<jobject> m_self;

    jfieldID m_groupID;
    jfieldID m_countryID;
    jfieldID m_regionID;

    jobject object() const { return *m_self.get(); }

  public:
    IndexBinding(jobject self) : m_self(jni::make_global_ref(self))
    {
      jclass klass = jni::GetEnv()->GetObjectClass(object());

      m_groupID = jni::GetEnv()->GetFieldID(klass, "mGroup", "I");
      m_countryID = jni::GetEnv()->GetFieldID(klass, "mCountry", "I");
      m_regionID = jni::GetEnv()->GetFieldID(klass, "mRegion", "I");
    }

    int group() const
    {
      return jni::GetEnv()->GetIntField(object(), m_groupID);
    }

    int country() const
    {
      return jni::GetEnv()->GetIntField(object(), m_countryID);
    }

    int region() const
    {
      return jni::GetEnv()->GetIntField(object(), m_regionID);
    }

    storage::TIndex const toNative() const
    {
      return storage::TIndex(group(), country(), region());
    }

    static jobject toJava(storage::TIndex const & idx)
    {
      JNIEnv * env = jni::GetEnv();

      jclass klass = env->FindClass("com/mapswithme/maps/MapStorage$Index");
      ASSERT(klass, ());

      jmethodID methodID = env->GetMethodID(klass, "<init>", "(III)V");
      ASSERT(methodID, ());

      return env->NewObject(klass, methodID,
                            (jint)idx.m_group, (jint)idx.m_country, (jint)idx.m_region);
    }
  };

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_MapStorage_countriesCount(JNIEnv * env, jobject thiz, jobject idx)
  {
    return static_cast<jint>(g_framework->Storage().CountriesCount(IndexBinding(idx).toNative()));
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_MapStorage_countryName(JNIEnv * env, jobject thiz, jobject idx)
  {
    string const name = g_framework->Storage().CountryName(IndexBinding(idx).toNative());
    return env->NewStringUTF(name.c_str());
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_MapStorage_countryFlag(JNIEnv * env, jobject thiz, jobject idx)
  {
    string const name = g_framework->Storage().CountryFlag(IndexBinding(idx).toNative());
    return env->NewStringUTF(name.c_str());
  }

  JNIEXPORT jlong JNICALL
  Java_com_mapswithme_maps_MapStorage_countryLocalSizeInBytes(JNIEnv * env, jobject thiz, jobject idx)
  {
    return g_framework->Storage().CountrySizeInBytes(IndexBinding(idx).toNative()).first;
  }

  JNIEXPORT jlong JNICALL
  Java_com_mapswithme_maps_MapStorage_countryRemoteSizeInBytes(JNIEnv * env, jobject thiz, jobject idx)
  {
    return g_framework->Storage().CountrySizeInBytes(IndexBinding(idx).toNative()).second;
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_MapStorage_countryStatus(JNIEnv * env, jobject thiz, jobject idx)
  {
    return static_cast<jint>(g_framework->Storage().CountryStatus(IndexBinding(idx).toNative()));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapStorage_downloadCountry(JNIEnv * env, jobject thiz, jobject idx)
  {
    g_framework->Storage().DownloadCountry(IndexBinding(idx).toNative());
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapStorage_deleteCountry(JNIEnv * env, jobject thiz, jobject idx)
  {
    g_framework->Storage().DeleteCountry(IndexBinding(idx).toNative());
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_MapStorage_findIndexByName(JNIEnv * env, jobject thiz, jstring name)
  {
    char const * strCountry = env->GetStringUTFChars(name, 0);

    if (!strCountry)
      return IndexBinding::toJava(storage::TIndex());

    // In case of countries, splitted on regions.
    string group, map;
    storage::CountryInfo::FullName2GroupAndMap(strCountry, group, map);

    return IndexBinding::toJava(g_framework->Storage().FindIndexByName(map));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapStorage_showCountry(JNIEnv * env, jobject thiz, jobject idx)
  {
    m2::RectD const rect = g_framework->Storage().CountryBounds(IndexBinding(idx).toNative());
    g_framework->ShowCountry(rect);
  }

  void ReportChangeCountryStatus(shared_ptr<jobject> const & obj, storage::TIndex const & idx)
  {
    JNIEnv * env = jni::GetEnv();

    jmethodID methodID = jni::GetJavaMethodID(env, *obj.get(), "onCountryStatusChanged", "(Lcom/mapswithme/maps/MapStorage$Index;)V");
    env->CallVoidMethod(*obj.get(), methodID, IndexBinding::toJava(idx));
  }

  void ReportCountryProgress(shared_ptr<jobject> const & obj, storage::TIndex const & idx, pair<int64_t, int64_t> const & p)
  {
    jlong const current = p.first;
    jlong const total = p.second;

    JNIEnv * env = jni::GetEnv();

    jmethodID methodID = jni::GetJavaMethodID(env, *obj.get(), "onCountryProgress", "(Lcom/mapswithme/maps/MapStorage$Index;JJ)V");
    env->CallVoidMethod(*obj.get(), methodID, IndexBinding::toJava(idx), current, total);
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_MapStorage_subscribe(JNIEnv * env, jobject thiz, jobject obj)
  {
    LOG(LDEBUG, ("Subscribe on storage"));

    return g_framework->Storage().Subscribe(bind(&ReportChangeCountryStatus, jni::make_global_ref(obj), _1),
                                            bind(&ReportCountryProgress, jni::make_global_ref(obj), _1, _2));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_MapStorage_unsubscribe(JNIEnv * env, jobject thiz, jint slotID)
  {
    LOG(LDEBUG, ("UnSubscribe from storage"));

    g_framework->Storage().Unsubscribe(slotID);
  }
}
