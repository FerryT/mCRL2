// Author(s): Gijs Kant
// Copyright: see the accompanying file COPYING or copy at
// https://svn.win.tue.nl/trac/MCRL2/browser/trunk/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
/// \file pbes_explorer_test.cpp
/// \brief Test for the PBES_Explorer interface.

#include <iostream>
#include <cstdio>
#include <boost/test/included/unit_test_framework.hpp>
#include "mcrl2/utilities/test_utilities.h"
#include "mcrl2/pbes/txt2pbes.h"
#include "mcrl2/pbes/detail/pbes_greybox_interface.h"
#include "mcrl2/pbes/normalize.h"
#include "mcrl2/pbes/pbes_explorer.h"
#include "mcrl2/pbes/io.h"
#include "mcrl2/pbes/detail/ppg_rewriter.h"

using namespace mcrl2;
using namespace mcrl2::pbes_system;

using mcrl2::utilities::collect_after_test_case;
BOOST_GLOBAL_FIXTURE(collect_after_test_case)

namespace ltsmin
{

namespace test
{

class explorer : public mcrl2::pbes_system::explorer {
private:
    std::set<std::vector<int> > visited;
    std::deque<std::vector<int> > fresh;
    size_t transition_count;

public:
#ifdef MCRL2_JITTYC_AVAILABLE
    explorer(const pbes& p, const std::string& rewrite_strategy = "jittyc", bool reset = false, bool always_split = false) :
#else
    explorer(const pbes& p, const std::string& rewrite_strategy = "jitty", bool reset = false, bool always_split = false) :
#endif
        mcrl2::pbes_system::explorer(p, rewrite_strategy, reset, always_split),
        transition_count(0)
    {}

    explorer(const std::string& f, const std::string& rewrite_strategy = "jittyc", bool reset = false, bool always_split = false) :
      mcrl2::pbes_system::explorer(f, rewrite_strategy, reset, always_split),
      transition_count(0)
    {}

    size_t get_state_count() const
    {
        return visited.size();
    }

    size_t get_transition_count() const
    {
        return transition_count;
    }

    const std::set<std::vector<int> >& get_visited() const
    {
        return visited;
    }

    std::vector<int> to_int_vector(int size, int* data)
    {
      std::vector<int> result;
      for(int i=0; i < size; i++)
      {
        // testing serialisation/deserialisation
        int type_no = this->get_info()->get_lts_type().get_state_type_no(i);
        std::string s = this->get_value(type_no, data[i]);
        //std::clog << "Testing (de)serialisation of expression: " << s << std::endl;
        int index = this->get_index(type_no, s);
        BOOST_CHECK(index==data[i]);

        result.push_back(data[i]);
      }
      return result;
    }

    void from_int_vector(int size, std::vector<int> data, int* dst)
    {
      BOOST_CHECK(size==(int)data.size());
      auto it = data.begin();
      for(int i=0; i < size; i++, it++)
      {
        dst[i] = *it;
      }
    }

    template <typename callback>
    void next_state_long(int* const& src, std::size_t group, callback& f)
    {
        mcrl2::pbes_system::explorer::next_state_long(src, group, f);
    }

    template <typename callback>
    void next_state_all(int* const& src, callback& f)
    {
        mcrl2::pbes_system::explorer::next_state_all(src, f);
    }

    void initial_state(int* state)
    {
        mcrl2::pbes_system::explorer::initial_state(state);
    }

    int state_label(int label, int* const& s)
    {
        std::string varname = this->get_string_value(s[0]);
        if (label==0)
        {
            int priority = map_at(this->get_info()->get_variable_priorities(), varname);
            return priority;
        }
        else if (label==1)
        {
            lts_info::operation_type type = map_at(this->get_info()->get_variable_types(), varname);
            return type==parity_game_generator::PGAME_AND ? 1 : 0;
        }
        return 0;
    }

    void bfs();
};

struct pbes_state_cb
{
    ltsmin::test::explorer* explorer;
    std::vector<std::vector<int> > successors;
    size_t count;

    pbes_state_cb (ltsmin::test::explorer* explorer_)
        : explorer(explorer_), count(0)
    {}

    void operator()(int* const& next_state,
                    int group = -1)
    {
        int state_length = explorer->get_info()->get_lts_type().get_state_length();
        successors.push_back(explorer->to_int_vector(state_length, next_state));
        count++;
    }

    const std::vector<std::vector<int> >& get_successors() const
    {
        return successors;
    }

