#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstring>
#include "lpsparunfoldlib.h"
#include <iostream>
#include <string>
#include "mcrl2/core/messaging.h"
#include "mcrl2/core/print.h"
#include <iterator>
#include <mcrl2/lps/linear_process.h>

#include "mcrl2/data/function_symbol.h"
#include "mcrl2/data/classic_enumerator.h"
#include "mcrl2/data/data_specification.h"

using namespace std;
using namespace mcrl2;
using namespace mcrl2::core;
using namespace mcrl2::data;

/* Remarks
- replace on vectors does not work
- vector pretty print does not work
- alias::name() [basic_sort] results in a basic sort, differs form basic_sort::name() [string]
*/

lpsparunfold::lpsparunfold( mcrl2::lps::specification spec)
{
  gsDebugMsg("Preprocessing\n");
  m_data_specification = spec.data() ;
  m_lps = spec.process();
  m_init_process = spec.initial_process();
  m_action_label_list = spec.action_labels();

  for (data_specification::sorts_const_range::const_iterator i =  m_data_specification.sorts().begin();
                                                             i != m_data_specification.sorts().end();
                                                             ++i){
    bool processed = false;
    if (i->is_basic_sort()) {
      gsDebugMsg("\tFound Basic Sort: %s\n", std::string(basic_sort( *i ).name()).c_str() );
      sort_names.insert( (basic_sort(*i)).name() );
      processed = true;
    }

    if (i->is_alias()){
      gsDebugMsg("\tFound Alias Sort: %s\n", std::string(alias( *i ).name().name()).c_str() );
      sort_names.insert( (alias(*i)).name().name() );
      processed = true;
    }

    if (i->is_structured_sort()){
      gsDebugMsg("\tFound Structured Sort: %s\n", pp(structured_sort( *i )).c_str() );
      processed = true;
    }
    
    if (!processed){
      cerr << "Failed to preprocess sort: "<< *i << endl;
      abort(); 
    }
  };

  {
    int size = mapping_and_constructor_names.size();
    data_specification::constructors_const_range fsl= m_data_specification.constructors();
    for (data_specification::constructors_const_range::const_iterator i = fsl.begin();
                                                                      i != fsl.end();
                                                                      ++i){
      mapping_and_constructor_names.insert( i -> name() );
    };
    gsDebugMsg("\t");
    gsVerboseMsg("Specification has %d constructors\n",  mapping_and_constructor_names.size() - size );
  }

  {
    int size = mapping_and_constructor_names.size();
    data_specification::mappings_const_range fsl= m_data_specification.mappings();
    for (data_specification::mappings_const_range::const_iterator i = fsl.begin();
                                                                  i != fsl.end();
                                                                  ++i){
      mapping_and_constructor_names.insert( i -> name() );
    };
    gsDebugMsg("\t");
    gsVerboseMsg("Specification has %d mappings \n", mapping_and_constructor_names.size() - size );
  }
}

mcrl2::data::basic_sort lpsparunfold::generate_fresh_basic_sort( std::string str )
{
  //Generate a fresh Basic Sort
  mcrl2::data::postfix_identifier_generator generator = mcrl2::data::postfix_identifier_generator ("");
  generator.add_identifiers( sort_names );
  mcrl2::core::identifier_string nstr = generator( str );
  gsDebugMsg("\t");
  gsVerboseMsg("Generated a fresh sort for %s\n", string(str).c_str() );
  gsDebugMsg("Generated sort: \t%s\n", string(nstr).c_str() );
  sort_names.insert(nstr);
  return basic_sort( std::string(nstr) );
}

mcrl2::core::identifier_string lpsparunfold::generate_fresh_constructor_and_mapping_name(std::string str)
{
  //Generate a fresh name for a constructor of mapping
  mcrl2::data::postfix_identifier_generator generator = mcrl2::data::postfix_identifier_generator ("");
  generator.add_identifiers( mapping_and_constructor_names );
  mcrl2::core::identifier_string nstr = generator( str );
  gsDebugMsg("Generated a fresh mapping: %s\n", string(nstr).c_str() ); 
  mapping_and_constructor_names.insert( nstr );
  return nstr;
}

