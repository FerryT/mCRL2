// Author(s): Wieger Wesselink
// Copyright: see the accompanying file COPYING or copy at
// https://svn.win.tue.nl/trac/MCRL2/browser/trunk/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file mcrl2/pbes/detail/is_pfnf.h
/// \brief add your file description here.

#ifndef MCRL2_PBES_DETAIL_IS_PFNF_H
#define MCRL2_PBES_DETAIL_IS_PFNF_H

#include "mcrl2/pbes/pbes_expression.h"
#include "mcrl2/pbes/traverser.h"
#include "mcrl2/pbes/pbes_functions.h"

namespace mcrl2 {

namespace pbes_system {

namespace detail {

inline
bool is_pfnf_or_expression(const pbes_expression& x)
{
  if (is_or(x))
  {
    return is_pfnf_or_expression(or_(x).left()) && is_pfnf_or_expression(or_(x).right());
  }
  else
  {
    return is_propositional_variable_instantiation(x);
  }
}

// Determines if an expression has the format \/_{j in Ji} Xj(ej)
inline
bool is_pfnf_or(const pbes_expression& x)
{
  return is_true(x) || is_pfnf_or_expression(x);
}

// Determines if an expression has the format g_i => x with is_pfnf_or(x)
inline
bool is_pfnf_imp(const pbes_expression& x)
{
  if (is_imp(x))
  {
    return  (is_simple_expression(imp(x).left()) && is_pfnf_or(imp(x).right()))
         || (is_simple_expression(imp(x).right()) && is_pfnf_or(imp(x).left()));
  }
  else
  {
    return is_pfnf_or(x);
  }
}

// Determines if an expression has the format /\_{i in I} x_i with is_pfnf_imp(x_i)
inline
bool is_pfnf_inner_and(const pbes_expression& x)
{
  atermpp::set<pbes_expression> terms = pbes_expr::split_and(x);
  for (atermpp::set<pbes_expression>::const_iterator i = terms.begin(); i != terms.end(); ++i)
  {
    if (!is_pfnf_imp(*i))
    {
      return false;
    }
  }
  return true;
}

// Determines if an expression has the format h /\ x with is_pfnf_inner_and(x), where the 'h /\' part is optional
inline
bool is_pfnf_outer_and(const pbes_expression& x)
{
  if (is_and(x))
  {
    return (is_simple_expression(and_(x).left()) && is_pfnf_inner_and(and_(x).right()))
        || (is_simple_expression(and_(x).right()) && is_pfnf_inner_and(and_(x).left()));
  }
  else
  {
    return is_pfnf_inner_and(x);
  }
}

// Determines if an expression is in pfnf format
inline
bool is_pfnf_expression(const pbes_expression& x)
{
  if (is_forall(x))
  {
    return is_pfnf_expression(forall(x).body());
  }
  else if (is_exists(x))
  {
    return is_pfnf_expression(exists(x).body());
  }
  else
  {
    return is_pfnf_outer_and(x);
  }
}

struct is_pfnf_traverser: public pbes_expression_traverser<is_pfnf_traverser>
{
  typedef pbes_expression_traverser<is_pfnf_traverser> super;
  using super::enter;
  using super::leave;
  using super::operator();

  bool result;

  is_pfnf_traverser()
    : result(true)
  {}

  void operator()(const pbes_expression& x)
  {
    result = is_pfnf_expression(x) && result;
  }
};

template <typename T>
bool is_pfnf(const T& x)
{
  is_pfnf_traverser f;
  f(x);
  return f.result;
}

} // namespace detail

} // namespace pbes_system

} // namespace mcrl2

#endif // MCRL2_PBES_DETAIL_IS_PFNF_H