    size_t get_count() const
    {
        return count;
    }
};

void explorer::bfs()
{
    int state_length = get_info()->get_lts_type().get_state_length();
    int num_rows = get_info()->get_number_of_groups();
    // int initial_state[state_length]; N.B. This is not portable C++
    MCRL2_SYSTEM_SPECIFIC_ALLOCA(initial_state, int, state_length);
    this->initial_state(initial_state);
    std::vector<int> initial_state_vector = this->to_int_vector(state_length, initial_state);
    visited.insert(initial_state_vector);
    fresh.push_back(initial_state_vector);
    // int state[state_length]; N.B. This is not portable C++
    MCRL2_SYSTEM_SPECIFIC_ALLOCA(state, int, state_length);
    while (!fresh.empty()) {
        std::vector<int> state_vector = fresh.front();
        fresh.pop_front();
        from_int_vector(state_length, state_vector, state);
        ltsmin::test::pbes_state_cb f(this);
        this->next_state_all(state, f);
        std::vector<std::vector<int> > successors = f.get_successors();

        std::set<std::vector<int> > succ_set;
        //std::clog << successors.size() << " successors" << std::endl;
        transition_count += successors.size();
        for(std::vector<std::vector<int> >::iterator succ = successors.begin(); succ != successors.end(); ++succ)
        {
          std::vector<int> s = *succ;
          std::pair<std::set<std::vector<int> >::iterator,bool> ret;
          ret = visited.insert(s);
          if (ret.second)
          {
            fresh.push_back(s);
          }
          succ_set.insert(s);
        }
        // check if the result is the same if we use groups:

        std::set<std::vector<int> > succ_set_groups;
        for(int g=0; g < num_rows; g++)
        {
            ltsmin::test::pbes_state_cb f(this);
            this->next_state_long(state, g, f);
            std::vector<std::vector<int> > successors_groups = f.get_successors();

            for(std::vector<std::vector<int> >::iterator succ = successors_groups.begin(); succ != successors_groups.end(); ++succ)
            {
                std::vector<int> s = *succ;
                succ_set_groups.insert(s);
            }
        }
        BOOST_CHECK(succ_set==succ_set_groups);
    }
}

} // namespace test

} // namespace ltsmin


void run_pbes_explorer(std::string pbes_text, int num_parts, int num_groups, int num_states, int num_transitions,
    const std::string& rewrite_strategy = "jitty")
{
  std::clog << "run_pbes_explorer" << std::endl;
  pbes p = txt2pbes(pbes_text);
  normalize(p);
  if (!is_ppg(p))
  {
    std::clog << "Rewriting to PPG..." << std::endl;
    p = to_ppg(p);
    std::clog << "done." << std::endl;
  }
  ltsmin::test::explorer* pbes_explorer = new ltsmin::test::explorer(p, rewrite_strategy);
  lts_info* info = pbes_explorer->get_info();
  int state_length = info->get_lts_type().get_state_length();
  BOOST_CHECK(num_parts==state_length);
  //std::clog << state_length << " parts" << std::endl;
  //int num_rows = info->get_number_of_groups();
  //std::clog << num_rows << " groups" << std::endl;
  //BOOST_CHECK(num_groups==num_rows);
  std::map<int,std::vector<bool> > matrix = info->get_dependency_matrix();
  std::map<int,std::vector<bool> > read_matrix = info->get_read_matrix();
  std::map<int,std::vector<bool> > write_matrix = info->get_write_matrix();
  (void)matrix;
  (void)read_matrix;
  (void)write_matrix;
  // TODO: check matrices ...

  pbes_explorer->bfs();
  // check number of states and transitions:
  //BOOST_CHECK(num_states==(int)pbes_explorer->get_state_count());
  //BOOST_CHECK(num_transitions==(int)pbes_explorer->get_transition_count());
  delete pbes_explorer;

}

