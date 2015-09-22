#include "thread.hpp"
#include "assert.hpp"

#include "../std/target_os.hpp"

#if defined(OMIM_OS_BADA)
  #include <FBaseRtThreadThread.h>
#elif defined(OMIM_OS_WINDOWS_NATIVE)
  #include "../std/windows.hpp"
#else
  #include <pthread.h>
#endif

#include "../base/start_mem_debug.hpp"

namespace threads
{
#if defined(OMIM_OS_BADA)
  /// BADA specific implementation
  class ThreadImpl : public Osp::Base::Runtime::Thread
  {
    IRoutine * m_pRoutine;

  public:
    /// Doesn't own pRoutine
    ThreadImpl() : m_pRoutine(0)
    {
      result error = Construct(Osp::Base::Runtime::THREAD_TYPE_WORKER,
                               Osp::Base::Runtime::Thread::DEFAULT_STACK_SIZE,
                               Osp::Base::Runtime::THREAD_PRIORITY_MID);
      ASSERT_EQUAL(error, E_SUCCESS, ("Constructing thread error"));
    }

    int Create(IRoutine * pRoutine)
    {
      ASSERT(pRoutine, ("Can't be NULL"));
      m_pRoutine = pRoutine;
      return Start();
    }

    int Join()
    {
      return Join();
    }

    virtual Osp::Base::Object * Run()
    {
      m_pRoutine->Do();
      return 0;
    }
  };

#elif defined(OMIM_OS_WINDOWS_NATIVE)
  /// Windows native implementation
  class ThreadImpl
  {
    HANDLE m_handle;

  public:
    ThreadImpl() : m_handle(0) {}

    ~ThreadImpl()
    {
      if (m_handle)
        ::CloseHandle(m_handle);
    }

    static DWORD WINAPI WindowsWrapperThreadProc(void * p)
    {
      IRoutine * pRoutine = reinterpret_cast<IRoutine *>(p);
      pRoutine->Do();
      return 0;
    }

    int Create(IRoutine * pRoutine)
    {
      int error = 0;
      m_handle = ::CreateThread(NULL, 0, &WindowsWrapperThreadProc, reinterpret_cast<void *>(pRoutine), 0, NULL);
      if (0 == m_handle)
        error = ::GetLastError();
      return error;
    }

    int Join()
    {
      int error = 0;
      if (WAIT_OBJECT_0 != ::WaitForSingleObject(m_handle, INFINITE))
        error = ::GetLastError();
      return error;
    }
  };
  // end of Windows Native implementation

#else
  /// POSIX pthreads implementation
  class ThreadImpl
  {
    pthread_t m_handle;

  public:
    ThreadImpl() {}

    static void * PthreadsWrapperThreadProc(void * p)
    {
      IRoutine * pRoutine = reinterpret_cast<IRoutine *>(p);
      pRoutine->Do();

      ::pthread_exit(NULL);
      return NULL;
    }

    int Create(IRoutine * pRoutine)
    {
      return ::pthread_create(&m_handle, NULL, &PthreadsWrapperThreadProc, reinterpret_cast<void *>(pRoutine));
    }

    int Join()
    {
      return ::pthread_join(m_handle, NULL);
    }
  };
  //////////////////////// end of POSIX pthreads implementation
#endif

  /////////////////////////////////////////////////////////////////////
  // Thread wrapper implementation
  Thread::Thread() : m_impl(new ThreadImpl()), m_routine(0)
  {
  }

  Thread::~Thread()
  {
    delete m_impl;
  }

  bool Thread::Create(IRoutine * pRoutine)
  {
    ASSERT_EQUAL(m_routine, 0, ("Current implementation doesn't allow to reuse thread"));
    int error = m_impl->Create(pRoutine);
    if (0 != error)
    {
      ASSERT ( !"Thread creation failed with error", (error) );
      return false;
    }
    m_routine = pRoutine;
    return true;
  }

  void Thread::Cancel()
  {
    if (m_routine)
    {
      m_routine->Cancel();
      Join();
    }
  }

  void Thread::Join()
  {
    if (m_routine)
    {
      int error = m_impl->Join();
      if (0 != error)
      {
        ASSERT ( !"Thread join failed. See error value.", (error) );
      }
    }
  }

  void Sleep(size_t ms)
  {
#ifdef OMIM_OS_WINDOWS
    ::Sleep(ms);
#else
    timespec t;
    t.tv_nsec =(ms * 1000000) % 1000000000;
    t.tv_sec = (ms * 1000000) / 1000000000;
    nanosleep(&t, 0);
#endif
  }

  int GetCurrentThreadID()
  {
#ifdef OMIM_OS_WINDOWS
    return ::GetCurrentThreadId();
#else
    return pthread_self();
#endif
  }
}
