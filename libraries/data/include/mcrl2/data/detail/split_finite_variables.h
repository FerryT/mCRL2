// Author(s): Wieger Wesselink
// Copyright: see the accompanying file COPYING or copy at
// https://github.com/mCRL2org/mCRL2/blob/master/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file mcrl2/data/detail/split_finite_variables.h
/// \brief add your file description here.

#ifndef MCRL2_DATA_DETAIL_SPLIT_FINITE_VARIABLES_H
#define MCRL2_DATA_DETAIL_SPLIT_FINITE_VARIABLES_H

#include "mcrl2/data/data_specification.h"

namespace mcrl2 {

namespace data {

namespace detail {

/// \brief Computes the subset with variables of finite sort and infinite.
// TODO: this should be done more efficiently, by avoiding aterm lists
/// \param variables A sequence of data variables
/// \param data A data specification
/// \param finite_variables A sequence of data variables
/// \param infinite_variables A sequence of data variables
inline
void split_finite_variables(data::variable_list variables, 
                            const data::data_specification& data, 
                            data::variable_list& finite_variables, 
                            data::variable_list& infinite_variables)
{
  for (data::variable_list::const_reverse_iterator i = variables.rbegin(); i!=variables.rend(); i++)
  {
    if (data.is_certainly_finite(i->sort()))
    {
      finite_variables.push_front(*i);
    }
    else
    {
      infinite_variables.push_front(*i);
    }
  }
}

template <typename Rewriter>
inline
void split_enumerable_variables(data::variable_list variables, 
                                const data::data_specification& data, 
                                const Rewriter& rewr, 
                                data::variable_list& enumerable_variables, 
                                data::variable_list& non_enumerable_variables)
{
  for (data::variable_list::const_reverse_iterator i = variables.rbegin(); i!=variables.rend(); i++)
  {
    if (is_enumerable(data, rewr, i->sort()))
    {
      enumerable_variables.push_front(*i);
    }
    else
    {
      non_enumerable_variables.push_front(*i);
    }
  }
}

} // namespace detail

} // namespace data

} // namespace mcrl2

#endif // MCRL2_DATA_DETAIL_SPLIT_FINITE_VARIABLES_H