void run_pbes_explorer_file(std::string filename, int num_parts, int num_groups, int num_states, int num_transitions,
    const std::string& rewrite_strategy = "jitty")
{
  std::clog << "run_pbes_explorer_file" << std::endl;
  ltsmin::test::explorer* pbes_explorer = new ltsmin::test::explorer(filename, rewrite_strategy);
  lts_info* info = pbes_explorer->get_info();
  int state_length = info->get_lts_type().get_state_length();
  BOOST_CHECK(num_parts==state_length);
  //std::clog << state_length << " parts" << std::endl;
  //int num_rows = info->get_number_of_groups();
  //std::clog << num_rows << " groups" << std::endl;
  //BOOST_CHECK(num_groups==num_rows);
  std::map<int,std::vector<bool> > matrix = info->get_dependency_matrix();
  std::map<int,std::vector<bool> > read_matrix = info->get_read_matrix();
  std::map<int,std::vector<bool> > write_matrix = info->get_write_matrix();
  (void)matrix;
  (void)read_matrix;
  (void)write_matrix;
  // TODO: check matrices ...

  pbes_explorer->bfs();
  // check number of states and transitions:
  //BOOST_CHECK(num_states==(int)pbes_explorer->get_state_count());
  //BOOST_CHECK(num_transitions==(int)pbes_explorer->get_transition_count());
  delete pbes_explorer;
}


BOOST_AUTO_TEST_CASE(buffer)
{
  // buffer.nodeadlock.pbesparelm
  std::string pbes_text =
    "sort D = struct d1 | d2;\n"
    "map  N: Pos;\n"
    "eqn  N  =  2;\n"
    "pbes nu X(q_Buffer: List(D)) =\n"
    "  (val(#q_Buffer < 2) || val(!(q_Buffer == [])))\n"
    "  && (forall d_Buffer1: D. val(!(#q_Buffer < 2)) || X(q_Buffer <| d_Buffer1))\n"
    "  && (val(!!(q_Buffer == [])) || X(tail(q_Buffer)));\n"
    "init X([]);"
  ;
  int num_parts = 2; // 1 var + 1 parameter
  int num_groups = 3; // each of the conjuncts
  int num_states = 7;
  int num_transitions = 12;
  run_pbes_explorer(pbes_text, num_parts, num_groups, num_states, num_transitions, "jitty");
#ifdef MCRL2_JITTYC_AVAILABLE
  run_pbes_explorer(pbes_text, num_parts, num_groups, num_states, num_transitions, "jittyc");
#endif
}
BOOST_AUTO_TEST_CASE(buffer_2_read_then_eventually_send_pbesparelm_simple)
{
  std::string pbes_text =
      "sort D = struct d1 | d2;\n"
      "map  M: Pos;\n"
      "eqn  M  =  2;\n"
      "pbes nu Z(q_In,q_Out: List(D)) =\n"
      "  (forall d: D. forall d_In2: D. val(!(d_In2 == d)) || val(!(#q_In < 2)) || X(q_In <| d_In2, q_Out, d)) && (forall d_In1: D. val(!(#q_In < 2)) || Z(q_In <| d_In1, q_Out)) && (val(q_Out == []) || Z(q_In, tail(q_Out))) && (val(!(!(q_In == []) && #q_Out < 2)) || Z(tail(q_In), q_Out <| head(q_In)));\n"
      "nu X(q_In,q_Out: List(D), d: D) =\n"
      "  Y(q_In, q_Out, d);\n"
      "mu Y(q_In,q_Out: List(D), d: D) =\n"
      "  (val(!(head(q_Out) == d)) || val(q_Out == []) || X(q_In, tail(q_Out), d)) && (forall d_In1: D. val(!(#q_In < 2)) || Y(q_In <| d_In1, q_Out, d)) && (val(head(q_Out) == d) || val(q_Out == []) || Y(q_In, tail(q_Out), d)) && (val(!(!(q_In == []) && #q_Out < 2)) || Y(tail(q_In), q_Out <| head(q_In), d));\n"
      "init Z([], []);\n"
  ;
  int num_parts = 4; // 1 var + 3 parameters: q_In, q_Out, d
  int num_groups = 9; // each of the conjuncts of every equation
  int num_states = 213;
  int num_transitions = 414;

  std::string pbes_filename = utilities::temporary_filename("pbes_explorer_test");
  save_pbes(txt2pbes(pbes_text), pbes_filename);

  run_pbes_explorer(pbes_text, num_parts, num_groups, num_states, num_transitions, "jitty");
  run_pbes_explorer_file(pbes_filename, num_parts, num_groups, num_states, num_transitions, "jitty");
#ifdef MCRL2_JITTYC_AVAILABLE
  run_pbes_explorer(pbes_text, num_parts, num_groups, num_states, num_transitions, "jittyc");
  run_pbes_explorer_file(pbes_filename, num_parts, num_groups, num_states, num_transitions, "jittyc");
#endif
  // clean up
  std::remove(pbes_filename.c_str());
}

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[])
{
  log::mcrl2_logger::set_reporting_level(log::debug);
  return 0;
}
