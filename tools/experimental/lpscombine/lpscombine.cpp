// Author(s): Maurice Laveaux
// Copyright: see the accompanying file COPYING or copy at
// https://github.com/mCRL2org/mCRL2/blob/master/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include "mcrl2/lps/io.h"
#include "mcrl2/lps/stochastic_specification.h"
#include "mcrl2/utilities/input_input_output_tool.h"
#include "mcrl2/process/print.h"

#include <boost/algorithm/string.hpp>

namespace mcrl2
{

using log::log_level_t;
using mcrl2::utilities::tools::input_input_output_tool;

using namespace mcrl2::lps;
using namespace mcrl2::process;


/// \returns A list of action labels that occur in the given multi-action.
core::identifier_string_list nodata(const multi_action& multi_action)
{
  core::identifier_string_list names;
  for (const auto& action : multi_action.actions())
  {
    names.emplace_front(action.label().name());
  }
  return names;
}

/// \brief Replaced sync{left,right}_i by sync_i
std::string convert_sync(const std::string& name, const std::string& prefix)
{
  // Only consider the indices.
  auto index = name.find_last_of("_");
  return std::string(prefix) += std::string("sync") += name.substr(index);
}

/// \returns The allowed action set A for the given specification.
std::set<action_name_multiset> compute_A(const lps::stochastic_specification& spec)
{
  std::set<action_name_multiset> result;

  // For every multiact alpha compute bar alpha, replace occurences of syncleft_i by sync_i
  for (const auto& summand : spec.process().action_summands())
  {
    auto names = nodata(summand.multi_action());

    // Construct a new list where the sync actions are removed.
    core::identifier_string_list removed_sync;
    for (const auto& name : names)
    {
      if (static_cast<std::string>(name).find("sync") == std::string::npos)
      {
        removed_sync.push_front(name);
      }
    }

    if (!removed_sync.empty())
    {
      // Do not include empty lists, as this breaks the allow expression.
      result.emplace(removed_sync);
    }
  }

  return result;
}

/// \brief
inline
void combine_specification(const lps::stochastic_specification& left_spec,
  const lps::stochastic_specification& right_spec,
  const std::string& prefix,
  std::ostream& stream)
{
  stream << left_spec.data();

  // Merge the action labels as some actions might not occur in only one component.
  core::detail::apply_printer<process::detail::printer> printer(stream);
  std::set<action_label> actions(left_spec.action_labels().begin(), left_spec.action_labels().end());
  actions.insert(right_spec.action_labels().begin(), right_spec.action_labels().end());

  action_label_list action_labels(actions.begin(), actions.end());

  // Find syncleft_i synchronization actions and define sync_i for each in i in I.

  // H = {actsync_i | i in I}. However, we only need to consider existing indices.
  core::identifier_string_list H;

  // The name of a left synchronisation action.
  std::string syncleft(prefix);
  syncleft += "syncleft";

  for (const auto& action : left_spec.action_labels())
  {
    std::string name = action.name();
    if (name.find(syncleft) != std::string::npos)
    {
      H.emplace_front(convert_sync(name, prefix));

      // Also insert the new action label into the action_label
      action_labels.emplace_front(action_label(H.front(), action.sorts()));
    }
  }

  printer.print_action_declarations(action_labels, "act  ",";\n\n", ";\n     ");

  // Print the process P, the left component.
  stream << left_spec.process() << "\n";

  // Internally we are going to construct a merge of two (linear process) specifications. However, first we need to rename
  // the process P from the right LPS to Q.
  std::stringstream right_process;
  right_process << right_spec.process();

  std::string replacement(right_process.str());
  boost::replace_all(replacement, "P(", "Q(");

  // Print the process Q.
  stream << replacement << "\n";

  // H_prime = {tag_left, tag_right}
  core::identifier_string_list H_prime;
  H_prime.emplace_front("tag");

  // A = {alpha_i or alpha_i + tag{left,right} depending on i}. This is equivalent to the multi-actions that
  // are generated by summands without sync{left,right} actions.
  action_name_multiset_list A;

  std::set<action_name_multiset> allowed(compute_A(left_spec));
  {
    // Determine the allowed set for the right specification and compute the union.
    auto allowed_right(compute_A(right_spec));
    allowed.insert(allowed_right.begin(), allowed_right.end());
  }

  A = action_name_multiset_list(allowed.begin(), allowed.end());

  // C = { syncleft_i | syncright_i -> sync_i | i in I }.
  communication_expression_list C;

  for (const auto& actsync : H)
  {
    // The sync_i action must be renamed to syncleft_i and syncright_i and put in a communication expression.
    std::string name = actsync;
    auto index = name.find_last_of("_");

    core::identifier_string_list list;
    list.emplace_front(name.substr(0, index) += std::string("left") += name.substr(index));
    list.emplace_front(name.substr(0, index) += std::string("right") += name.substr(index));
    C.emplace_front(action_name_multiset(list), name);
  }

  // Construct the composed process equation PQ = C(P || Q) where C is the context defined in the paper.
  process_identifier PQ(core::identifier_string("PQ"), data::variable_list());

  stream << "init " << hide(H_prime,
      allow(A,
        hide(H,
          comm(C,
            merge(
              process_instance(process_identifier("P", left_spec.process().process_parameters()), left_spec.initial_process().expressions()),
              process_instance(process_identifier("Q", right_spec.process().process_parameters()), right_spec.initial_process().expressions())
            )
          )
        )
      )
    )    
    << ";" ;
}

class lpscleave_tool : public input_input_output_tool
{
  typedef input_input_output_tool super;

public:
  lpscleave_tool() : super(
      "lpscleave",
      "Maurice Laveaux",
      "Cleaves LPSs",
      "Combines two linear process specifications (LPS) obtained from lpscleave "
      "in INFILE1 and INFILE2 and writes the resulting mCRL2 specification to OUTFILE1. "
      "This is mainly a debugging tool to ensure that lpscleave results in a strongly bisimilar "
      "decomposition. Internally uses string replacement to rename the right process to Q, which "
      "is not robust. "
      "If INFILE2 is not present, stdin is used.")
  {}

  bool run() override
  {
    // Load the processes.
    lps::stochastic_specification left_spec;
    load_lps(left_spec, input_filename1());

    lps::stochastic_specification right_spec;

    if (input_filename2().empty())
    {
      load_lps(right_spec, std::cin);
    }
    else
    {
      load_lps(right_spec, input_filename2());
    }

    if (output_filename().empty())
    {
      combine_specification(left_spec, right_spec, m_action_prefix, std::cout);
    }
    else
    {
      std::ofstream file(output_filename());
      combine_specification(left_spec, right_spec, m_action_prefix, file);
    }

    return true;
  }

protected:
  void add_options(utilities::interface_description& desc) override
  {
    super::add_options(desc);

    desc.add_option("prefix", utilities::make_mandatory_argument("PREFIX"),"Add a prefix to the synchronisation action labels to ensure that do not already occur in the specification", 'f');
  }

  void parse_options(const utilities::command_line_parser& parser) override
  {
    super::parse_options(parser);


    if (parser.options.count("prefix"))
    {
      m_action_prefix = parser.option_argument_as< std::string >("prefix");
    }
  }

private:
  std::string m_action_prefix;
};

} // namespace mcrl2

int main(int argc, char** argv)
{
  return mcrl2::lpscleave_tool().execute(argc, argv);
}
