#pragma once
#include "common_defines.hpp"

#ifdef new
#undef new
#endif

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
using boost::tuple;
using boost::make_tuple;
//using boost::get; // "get" is very common name, use "get" member function

/*
#include <tr1/tuple>
using std::tr1::tuple;
using std::tr1::make_tuple;
using std::tr1::get;
*/

template <class Tuple> struct tuple_length
{
  static const int value = boost::tuples::length<Tuple>::value;
};

template <int N, class T> struct tuple_element
{
  typedef typename boost::tuples::element<N, T>::type type;
};

namespace impl
{
  template <int N> struct for_each_tuple_impl
  {
    template <class Tuple, class ToDo> void operator() (Tuple & t, ToDo & toDo)
    {
      toDo(boost::tuples::get<N>(t), N);
      for_each_tuple_impl<N-1> c;
      c(t, toDo);
    }
  };

  template <> struct for_each_tuple_impl<-1>
  {
    template <class Tuple, class ToDo> void operator() (Tuple &, ToDo &) {}
  };
}

template <class Tuple, class ToDo>
void for_each_tuple(Tuple & t, ToDo & toDo)
{
  impl::for_each_tuple_impl<tuple_length<Tuple>::value-1> c;
  c(t, toDo);
}

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif
