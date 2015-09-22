#include "../../Framework.hpp"

#include "../../../core/jni_helper.hpp"


namespace
{
  ::Framework * frm() { return g_framework->NativeFramework(); }

  Bookmark const * getBookmark(jint c, jlong b)
  {
    BookmarkCategory const * pCat = frm()->GetBmCategory(c);
    ASSERT(pCat, ("Category not found", c));
    Bookmark const * pBmk = pCat->GetBookmark(b);
    ASSERT(pBmk, ("Bookmark not found", c, b));
    return pBmk;
  }
}

extern "C"
{
  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_g2p(JNIEnv * env, jobject thiz, jdouble x, jdouble y)
  {
    return jni::GetNewParcelablePointD(env, frm()->GtoP(m2::PointD(x, y)));
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_getName(
       JNIEnv * env, jobject thiz, jint cat, jlong bmk)
  {
    return jni::ToJavaString(env, getBookmark(cat, bmk)->GetName());
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_getBookmarkDescription(
       JNIEnv * env, jobject thiz, jint cat, jlong bmk)
  {
    return jni::ToJavaString(env, getBookmark(cat, bmk)->GetDescription());
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_getIcon(
       JNIEnv * env, jobject thiz, jint cat, jlong bmk)
  {
    return jni::ToJavaString(env, getBookmark(cat, bmk)->GetType());
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_setBookmarkParams(
         JNIEnv * env, jobject thiz, jint cat, jlong bmk,
         jstring name, jstring type, jstring descr)
  {
    Bookmark const * p = getBookmark(cat, bmk);

    // initialize new bookmark
    Bookmark bm(p->GetOrg(), jni::ToNativeString(env, name), jni::ToNativeString(env, type));
    if (descr != 0)
      bm.SetDescription(jni::ToNativeString(env, descr));
    else
      bm.SetDescription(p->GetDescription());

    g_framework->ReplaceBookmark(BookmarkAndCategory(cat, bmk), bm);
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_changeCategory(
         JNIEnv * env, jobject thiz, jint oldCat, jint newCat, jlong bmk)
  {
    return g_framework->ChangeBookmarkCategory(BookmarkAndCategory(oldCat, bmk), newCat);
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_p2g(
       JNIEnv * env, jobject thiz, jdouble px, jdouble py)
  {
    return jni::GetNewParcelablePointD(env, frm()->PtoG(m2::PointD(px, py)));
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_getDistanceAndAzimut(
      JNIEnv * env, jobject thiz, jdouble x, jdouble y, jdouble cLat, jdouble cLon, jdouble north)
  {
    string distance;
    double azimut = -1.0;
    g_framework->NativeFramework()->GetDistanceAndAzimut(
        m2::PointD(x, y), cLat, cLon, north, distance, azimut);

    jclass klass = env->FindClass("com/mapswithme/maps/bookmarks/data/DistanceAndAzimut");
    ASSERT ( klass, () );
    jmethodID methodID = env->GetMethodID(
        klass, "<init>",
        "(Ljava/lang/String;D)V");
    ASSERT ( methodID, () );

    return env->NewObject(klass, methodID,
                          jni::ToJavaString(env, distance.c_str()),
                          static_cast<jdouble>(azimut));
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_bookmarks_data_Bookmark_getXY(
       JNIEnv * env, jobject thiz, jint cat, jlong bmk)
  {
    return jni::GetNewParcelablePointD(env, getBookmark(cat, bmk)->GetOrg());
  }
}
