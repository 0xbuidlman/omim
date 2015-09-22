#pragma once

#include "../std/shared_ptr.hpp"
#include "../std/map.hpp"
#include "../std/string.hpp"
#include "../std/list.hpp"
#include "../std/vector.hpp"

#include "../base/mutex.hpp"
#include "../base/resource_pool.hpp"

#include "opengl/storage.hpp"
#include "opengl/program_manager.hpp"
#include "glyph_cache.hpp"
#include "data_formats.hpp"
#include "defines.hpp"

namespace graphics
{
  class ResourceCache;

  namespace gl
  {
    class BaseTexture;
    class Storage;
    class ProgramManager;
  }

  struct GlyphInfo;

  struct TTextureFactory : BasePoolElemFactory
  {
    size_t m_w;
    size_t m_h;
    graphics::DataFormat m_format;
    TTextureFactory(size_t w,
                    size_t h,
                    graphics::DataFormat format,
                    char const * resName,
                    size_t batchSize);
    shared_ptr<gl::BaseTexture> const Create();
  };

  struct TStorageFactory : BasePoolElemFactory
  {
    size_t m_vbSize;
    size_t m_ibSize;
    bool m_useSingleThreadedOGL;
    TStorageFactory(size_t vbSize,
                    size_t ibSize,
                    bool useSingleThreadedOGL,
                    char const * resName,
                    size_t batchSize);
    gl::Storage const Create();
    void BeforeMerge(gl::Storage const & e);
  };

  /// ---- Texture Pools ----

  /// Basic texture pool traits
  typedef BasePoolTraits<shared_ptr<gl::BaseTexture>, TTextureFactory> TBaseTexturePoolTraits;

  /// Fixed-Size texture pool
  typedef FixedSizePoolTraits<TTextureFactory, TBaseTexturePoolTraits > TFixedSizeTexturePoolTraits;
  typedef ResourcePoolImpl<TFixedSizeTexturePoolTraits> TFixedSizeTexturePoolImpl;

  /// On-Demand multi-threaded texture pool

  typedef AllocateOnDemandMultiThreadedPoolTraits<TTextureFactory, TBaseTexturePoolTraits> TOnDemandMultiThreadedTexturePoolTraits;
  typedef ResourcePoolImpl<TOnDemandMultiThreadedTexturePoolTraits> TOnDemandMultiThreadedTexturePoolImpl;

  /// On-Demand single-threaded texture pool
  /// (with postponed resource allocation)
  typedef AllocateOnDemandSingleThreadedPoolTraits<TTextureFactory, TBaseTexturePoolTraits> TOnDemandSingleThreadedTexturePoolTraits;
  typedef ResourcePoolImpl<TOnDemandSingleThreadedTexturePoolTraits> TOnDemandSingleThreadedTexturePoolImpl;

  /// Interface for texture pool
  typedef ResourcePool<shared_ptr<gl::BaseTexture> > TTexturePool;

  /// ---- Storage Pools ----

  /// Basic storage traits
  typedef BasePoolTraits<gl::Storage, TStorageFactory> TBaseStoragePoolTraits;

  /// Fixed-Size mergeable storage pool
  typedef SeparateFreePoolTraits<TStorageFactory, TBaseStoragePoolTraits> TSeparateFreeStoragePoolTraits;
  typedef FixedSizePoolTraits<TStorageFactory, TSeparateFreeStoragePoolTraits> TFixedSizeMergeableStoragePoolTraits;
  typedef ResourcePoolImpl<TFixedSizeMergeableStoragePoolTraits> TFixedSizeMergeableStoragePoolImpl;

  /// Fixed-Size non-mergeable storage pool
  typedef FixedSizePoolTraits<TStorageFactory, TBaseStoragePoolTraits> TFixedSizeNonMergeableStoragePoolTraits;
  typedef ResourcePoolImpl<TFixedSizeNonMergeableStoragePoolTraits> TFixedSizeNonMergeableStoragePoolImpl;

  /// On-Demand single-threaded storage pool
  /// (with postponed resource allocation and separate list of freed resources)
  typedef AllocateOnDemandSingleThreadedPoolTraits<TStorageFactory, TSeparateFreeStoragePoolTraits> TOnDemandSingleThreadedStoragePoolTraits;
  typedef ResourcePoolImpl<TOnDemandSingleThreadedStoragePoolTraits> TOnDemandSingleThreadedStoragePoolImpl;

  /// On-Demand multi-threaded storage pool
  typedef AllocateOnDemandMultiThreadedPoolTraits<TStorageFactory, TBaseStoragePoolTraits> TOnDemandMultiThreadedStoragePoolTraits;
  typedef ResourcePoolImpl<TOnDemandMultiThreadedStoragePoolTraits> TOnDemandMultiThreadedStoragePoolImpl;

