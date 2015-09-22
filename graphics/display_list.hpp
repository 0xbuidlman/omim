#pragma once

#include "uniforms_holder.hpp"
#include "defines.hpp"
#include "display_list_renderer.hpp"
#include "opengl/base_texture.hpp"
#include "opengl/buffer_object.hpp"

#include "../std/set.hpp"

namespace graphics
{
  class DisplayList
  {
  private:

    typedef gl::GeometryRenderer::DrawGeometry DrawGeometryCmd;
    typedef gl::GeometryRenderer::DiscardStorage DiscardStorageCmd;
    typedef gl::GeometryRenderer::FreeTexture FreeTextureCmd;
    typedef gl::GeometryRenderer::UnlockStorage UnlockStorageCmd;
    typedef gl::GeometryRenderer::FreeStorage FreeStorageCmd;
    typedef gl::GeometryRenderer::ApplyBlitStates ApplyBlitStatesCmd;
    typedef gl::GeometryRenderer::ApplyStates ApplyStatesCmd;
    typedef gl::GeometryRenderer::ApplySharpStates ApplySharpStatesCmd;
    typedef gl::GeometryRenderer::UploadData UploadDataCmd;
    typedef gl::GeometryRenderer::ClearCommand ClearCommandCmd;

    list<shared_ptr<Command> > m_commands;

    typedef DisplayListRenderer::TextureRef TextureRef;
    typedef DisplayListRenderer::StorageRef StorageRef;

    set<TextureRef> m_textures;
    set<StorageRef> m_storages;

    DisplayListRenderer * m_parent;
    bool m_isDebugging;

    DisplayList(DisplayListRenderer * parent);

    friend class DisplayListRenderer;

  public:

    ~DisplayList();

    void applyStates(shared_ptr<ApplyStatesCmd> const & cmd);
    void applyBlitStates(shared_ptr<ApplyBlitStatesCmd> const & cmd);
    void applySharpStates(shared_ptr<ApplySharpStatesCmd> const & cmd);
    void drawGeometry(shared_ptr<DrawGeometryCmd> const & cmd);
    void unlockStorage(shared_ptr<UnlockStorageCmd> const & cmd);
    void discardStorage(shared_ptr<DiscardStorageCmd> const & cmd);
    void freeTexture(shared_ptr<FreeTextureCmd> const & cmd);
    void freeStorage(shared_ptr<FreeStorageCmd> const & cmd);
    void uploadResources(shared_ptr<UploadDataCmd> const & cmd);
    void clear(shared_ptr<ClearCommandCmd> const & cmd);
    void addCheckPoint();

    void draw(DisplayListRenderer * r,
              math::Matrix<double, 3, 3> const & m,
              UniformsHolder * holder = NULL);
  };
}