function_symbol_vector lpsparunfold::determine_affected_constructors()
{
  data_specification::constructors_const_range t = m_data_specification.constructors( m_unfold_process_parameter );
  function_symbol_vector k = function_symbol_vector( t.begin(), t.end() );    
  
  gsDebugMsg("k:\t");
  gsVerboseMsg("%s has %d constructor function(s)\n", unfold_parameter_name.c_str() , k.size() );
  for( function_symbol_vector::iterator i = k.begin(); i != k.end(); ++i )
  {
    gsDebugMsg( "\t%s\n", i->to_string().c_str() );
  }
  return k;
}


function_symbol_vector lpsparunfold::new_constructors( mcrl2::data::function_symbol_vector k )
{
  using namespace mcrl2::data;

  function_symbol_vector set_of_new_sorts;

  for( function_symbol_vector::iterator i = k.begin(); i != k.end(); ++i  )
  {

    std::string prefix = "c_";
    mcrl2::core::identifier_string fresh_name = generate_fresh_constructor_and_mapping_name( prefix.append( i -> name() ) );
    set_of_new_sorts.push_back( function_symbol( fresh_name , fresh_basic_sort ) );
    gsDebugMsg("\t%s\n", function_symbol( fresh_name , fresh_basic_sort ).to_string().c_str() );
    mapping_and_constructor_names.insert(fresh_name);

  }
  gsDebugMsg("\t");
  gsVerboseMsg("Created %d fresh \" c_ \" mapping function(s)\n", set_of_new_sorts.size());
  return set_of_new_sorts;
}

mcrl2::data::function_symbol lpsparunfold::create_case_function(int k)
{
  mcrl2::data::function_symbol fs;
  std::string str = "C_";
  str.append( fresh_basic_sort.name() );
  mcrl2::core::identifier_string idstr = generate_fresh_constructor_and_mapping_name( str );
  mcrl2::data::sort_expression_vector fsl;
  fsl.push_back(fresh_basic_sort);
  for(int i = 0; i < k; ++i)
  {
    fsl.push_back( m_unfold_process_parameter );
  }

  fs = function_symbol( idstr , mcrl2::data::function_sort( fsl, m_unfold_process_parameter ));

  gsDebugMsg("\t");
  gsVerboseMsg("Created C map: %s\n", fs.to_string().c_str());

  return fs;
}

mcrl2::data::function_symbol lpsparunfold::create_determine_function()
{
  mcrl2::data::function_symbol fs;
  std::string str = "Det_";
  str.append( fresh_basic_sort.name() );
  mcrl2::core::identifier_string idstr = generate_fresh_constructor_and_mapping_name( str );
  mcrl2::data::sort_expression_list fsl;
  fs = function_symbol( idstr , mcrl2::data::function_sort( m_unfold_process_parameter , fresh_basic_sort ));
  gsDebugMsg("\t%s\n", fs.to_string().c_str());

  return fs;
}

mcrl2::data::function_symbol_vector lpsparunfold::create_projection_functions(function_symbol_vector k)
{
  mcrl2::data::function_symbol_vector sfs;
  std::string str = "pi_";
  str.append( fresh_basic_sort.name() );

  std::set<mcrl2::data::sort_expression> processed;
  for( function_symbol_vector::iterator i = k.begin() ; i != k.end(); ++i )
  {
    if ( i->sort().is_function_sort() )
    {
      function_sort fs = function_sort( i->sort() );
      boost::iterator_range<sort_expression_list::const_iterator> sel  = fs.domain();
      for(sort_expression_list::const_iterator j = sel.begin(); j != sel.end(); j++ )
      {
          mcrl2::core::identifier_string idstr = generate_fresh_constructor_and_mapping_name( str );
          sfs.push_back(function_symbol( idstr , mcrl2::data::function_sort( m_unfold_process_parameter , *j )));
          processed.insert( *j );
      }
    }
  }
  for( function_symbol_vector::iterator i = sfs.begin(); i != sfs.end(); ++i )
  {
    gsDebugMsg( "\t%s\n", i->to_string().c_str() );
  }
  return sfs;
}

data_equation_vector lpsparunfold::create_data_equations(function_symbol_vector pi, function_symbol case_function, function_symbol_vector set_of_new_sorts, function_symbol_vector k, function_symbol determine_function)
{
  variable_vector vars;        /* Equation variables  */
  data_equation_vector del;    /* Generated equations */
  std::set<mcrl2::core::identifier_string> var_names; /* var_names */
  mcrl2::data::postfix_identifier_generator generator = mcrl2::data::postfix_identifier_generator ("");
  variable v;

  std::string fstr = "y";
  std::string cstr = "c";
  std::string dstr = "d";

  /* Generate variable identifier string for projection */
  generator.add_identifiers( var_names );
  mcrl2::core::identifier_string istr = generator( fstr );

  variable lv = variable( istr, m_unfold_process_parameter );
  for(int i = 0 ; i < int(k.size()) ; ++i){
    istr = generator( fstr );
    v = variable( istr, m_unfold_process_parameter );
    vars.push_back( v );
  }
  vars.push_back(lv);

  /* Creating variable for detector function */
  istr = generator( cstr );
  v = variable( istr, fresh_basic_sort );
  vars.push_back( v );

  /* Create Equations */
  int e = 0;
  gsDebugMsg("");
  gsVerboseMsg("Adding equations\n");
  for(function_symbol_vector::iterator i = k.begin(); i != k.end(); ++i){
    data_expression_vector args;
    args.push_back(set_of_new_sorts[e]);
    for(int j = 0 ; j < int(k.size()) ; ++j){
      args.push_back(vars[ j ]);
    }
    data_expression lhs = application(  case_function , mcrl2::data::data_expression_list(args.begin() , args.end()) );

    gsDebugMsg("\tAdded \"C_\" equation %s\n", pp(data_equation( lhs, vars[e] )).c_str());
    set< variable > svars = find_all_variables( lhs );
    set< variable > tmp_var = find_all_variables( vars[e] );
    svars.insert( tmp_var.begin(), tmp_var.end() );
    del.push_back( data_equation( variable_list(svars.begin(), svars.end()), lhs, vars[e] ) );
    ++e;
  }

  {
    data_expression_vector args;
    args.push_back( vars[e+1] );
    for(int j = 0 ; j < int(k.size()) ; ++j){
        args.push_back(vars[ e ]);
    }
    boost::iterator_range<data_expression_vector::const_iterator> arg (args); /* Omslachtig */
    data_expression lhs = application(  case_function , arg );
    gsDebugMsg("\tAdded \"C_\" equation %s\n", pp(data_equation( lhs, vars[e] )).c_str());
    set< variable > svars = find_all_variables( lhs );
    svars.insert(vars[e] );
    del.push_back( data_equation( variable_list(svars.begin(), svars.end()), lhs, vars[e] ) );
  }

  /* Creating Detector equations */
  std::map< sort_expression, variable_vector > sort_vars;//type_var_list;   /* Mapping for Sort |-> [Variable] */
  std::map< sort_expression, int             > sort_index;//type_var_count;  /* Mapping for counting the number of unique Sorts of an equation */

  e = 0;
  for(function_symbol_vector::iterator i = k.begin(); i != k.end(); ++i)
  {
    //type_var_count.clear();
    sort_index.clear();

    if( i -> sort().is_basic_sort() )
    {
      data_expression lhs = application( determine_function, *i );
      gsDebugMsg("\tAdded \"Det\" equation %s\n", pp(data_equation( lhs, set_of_new_sorts[e] )).c_str());
      set< variable > svars = find_all_variables( lhs );
      set< variable > tmp_var = find_all_variables( set_of_new_sorts[e] );
      svars.insert( tmp_var.begin(), tmp_var.end() );
      del.push_back( data_equation( variable_list(svars.begin(), svars.end()), lhs, set_of_new_sorts[e] ) );
    }

    //cout << "i: " << *i << endl;
    if( i -> sort().is_function_sort() )
    {
      function_sort fs = function_sort( i -> sort() );
      boost::iterator_range<sort_expression_list::const_iterator> sel = fs.domain();

      data_expression_vector dal;
      for(sort_expression_list::const_iterator j = sel.begin(); j != sel.end(); ++j )
      {
           if ((int)sort_vars[*j].size() == sort_index[ *j] )
           {
             istr = generator( dstr );
             data_expression v = variable( istr, *j );
             sort_vars[*j].push_back(v);
           }
           variable y = sort_vars[*j].at( sort_index[*j]);
           sort_index[*j] = sort_index[*j]+1;
           dal.push_back(y);
      }
      data_expression lhs = application( determine_function , mcrl2::data::application( *i, mcrl2::data::data_expression_list( dal.begin(), dal.end() ) ) );
      gsDebugMsg("\tAdded \"Det\" equation %s\n", pp(data_equation( lhs, set_of_new_sorts[e] )).c_str());
      set< variable > svars = find_all_variables( lhs );
      set< variable > tmp_var = find_all_variables( set_of_new_sorts[e] );
      svars.insert( tmp_var.begin(), tmp_var.end() );
      del.push_back( data_equation( variable_list(svars.begin(), svars.end()), lhs, set_of_new_sorts[e] ) );

      /* Equations for projection functions */
      int f = 0;
   
      while (!pi.empty() && f < std::distance(dal.begin(), dal.end()) ){
          data_expression lhs = application( pi.front(), mcrl2::data::application( *i, mcrl2::data::data_expression_list( dal.begin(), dal.end() )));
          gsDebugMsg("\tAdded \"pi\" equation %s\n", pp(data_equation( lhs, dal[f] )).c_str());
          set< variable > vars = find_all_variables( lhs );
          set< variable > tmp_var = find_all_variables( dal[f] );
          vars.insert( tmp_var.begin(), tmp_var.end() );
          del.push_back( data_equation( variable_list(vars.begin(), vars.end()), lhs, dal[f] ) );
          ++f;
          pi.erase(pi.begin());
      }
    }

    if(i -> sort().is_structured_sort())
    {
      data_expression lhs = application( determine_function , *i );
      gsDebugMsg("\tAdded \"Det\" equation %s\n", pp(data_equation( lhs, set_of_new_sorts[e] )).c_str());
      set< variable > vars = find_all_variables( lhs );
      set< variable > tmp_var = find_all_variables( set_of_new_sorts[e] );
      vars.insert( tmp_var.begin(), tmp_var.end() );
      del.push_back( data_equation( variable_list(vars.begin(), vars.end()), lhs, set_of_new_sorts[e] ) );

    }
  e++;
  }

  return del;
}

mcrl2::core::identifier_string lpsparunfold::generate_fresh_process_parameter_name(std::string str)
{
  mcrl2::data::postfix_identifier_generator generator = mcrl2::data::postfix_identifier_generator ("");
  generator.add_identifiers( process_parameter_names );
  mcrl2::core::identifier_string idstr = generator( str.append( "_pp" ) );
  process_parameter_names.insert( idstr );
  return idstr;
}

mcrl2::lps::linear_process lpsparunfold::update_linear_process(function_symbol case_function , function_symbol_vector affected_constructors, function_symbol determine_function, int parameter_at_index, function_symbol_vector pi)
{
   /* Get process parameters from lps */
   mcrl2::data::variable_list lps_proc_pars =  m_lps.process_parameters();

   /* Get process_parameters names from lps */
   process_parameter_names.clear();
   std::set<mcrl2::core::identifier_string> process_parameter_names;
   for(mcrl2::data::variable_list::iterator i = lps_proc_pars.begin();
                                                i != lps_proc_pars.end();
                                                ++i)
   {
     process_parameter_names.insert(i -> name() );
   }


   gsDebugMsg("Updating LPS\n");
   /* Create new process parameters */
   mcrl2::data::variable_vector new_process_parameters;
   for(mcrl2::data::variable_list::iterator i = lps_proc_pars.begin();
                                                i != lps_proc_pars.end();
                                                ++i) 
   {
     if( std::distance( lps_proc_pars.begin(), i ) == parameter_at_index )
     {
       gsDebugMsg("");
       gsVerboseMsg("Unfold parameter %s found at index %d\n", std::string(i->name()).c_str(), std::distance( lps_proc_pars.begin(), i ) );
       gsDebugMsg("Inject process parameters\n");
       mcrl2::data::variable_vector process_parameters_injection;

       /* Generate fresh process parameter for new Sort */
       mcrl2::core::identifier_string idstr = generate_fresh_process_parameter_name( unfold_parameter_name );
       process_parameters_injection.push_back( mcrl2::data::variable( idstr , fresh_basic_sort ) );

       gsDebugMsg("\t");
       gsVerboseMsg("Created process parameter %s of type %s\n", pp( process_parameters_injection.back() ).c_str(), pp( fresh_basic_sort ).c_str());

       for(mcrl2::data::function_symbol_vector::iterator j = affected_constructors.begin()
                                                ; j != affected_constructors.end()
                                                ; ++j )
       {
         //cout << *j << endl;  

         bool processed = false;
         if (j -> sort().is_function_sort())
         {
           function_sort::domain_range dom = function_sort(j -> sort()).domain();
           for(function_sort::domain_range::iterator k = dom.begin(); k != dom.end(); ++k)
           {
             mcrl2::core::identifier_string idstr = generate_fresh_process_parameter_name( unfold_parameter_name ); 
             process_parameters_injection.push_back( mcrl2::data::variable( idstr ,  *k  ) );
             gsDebugMsg( "\tInjected process parameter: %s::%s\n", pp( idstr ).c_str(), pp( *k ). c_str() );
           }
           processed = true;
         }

         if (j -> sort().is_basic_sort())
         {
           gsDebugMsg("\tNo processed parameter are injected for basic sort: %s\n", j->to_string().c_str() );
           processed = true;
         }

         if (j -> sort().is_structured_sort())
         {
           processed = true;
         }

         if (!processed) {
           cerr << pp(*j) << " is not processed" << endl;
           cerr << *j << endl;
           abort();
         } 
       }
       new_process_parameters.insert( new_process_parameters.end(), process_parameters_injection.begin(), process_parameters_injection.end() );

       /* store mapping: process parameter -> process parameter injection:
          Required for process parameter replacement in summands
      */
       proc_par_to_proc_par_inj[*i] = process_parameters_injection;

     } else {
       new_process_parameters.push_back( *i );
     }
   }
   gsDebugMsg("\tNew LPS process parameters: %s\n", mcrl2::data::pp(new_process_parameters).c_str() );

  /* Reconstruct summands */
  mcrl2::lps::summand_vector new_summands;

  //Prepare parameter substitution
  std::map<mcrl2::data::data_expression, mcrl2::data::data_expression> parsub = parameter_substitution(proc_par_to_proc_par_inj, affected_constructors, case_function );

  mcrl2::lps::summand_list s = m_lps.summands();
  for(mcrl2::lps::summand_list::iterator j = s.begin()
                                     ; j != s.end()
                                     ; ++j)

  //Traversing summands for process unfolding
  {

    mcrl2::data::assignment_list ass = j-> assignments();
    //Create new left-hand assignment_list 
    mcrl2::data::data_expression_vector new_ass_left;
    for(mcrl2::data::assignment_list::iterator k = ass.begin()
                                                 ; k != ass.end()
                                                 ; ++k)
    {
      if (proc_par_to_proc_par_inj.find( k-> lhs() ) != proc_par_to_proc_par_inj.end() )
      {
        for ( mcrl2::data::variable_vector::iterator l =  proc_par_to_proc_par_inj[ k -> lhs() ].begin()
                                                       ; l != proc_par_to_proc_par_inj[ k -> lhs() ].end()
                                                       ; ++l )
        {
         new_ass_left.push_back( *l );
        }
      } else {
        new_ass_left.push_back( k-> lhs() );
      }
    }
    //Create new right-hand assignment_list 
    //Unfold parameters
    mcrl2::data::data_expression_vector new_ass_right;
    for(mcrl2::data::assignment_list::iterator k = ass.begin()
                                                 ; k != ass.end()
                                                 ; ++k)
    {
      if (std::distance( ass.begin(), k ) == parameter_at_index)
      {

        mcrl2::data::data_expression_vector ins = unfold_constructor(k -> rhs(), determine_function, pi );
        //Replace unfold parameters in affected assignments
        new_ass_right.insert(new_ass_right.end(), ins.begin(), ins.end());
      } else {
        new_ass_right.push_back( k-> rhs() );
      }
    }

    //cout << new_ass_left.size()<< " " << new_ass_right.size() << endl;
    //cout << mcrl2::data::pp(new_ass_left)  << endl;
    //cout << mcrl2::data::pp(new_ass_right)  << endl;
   
    assert( new_ass_left.size() == new_ass_right.size() );
    mcrl2::data::assignment_vector new_ass;
    while (!new_ass_left.empty())
    {
      new_ass.push_back( mcrl2::data::assignment( new_ass_left.front(), new_ass_right.front() ) );
      new_ass_left.erase( new_ass_left.begin() );
      new_ass_right.erase( new_ass_right.begin() );
    }

    mcrl2::lps::summand new_summand = set_assignments( *j, mcrl2::data::assignment_list( new_ass.begin(), new_ass.end() ) );
    for( std::map<mcrl2::data::data_expression, mcrl2::data::data_expression>::iterator i = parsub.begin()
                                                                                              ; i != parsub.end()
                                                                                              ; ++i)
    {
      new_summand = atermpp::replace( new_summand, i->first , i->second );
    }

    new_summands.push_back( new_summand );
  }

  mcrl2::lps::linear_process new_lps;
  new_lps.free_variables() = m_lps.free_variables();
  new_lps.process_parameters() = mcrl2::data::variable_list(new_process_parameters.begin(), new_process_parameters.end());
  new_lps.set_summands(mcrl2::lps::summand_list(new_summands.begin(), new_summands.end()));

  gsDebugMsg("\nNew LPS:\n%s\n", pp(lps::linear_process_to_aterm(new_lps)).c_str() );
  assert( new_lps.is_well_typed());

  return new_lps;
}

mcrl2::lps::process_initializer lpsparunfold::update_linear_process_initialization(function_symbol case_function , function_symbol_vector AffectedConstructors, function_symbol determine_function, int parameter_at_index, function_symbol_vector pi)
{
  //
  //update inital process  
  //
  gsDebugMsg("Updating initialization\n\n");
  
  data::assignment_list ass = m_init_process.assignments();
  //Create new left-hand assignment_list 
  mcrl2::data::data_expression_vector new_ass_left;
  for(mcrl2::data::assignment_list::iterator k = ass.begin()
                                               ; k != ass.end()
                                               ; ++k)
  {
    if (proc_par_to_proc_par_inj.find( k-> lhs() ) != proc_par_to_proc_par_inj.end() )
    {
      for ( mcrl2::data::variable_vector::iterator l =  proc_par_to_proc_par_inj[ k -> lhs() ].begin()
                                                     ; l != proc_par_to_proc_par_inj[ k -> lhs() ].end()
                                                     ; ++l )
      {
       new_ass_left.push_back( *l );
      }
    } else {
      new_ass_left.push_back( k-> lhs() );
    }
  }
  //Create new right-hand assignment_list 
  //Unfold parameters
  mcrl2::data::data_expression_vector new_ass_right;
  for(mcrl2::data::assignment_list::iterator k = ass.begin()
                                               ; k != ass.end()
                                               ; ++k)
  {
    if (std::distance( ass.begin(), k ) == parameter_at_index)
    {

      mcrl2::data::data_expression_vector ins = unfold_constructor(k -> rhs(), determine_function, pi );
      //Replace unfold parameters in affected assignments
      new_ass_right.insert(new_ass_right.end(), ins.begin(), ins.end());
    } else {
      new_ass_right.push_back( k-> rhs() );
    }
  }

  //cout << new_ass_left.size()<< " " << new_ass_right.size() << endl;
  //cout << mcrl2::data::pp(new_ass_left)  << endl;
  //cout << mcrl2::data::pp(new_ass_right)  << endl;
 
  assert( new_ass_left.size() == new_ass_right.size() );
  mcrl2::data::assignment_vector new_ass;
  while (!new_ass_left.empty())
  {
    new_ass.push_back( mcrl2::data::assignment( new_ass_left.front(), new_ass_right.front() ) );
    new_ass_left.erase( new_ass_left.begin() );
    new_ass_right.erase( new_ass_right.begin() );
  }

  mcrl2::lps::process_initializer new_init = mcrl2::lps::process_initializer (m_init_process.free_variables(), mcrl2::data::assignment_list(new_ass.begin(), new_ass.end()) );
  gsDebugMsg("%s\n", pp( new_init ).c_str() );
 
  return new_init;
}

std::map<mcrl2::data::data_expression, mcrl2::data::data_expression> lpsparunfold::parameter_substitution(std::map<mcrl2::data::variable, mcrl2::data::variable_vector > proc_par_to_proc_par_inj, mcrl2::data::function_symbol_vector k, mcrl2::data::function_symbol case_function )
{
   std::map<mcrl2::data::data_expression, mcrl2::data::data_expression> result;
   data_expression_vector dev;

   set<mcrl2::data::variable_vector::iterator> used_iters;

   mcrl2::data::variable prev;
   for(std::map<mcrl2::data::variable, mcrl2::data::variable_vector >::iterator i = proc_par_to_proc_par_inj.begin()
      ; i != proc_par_to_proc_par_inj.end()
      ; ++i)
   {
     if (prev != i->first)
     {
       dev.clear();
     }

     dev.push_back( data_expression( i->second.front() ));

     for( mcrl2::data::function_symbol_vector::iterator m = k.begin()
                                                    ; m != k.end()
                                                    ; ++m )
     {
       if (m -> sort().is_basic_sort())
       {
         dev.push_back( *m );
       }

       if (m -> sort().is_structured_sort())
       {
         dev.push_back( *m );
       }

       if (m -> sort().is_function_sort())
       {
         function_sort::domain_range dom = function_sort( m -> sort() ). domain();
         data_expression_vector arg;

         for(function_sort::domain_range::iterator n = dom.begin(); n != dom.end(); ++n  )
         {
           for (mcrl2::data::variable_vector::iterator o = i->second.begin()
                                              ; o != i->second.end()
                                              ; ++o)
           {
             if (o -> sort() == *n && used_iters.find(o) == used_iters.end() )
             {
               used_iters.insert( o );
               arg.push_back( *o );
               break;
             }
           }
         }
         dev.push_back( mcrl2::data::application( *m, arg ) );
       }
     }
     gsDebugMsg("");
     gsVerboseMsg( "parameter substitution:\t%s\t->\t%s\n", pp( i -> first ).c_str(), pp( mcrl2::data::application( case_function, dev  ) ).c_str());
     result.insert( std::pair<mcrl2::data::data_expression, mcrl2::data::data_expression>(i -> first,  mcrl2::data::application( case_function, dev ) ) );
   }
  return result ;
}