  /// Interface for storage pool
  typedef ResourcePool<gl::Storage> TStoragePool;

  class ResourceManager
  {
  public:

    struct StoragePoolParams
    {
      size_t m_vbSize;
      size_t m_vertexSize;
      size_t m_ibSize;
      size_t m_indexSize;
      size_t m_storagesCount;
      EStorageType m_storageType;
      bool m_isDebugging;

      StoragePoolParams();
      StoragePoolParams(EStorageType storageType);
      StoragePoolParams(size_t vbSize,
                        size_t vertexSize,
                        size_t ibSize,
                        size_t indexSize,
                        size_t storagesCount,
                        EStorageType storageType,
                        bool isDebugging);

      bool isValid() const;
    };

    struct TexturePoolParams
    {
      size_t m_texWidth;
      size_t m_texHeight;
      size_t m_texCount;
      graphics::DataFormat m_format;
      ETextureType m_textureType;
      bool m_isDebugging;

      TexturePoolParams();
      TexturePoolParams(ETextureType textureType);
      TexturePoolParams(size_t texWidth,
                        size_t texHeight,
                        size_t texCount,
                        graphics::DataFormat format,
                        ETextureType textureType,
                        bool isDebugging);

      bool isValid() const;
    };

    struct GlyphCacheParams
    {
      string m_unicodeBlockFile;
      string m_whiteListFile;
      string m_blackListFile;

      size_t m_glyphCacheMemoryLimit;

      GlyphCacheParams();
      GlyphCacheParams(string const & unicodeBlockFile,
                       string const & whiteListFile,
                       string const & blackListFile,
                       size_t glyphCacheMemoryLimit);
    };

    struct Params
    {
    private:

      string m_vendorName;
      string m_rendererName;

      /// check non-strict matching upon vendorName and rendererName
      bool isGPU(char const * vendorName, char const * rendererName, bool strictMatch) const;

    public:

      DataFormat m_rtFormat;
      DataFormat m_texFormat;
      DataFormat m_texRtFormat;
      bool m_useSingleThreadedOGL;
      bool m_useReadPixelsToSynchronize;

      size_t m_videoMemoryLimit;

      /// storages params
      vector<StoragePoolParams> m_storageParams;

      /// textures params
      vector<TexturePoolParams> m_textureParams;

      /// glyph caches params
      GlyphCacheParams m_glyphCacheParams;

      unsigned m_renderThreadsCount;
      unsigned m_threadSlotsCount;

      Params();

      void distributeFreeMemory(int freeVideoMemory);
      void checkDeviceCaps();
      int memoryUsage() const;
      int fixedMemoryUsage() const;
      void initScaleWeights();
    };

  private:

    typedef map<string, shared_ptr<gl::BaseTexture> > TStaticTextures;

    TStaticTextures m_staticTextures;

    threads::Mutex m_mutex;

    vector<shared_ptr<TTexturePool> > m_texturePools;
    vector<shared_ptr<TStoragePool> > m_storagePools;

    struct ThreadSlot
    {
      shared_ptr<gl::ProgramManager> m_programManager;
      shared_ptr<GlyphCache> m_glyphCache;
    };

    vector<ThreadSlot> m_threadSlots;

    Params m_params;

  public:

    ResourceManager(Params const & p);

    void initThreadSlots(Params const & p);

    void initStoragePool(StoragePoolParams const & p, shared_ptr<TStoragePool> & pool);

    TStoragePool * storagePool(EStorageType type);

    void initTexturePool(TexturePoolParams const & p, shared_ptr<TTexturePool> & pool);

    TTexturePool * texturePool(ETextureType type);

    shared_ptr<gl::BaseTexture> const & getTexture(string const & fileName);

    Params const & params() const;

    GlyphCache * glyphCache(int threadSlot = 0);

    int renderThreadSlot(int threadNum) const;
    int guiThreadSlot() const;
    int cacheThreadSlot() const;

    void addFonts(vector<string> const & fontNames);

    void memoryWarning();
    void enterBackground();
    void enterForeground();

    void updatePoolState();

    void cancel();

    bool useReadPixelsToSynchronize() const;

    shared_ptr<graphics::gl::BaseTexture> createRenderTarget(unsigned w, unsigned h);
    gl::ProgramManager * programManager(int threadSlot);
  };

  void loadSkin(shared_ptr<ResourceManager> const & rm,
                string const & fileName,
                vector<shared_ptr<ResourceCache> > & caches);
}

