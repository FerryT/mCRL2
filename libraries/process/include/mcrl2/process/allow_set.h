// Author(s): Wieger Wesselink
// Copyright: see the accompanying file COPYING or copy at
// https://svn.win.tue.nl/trac/MCRL2/browser/trunk/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file mcrl2/process/allow_set.h
/// \brief add your file description here.

#ifndef MCRL2_PROCESS_ALLOW_SET_H
#define MCRL2_PROCESS_ALLOW_SET_H

#include <algorithm>
#include "mcrl2/process/alphabet_operations.h"
#include "mcrl2/process/utility.h"

namespace mcrl2 {

namespace process {

struct allow_set;
std::ostream& operator<<(std::ostream& out, const allow_set& x);

/// \brief Represents the set AI*. If the attribute A_includes_subsets is true, also subsets of the elements are included.
/// An invariant of the allow_set is that elements of A do not contain elements of I. This invariant will be
/// established during construction.
struct allow_set
{
  multi_action_name_set A;
  bool A_includes_subsets;
  std::set<core::identifier_string> I;

  void establish_invariant()
  {
    multi_action_name_set A1;
    for (const multi_action_name& i : A)
    {
      A1.insert(alphabet_operations::hide(I, i));
    }
    std::swap(A, A1);
    assert(check_invariant());
  }

  bool check_invariant() const
  {
    using utilities::detail::contains;
    if (I.empty())
    {
      return true;
    }
    for (const multi_action_name& alpha: A)
    {
      for (const core::identifier_string& j: alpha)
      {
        if (contains(I, j))
        {
          return false;
        }
      }
    }
    return true;
  }

  allow_set()
  {}

  allow_set(const multi_action_name_set& A_, bool A_includes_subsets_ = false, const std::set<core::identifier_string>& I_ = std::set<core::identifier_string>())
    : A_includes_subsets(A_includes_subsets_), I(I_)
  {
    for (const multi_action_name& i: A_)
    {
      A.insert(alphabet_operations::hide(I_, i));
    }
  }

  /// \brief Returns true if the allow set contains the multi action name alpha.
  bool contains(const multi_action_name& alpha) const
  {
    multi_action_name beta = alphabet_operations::hide(I, alpha);
    return beta.empty() || (A_includes_subsets ? alphabet_operations::includes(A, beta) : alphabet_operations::contains(A, beta));
  }

  /// \brief Returns the intersection of the allow set with alphabet.
  multi_action_name_set intersect(const multi_action_name_set& alphabet) const
  {
    multi_action_name_set result;
    for (const multi_action_name& alpha: alphabet)
    {
      if (contains(alpha))
      {
        result.insert(alpha);
      }
    }
    multi_action_name tau;
    if (alphabet.find(tau) != alphabet.end())
    {
      result.insert(tau);
    }
    return result;
  }

  bool operator==(const allow_set& other) const
  {
    return A == other.A && A_includes_subsets == other.A_includes_subsets && I == other.I;
  }

  bool operator<(const allow_set& other) const
  {
    if (A_includes_subsets != other.A_includes_subsets)
    {
      return A_includes_subsets < other.A_includes_subsets;
    }
    if (A.size() != other.A.size())
    {
      return A.size() < other.A.size();
    }
    if (I.size() != other.I.size())
    {
      return I.size() < other.I.size();
    }
    auto m = std::mismatch(A.begin(), A.end(), other.A.begin());
    if (m.first != A.end())
    {
      return *m.first < *m.second;
    }
    return I < other.I;
  }
};

inline
std::ostream& operator<<(std::ostream& out, const allow_set& x)
{
  if (!x.A.empty())
  {
    out << pp(x.A) << (x.A_includes_subsets ? "@" : "");
  }
  if (!x.I.empty())
  {
    out << "{" << core::pp(x.I) << "}*";
  }
  if (x.A.empty() && x.I.empty())
  {
    out << "{}";
  }
  return out;
}

// operations on allow_set

namespace alphabet_operations {

inline
allow_set block(const core::identifier_string_list& B, const allow_set& x)
{
  if (x.A_includes_subsets)
  {
    return allow_set(alphabet_operations::hide(B, x.A), x.A_includes_subsets, alphabet_operations::hide(B, x.I));
  }
  else
  {
    return allow_set(alphabet_operations::block(B, x.A), x.A_includes_subsets, alphabet_operations::hide(B, x.I));
  }
}

inline
allow_set hide_inverse(const core::identifier_string_list& I, const allow_set& x)
{
  allow_set result = x;
  result.A = alphabet_operations::hide_inverse(I, result.A, result.A_includes_subsets);
  result.I.insert(I.begin(), I.end());
  result.establish_invariant();
  return result;
}

inline
allow_set allow(const action_name_multiset_list& V, const allow_set& x)
{
  multi_action_name_set A;
  for (const action_name_multiset& v: V)
  {
    const core::identifier_string_list& names = v.names();
    multi_action_name beta(names.begin(), names.end());
    bool add = x.A_includes_subsets ? alphabet_operations::includes(x.A, beta) : alphabet_operations::contains(x.A, alphabet_operations::hide(x.I, beta));
    if (add)
    {
      A.insert(beta);
    }
  }
  return allow_set(A);
}

inline
allow_set rename_inverse(const rename_expression_list& R, const allow_set& x)
{
  allow_set result(alphabet_operations::rename_inverse(R, x.A, x.A_includes_subsets), x.A_includes_subsets, rename_inverse(R, x.I));
  mCRL2log(log::debug1) << "rename_inverse(" << R << ", " << x << ") = " << result << std::endl;
  return result;
}

inline
allow_set comm_inverse(const communication_expression_list& C, const allow_set& x)
{
  allow_set result(comm_inverse1(C, x.A), x.A_includes_subsets, comm_inverse(C, x.I));
  mCRL2log(log::debug1) << "comm_inverse(" << C << ", " << x << ") = " << result << std::endl;
  return result;
}

inline
allow_set left_arrow(const allow_set& x, const multi_action_name_set& A)
{
  allow_set result = x;
  if (!x.A_includes_subsets)
  {
    result.A = left_arrow2(x.A, x.I, A);
  }
  result.establish_invariant();
  mCRL2log(log::debug1) << "left_arrow(" << x << ", " << process::pp(A) << ") = " << result << std::endl;
  return result;
}

inline
allow_set subsets(const allow_set& x)
{
  allow_set result = x;
  result.A_includes_subsets = true;
  result.A = alphabet_operations::remove_subsets(result.A);
  result.establish_invariant();
  return result;
}

} // namespace alphabet_operations

} // namespace process

} // namespace mcrl2

#endif // MCRL2_PROCESS_ALLOW_SET_H