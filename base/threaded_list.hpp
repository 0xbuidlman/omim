#pragma once

#include "condition.hpp"
#include "logging.hpp"
#include "../std/list.hpp"

#include "threaded_container.hpp"

template <typename T>
class ThreadedList : public ThreadedContainer
{
private:

  list<T> m_list;
  bool m_isEmpty;
  string m_resName;

public:

  ThreadedList()
    : m_isEmpty(true)
  {}

  template <typename Fn>
  void ProcessList(Fn const & fn)
  {
    threads::ConditionGuard g(m_Cond);

    bool hadElements = !m_list.empty();

    fn(m_list);

    bool hasElements = !m_list.empty();

    m_isEmpty = !hasElements;

    if (!hadElements && hasElements)
      m_Cond.Signal();
  }

  void PushBack(T const & t)
  {
    threads::ConditionGuard g(m_Cond);

    bool doSignal = m_list.empty();

    m_isEmpty = doSignal;

    m_list.push_back(t);

    if (doSignal)
      m_Cond.Signal();
  }

  void PushFront(T const & t)
  {
    threads::ConditionGuard g(m_Cond);

    bool doSignal = m_list.empty();

    m_isEmpty = doSignal;

    m_list.push_front(t);

    if (doSignal)
      m_Cond.Signal();
  }

  void SetName(char const * name)
  {
    m_resName = name;
  }

  string const & GetName() const
  {
    return m_resName;
  }

  bool WaitNonEmpty()
  {
    double StartWaitTime = m_Timer.ElapsedSeconds();

    bool doFirstWait = true;

    while ((m_isEmpty = m_list.empty()))
    {
      if (IsCancelled())
        break;

      if (doFirstWait)
      {
        doFirstWait = false;
        if (!m_resName.empty())
          LOG(LINFO, ("consumer is waiting for", m_resName));
      }

      m_Cond.Wait();
    }

    m_WaitTime += m_Timer.ElapsedSeconds() - StartWaitTime;

    if (IsCancelled())
      return true;

    return false;
  }

  T const Front(bool doPop)
  {
    threads::ConditionGuard g(m_Cond);

    if (WaitNonEmpty())
      return T();

    T res = m_list.front();

    if (doPop)
      m_list.pop_front();

    m_isEmpty = m_list.empty();

    return res;
  }

  T const Back(bool doPop)
  {
    threads::ConditionGuard g(m_Cond);

    if (WaitNonEmpty())
      return T();

    T res = m_list.back();

    if (doPop)
      m_list.pop_back();

    m_isEmpty = m_list.empty();

    return res;
  }

  size_t Size() const
  {
    threads::ConditionGuard g(m_Cond);
    return m_list.size();
  }

  bool Empty() const
  {
    return m_isEmpty;
  }

  void Clear()
  {
    threads::ConditionGuard g(m_Cond);
    m_list.clear();
    m_isEmpty = true;
  }
};
