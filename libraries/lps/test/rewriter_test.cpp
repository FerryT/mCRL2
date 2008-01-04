// Author(s): Wieger Wesselink
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file rewriter_test.cpp
/// \brief Add your file description here.

#include <cstdlib>
#include <string>
#include <algorithm>
#include <boost/test/minimal.hpp>
#include "mcrl2/lps/mcrl22lps.h"
#include "mcrl2/data/sort_identifier.h"
#include "mcrl2/data/rewriter.h"
#include "mcrl2/lps/specification.h"
#include "mcrl2/lps/mcrl22lps.h"

using namespace std;
using namespace atermpp;
using namespace lps;
using namespace lps::detail;

const std::string SPECIFICATION =
"sort Natural;                                    \n" 
"                                                 \n"
"cons _0: Natural;                                \n"
"    S: Natural -> Natural;                       \n"
"                                                 \n"
"map  eq: Natural # Natural -> Bool;              \n"
"    less: Natural # Natural -> Bool;             \n"
"    plus: Natural # Natural -> Natural;          \n"
"    _1, _2, _3, _4, _5, _6, _7, _8, _9: Natural; \n"
"    P: Natural -> Natural;                       \n"
"    even: Natural -> Bool;                       \n"
"                                                 \n"
"var  n, m: Natural;                              \n"
"                                                 \n"
"eqn  eq(n, n) = true;                            \n"
"    eq(S(n), _0) = false;                        \n"
"    eq(_0, S(m)) = false;                        \n"
"    eq(S(n), S(m)) = eq(n, m);                   \n"
"                                                 \n"
"    less(n, n) = false;                          \n"
"    less(n, _0) = false;                         \n"
"    less(_0, S(m)) = true;                       \n"
"    less(S(n), S(m)) = less(n, m);               \n"
"                                                 \n"
"    plus(_0, n) = n;                             \n"
"    plus(n, _0) = n;                             \n"
"    plus(S(n), m) = S(plus(n, m));               \n"
"    plus(n, S(m)) = S(plus(n, m));               \n"
"                                                 \n"
"    even(_0) = true;                             \n"
"    even(S(n)) = !(even(n));                     \n"
"                                                 \n"
"    P(S(n)) = n;                                 \n"
"                                                 \n"
"    _1 = S(_0);                                  \n"
"    _2 = S(_1);                                  \n"
"    _3 = S(_2);                                  \n"
"    _4 = S(_3);                                  \n"
"    _5 = S(_4);                                  \n"
"    _6 = S(_5);                                  \n"
"    _7 = S(_6);                                  \n"
"    _8 = S(_7);                                  \n"
"    _9 = S(_8);                                  \n"
"                                                 \n"
"act a: Natural;                                  \n"
"                                                 \n"
"proc P(n: Natural) = sum m: Natural. a(m). P(m); \n"
"                                                 \n"
"init P(_0);                                      \n"
;

struct sort_has_name
{
  std::string m_name;
    
  sort_has_name(std::string name)
    : m_name(name)
  {}
  
  bool operator()(sort_expression s) const
  {
    return is_sort_identifier(s) && std::string(sort_identifier(s).name()) == s;
  }
};

struct data_operation_has_name
{
  std::string m_name;
    
  data_operation_has_name(std::string name)
    : m_name(name)
  {}
  
  bool operator()(data_operation c) const
  {
    return std::string(c.name()) == m_name;
  }
};

/// Find the mapping named s.
data_operation find_mapping(data_specification data, std::string s)
{
  data_operation_list::iterator i = std::find_if(data.mappings().begin(), data.mappings().end(), data_operation_has_name(s));
  if (i == data.mappings().end())
  {
    return data_operation();
  }
  return *i;
}

/// Find the constructor named s.
data_operation find_constructor(data_specification data, std::string s)
{
  data_operation_list::iterator i = std::find_if(data.constructors().begin(), data.constructors().end(), data_operation_has_name(s));
  if (i == data.constructors().end())
  {
    return data_operation();
  }
  return *i;
}

/// Find the sort named s.
sort_expression find_sort(data_specification data, std::string s)
{
  sort_expression_list::iterator i = std::find_if(data.sorts().begin(), data.sorts().end(), sort_has_name(s));
  if (i == data.sorts().end())
  {
    return sort_expression();
  }
  return *i;
}

void test1()
{
  specification spec = mcrl22lps(SPECIFICATION);
  rewriter r(spec.data());

  data_expression n1    = find_mapping(spec.data(), "_1");
  data_expression n2    = find_mapping(spec.data(), "_2");
  data_expression n3    = find_mapping(spec.data(), "_3");
  data_expression n4    = find_mapping(spec.data(), "_4");
  data_expression n5    = find_mapping(spec.data(), "_5");
  data_expression n6    = find_mapping(spec.data(), "_6");
  data_expression n7    = find_mapping(spec.data(), "_7");
  data_expression n8    = find_mapping(spec.data(), "_8");
  data_expression n9    = find_mapping(spec.data(), "_9");
  data_expression plus  = find_mapping(spec.data(), "plus");
  
  cout << pp(data_application(plus, make_list(n4, n5))) << endl;
  BOOST_CHECK(r(data_application(plus, make_list(n4, n5))) == r(data_application(plus, make_list(n2, n7))));
}

int test_main(int argc, char** argv)
{
  MCRL2_CORE_LIBRARY_INIT()

  test1();

  return 0;
}