mcrl2::data::data_expression_vector lpsparunfold::unfold_constructor( data_expression de, function_symbol determine_function, function_symbol_vector pi )
{
    mcrl2::data::data_expression_vector result;
    {
      /* Unfold parameter if function symbol occurs  */
      /* size of unfold parameter must be equal to 1 */
      data_expression_vector new_ass;

      /* Det function */
      new_ass.push_back( mcrl2::data::application( determine_function, de ) ) ;

      for(function_symbol_vector::iterator i = pi.begin(); i != pi.end(); ++i )
      {
         new_ass.push_back( mcrl2::data::application( *i, de ) ) ;
      }

      result = new_ass;
    }
    return result;
}

mcrl2::data::sort_expression lpsparunfold::sort_at_process_parameter_index(int parameter_at_index)
{
  mcrl2::data::variable_list lps_proc_pars_list =  m_lps.process_parameters();
  mcrl2::data::variable_vector lps_proc_pars = mcrl2::data::variable_vector( lps_proc_pars_list.begin(), lps_proc_pars_list.end() );
  gsDebugMsg( "\tNumber of parameters in LPS: %d\n", lps_proc_pars.size() );
  gsDebugMsg( "\tIndex parameters to unfold: %d\n", parameter_at_index );
  if(    (int(lps_proc_pars.size()) <= parameter_at_index) || parameter_at_index < 0  )
  {
    cerr << "Given index out of bounds. Index value needs to be in the range [0," << lps_proc_pars.size() <<")." << endl;
    abort();
  }

  if(lps_proc_pars[parameter_at_index].sort().is_basic_sort())
  {
    unfold_parameter_name = basic_sort(lps_proc_pars[parameter_at_index].sort()).name();
  }

  if(lps_proc_pars[parameter_at_index].sort().is_alias())
  {
    unfold_parameter_name = alias(lps_proc_pars[parameter_at_index]).name().name();
  }

  if(lps_proc_pars[parameter_at_index].sort().is_structured_sort())
  {
    mcrl2::data::postfix_identifier_generator generator = mcrl2::data::postfix_identifier_generator ("");
    generator.add_identifiers( sort_names );
    mcrl2::core::identifier_string nstr = generator( "S" );
    unfold_parameter_name = nstr; 
  }

  return lps_proc_pars[parameter_at_index].sort();
}


