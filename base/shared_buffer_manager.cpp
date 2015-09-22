#include "../base/SRC_FIRST.hpp"
#include "shared_buffer_manager.hpp"
#include "ptr_utils.hpp"

SharedBufferManager & SharedBufferManager::instance()
{
  static SharedBufferManager i;
  return i;
}

SharedBufferManager::shared_buffer_ptr_t SharedBufferManager::reserveSharedBuffer(size_t s)
{
  threads::MutexGuard g(m_mutex);

  shared_buffer_ptr_list_t l = m_sharedBuffers[s];

  if (l.empty())
    l.push_back(make_shared_ptr(new shared_buffer_t(s)));

  shared_buffer_ptr_t res = l.front();
  l.pop_front();

  return res;
}

void SharedBufferManager::freeSharedBuffer(size_t s, shared_buffer_ptr_t buf)
{
  threads::MutexGuard g(m_mutex);

  shared_buffer_ptr_list_t l = m_sharedBuffers[s];

  l.push_back(buf);
}
