#pragma once

#include "../base/assert.hpp"
#include "../base/mutex.hpp"

#include "../std/map.hpp"

class PointerTracker
{
public:
  ~PointerTracker();

  template <typename T>
  void Ref(T * p, bool needDestroyCheck)
  {
    threads::MutexGuard g(m_mutex);
    if (p == NULL)
      return;

    map_t::iterator it = m_countMap.find(p);
    if (it == m_countMap.end())
    {
      m_countMap.insert(make_pair((void *)p, make_pair(1, typeid(p).name())));
      if (needDestroyCheck)
        m_alivePointers.insert(p);
    }
    else
      it->second.first++;
  }

  void Deref(void * p);
  void Destroy(void * p);

private:
  typedef map<void *, pair<int, string> > map_t;
  map_t m_countMap;
  typedef set<void *> alive_pointers_t;
  alive_pointers_t m_alivePointers;
  threads::Mutex m_mutex;
};

#ifdef DEBUG
  extern PointerTracker g_tracker;

  #define REF_POINTER(p, c) g_tracker.Ref(p, c)
  #define DEREF_POINTER(p) g_tracker.Deref(p)
  #define DESTROY_POINTER(p) g_tracker.Destroy(p)

  #define DECLARE_CHECK bool m_checkOnDestroy
  #define DECLARE_CHECK_GET bool IsCheckOnDestroy() const { return m_checkOnDestroy; }
  #define DECLARE_CHECK_SET void SetCheckOnDestroy(bool doCheck) { m_checkOnDestroy = doCheck; }
  #define SET_CHECK_FLAG(x) SetCheckOnDestroy(x)
  #define GET_CHECK_FLAG(x) (x).IsCheckOnDestroy()
#else
  #define REF_POINTER(p, c)
  #define DEREF_POINTER(p)
  #define DESTROY_POINTER(p)

  #define DECLARE_CHECK
  #define DECLARE_CHECK_GET
  #define DECLARE_CHECK_SET
  #define SET_CHECK_FLAG(x)
  #define GET_CHECK_FLAG(x) false
#endif

template <typename T>
class DrapePointer
{
public:
  DrapePointer() : m_p(NULL) { SET_CHECK_FLAG(true); }

protected:
  DrapePointer(T * p, bool needDestroyedCheck = true)
    : m_p(p)
  {
    SET_CHECK_FLAG(needDestroyedCheck);
    REF_POINTER(m_p, GET_CHECK_FLAG(*this));
  }

  DrapePointer(DrapePointer<T> const & other)
    : m_p(NULL)
  {
    SET_CHECK_FLAG(GET_CHECK_FLAG(other));
    Reset(other.GetNonConstRaw());
  }

  DrapePointer<T> & operator=(DrapePointer<T> const & other)
  {
    SET_CHECK_FLAG(GET_CHECK_FLAG(other));
    Reset(other.GetNonConstRaw());
    return *this;
  }

  void Destroy()
  {
    DESTROY_POINTER(m_p);
    delete m_p;
    DEREF_POINTER(m_p);
    m_p = NULL;
  }

  void Reset(T * p)
  {
    ResetImpl(p);
  }

  T * GetRaw()                { return m_p; }
  T const * GetRaw() const    { return m_p; }
  T * GetNonConstRaw() const  { return m_p; }

  DECLARE_CHECK_GET;
  DECLARE_CHECK_SET;

  // Need to be const for copy constructor and assigment operator of TransfromPointer
  void SetToNull() const
  {
    ResetImpl(NULL);
  }

private:
  void ResetImpl(T * p) const
  {
    DEREF_POINTER(m_p);
    m_p = p;
    REF_POINTER(m_p, GET_CHECK_FLAG(*this));
  }

private:
  // Mutable for Move method
  mutable T * m_p;
  DECLARE_CHECK;
};

template <typename T> class MasterPointer;