mcrl2::lps::specification lpsparunfold::algorithm(int parameter_at_index)
{
   m_unfold_process_parameter = sort_at_process_parameter_index( parameter_at_index);

   /* Var Dec */
   function_symbol_vector k;
   function_symbol_vector set_of_new_sorts;
   function_symbol_vector projection_functions;
   function_symbol case_function;
   function_symbol determine_function;
   data_equation_vector data_equations;

   /*   Alg */
   /*     1 */ fresh_basic_sort = generate_fresh_basic_sort( unfold_parameter_name );
   /*     2 */ k = determine_affected_constructors();
   /*     4 */ set_of_new_sorts = new_constructors( k );
   /*     6 */ case_function = create_case_function( k.size() );
   /*     7 */ determine_function = create_determine_function( );
   /*  8-12 */ projection_functions = create_projection_functions(k);
   /* 13-xx */ data_equations = create_data_equations( projection_functions, case_function, set_of_new_sorts, k, determine_function);

   /*----------------*/
   mcrl2::lps::linear_process new_lps = update_linear_process(case_function, k, determine_function, parameter_at_index, projection_functions);
   mcrl2::lps::process_initializer new_init = update_linear_process_initialization(case_function, k, determine_function, parameter_at_index, projection_functions);

  //Reconstruct specification
  m_data_specification.add_sort( fresh_basic_sort );
  boost::iterator_range<function_symbol_vector::const_iterator> pi_range(boost::make_iterator_range(projection_functions));
  m_data_specification.add_mappings( pi_range );
  m_data_specification.add_mapping( determine_function );
  m_data_specification.add_mapping( case_function );
  boost::iterator_range<function_symbol_vector::const_iterator> set_of_new_sorts_range(boost::make_iterator_range(set_of_new_sorts));
  m_data_specification.add_mappings( set_of_new_sorts_range );

  boost::iterator_range<data_equation_vector::const_iterator> dev_range(boost::make_iterator_range( data_equations ) );
  m_data_specification.add_equations( dev_range );

  mcrl2::lps::specification new_spec = mcrl2::lps::specification( m_data_specification, m_action_label_list, new_lps, new_init );

  assert(  new_spec.is_well_typed() );

  return new_spec; 
}
