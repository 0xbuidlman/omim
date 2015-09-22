#pragma once

#include "rect2d.hpp"
#include "point2d.hpp"

#include "../base/stl_add.hpp"

#include "../std/kdtree.hpp"


namespace m4
{
  template <typename T>
  struct Traits
  {
    m2::RectD const LimitRect(T const & t)
    {
      return t.GetLimitRect();
    }
  };

  template <class T, typename Traits = Traits<T> >
  class Tree
  {
    struct value_t
    {
      T m_val;
      double m_pts[4];

      typedef double value_type;

      value_t(T const & t, m2::RectD const & r) : m_val(t)
      {
        m_pts[0] = r.minX();
        m_pts[1] = r.minY();
        m_pts[2] = r.maxX();
        m_pts[3] = r.maxY();
      }

      bool IsIntersect(m2::RectD const & r) const
      {
        return !((m_pts[2] <= r.minX()) || (m_pts[0] >= r.maxX()) ||
                 (m_pts[3] <= r.minY()) || (m_pts[1] >= r.maxY()));
      }

      double operator[](size_t i) const { return m_pts[i]; }

      m2::RectD GetRect() const { return m2::RectD(m_pts[0], m_pts[1], m_pts[2], m_pts[3]); }
    };

    typedef KDTree::KDTree<4, value_t> tree_t;
    tree_t m_tree;

    typedef vector<value_t const *> store_vec_t;

    // Base do-class for rect-iteration in tree.
    class for_each_base
    {
    protected:
      m2::RectD const & m_rect;

    public:
      for_each_base(m2::RectD const & r) : m_rect(r)
      {
      }

      bool ScanLeft(size_t plane, value_t const & v) const
      {
        switch (plane & 3)    // % 4
        {
        case 2: return m_rect.minX() < v[2];
        case 3: return m_rect.minY() < v[3];
        default: return true;
        }
      }

      bool ScanRight(size_t plane, value_t const & v) const
      {
        switch (plane & 3)  // % 4
        {
        case 0: return m_rect.maxX() > v[0];
        case 1: return m_rect.maxY() > v[1];
        default: return true;
        }
      }
    };

    // Do-class for getting elements in rect.
    class insert_if_intersect : public for_each_base
    {
      typedef for_each_base base_t;

      store_vec_t & m_isect;

    public:
      insert_if_intersect(store_vec_t & isect, m2::RectD const & r)
        : for_each_base(r), m_isect(isect)
      {
      }
      void operator() (value_t const & v)
      {
        if (v.IsIntersect(base_t::m_rect))
          m_isect.push_back(&v);
      }
    };

    // Do-class for processing elements in rect.
    template <class ToDo> class for_each_in_rect : public for_each_base
    {
      typedef for_each_base base_t;

      ToDo & m_toDo;
    public:
      for_each_in_rect(ToDo & toDo, m2::RectD const & rect)
        : for_each_base(rect), m_toDo(toDo)
      {
      }
      void operator() (value_t const & v)
      {
        if (v.IsIntersect(base_t::m_rect))
          m_toDo(v.m_val);
      }
    };

  public:

    typedef T elem_t;

    void Add(T const & obj, m2::RectD const & rect)
    {
      m_tree.insert(value_t(obj, rect));
    }

    template <class TCompare>
    void ReplaceIf(T const & obj, m2::RectD const & rect, TCompare comp)
    {
      store_vec_t isect;
      m_tree.for_each(insert_if_intersect(isect, rect));

      for (size_t i = 0; i < isect.size(); ++i)
        if (!comp(obj, isect[i]->m_val))
          return;

      for (typename store_vec_t::const_iterator i = isect.begin(); i != isect.end(); ++i)
        m_tree.erase(**i);

      Add(obj, rect);
    }

    template <class TCompare>
    void ReplaceIf(T const & obj, TCompare comp)
    {
      ReplaceIf(obj, Traits::LimitRect(obj), comp);
    }

    template <class ToDo>
    void ForEach(ToDo toDo) const
    {
      for (typename tree_t::const_iterator i = m_tree.begin(); i != m_tree.end(); ++i)
        toDo((*i).m_val);
    }

    template <class ToDo>
    void ForEachWithRect(ToDo toDo) const
    {
      for (typename tree_t::const_iterator i = m_tree.begin(); i != m_tree.end(); ++i)
        toDo((*i).GetRect(), (*i).m_val);
    }

    template <class ToDo>
    void ForEachInRect(m2::RectD const & rect, ToDo toDo) const
    {
      m_tree.for_each(for_each_in_rect<ToDo>(toDo, rect));
    }

    bool IsEmpty() const { return m_tree.empty(); }

    size_t GetSize() const { return m_tree.size(); }

    void Clear() { m_tree.clear(); }
  };
}
