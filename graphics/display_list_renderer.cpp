#include "display_list_renderer.hpp"
#include "display_list.hpp"

namespace graphics
{
  DisplayListRenderer::DisplayListRenderer(Params const & p)
    : base_t(p),
      m_displayList(0)
  {
  }

  void DisplayListRenderer::addStorageRef(StorageRef const & storage)
  {
    m_discardStorageCmds[storage].first++;
    m_freeStorageCmds[storage].first++;
  }

  void DisplayListRenderer::removeStorageRef(StorageRef const & storage)
  {
    CHECK(m_discardStorageCmds.find(storage) != m_discardStorageCmds.end(), ());
    pair<int, shared_ptr<DiscardStorageCmd> > & dval = m_discardStorageCmds[storage];
    --dval.first;
    if ((dval.first == 0) && dval.second)
    {
      dval.second->perform();
      dval.second.reset();
    }

    CHECK(m_freeStorageCmds.find(storage) != m_freeStorageCmds.end(), ());
    pair<int, shared_ptr<FreeStorageCmd> > & fval = m_freeStorageCmds[storage];
    --fval.first;
    if ((fval.first == 0) && fval.second)
    {
      fval.second->perform();
      fval.second.reset();
    }
  }

  void DisplayListRenderer::addTextureRef(TextureRef const & texture)
  {
    pair<int, shared_ptr<FreeTextureCmd> > & val = m_freeTextureCmds[texture];
    val.first++;
  }

  void DisplayListRenderer::removeTextureRef(TextureRef const & texture)
  {
    CHECK(m_freeTextureCmds.find(texture) != m_freeTextureCmds.end(), ());
    pair<int, shared_ptr<FreeTextureCmd> > & val = m_freeTextureCmds[texture];

    --val.first;
    if ((val.first == 0) && val.second)
    {
      val.second->perform();
      val.second.reset();
    }
  }

  DisplayList * DisplayListRenderer::createDisplayList()
  {
    return new DisplayList(this);
  }

  void DisplayListRenderer::setDisplayList(DisplayList * dl)
  {
    m_displayList = dl;
  }

  DisplayList * DisplayListRenderer::displayList() const
  {
    return m_displayList;
  }

  void DisplayListRenderer::drawDisplayList(DisplayList * dl,
                                            math::Matrix<double, 3, 3> const & m)
  {
    dl->draw(this, m);
  }

  void DisplayListRenderer::drawGeometry(shared_ptr<gl::BaseTexture> const & texture,
                                         gl::Storage const & storage,
                                         size_t indicesCount,
                                         size_t indicesOffs,
                                         EPrimitives primType)
  {
    if (isCancelled())
      return;

    if (m_displayList)
    {
      shared_ptr<DrawGeometry> command(new DrawGeometry());

      command->m_texture = texture;
      command->m_storage = storage;
      command->m_indicesCount = indicesCount;
      command->m_indicesOffs = indicesOffs;
      command->m_primitiveType = primType;

      m_displayList->drawGeometry(command);
    }
    else
      base_t::drawGeometry(texture,
                           storage,
                           indicesCount,
                           indicesOffs,
                           primType);

  }

  void DisplayListRenderer::uploadResources(shared_ptr<Resource> const * resources,
                                            size_t count,
                                            shared_ptr<gl::BaseTexture> const & texture)
  {
    if (isCancelled())
      return;

    if (m_displayList)
      m_displayList->uploadResources(make_shared_ptr(new UploadData(resources, count, texture)));
    else
      base_t::uploadResources(resources, count, texture);
  }

  void DisplayListRenderer::freeTexture(shared_ptr<gl::BaseTexture> const & texture,
                                        TTexturePool * texturePool)
  {
    if (m_displayList)
    {
      shared_ptr<FreeTexture> command(new FreeTexture());

      command->m_texture = texture;
      command->m_texturePool = texturePool;

      m_freeTextureCmds[texture.get()].second = command;

//      m_displayList->freeTexture(command);
    }
    else
      base_t::freeTexture(texture, texturePool);
  }

  void DisplayListRenderer::freeStorage(gl::Storage const & storage,
                                        TStoragePool * storagePool)
  {
    if (m_displayList)
    {
      shared_ptr<FreeStorage> command(new FreeStorage());

      command->m_storage = storage;
      command->m_storagePool = storagePool;

      StorageRef sref(storage.m_vertices.get(), storage.m_indices.get());

      m_freeStorageCmds[sref].second = command;
//      m_displayList->freeStorage(command);
    }
    else
      base_t::freeStorage(storage, storagePool);
  }

  void DisplayListRenderer::unlockStorage(gl::Storage const & storage)
  {
    if (m_displayList)
    {
      shared_ptr<UnlockStorage> cmd(new UnlockStorage());

      cmd->m_storage = storage;

      m_displayList->unlockStorage(cmd);
    }
    else
      base_t::unlockStorage(storage);
  }

  void DisplayListRenderer::discardStorage(gl::Storage const & storage)
  {
    if (m_displayList)
    {
      shared_ptr<DiscardStorage> cmd(new DiscardStorage());

      cmd->m_storage = storage;

      StorageRef sref(storage.m_vertices.get(), storage.m_indices.get());

      m_discardStorageCmds[sref].second = cmd;
//      m_displayList->discardStorage(cmd);
    }
    else
      base_t::discardStorage(storage);
  }

  void DisplayListRenderer::applyBlitStates()
  {
    if (m_displayList)
      m_displayList->applyBlitStates(make_shared_ptr(new ApplyBlitStates()));
    else
      base_t::applyBlitStates();
  }

  void DisplayListRenderer::applyStates()
  {
    if (m_displayList)
      m_displayList->applyStates(make_shared_ptr(new ApplyStates()));
    else
      base_t::applyStates();
  }

  void DisplayListRenderer::applySharpStates()
  {
    if (m_displayList)
      m_displayList->applySharpStates(make_shared_ptr(new ApplySharpStates()));
    else
      base_t::applySharpStates();
  }

  void DisplayListRenderer::addCheckPoint()
  {
    if (m_displayList)
      m_displayList->addCheckPoint();
    else
      base_t::addCheckPoint();
  }

}