template <typename T>
class TransferPointer : public DrapePointer<T>
{
  typedef DrapePointer<T> base_t;
public:
  TransferPointer(const TransferPointer<T> & other)
    : base_t(other)
  {
    ASSERT(GET_CHECK_FLAG(other), ());
    other.SetToNull();
  }

  TransferPointer<T> & operator=(TransferPointer<T> const & other)
  {
    ASSERT(GET_CHECK_FLAG(other), ());;
    base_t::operator =(other);
    other.SetToNull();
    return *this;
  }

  ~TransferPointer()
  {
    ASSERT(base_t::GetRaw() == NULL, ());
    Destroy();
  }
  void Destroy() { base_t::Destroy(); }

private:
  friend class MasterPointer<T>;
  TransferPointer() {}
  explicit TransferPointer(T * p) : base_t(p) {}
};

template <typename T> class RefPointer;
template <typename T> RefPointer<T> MakeStackRefPointer(T * p);

template<typename T>
class RefPointer : public DrapePointer<T>
{
  typedef DrapePointer<T> base_t;
public:
  RefPointer() : base_t() {}
  ~RefPointer() { base_t::Reset(NULL); }

  template <typename Y>
  RefPointer(const RefPointer<Y> & p) : base_t(p.GetNonConstRaw(), GET_CHECK_FLAG(p)) {}

  bool IsContentLess(RefPointer<T> const & other) const { return *GetRaw() < *other.GetRaw(); }
  bool IsNull() const          { return base_t::GetRaw() == NULL; }
  T * operator->()             { return base_t::GetRaw(); }
  T const * operator->() const { return base_t::GetRaw(); }
  T * GetRaw()                 { return base_t::GetRaw(); }
  T const * GetRaw() const     { return base_t::GetRaw(); }

private:
  template <typename Y> friend class RefPointer;
  friend class MasterPointer<T>;
  friend RefPointer<T> MakeStackRefPointer<T>(T *);
  explicit RefPointer(T * p, bool needDestroyedCheck = true) : base_t(p, needDestroyedCheck) {}
};

template <typename T>
RefPointer<T> MakeStackRefPointer(T * p) { return RefPointer<T>(p, false); }

template <typename T>
class MasterPointer : public DrapePointer<T>
{
  typedef DrapePointer<T> base_t;
public:
  MasterPointer() : base_t() {}
  explicit MasterPointer(T * p) : base_t(p) {}
  explicit MasterPointer(TransferPointer<T> & transferPointer)
  {
    Reset(transferPointer.GetRaw());
    transferPointer.Reset(NULL);
  }

  ~MasterPointer()
  {
    base_t::Reset(NULL);
  }

  RefPointer<T> GetRefPointer() const
  {
    return RefPointer<T>(base_t::GetNonConstRaw());
  }

  TransferPointer<T> Move()
  {
    TransferPointer<T> result(GetRaw());
    base_t::Reset(NULL);
    return result;
  }

  void Destroy()
  {
    Reset(NULL);
  }

  void Reset(T * p)
  {
    base_t::Destroy();
    base_t::Reset(p);
  }

  bool IsNull() const          { return base_t::GetRaw() == NULL; }
  T * operator->()             { return base_t::GetRaw(); }
  T const * operator->() const { return base_t::GetRaw(); }
  T * GetRaw()                 { return base_t::GetRaw(); }
  T const * GetRaw() const     { return base_t::GetRaw(); }
};

template<typename T>
TransferPointer<T> MovePointer(T * p)
{
  return MasterPointer<T>(p).Move();
}

struct MasterPointerDeleter
{
  template <typename Y, typename T>
  void operator() (pair<Y, MasterPointer<T> > & value)
  {
    Destroy(value.second);
  }

  template <typename T>
  void operator() (MasterPointer<T> & value)
  {
    Destroy(value);
  }

private:
  template <typename T>
  void Destroy(MasterPointer<T> & value)
  {
    value.Destroy();
  }
};
