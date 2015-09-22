#pragma once

#include "rect2d.hpp"
#include "point2d.hpp"

namespace m2
{
  namespace detail
  {
    static const int LEFT = 1;    // �������� 0001
    static const int RIGHT = 2;   // �������� 0010
    static const int BOT = 4;     // �������� 0100
    static const int TOP = 8;     // �������� 1000

    template <class T>
    int vcode(m2::Rect<T> const & r, m2::Point<T> const & p)
    {
      return ((p.x < r.minX() ? LEFT : 0)  +    // +1 ���� ����� ����� ��������������
              (p.x > r.maxX() ? RIGHT : 0) +    // +2 ���� ����� ������ ��������������
              (p.y < r.minY() ? BOT : 0)   +    // +4 ���� ����� ���� ��������������
              (p.y > r.maxY() ? TOP : 0));      // +8 ���� ����� ���� ��������������
    }
  }

  template <class T>
  bool Intersect(m2::Rect<T> const & r, m2::Point<T> & p1, m2::Point<T> & p2)
  {
    int code[2] = { detail::vcode(r, p1), detail::vcode(r, p2) };

    // ���� ���� �� ����� ������� ��� ��������������
    while (code[0] || code[1])
    {
      if (code[0] & code[1])
      {
        // ���� ��� ����� � ����� ������� ��������������, �� ������� �� ���������� �������������
        return false;
      }

      // �������� ����� c � ��������� �����
      m2::Point<T> * pp;
      int i;
      if (code[0])
      {
        i = 0;
        pp = &p1;
      }
      else
      {
        i = 1;
        pp = &p2;
      }

      // ���� pp ����� r, �� ����������� pp �� ������ x = r->x_min
      // ���� pp ������ r, �� ����������� pp �� ������ x = r->x_max
      if (code[i] & detail::LEFT)
      {
        pp->y += (p1.y - p2.y) * (r.minX() - pp->x) / (p1.x - p2.x);
        pp->x = r.minX();
      }
      else if (code[i] & detail::RIGHT)
      {
        pp->y += (p1.y - p2.y) * (r.maxX() - pp->x) / (p1.x - p2.x);
        pp->x = r.maxX();
      }

      // ���� pp ���� r, �� ����������� pp �� ������ y = r->y_min
      // ���� pp ���� r, �� ����������� pp �� ������ y = r->y_max
      if (code[i] & detail::BOT)
      {
        pp->x += (p1.x - p2.x) * (r.minY() - pp->y) / (p1.y - p2.y);
        pp->y = r.minY();
      }
      else if (code[i] & detail::TOP)
      {
        pp->x += (p1.x - p2.x) * (r.maxY() - pp->y) / (p1.y - p2.y);
        pp->y = r.maxY();
      }

      // ��������� ���
      code[i] = detail::vcode(r, *pp);
    }

    // ��� ���� ����� 0, ������������� ��� ����� � ��������������
    return true;
  }
}
