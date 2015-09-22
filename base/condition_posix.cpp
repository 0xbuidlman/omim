#include "../std/target_os.hpp"

#if !defined(OMIM_OS_BADA) && !defined(OMIM_OS_WINDOWS_NATIVE)

#include "condition.hpp"
#include "mutex.hpp"

#include "../std/stdint.hpp"
#include "../std/systime.hpp"

#include <sys/errno.h>

#include <pthread.h>

namespace threads
{
  namespace impl
  {
    class ConditionImpl
    {
    public:
      Mutex m_Mutex;
      pthread_cond_t m_Condition;
    };
  }

  Condition::Condition() : m_pImpl(new impl::ConditionImpl)
  {
    ::pthread_cond_init(&m_pImpl->m_Condition, 0);
  }

  Condition::~Condition()
  {
    ::pthread_cond_destroy(&m_pImpl->m_Condition);
    delete m_pImpl;
  }

  void Condition::Signal(bool broadcast)
  {
    if (broadcast)
      ::pthread_cond_broadcast(&m_pImpl->m_Condition);
    else
      ::pthread_cond_signal(&m_pImpl->m_Condition);
  }

  void Condition::Wait()
  {
    ::pthread_cond_wait(&m_pImpl->m_Condition, &m_pImpl->m_Mutex.m_Mutex);
  }

  bool Condition::Wait(unsigned ms)
  {
    if (ms == -1)
    {
      Wait();
      return false;
    }

    ::timeval curtv;
    ::gettimeofday(&curtv, 0);

    ::timespec ts;

    uint64_t deltaNanoSec = curtv.tv_usec * 1000 + ms * 1000000;

    ts.tv_sec = curtv.tv_sec + deltaNanoSec / 1000000000;
    ts.tv_nsec = deltaNanoSec % 1000000000;

    int res = ::pthread_cond_timedwait(&m_pImpl->m_Condition, &m_pImpl->m_Mutex.m_Mutex, &ts);

    return (res == ETIMEDOUT);
  }

  void Condition::Lock()
  {
    m_pImpl->m_Mutex.Lock();
  }

  void Condition::Unlock()
  {
    m_pImpl->m_Mutex.Unlock();
  }
}

#endif
