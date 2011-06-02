// Author(s): Muck van Weerdenburg
// Copyright: see the accompanying file COPYING or copy at
// https://svn.win.tue.nl/trac/MCRL2/browser/trunk/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost.hpp" // precompiled headers

#include "mcrl2/data/detail/rewrite.h"

#ifdef MCRL2_INNERC_AVAILABLE

#define USE_VARAFUN_VALUE 1

#define NAME std::string("rewr_innerc")

#include <cstdio>
#include <cstdlib>
// #include <stdint.h>
#include <unistd.h>
#include <cerrno>
#include <string.h>
#include <dlfcn.h>
#include <cassert>
#include <stdexcept>
#include <sstream>
#include "mcrl2/aterm/aterm2.h"
#include "mcrl2/aterm/aterm_ext.h"
#include "mcrl2/core/detail/memory_utility.h"
#include "mcrl2/core/messaging.h"
#include "mcrl2/core/detail/struct_core.h"
#include "mcrl2/core/print.h"
#include "mcrl2/data/data_specification.h"
#include "mcrl2/setup.h"
#include "mcrl2/data/detail/rewrite/innerc.h"

using namespace mcrl2::core;
using namespace mcrl2::core::detail;

#define INNERC_COMPILE_COMMAND (CXX " -c " CXXFLAGS " " SCXXFLAGS " " CPPFLAGS " " ATERM_CPPFLAGS " %s.cpp")
#define INNERC_LINK_COMMAND (CXX " " LDFLAGS " " SLDFLAGS " -o %s.so %s.o")

#define ATXgetArgument(x,y) ((unsigned int) (intptr_t) ATgetArgument(x,y))

namespace mcrl2
{
namespace data
{
namespace detail
{

static AFun afunS, afunM, afunF, afunN, afunD, afunR, afunCR, afunC, afunX, afunRe, afunCRe, afunMe;
static ATerm dummy;

#define isS(x) ATisEqualAFun(ATgetAFun(x),afunS)
#define isM(x) ATisEqualAFun(ATgetAFun(x),afunM)
#define isF(x) ATisEqualAFun(ATgetAFun(x),afunF)
#define isN(x) ATisEqualAFun(ATgetAFun(x),afunN)
#define isD(x) ATisEqualAFun(ATgetAFun(x),afunD)
#define isR(x) ATisEqualAFun(ATgetAFun(x),afunR)
#define isCR(x) ATisEqualAFun(ATgetAFun(x),afunCR)
#define isC(x) ATisEqualAFun(ATgetAFun(x),afunC)
#define isX(x) ATisEqualAFun(ATgetAFun(x),afunX)
#define isRe(x) ATisEqualAFun(ATgetAFun(x),afunRe)
#define isCRe(x) ATisEqualAFun(ATgetAFun(x),afunCRe)
#define isMe(x) ATisEqualAFun(ATgetAFun(x),afunMe)

static unsigned int is_initialised = 0;

static void initialise_common()
{
  if (is_initialised == 0)
  {
    afunS = ATmakeAFun("@@S",2,false); // Store term ( target_variable, result_tree )
    ATprotectAFun(afunS);
    afunM = ATmakeAFun("@@M",3,false); // Match term ( match_variable, true_tree , false_tree )
    ATprotectAFun(afunM);
    afunF = ATmakeAFun("@@F",3,false); // Match function ( match_function, true_tree, false_tree )
    ATprotectAFun(afunF);
    afunN = ATmakeAFun("@@N",1,false); // Go to next parameter ( result_tree )
    ATprotectAFun(afunN);
    afunD = ATmakeAFun("@@D",1,false); // Go down a level ( result_tree )
    ATprotectAFun(afunD);
    afunR = ATmakeAFun("@@R",1,false); // End of tree ( matching_rule )
    ATprotectAFun(afunR);
    afunCR = ATmakeAFun("@@CR",2,false); // End of tree ( condition, matching_rule )
    ATprotectAFun(afunCR);
    afunC = ATmakeAFun("@@C",3,false); // Check condition ( condition, true_tree, false_tree )
    ATprotectAFun(afunC);
    afunX = ATmakeAFun("@@X",0,false); // End of tree
    ATprotectAFun(afunX);
    afunRe = ATmakeAFun("@@Re",2,false); // End of tree ( matching_rule , vars_of_rule)
    ATprotectAFun(afunRe);
    afunCRe = ATmakeAFun("@@CRe",4,false); // End of tree ( condition, matching_rule, vars_of_condition, vars_of_rule )
    ATprotectAFun(afunCRe);
    afunMe = ATmakeAFun("@@Me",2,false); // Match term ( match_variable, variable_index )
    ATprotectAFun(afunMe);
    dummy = (ATerm) gsMakeNil();
    ATprotect(&dummy);
  }

  is_initialised++;
}

static void finalise_common()
{
  assert(is_initialised > 0);
  is_initialised--;

  if (is_initialised == 0)
  {
    ATunprotect(&dummy);
    ATunprotectAFun(afunMe);
    ATunprotectAFun(afunCRe);
    ATunprotectAFun(afunRe);
    ATunprotectAFun(afunX);
    ATunprotectAFun(afunC);
    ATunprotectAFun(afunCR);
    ATunprotectAFun(afunR);
    ATunprotectAFun(afunD);
    ATunprotectAFun(afunN);
    ATunprotectAFun(afunF);
    ATunprotectAFun(afunM);
    ATunprotectAFun(afunS);
  }
}


ATerm RewriterCompilingInnermost::toRewriteFormat(ATermAppl t)
{
  size_t old_opids = get_num_opids();

  ATerm r = (ATerm)toInnerc(t,true);

  if (old_opids != get_num_opids())
  {
    need_rebuild = true;
  }

  return r;
}

ATermAppl RewriterCompilingInnermost::fromRewriteFormat(ATerm t)
{
  if (need_rebuild)
  {
    BuildRewriteSystem();
  }

  return fromInner(t);

  /* if (ATisInt(t))
  {
    return get_int2term(ATgetInt((ATermInt) t));
  }
  else if (gsIsDataVarId((ATermAppl) t))
  {
    return (ATermAppl) t;
  }

  ATermAppl a = fromRewriteFormat(ATgetArgument((ATermAppl) t, 0));
  assert(gsIsOpId(a) || gsIsDataVarId(a));

  int i = 1;
  int arity = ATgetArity(ATgetAFun((ATermAppl) t));
  ATermAppl sort = ATAgetArgument(a, 1);
  while (is_function_sort(sort_expression(sort)) && (i < arity))
  {
    ATermList sort_dom = ATLgetArgument(sort, 0);
    ATermList list = ATmakeList0();
    while (!ATisEmpty(sort_dom))
    {
      assert(i < arity);
      list = ATinsert(list, (ATerm) fromRewriteFormat(ATgetArgument((ATermAppl) t,i)));
      sort_dom = ATgetNext(sort_dom);
      ++i;
    }
    list = ATreverse(list);
    a = gsMakeDataAppl(a, list);
    sort = ATAgetArgument(sort, 1);
  }

  return a; */
}

static char* whitespace_str = NULL;
static int whitespace_len;
static int whitespace_pos;
static char* whitespace(int len)
{
  int i;

  if (whitespace_str == NULL)
  {
    whitespace_str = (char*) malloc((2*len+1)*sizeof(char));
    for (i=0; i<2*len; i++)
    {
      whitespace_str[i] = ' ';
    }
    whitespace_len = 2*len;
    whitespace_pos = len;
    whitespace_str[whitespace_pos] = 0;
  }
  else
  {
    if (len > whitespace_len)
    {
      whitespace_str = (char*) realloc(whitespace_str,(2*len+1)*sizeof(char));
      for (i=whitespace_len; i<2*len; i++)
      {
        whitespace_str[i] = ' ';
      }
      whitespace_len = 2*len;
    }

    whitespace_str[whitespace_pos] = ' ';
    whitespace_pos = len;
    whitespace_str[whitespace_pos] = 0;
  }

  return whitespace_str;
}


#ifdef _INNERC_STORE_TREES
int RewriterCompilingInnermost::write_tree(FILE* f, ATermAppl tree, int* num_states)
{
  if (isS(tree))
  {
    int n = write_tree(f,ATAgetArgument(tree,1),num_states);
    fprintf(f,"n%i [label=\"S(%s)\"]\n",*num_states,ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0))));
    fprintf(f,"n%i -> n%i\n",*num_states,n);
    return (*num_states)++;
  }
  else if (isM(tree))
  {
    int n = write_tree(f,ATAgetArgument(tree,1),num_states);
    int m = write_tree(f,ATAgetArgument(tree,2),num_states);
    if (ATisInt(ATgetArgument(tree,0)))
    {
      fprintf(f,"n%i [label=\"M(%i)\"]\n",*num_states,ATgetInt((ATermInt) ATgetArgument(tree,0)));
    }
    else
    {
      fprintf(f,"n%i [label=\"M(%s)\"]\n",*num_states,ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0))));
    }
    fprintf(f,"n%i -> n%i [label=\"true\"]\n",*num_states,n);
    fprintf(f,"n%i -> n%i [label=\"false\"]\n",*num_states,m);
    return (*num_states)++;
  }
  else if (isF(tree))
  {
    int n = write_tree(f,ATAgetArgument(tree,1),num_states);
    int m = write_tree(f,ATAgetArgument(tree,2),num_states);
    if (ATisInt(ATgetArgument(tree,0)))
    {
      fprintf(f,"n%i [label=\"F(%s)\"]\n",*num_states,ATgetName(ATgetAFun(ATAgetArgument(int2term[ATgetInt((ATermInt) ATgetArgument(tree,0))],0))));
    }
    else
    {
      fprintf(f,"n%i [label=\"F(%s)\"]\n",*num_states,ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0))));
    }
    fprintf(f,"n%i -> n%i [label=\"true\"]\n",*num_states,n);
    fprintf(f,"n%i -> n%i [label=\"false\"]\n",*num_states,m);
    return (*num_states)++;
  }
  else if (isD(tree))
  {
    int n = write_tree(f,ATAgetArgument(tree,0),num_states);
    fprintf(f,"n%i [label=\"D\"]\n",*num_states);
    fprintf(f,"n%i -> n%i\n",*num_states,n);
    return (*num_states)++;
  }
  else if (isN(tree))
  {
    int n = write_tree(f,ATAgetArgument(tree,0),num_states);
    fprintf(f,"n%i [label=\"N\"]\n",*num_states);
    fprintf(f,"n%i -> n%i\n",*num_states,n);
    return (*num_states)++;
  }
  else if (isC(tree))
  {
    int n = write_tree(f,ATAgetArgument(tree,1),num_states);
    int m = write_tree(f,ATAgetArgument(tree,2),num_states);
    gsfprintf(f,"n%i [label=\"C(%P)\"]\n",*num_states,fromInner(ATgetArgument(tree,0)));
    fprintf(f,"n%i -> n%i [label=\"true\"]\n",*num_states,n);
    fprintf(f,"n%i -> n%i [label=\"false\"]\n",*num_states,m);
    return (*num_states)++;
  }
  else if (isR(tree))
  {
    gsfprintf(f,"n%i [label=\"R(%P)\"]\n",*num_states,fromInner(ATgetArgument(tree,0)));
    return (*num_states)++;
  }
  else if (isX(tree))
  {
    ATfprintf(f,"n%i [label=\"X\"]\n",*num_states);
    return (*num_states)++;
  }

  return -1;
}

void RewriterCompilingInnermost::tree2dot(ATermAppl tree, char* name, char* filename)
{
  FILE* f;
  int num_states = 0;

  if ((f = fopen(filename,"w")) == NULL)
  {
    perror("fopen");
    return;
  }

  fprintf(f,"digraph \"%s\" {\n",name);
  write_tree(f,tree,&num_states);
  fprintf(f,"}\n");

  fclose(f);
}
#endif

static void term2seq(ATerm t, ATermList* s, int* var_cnt)
{
  if (ATisList(t))
  {
    ATermList l;

    l = ATgetNext((ATermList) t);
    t = ATgetFirst((ATermList) t);

    *s = ATinsert(*s, (ATerm) ATmakeAppl3(afunF,(ATerm) t,dummy,dummy));

    for (; !ATisEmpty(l); l=ATgetNext(l))
    {
      term2seq(ATgetFirst(l),s,var_cnt);
      if (!ATisEmpty(ATgetNext(l)))
      {
        *s = ATinsert(*s, (ATerm) ATmakeAppl1(afunN,dummy));
      }
    }
    *s = ATinsert(*s, (ATerm) ATmakeAppl1(afunD,dummy));
  }
  else if (ATisInt(t))
  {
    term2seq((ATerm) ATmakeList1(t),s,var_cnt);
  }
  else if (gsIsDataVarId((ATermAppl) t))
  {
    ATerm store = (ATerm) ATmakeAppl2(afunS,(ATerm) t,dummy);

    if (ATindexOf(*s,store,0) != ATERM_NON_EXISTING_POSITION)
    {
      *s = ATinsert(*s, (ATerm) ATmakeAppl3(afunM,(ATerm) t,dummy,dummy));
    }
    else
    {
      (*var_cnt)++;
      *s = ATinsert(*s, store);
    }
  }
  else
  {
    assert(0);
  }

}

static void get_used_vars_aux(ATerm t, ATermList* vars)
{
  if (ATisList(t))
  {
    for (; !ATisEmpty((ATermList) t); t=(ATerm) ATgetNext((ATermList) t))
    {
      get_used_vars_aux(ATgetFirst((ATermList) t),vars);
    }
  }
  else if (ATisAppl(t))
  {
    if (gsIsDataVarId((ATermAppl) t))
    {
      if (ATindexOf(*vars,t,0) == ATERM_NON_EXISTING_POSITION)
      {
        *vars = ATinsert(*vars,t);
      }
    }
    else
    {
      int a = ATgetArity(ATgetAFun((ATermAppl) t));
      for (int i=0; i<a; i++)
      {
        get_used_vars_aux(ATgetArgument((ATermAppl) t,i),vars);
      }
    }
  }
}

static ATermList get_used_vars(ATerm t)
{
  ATermList l = ATmakeList0();

  get_used_vars_aux(t,&l);

  return l;
}

static ATermList create_sequence(ATermList rule, int* var_cnt, ATermInt true_inner)
{
  ATermAppl pat = (ATermAppl) ATelementAt(rule,2);
  ATerm cond = ATelementAt(rule,1);
  ATerm rslt = ATelementAt(rule,3);
  ATermList pars = ATmakeList0();
  ATermList rseq = ATmakeList0();

  pars = (ATermList) pat;
  //ATfprintf(stderr,"pattern pars: %t\n",pars);
  for (; !ATisEmpty(pars); pars=ATgetNext(pars))
  {
    term2seq(ATgetFirst(pars),&rseq,var_cnt);
    if (!ATisEmpty(ATgetNext(pars)))
    {
      rseq = ATinsert(rseq, (ATerm) ATmakeAppl1(afunN,dummy));
    }
  }
  //ATfprintf(stderr,"rseq: %t\n",rseq);
  if (ATisInt(cond)/* && gsIsNil((ATermAppl) cond)*/ && ATisEqual(cond, true_inner))   // JK 15/10/2009 recognise true as condition
  {
    rseq = ATinsert(rseq,(ATerm) ATmakeAppl2(afunRe,rslt,(ATerm) get_used_vars(rslt)));
  }
  else
  {
    rseq = ATinsert(rseq,(ATerm) ATmakeAppl4(afunCRe,cond,rslt,(ATerm) get_used_vars(cond),(ATerm) get_used_vars(rslt)));
  }

  return ATreverse(rseq);
}


// Structure for build_tree paramters
typedef struct
{
  ATermList Flist;   // List of sequences of which the first action is an F
  ATermList Slist;   // List of sequences of which the first action is an S
  ATermList Mlist;   // List of sequences of which the first action is an M
  ATermList stack;   // Stack to maintain the sequences that do not have to
  // do anything in the current term
  ATermList upstack; // List of sequences that have done an F at the current
  // level
} build_pars;

static void initialise_build_pars(build_pars* p)
{
  p->Flist = ATmakeList0();
  p->Slist = ATmakeList0();
  p->Mlist = ATmakeList0();
  p->stack = ATmakeList1((ATerm) ATmakeList0());
  p->upstack = ATmakeList0();
}

static ATermList add_to_stack(ATermList stack, ATermList seqs, ATermAppl* r, ATermList* cr)
{
  if (ATisEmpty(stack))
  {
    return stack;
  }

  ATermList l = ATmakeList0();
  ATermList h = ATLgetFirst(stack);

  for (; !ATisEmpty(seqs); seqs=ATgetNext(seqs))
  {
    ATermList e = ATLgetFirst(seqs);

    if (isD(ATAgetFirst(e)))
    {
      l = ATinsert(l,(ATerm) ATgetNext(e));
    }
    else if (isN(ATAgetFirst(e)))
    {
      h = ATinsert(h,(ATerm) ATgetNext(e));
    }
    else if (isRe(ATAgetFirst(e)))
    {
      *r = ATAgetFirst(e);
    }
    else
    {
      *cr = ATinsert(*cr,ATgetFirst(e));
    }
  }

  return ATinsert(add_to_stack(ATgetNext(stack),l,r,cr),(ATerm) h);
}

static void add_to_build_pars(build_pars* pars,ATermList seqs, ATermAppl* r, ATermList* cr)
{
  ATermList l = ATmakeList0();

  for (; !ATisEmpty(seqs); seqs=ATgetNext(seqs))
  {
    ATermList e = ATLgetFirst(seqs);

    if (isD(ATAgetFirst(e)) || isN(ATAgetFirst(e)))
    {
      l = ATinsert(l,(ATerm) e);
    }
    else if (isS(ATAgetFirst(e)))
    {
      pars->Slist = ATinsert(pars->Slist,(ATerm) e);
    }
    else if (isMe(ATAgetFirst(e)))     // M should not appear at the head of a seq
    {
      pars->Mlist = ATinsert(pars->Mlist,(ATerm) e);
    }
    else if (isF(ATAgetFirst(e)))
    {
      pars->Flist = ATinsert(pars->Flist,(ATerm) e);
    }
    else if (isRe(ATAgetFirst(e)))
    {
      *r = ATAgetFirst(e);
    }
    else
    {
      *cr = ATinsert(*cr,ATgetFirst(e));
    }
  }

  pars->stack = add_to_stack(pars->stack,l,r,cr);
}

static char tree_var_str[20];
static ATermAppl createFreshVar(ATermAppl sort,int* i)
{
  sprintf(tree_var_str,"@var_%i",(*i)++);
  return gsMakeDataVarId(gsString2ATermAppl(tree_var_str),sort);
}

static ATermList subst_var(ATermList l, ATermAppl old, ATerm new_val, ATerm num, ATermList substs)
{
  if (ATisEmpty(l))
  {
    return l;
  }

  ATermAppl head = (ATermAppl) ATgetFirst(l);
  l = ATgetNext(l);

  if (isM(head))
  {
    if (ATisEqual(ATgetArgument(head,0),old))
    {
      head = ATmakeAppl2(afunMe,new_val,num);
    }
  }
  else if (isCRe(head))
  {
    ATermList l = (ATermList) ATgetArgument(head,2);
    ATermList m = ATmakeList0();
    for (; !ATisEmpty(l); l=ATgetNext(l))
    {
      if (ATisEqual(ATgetFirst(l),old))
      {
        m = ATinsert(m,num);
      }
      else
      {
        m = ATinsert(m,ATgetFirst(l));
      }
    }
    l = (ATermList) ATgetArgument(head,3);
    ATermList n = ATmakeList0();
    for (; !ATisEmpty(l); l=ATgetNext(l))
    {
      if (ATisEqual(ATgetFirst(l),old))
      {
        n = ATinsert(n,num);
      }
      else
      {
        n = ATinsert(n,ATgetFirst(l));
      }
    }
    head = ATmakeAppl4(afunCRe,gsSubstValues(substs,ATgetArgument(head,0),true),gsSubstValues(substs,ATgetArgument(head,1),true),(ATerm) m, (ATerm) n);
  }
  else if (isRe(head))
  {
    ATermList l = (ATermList) ATgetArgument(head,1);
    ATermList m = ATmakeList0();
    for (; !ATisEmpty(l); l=ATgetNext(l))
    {
      if (ATisEqual(ATgetFirst(l),old))
      {
        m = ATinsert(m,num);
      }
      else
      {
        m = ATinsert(m,ATgetFirst(l));
      }
    }
    head = ATmakeAppl2(afunRe,gsSubstValues(substs,ATgetArgument(head,0),true),(ATerm) m);
  }

  return ATinsert(subst_var(l,old,new_val,num,substs),(ATerm) head);
}

//#define BT_DEBUG
#ifdef BT_DEBUG
#define print_return(x,y) ATermAppl a = y; ATfprintf(stderr,x "return %t\n\n",a); return a;
#else
#define print_return(x,y) return y;
#endif
//static int max_tree_vars;
static int* treevars_usedcnt;

static void inc_usedcnt(ATermList l)
{
  for (; !ATisEmpty(l); l=ATgetNext(l))
  {
    treevars_usedcnt[ATgetInt((ATermInt) ATgetFirst(l))]++;
  }
}

static ATermAppl build_tree(build_pars pars, int i)
{
#ifdef BT_DEBUG
  ATfprintf(stderr,"build_tree(  %t  ,  %t  ,  %t  ,  %t  ,  %t  ,  %i  )\n\n",pars.Flist,pars.Slist,pars.Mlist,pars.stack,pars.upstack,i);
#endif

  if (!ATisEmpty(pars.Slist))
  {
    ATermList l,m;

    int k = i;
    ATermAppl v = createFreshVar(ATAgetArgument(ATAgetArgument(ATAgetFirst(ATLgetFirst(pars.Slist)),0),1),&i);
    treevars_usedcnt[k] = 0;

    l = ATmakeList0();
    m = ATmakeList0();
    for (; !ATisEmpty(pars.Slist); pars.Slist=ATgetNext(pars.Slist))
    {
      ATermList e = ATLgetFirst(pars.Slist);

      e = subst_var(e,ATAgetArgument(ATAgetFirst(e),0),(ATerm) v,(ATerm) ATmakeInt(k),ATmakeList1((ATerm) gsMakeSubst(ATgetArgument(ATAgetFirst(e),0),(ATerm) v)));
//      e = gsSubstValues_List(ATmakeList1((ATerm) gsMakeSubst(ATgetArgument(ATAgetFirst(e),0),(ATerm) v)),e,true);

      l = ATinsert(l,ATgetFirst(e));
      m = ATinsert(m,(ATerm) ATgetNext(e));
    }

    ATermAppl r = NULL;
    ATermList readies = ATmakeList0();

    pars.stack = add_to_stack(pars.stack,m,&r,&readies);

    if (r == NULL)
    {
      ATermAppl tree;

      tree = build_tree(pars,i);
      for (; !ATisEmpty(readies); readies=ATgetNext(readies))
      {
        inc_usedcnt((ATermList) ATgetArgument(ATAgetFirst(readies),2));
        inc_usedcnt((ATermList) ATgetArgument(ATAgetFirst(readies),3));
        tree = ATmakeAppl3(afunC,ATgetArgument(ATAgetFirst(readies),0),(ATerm) ATmakeAppl1(afunR,ATgetArgument(ATAgetFirst(readies),1)),(ATerm) tree);
      }
      r = tree;
    }
    else
    {
      inc_usedcnt((ATermList) ATgetArgument(r,1));
      r = ATmakeAppl1(afunR,ATgetArgument(r,0));
    }

    if ((treevars_usedcnt[k] > 0) || ((k == 0) && isR(r)))
    {
      print_return("",ATmakeAppl2(afunS,(ATerm) v,(ATerm) r));
    }
    else
    {
      print_return("",r);
    }
  }
  else if (!ATisEmpty(pars.Mlist))
  {
    ATerm M = ATgetFirst(ATLgetFirst(pars.Mlist));

    ATermList l = ATmakeList0();
    ATermList m = ATmakeList0();
    for (; !ATisEmpty(pars.Mlist); pars.Mlist=ATgetNext(pars.Mlist))
    {
      if (ATisEqual(M,ATgetFirst(ATLgetFirst(pars.Mlist))))
      {
        l = ATinsert(l,(ATerm) ATgetNext(ATLgetFirst(pars.Mlist)));
      }
      else
      {
        m = ATinsert(m,ATgetFirst(pars.Mlist));
      }
    }
    pars.Mlist = m;

    ATermAppl true_tree,false_tree;
    ATermAppl r = NULL;
    ATermList readies = ATmakeList0();

    ATermList newstack = add_to_stack(pars.stack,l,&r,&readies);

    false_tree = build_tree(pars,i);

    if (r == NULL)
    {
      pars.stack = newstack;
      true_tree = build_tree(pars,i);
      for (; !ATisEmpty(readies); readies=ATgetNext(readies))
      {
        inc_usedcnt((ATermList) ATgetArgument(ATAgetFirst(readies),2));
        inc_usedcnt((ATermList) ATgetArgument(ATAgetFirst(readies),3));
        true_tree = ATmakeAppl3(afunC,ATgetArgument(ATAgetFirst(readies),0),(ATerm) ATmakeAppl1(afunR,ATgetArgument(ATAgetFirst(readies),1)),(ATerm) true_tree);
      }
    }
    else
    {
      inc_usedcnt((ATermList) ATgetArgument(r,1));
      true_tree = ATmakeAppl1(afunR,ATgetArgument(r,0));
    }

    if (ATisEqual(true_tree,false_tree))
    {
      print_return("",true_tree);
    }
    else
    {
      treevars_usedcnt[ATgetInt((ATermInt) ATgetArgument((ATermAppl) M,1))]++;
      print_return("",ATmakeAppl3(afunM,ATgetArgument((ATermAppl) M,0),(ATerm) true_tree,(ATerm) false_tree));
    }
  }
  else if (!ATisEmpty(pars.Flist))
  {
    ATermList F = ATLgetFirst(pars.Flist);
    ATermAppl true_tree,false_tree;

    ATermList newupstack = pars.upstack;
    ATermList l = ATmakeList0();

    for (; !ATisEmpty(pars.Flist); pars.Flist=ATgetNext(pars.Flist))
    {
      if (ATisEqual(ATgetFirst(ATLgetFirst(pars.Flist)),ATgetFirst(F)))
      {
        newupstack = ATinsert(newupstack, (ATerm) ATgetNext(ATLgetFirst(pars.Flist)));
      }
      else
      {
        l = ATinsert(l,ATgetFirst(pars.Flist));
      }
    }

    pars.Flist = l;
    false_tree = build_tree(pars,i);
    pars.Flist = ATmakeList0();
    pars.upstack = newupstack;
    true_tree = build_tree(pars,i);

    if (ATisEqual(true_tree,false_tree))
    {
      print_return("",true_tree);
    }
    else
    {
      print_return("",ATmakeAppl3(afunF,ATgetArgument(ATAgetFirst(F),0),(ATerm) true_tree,(ATerm) false_tree));
    }
  }
  else if (!ATisEmpty(pars.upstack))
  {
    ATermList l;

    ATermAppl r = NULL;
    ATermList readies = ATmakeList0();

    pars.stack = ATinsert(pars.stack,(ATerm) ATmakeList0());
    l = pars.upstack;
    pars.upstack = ATmakeList0();
    add_to_build_pars(&pars,l,&r,&readies);


    if (r == NULL)
    {
      ATermAppl t = build_tree(pars,i);

      for (; !ATisEmpty(readies); readies=ATgetNext(readies))
      {
        inc_usedcnt((ATermList) ATgetArgument(ATAgetFirst(readies),2));
        inc_usedcnt((ATermList) ATgetArgument(ATAgetFirst(readies),3));
        t = ATmakeAppl3(afunC,ATgetArgument(ATAgetFirst(readies),0),(ATerm) ATmakeAppl1(afunR,ATgetArgument(ATAgetFirst(readies),1)),(ATerm) t);
      }

      print_return("",t);
    }
    else
    {
      inc_usedcnt((ATermList) ATgetArgument(r,1));
      print_return("",ATmakeAppl1(afunR,ATgetArgument(r,0)));
    }
  }
  else
  {
    if (ATisEmpty(ATLgetFirst(pars.stack)))
    {
      if (ATisEmpty(ATgetNext(pars.stack)))
      {
        print_return("",ATmakeAppl0(afunX));
      }
      else
      {
        pars.stack = ATgetNext(pars.stack);
        print_return("",ATmakeAppl1(afunD,(ATerm) build_tree(pars,i)));
//        print_return("",build_tree(pars,i));
      }
    }
    else
    {
      ATermList l = ATLgetFirst(pars.stack);
      ATermAppl r = NULL;
      ATermList readies = ATmakeList0();

      pars.stack = ATinsert(ATgetNext(pars.stack),(ATerm) ATmakeList0());
      add_to_build_pars(&pars,l,&r,&readies);

      ATermAppl tree;
      if (r == NULL)
      {
        tree = build_tree(pars,i);
        for (; !ATisEmpty(readies); readies=ATgetNext(readies))
        {
          inc_usedcnt((ATermList) ATgetArgument(ATAgetFirst(readies),2));
          inc_usedcnt((ATermList) ATgetArgument(ATAgetFirst(readies),3));
          tree = ATmakeAppl3(afunC,ATgetArgument(ATAgetFirst(readies),0),(ATerm) ATmakeAppl1(afunR,ATgetArgument(ATAgetFirst(readies),1)),(ATerm) tree);
        }
      }
      else
      {
        inc_usedcnt((ATermList) ATgetArgument(r,1));
        tree = ATmakeAppl(afunR,ATgetArgument(r,0));
      }

      print_return("",ATmakeAppl1(afunN,(ATerm) tree));
    }
  }
}

#ifdef _INNERC_STORE_TREES
ATermAppl RewriterCompilingInnermost::create_tree(ATermList rules, int opid, int arity, ATermInt true_inner)
#else
static ATermAppl create_tree(ATermList rules, int /*opid*/, unsigned int arity, ATermInt true_inner)
#endif
// Create a match tree for OpId int2term[opid] and update the value of
// *max_vars accordingly.
//
// Pre:  rules is a list of rewrite rules for int2term[opid] in the
//       INNER internal format
//       opid is a valid entry in int2term
//       max_vars is a valid pointer to an integer
// Post: *max_vars is the maximum of the original *max_vars value and
//       the number of variables in the result tree
// Ret:  A match tree for int2term[opid]
{
//gsfprintf(stderr,"%P (%i)\n",int2term[opid],opid);
  // Create sequences representing the trees for each rewrite rule and
  // store the total number of variables used in these sequences.
  // (The total number of variables in all sequences should be an upper
  // bound for the number of variable in the final tree.)
  ATermList rule_seqs = ATmakeList0();
  int total_rule_vars = 0;
  for (; !ATisEmpty(rules); rules=ATgetNext(rules))
  {
    if (ATgetLength((ATermList) ATelementAt((ATermList) ATgetFirst(rules),2)) <= arity)
    {
      rule_seqs = ATinsert(rule_seqs, (ATerm) create_sequence((ATermList) ATgetFirst(rules),&total_rule_vars, true_inner));
    }
  }

  // Generate initial parameters for built_tree
  build_pars init_pars;
  ATermAppl r = NULL;
  ATermList readies = ATmakeList0();

  initialise_build_pars(&init_pars);
  add_to_build_pars(&init_pars,rule_seqs,&r,&readies);

  ATermAppl tree;
  if (r == NULL)
  {
    MCRL2_SYSTEM_SPECIFIC_ALLOCA(a,int,total_rule_vars);
    treevars_usedcnt = a;
//    treevars_usedcnt = (int *) malloc(total_rule_vars*sizeof(int));
    tree = build_tree(init_pars,0);
//    free(treevars_usedcnt);
    for (; !ATisEmpty(readies); readies=ATgetNext(readies))
    {
      tree = ATmakeAppl3(afunC,ATgetArgument(ATAgetFirst(readies),0),(ATerm) ATmakeAppl1(afunR,ATgetArgument(ATAgetFirst(readies),1)),(ATerm) tree);
    }
  }
  else
  {
    tree = ATmakeAppl1(afunR,ATgetArgument(r,0));
  }
  //ATprintf("tree: %t\n",tree);

#ifdef _INNERC_STORE_TREES
  char s[100],t[100];
  sprintf(s,"tree_%i_%s_%i",opid,ATgetName(ATgetAFun(ATAgetArgument(int2term[opid],0))),arity);
  sprintf(t,"tree_%i_%s_%i.dot",opid,ATgetName(ATgetAFun(ATAgetArgument(int2term[opid],0))),arity);
  tree2dot(tree,s,t);
#endif

  return tree;
}


void RewriterCompilingInnermost::calcTerm(FILE* f, ATerm t, int startarg)
{
  if (ATisList(t))
  {
    unsigned int arity = ATgetLength((ATermList) t)-1;
    ATermList l;
    bool b = false;
    bool v = false;

    if (!ATisInt(ATgetFirst((ATermList) t)))
    {
      if (arity == 0)
      {
        calcTerm(f,ATgetFirst((ATermList) t),0);
        return;
      }

      v = true;
      fprintf(f,"(isAppl(");
      calcTerm(f,ATgetFirst((ATermList) t),0);
      fprintf(f,")?varFunc%i(",arity);
      calcTerm(f,ATgetFirst((ATermList) t),0);
      l = ATgetNext((ATermList) t);
      int i = startarg;
      for (; !ATisEmpty(l); l=ATgetNext(l))
      {
        fprintf(f,",");
        if (ATisAppl(ATgetFirst(l)) && gsIsNil(ATAgetFirst(l)))
        {
          fprintf(f,"arg%i",i);
        }
        else
        {
          calcTerm(f,ATgetFirst(l),0);
        }
        i++;
      }
      fprintf(f,"):");
    }

    if (ATisInt(ATgetFirst((ATermList) t)) && (l = innerc_eqns[ATgetInt((ATermInt) ATgetFirst((ATermList) t))]) != NULL)
    {
      for (; !ATisEmpty(l); l=ATgetNext(l))
      {
        if (ATgetLength(ATLelementAt(ATLgetFirst(l),2)) <= arity)
        {
          b = true;
          break;
        }
      }
    }

    if (b)
    {
      fprintf(f,"rewr_%i_%i(",ATgetInt((ATermInt) ATgetFirst((ATermList) t)),arity);
    }
    else
    {
      if (arity == 0)
      {
        fprintf(f,"(rewrAppl%i",ATgetInt((ATermInt) ATgetFirst((ATermList) t)));
      }
      else
      {
        if (arity > 5)
        {
          fprintf(f,"ATmakeAppl(appl%i,",arity);
        }
        else
        {
          fprintf(f,"ATmakeAppl%i(appl%i,",arity+1,arity);
        }
        if (ATisInt(ATgetFirst((ATermList) t)))
        {
          fprintf(f,"(ATerm) int2ATerm%i",ATgetInt((ATermInt) ATgetFirst((ATermList) t)));
        }
        else
        {
          fprintf(f,"(ATerm) ");
          calcTerm(f,ATgetFirst((ATermList) t),0);
        }
      }
    }
    l = ATgetNext((ATermList) t);
    bool c = !b;
    int i = startarg;
    for (; !ATisEmpty(l); l=ATgetNext(l))
    {
      if (c)
      {
        fprintf(f,",");
      }
      else
      {
        c = true;
      }
      if (!b)
      {
        fprintf(f,"(ATerm) ");
      }
      if (ATisAppl(ATgetFirst(l)) && gsIsNil(ATAgetFirst(l)))
      {
        fprintf(f,"arg%i",i);
      }
      else
      {
        calcTerm(f,ATgetFirst(l),0);
      }
      i++;
    }
    fprintf(f,")");

    if (v)
    {
      fprintf(f,")");
    }
  }
  else if (ATisInt(t))
  {
    ATermList l;
    bool b = false;
    if ((l = innerc_eqns[ATgetInt((ATermInt) t)]) != NULL)
    {
      for (; !ATisEmpty(l); l=ATgetNext(l))
      {
        if (ATgetLength(ATLelementAt(ATLgetFirst(l),2)) == 0)
        {
          b = true;
          break;
        }
      }
    }

    if (b)
    {
      fprintf(f,"rewr_%i_0()",ATgetInt((ATermInt) t));
    }
    else
    {
      fprintf(f,"rewrAppl%i",ATgetInt((ATermInt) t));
    }
  }
  else
  {
    fprintf(f,"%s",ATgetName(ATgetAFun(ATAgetArgument((ATermAppl) t,0)))+1);
  }
}

static ATerm add_args(ATerm a, int num)
{
  if (num == 0)
  {
    return a;
  }
  else
  {
    ATermList l;

    if (ATisList(a))
    {
      l = (ATermList) a;
    }
    else
    {
      l = ATmakeList1(a);
    }

    while (num > 0)
    {
      l = ATappend(l,(ATerm) gsMakeNil());
      num--;
    }
    return (ATerm) l;
  }
}

static int get_startarg(ATerm a, int n)
{
  if (ATisList(a))
  {
    return n-ATgetLength((ATermList) a)+1;
  }
  else
  {
    return n;
  }
}


static int* i_t_st = NULL;
static int i_t_st_s = 0;
static int i_t_st_p = 0;
static void reset_st()
{
  i_t_st_p = 0;
}
static void push_st(int i)
{
  if (i_t_st_s <= i_t_st_p)
  {
    if (i_t_st_s == 0)
    {
      i_t_st_s = 16;
    }
    else
    {
      i_t_st_s = i_t_st_s*2;
    }
    i_t_st = (int*) realloc(i_t_st,i_t_st_s*sizeof(int));
  }
  i_t_st[i_t_st_p] = i;
  i_t_st_p++;
}
static int pop_st()
{
  if (i_t_st_p == 0)
  {
    return 0;
  }
  else
  {
    i_t_st_p--;
    return i_t_st[i_t_st_p];
  }
}
static int peekn_st(int n)
{
  if (i_t_st_p <= n)
  {
    return 0;
  }
  else
  {
    return i_t_st[i_t_st_p-n-1];
  }
}

//#define IT_DEBUG
#define IT_DEBUG_INLINE
#ifdef IT_DEBUG_INLINE
#define IT_DEBUG_FILE f,"//"
#else
#define IT_DEBUG_FILE stderr,
#endif
void RewriterCompilingInnermost::implement_tree_aux(FILE* f, ATermAppl tree, int cur_arg, int parent, int level, int cnt, int d, int arity)
// Print code representing tree to f.
//
// cur_arg   Indices refering to the variable that contains the current
// parent    term. For level 0 this means arg<cur_arg>, for level 1 it
//           means ATgetArgument(arg<parent>,<cur_arg) and for higher
//           levels it means ATgetArgument(t<parent>,<cur_arg>)
//
// parent    Index of cur_arg in the previous level
//
// level     Indicates the how deep we are in the term (e.g. in
//           f(.g(x),y) . indicates level 0 and in f(g(.x),y) level 1
//
// cnt       Counter indicating the number of variables t<i> (0<=i<cnt)
//           used so far (in the current scope)
//
// d         Indicates the current scope depth in the code (i.e. new
//           lines need to use at least 2*d spaces for indent)
//
// arity     Arity of the head symbol of the expression where are
//           matching (for construction of return values)
{
#ifdef IT_DEBUG
  fprintf(IT_DEBUG_FILE "implement_tree_aux: cur_arg=%i, parent=%i, level=%i, cnt=%i\n",cur_arg,parent,level,cnt);
#endif
  if (isS(tree))
  {
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "S(%P)\n",ATgetArgument(tree,0));
#endif
    if (level == 0)
    {
      fprintf(f,"%sATermAppl %s = arg%i; // S\n",whitespace(d*2),ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0)))+1,cur_arg);
    }
    else
    {
      fprintf(f,"%sATermAppl %s = (ATermAppl) ATgetArgument(%s%i,%i); // S\n",whitespace(d*2),ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0)))+1,(level==1)?"arg":"t",parent,cur_arg);
      //fprintf(f,"%sATermAppl %s = t%i; // S\n",whitespace(d*2),ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0)))+1,cur_arg);
    }
    implement_tree_aux(f,ATAgetArgument(tree,1),cur_arg,parent,level,cnt,d,arity);
    return;
  }
  else if (isM(tree))
  {
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "M(%P)\n",ATgetArgument(tree,0));
#endif
    if (level == 0)
    {
      fprintf(f,"%sif ( ATisEqual(%s,arg%i) ) // M\n"
              "%s{\n",
              whitespace(d*2),ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0)))+1,cur_arg,
              whitespace(d*2)
             );
    }
    else
    {
      fprintf(f,"%sif ( ATisEqual(%s,ATgetArgument(%s%i,%i)) ) // M\n"
              //fprintf(f,"%sif ( ATisEqual(%s,t%i) ) // M\n"
              "%s{\n",
              whitespace(d*2),ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0)))+1,(level==1)?"arg":"t",parent,cur_arg,
              //  whitespace(d*2),ATgetName(ATgetAFun(ATAgetArgument(ATAgetArgument(tree,0),0)))+1,cur_arg,
              whitespace(d*2)
             );
    }
    implement_tree_aux(f,ATAgetArgument(tree,1),cur_arg,parent,level,cnt,d+1,arity);
    fprintf(f,"%s} else {\n",whitespace(d*2));
    implement_tree_aux(f,ATAgetArgument(tree,2),cur_arg,parent,level,cnt,d+1,arity);
    fprintf(f,"%s}\n",whitespace(d*2));
    return;
  }
  else if (isF(tree))
  {
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "F(%P)\n",int2term[ATgetInt((ATermInt) ATgetArgument(tree,0))]);
#endif
    if (level == 0)
    {
      fprintf(f,"%sif ( isAppl(arg%i) && ATisEqual(ATgetArgument(arg%i,0),int2ATerm%i) ) // F\n"
              "%s{\n",
              whitespace(d*2),cur_arg,cur_arg,ATgetInt((ATermInt) ATgetArgument(tree,0)),
              whitespace(d*2)
             );
    }
    else
    {
      fprintf(f,"%sif ( isAppl(ATgetArgument(%s%i,%i)) && ATisEqual(ATgetArgument((ATermAppl) ATgetArgument(%s%i,%i),0),int2ATerm%i) ) // F\n"
              //fprintf(f,"%sif ( isAppl(t%i) && ATisEqual(ATgetArgument(t%i,0),int2ATerm%i) ) // F\n"
              "%s{\n"
              "%s  ATermAppl t%i = (ATermAppl) ATgetArgument(%s%i,%i);\n",
              //    "%s  ATermAppl t%i = (ATermAppl) ATgetArgument(t%i,1);\n",
              whitespace(d*2),(level==1)?"arg":"t",parent,cur_arg,(level==1)?"arg":"t",parent,cur_arg,ATgetInt((ATermInt) ATgetArgument(tree,0)),
              //  whitespace(d*2),cur_arg,cur_arg,ATgetInt((ATermInt) ATgetArgument(tree,0)),
              whitespace(d*2),
              whitespace(d*2),cnt,(level==1)?"arg":"t",parent,cur_arg
              //  whitespace(d*2),cnt,cur_arg
             );
    }
    push_st(cur_arg);
    push_st(parent);
    implement_tree_aux(f,ATAgetArgument(tree,1),1,(level==0)?cur_arg:cnt,level+1,cnt+1,d+1,arity);
    pop_st();
    pop_st();
    fprintf(f,"%s} else {\n",whitespace(d*2));
    implement_tree_aux(f,ATAgetArgument(tree,2),cur_arg,parent,level,cnt,d+1,arity);
    fprintf(f,"%s}\n",whitespace(d*2));
    return;
  }
  else if (isD(tree))
  {
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "D\n");
#endif
    int i = pop_st();
    int j = pop_st();
    implement_tree_aux(f,ATAgetArgument(tree,0),j,i,level-1,cnt,d,arity);
    push_st(j);
    push_st(i);
    return;
  }
  else if (isN(tree))
  {
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "N\n");
#endif
    implement_tree_aux(f,ATAgetArgument(tree,0),cur_arg+1,parent,level,cnt,d,arity);
    return;
  }
  else if (isC(tree))
  {
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "C\n");
#endif
    fprintf(f,"%sif ( ATisEqual(",whitespace(d*2));
    calcTerm(f,ATgetArgument(tree,0),0);
    fprintf(f,",rewrAppl%i) ) // C\n"
            "%s{\n",
            true_num,
            whitespace(d*2)
           );
    implement_tree_aux(f,ATAgetArgument(tree,1),cur_arg,parent,level,cnt,d+1,arity);
    fprintf(f,"%s} else {\n",whitespace(d*2));
    implement_tree_aux(f,ATAgetArgument(tree,2),cur_arg,parent,level,cnt,d+1,arity);
    fprintf(f,"%s}\n",whitespace(d*2));
    return;
  }
  else if (isR(tree))
  {
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "R\n");
#endif
    fprintf(f,"%sreturn ",whitespace(d*2));
    if (level > 0)
    {
      //cur_arg = peekn_st(level);
      cur_arg = peekn_st(2*level-1);
    }
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "arity=%i, cur_arg=%i\n",arity,cur_arg);
#endif
    calcTerm(f,add_args(ATgetArgument(tree,0),arity-cur_arg-1),get_startarg(ATgetArgument(tree,0),cur_arg+1));
    fprintf(f,";\n");
    return;
  }
  else
  {
#ifdef IT_DEBUG
    gsfprintf(IT_DEBUG_FILE "X\n");
#endif
    return;
  }
}

void RewriterCompilingInnermost::implement_tree(FILE* f, ATermAppl tree, int arity, int d, int /*opid*/)
{
#ifdef IT_DEBUG
  gsfprintf(IT_DEBUG_FILE "implement_tree %P (%i)\n",int2term[opid],opid);
#endif
  int l = 0;

  while (isC(tree))
  {
    fprintf(f,"%sif ( ATisEqual(",whitespace(d*2));
    calcTerm(f,ATgetArgument(tree,0),0);
    fprintf(f,",rewrAppl%i) ) // C\n"
            "%s{\n"
            "%sreturn ",
            true_num,
            whitespace(d*2),
            whitespace(d*2)
           );
    assert(isR(ATAgetArgument(tree,1)));
    calcTerm(f,add_args(ATgetArgument(ATAgetArgument(tree,1),0),arity),get_startarg(ATgetArgument(ATAgetArgument(tree,1),0),0));
    fprintf(f,";\n"
            "%s} else {\n",
            whitespace(d*2)
           );
    tree = ATAgetArgument(tree,2);
    d++;
    l++;
  }
  if (isR(tree))
  {
    fprintf(f,"%sreturn ",whitespace(d*2));
    calcTerm(f,add_args(ATgetArgument(tree,0),arity),get_startarg(ATgetArgument(tree,0),0));
    fprintf(f,";\n");
  }
  else
  {
    reset_st();
    implement_tree_aux(f,tree,0,0,0,0,d,arity);
  }
  while (l > 0)
  {
    d--;
    fprintf(f,"%s}\n",whitespace(d*2));
    l--;
  }
}

bool RewriterCompilingInnermost::addRewriteRule(ATermAppl Rule)
{
  ATermList n;

  try
  {
    CheckRewriteRule(Rule);
  }
  catch (std::runtime_error& e)
  {
    gsWarningMsg("%s\n",e.what());
    return false;
  }

  need_rebuild = true;

  ATerm u = toInner(ATAgetArgument(Rule,2),true);
  ATerm head;
  ATermList args;

  if (ATisInt(u))
  {
    head = u;
    args = ATmakeList0();
  }
  else
  {
    head = ATgetFirst((ATermList) u);
    args = ATgetNext((ATermList) u);
  }
  if ((n = (ATermList) ATtableGet(tmp_eqns,head)) == NULL)
  {
    n = ATempty;
  }
  n = ATinsert(n,
               (ATerm) ATmakeList4(
                 ATgetArgument(Rule,0),
                 toInner(ATAgetArgument(Rule,1),true),
                 (ATerm) args,
                 toInner(ATAgetArgument(Rule,3),true)));
  ATtablePut(tmp_eqns,head,(ATerm) n);

  return true;
}

bool RewriterCompilingInnermost::removeRewriteRule(ATermAppl /*Rule*/)
{
  return false;
}

void RewriterCompilingInnermost::CompileRewriteSystem(const data_specification& DataSpec, const bool add_rewrite_rules)
{
  made_files = false;
  need_rebuild = true;

  tmp_eqns = ATtableCreate(100,75); // XXX would be nice to know the number op OpIds

  // num_opids = 0;

  true_inner = (ATermInt) OpId2Int(sort_bool::true_(),true);
  true_num = ATgetInt(true_inner);

  const atermpp::vector< data_equation > l=DataSpec.equations();
  if (add_rewrite_rules)
  {
    for (atermpp::vector< data_equation >::const_iterator j=l.begin(); j!=l.end(); ++j)
    {
      addRewriteRule(*j);
    }
  }

  /* int2term = NULL; */
  innerc_eqns = NULL;
}

static void cleanup_file(std::string const& f)
{
  if (unlink(const_cast< char* >(f.c_str())))
  {
    fprintf(stderr,"unable to remove file %s: %s\n",const_cast< char* >(f.c_str()),strerror(errno));
  }
}

void RewriterCompilingInnermost::CleanupRewriteSystem()
{
  if (so_rewr_cleanup != NULL)
  {
    so_rewr_cleanup();
    dlclose(so_handle);
  }
  if (innerc_eqns != NULL)
  {
    // ATunprotectArray((ATerm*) int2term);
    ATunprotectArray((ATerm*) innerc_eqns);
    // free(int2term);
    free(innerc_eqns);
  }
}

void RewriterCompilingInnermost::BuildRewriteSystem()
{
  FILE* f;
  MCRL2_SYSTEM_SPECIFIC_ALLOCA(t,char,100+strlen(INNERC_COMPILE_COMMAND)+strlen(INNERC_LINK_COMMAND));
  void* h;

  CleanupRewriteSystem();

  innerc_eqns = (ATermList*) malloc(get_num_opids()*sizeof(ATermList));
  memset(innerc_eqns,0,get_num_opids()*sizeof(ATermList));
  ATprotectArray((ATerm*) innerc_eqns,get_num_opids());

  for (atermpp::map< ATerm, ATermInt >::const_iterator l=term2int_begin(); 
              l!=term2int_end(); ++l)
  {
    innerc_eqns[ATgetInt(l->second)] = (ATermList) ATtableGet(tmp_eqns,(ATerm) l->second);
  }

  std::ostringstream file_base("innerc_");

  file_base << getpid() << "_" << reinterpret_cast< long >(this);

  file_c  = file_base.str() + ".cpp";
  file_o  = file_base.str() + ".o";
  file_so = file_base.str() + ".so";

  f = fopen(const_cast< char* >(file_c.c_str()),"w");
  if (f == NULL)
  {
    perror("fopen");
    throw mcrl2::runtime_error("Could not create a temporary file for the rewriter");
  }

  //
  //  Print includes
  //
  fprintf(f,  "#include <stdlib.h>\n"
          "#include <string.h>\n"
          "#include \"mcrl2/aterm/aterm2.h\"\n"
          "#include \"assert.h\"\n"
          "#include \"mcrl2/data/detail/rewrite.h\"\n"
          "using namespace aterm;\n"
          "\n"
          "extern \"C\" { ATermAppl rewrite(ATermAppl); }\n"
          "\n"
         );

  //
  // Forward declarations of rewr_* functions
  //
  size_t max_arity = 0;
  for (size_t j=0; j < get_num_opids(); j++)
  {
    if (get_int2term(j)!=atermpp::aterm_appl())
    {
      /* if ( innerc_eqns[j] != NULL )
      { */
      size_t arity = getArity(get_int2term(j));
      if (arity > max_arity)
      {
        max_arity = arity;
      }
  
      /* Declare the function that gets function j in normal form */
      fprintf(f,  "static ATermAppl rewr_%ld_nnf(ATermAppl);\n",j);
  
      for (size_t a=0; a<=arity; a++)
      {
        {
          fprintf(f,  "static ATermAppl rewr_%ld_%ld(",j,a);
          for (size_t i=0; i<a; i++)
          {
            fprintf(f, (i==0)?"ATermAppl arg%ld":", ATermAppl arg%ld",i);
          }
          fprintf(f,  ");\n");
  
        }
      }
    }
  }
  fprintf(f,  "\n\n");

  //
  // Print defs
  //
  fprintf(f,
          "#define ATisInt(x) (ATgetType(x) == AT_INT)\n"
#ifdef USE_VARAFUN_VALUE
          "#define isAppl(x) (ATgetAFun(x) != %li)\n"
          "\n", (long int) ATgetAFun(static_cast<ATermAppl>(data::variable("x", data::sort_bool::bool_())))
#else
          "#define isAppl(x) (ATgetAFun(x) != varAFun)\n"
          "\n"
#endif
         );
  for (size_t i=0; i < get_num_opids(); i++)
  {
    if (get_int2term(i)!=atermpp::aterm_appl())
    {
      fprintf(f,  "static ATerm int2ATerm%ld;\n",i);
      fprintf(f,  "static ATermAppl rewrAppl%ld;\n",i);
    }
  }
  fprintf(f,  "\n"
#ifndef USE_VARAFUN_VALUE
          "static AFun varAFun;\n"
#endif
          "static AFun dataapplAFun;\n"
          "static AFun opidAFun;\n"
         );
  for (size_t i=0; i<=(max_arity==0?1:max_arity); i++)
  {
    fprintf(f,      "static AFun appl%ld;\n",i);
    fprintf(f,  "typedef ATermAppl (*ftype%ld)(",i);
    for (size_t j=0; j<i; j++)
    {
      if (j == 0)
      {
        fprintf(f,              "ATermAppl");
      }
      else
      {
        fprintf(f,              ",ATermAppl");
      }
    }
    fprintf(f,              ");\n");
    fprintf(f,  "\n"
            "static ftype%ld *int2func%ld;\n",i,i);
  }
  fprintf(f,  "static ftype1 *int2func;\n");

  //
  // Implement substitution functions
  //
  fprintf(f,  "\n"
          "\n"
          "static ATerm *substs = NULL;\n"
          "static long substs_size = 0;\n"
          "\n"
          "extern \"C\" {\n"
          "void set_subst(ATermAppl Var, ATerm Expr)\n"
          "{\n"
          "  long n = ATgetAFun(ATgetArgument(Var,0));\n"
          "\n"
          "  if ( n >= substs_size )\n"
          "  {\n"
          "    long newsize;\n"
          "\n"
          "    if ( n >= 2*substs_size )\n"
          "    {\n"
          "      if ( n < 1024 )\n"
          "      {\n"
          "        newsize = 1024;\n"
          "      } else {\n"
          "        newsize = n+1;\n"
          "      }\n"
          "    } else {\n"
          "      newsize = 2*substs_size;\n"
          "    }\n"
          "\n"
          "    if ( substs_size > 0 )\n"
          "    {\n"
          "      ATunprotectArray(substs);\n"
          "    }\n"
          "    substs = (ATerm *) realloc(substs,newsize*sizeof(ATerm));\n"
          "    \n"
          "    if ( substs == NULL )\n"
          "    {\n"
          "      fprintf(stderr,\"Failed to increase the size of a substitution array\");\n"
          "      exit(1);\n"
          "    }\n"
          "\n"
          "    for (long i=substs_size; i<newsize; i++)\n"
          "    {\n"
          "      substs[i]=NULL;\n"
          "    }\n"
          "\n"
          "    ATprotectArray(substs,newsize);\n"
          "    substs_size = newsize;\n"
          "  }\n"
          "\n"
          "  substs[n] = Expr;\n"
          "}\n"
          "\n"
          "ATerm get_subst(ATermAppl Var)\n"
          "{\n"
          "  long n = ATgetAFun(ATgetArgument(Var,0));\n"
          "\n"
          "  if ( n >= substs_size )\n"
          "  {\n"
          "    return (ATerm) Var;\n"
          "  }\n"
          "  \n"
          "  ATerm r = substs[n];\n"
          "  \n"
          "  if ( r == NULL )\n"
          "  {\n"
          "    return (ATerm) Var;\n"
          "  }\n"
          "  \n"
          "  return r;\n"
          "}\n"
          "\n"
          "void clear_subst(ATermAppl Var)\n"
          "{\n"
          "  long n = ATgetAFun(ATgetArgument(Var,0));\n"
          "\n"
          "  if ( n < substs_size )\n"
          "  {\n"
          "    substs[n] = NULL;\n"
          "  }\n"
          "}\n"
          "\n"
          "void clear_substs()\n"
          "{\n"
          "  for (long i=0; i<substs_size; i++)\n"
          "  {\n"
          "    substs[i] = NULL;\n"
          "  }\n"
          "}\n"
          "} // extern C\n"
          "\n"
          "\n"
         );


  for (size_t i=1; i<=max_arity; i++)
  {
    fprintf(f,
            "static ATermAppl varFunc%ld(ATermAppl a",i);
    for (size_t j=0; j<i; j++)
    {
      fprintf(f, ", ATermAppl arg%ld",j);
    }
    fprintf(f, ")\n"
            "{\n");
    fprintf(f,
//      "  ATprintf(\"%%t\\n\",a);\n"
            "  int arity = ATgetArity(ATgetAFun(a));\n"
            "  if ( arity == 1 )\n"
            "  {\n"
            "    if ( ATisInt(ATgetArgument(a,0)) && \n"
            "       (ATgetInt((ATermInt) ATgetArgument(a,0)) < %ld) && \n"
            "       (int2func%ld[ATgetInt((ATermInt) ATgetArgument(a,0))] != NULL) )\n"
            "    {\n"
            "       return int2func%ld[ATgetInt((ATermInt) ATgetArgument(a,0))](",
            get_num_opids(),i,i);
    for (size_t j=0; j<i; j++)
    {
      if (j == 0)
      {
        fprintf(f,"(ATermAppl) arg%ld",j);
      }
      else
      {
        fprintf(f, ", (ATermAppl) arg%ld",j);
      }
    }
    fprintf(f,");\n"
            "    }\n"
            "    else\n"
            "    {\n"
            "      return ATmakeAppl(appl%ld,ATgetArgument(a,0)", i);


    for (size_t j=0; j<i; j++)
    {
      fprintf(f, ",arg%ld",j);
    }
    fprintf(f, ");\n"
            "    }\n"
            "  } else {\n"
            "    ATerm args[arity+%ld];\n"
            "\n"
            "    for (int i=0; i<arity; i++)\n"
            "    {\n"
            "      args[i] = ATgetArgument(a,i);\n"
            "    }\n",i);
    for (size_t j=0; j<i; j++)
    {
      fprintf(f,
              "    args[arity+%ld] = (ATerm) arg%ld;\n",j,j);
    }
    fprintf(f,
            "    if ( ATisInt(args[0]) && (ATgetInt((ATermInt) args[0]) < %ld) )\n"
            "    {\n"
//                      "  gsprintf(\"switch %%i\\n\",i);\n"
            "      switch ( arity+%ld-1 )\n"
            "      {\n",get_num_opids(),i);
    for (size_t j=i; j<=max_arity; j++)
    {
      fprintf(f,
              "        case %ld:\n"
              "          if ( int2func%ld[ATgetInt((ATermInt) args[0])] != NULL )\n"
              "          {\n"
              "            return int2func%ld[ATgetInt((ATermInt) args[0])](",j,j,j);
      for (size_t k=0; k<j; k++)
      {
        if (k == 0)
        {
          fprintf(f,"(ATermAppl) args[%ld]",k+1);
        }
        else
        {
          fprintf(f,", (ATermAppl) args[%ld]",k+1);
        }
      }
      fprintf(f,");\n"
              "          }\n"
              "          break;\n");
    }
    fprintf(f,
            "        default:\n"
            "          break;\n"
            "      }\n"
            "    }\n"
            "\n"
            "    return ATmakeApplArray(mcrl2::data::detail::get_appl_afun_value(arity+%ld),args);\n"
            "  }\n"
            "}\n"
            "\n",i+1);
  }

  //
  // Implement the equations of every operation.
  //
  for (size_t j=0; j < get_num_opids(); j++)
  {
    if (get_int2term(j)!=atermpp::aterm_appl())
    { size_t arity = getArity(get_int2term(j));

      gsfprintf(f,  "// %T\n",get_int2term(j));
      fprintf(f,  "static ATermAppl rewr_%ld_nnf(ATermAppl t)\n"
              "{\n",j);
      if (arity>0)
      {
        fprintf(f,  "  int arity=ATgetArity(ATgetAFun(t))-1;\n");
      }
      for (size_t a=arity+1; a>0; a--)
      {
        if (a>1)
        {
          fprintf(f,  "  if (arity==%ld)\n"
                  "  { ",a-1);
        }
        else
        {
          fprintf(f,  "  ");
        }
        fprintf(f,  "return rewr_%ld_%ld(",j,a-1);
        for (size_t i=0; i<a-1; i++)
        {
          if (i>0)
          {
            fprintf(f,",");
          }
          fprintf(f,  "rewrite((ATermAppl)ATgetArgument(t,%ld))",i+1);
        }
        fprintf(f,  ");\n");
        if (a>1)
        {
          fprintf(f,  "  }\n");
        }
      }
      fprintf(f,  "}\n"
              "\n");
  
      for (size_t a=0; a<=arity; a++)
      {
        fprintf(f,  "static ATermAppl rewr_%ld_%ld(",j,a);
        for (size_t i=0; i<a; i++)
        {
          fprintf(f, (i==0)?"ATermAppl arg%ld":", ATermAppl arg%ld",i);
        }
        fprintf(f,  ")\n"
                "{\n"
               );
  
        // Implement tree
        if (innerc_eqns[j] != NULL)
        {
          implement_tree(f,create_tree(innerc_eqns[j],j,a,true_inner),a,1,j);
        }
  
  
  
        //
        // Finish up function
        //
        if (a == 0)
        {
          fprintf(f,  "  return (rewrAppl%ld",
                  j
                 );
        }
        else
        {
          if (a > 5)
          {
            fprintf(f,  "  return ATmakeAppl(appl%ld,(ATerm) int2ATerm%ld",
                    a,j
                   );
          }
          else
          {
            fprintf(f,  "  return ATmakeAppl%ld(appl%ld,(ATerm) int2ATerm%ld",
                    a+1,a,j
                   );
          }
        }
        for (size_t i=0; i<a; i++)
        {
          fprintf(f,                 ",(ATerm) arg%ld",i);
        }
        fprintf(f,                 ");\n"
                "}\n"
               );
      }
      fprintf(f,  "\n");
    }
  }

  fprintf(f, "extern \"C\" {\n"
          "void rewrite_init()\n"
          "{\n"
#ifndef USE_VARAFUN_VALUE
          "  varAFun = ATmakeAFun(\"DataVarId\", 2, false);\n"
          "  ATprotectAFun(varAFun);\n"
          "  dataapplAFun = ATmakeAFun(\"DataAppl\", 2, false);\n"
          "  ATprotectAFun(dataapplAFun);\n"
          "  opidAFun = ATmakeAFun(\"OpId\", 2, false);\n"
          "  ATprotectAFun(opidAFun);\n"
          "\n"
#endif
          "  mcrl2::data::detail::get_appl_afun_value(%ld);\n",
          max_arity+1
         );
  for (size_t i=0; i<=max_arity; i++)
  {
    fprintf(f,  "  appl%ld = mcrl2::data::detail::get_appl_afun_value(%ld);\n",i,i+1);
  }
  fprintf(f,  "\n");
  for (size_t i=0; i < get_num_opids(); i++)
  {
    if (get_int2term(i)!=atermpp::aterm_appl())
    {
      fprintf(f,  "  int2ATerm%ld=NULL;\n",i);
      fprintf(f,  "  ATprotect(&int2ATerm%ld);\n",i);
      fprintf(f,  "  int2ATerm%ld = (ATerm) ATmakeInt(%ld);\n",i,i);
      fprintf(f,  "  rewrAppl%ld=NULL;\n",i);
      fprintf(f,  "  ATprotectAppl(&rewrAppl%ld);\n",i);
      fprintf(f,  "  rewrAppl%ld = ATmakeAppl(appl0,int2ATerm%ld);\n",i,i);
    }
  }

  /* put the functions that start the rewriting in the array int2func */
  fprintf(f,  "\n");
  fprintf(f,  "\n");
  fprintf(f,  "  int2func = (ftype1 *) malloc(%ld*sizeof(ftype1));\n",get_num_opids());
  for (size_t j=0; j < get_num_opids(); j++)
  {
    if (get_int2term(j)!=atermpp::aterm_appl())
    {
      gsfprintf(f,  "  int2func[%ld] = rewr_%ld_nnf; // %T\n",j,j,get_int2term(j));
    }
  }
  fprintf(f,  "\n");
  for (size_t i=0; i<=max_arity; i++)
  {
    fprintf(f,  "  int2func%ld = (ftype%ld *) malloc(%ld*sizeof(ftype%ld));\n",i,i,get_num_opids(),i);
    for (size_t j=0; j < get_num_opids(); j++)
    {
      if (get_int2term(j)!=atermpp::aterm_appl())
      {
        if (i <= getArity(get_int2term(j)))
        {
          gsfprintf(f,  "  int2func%ld[%ld] = rewr_%ld_%ld;\n",i,j,j,i);
        }
      }
    }
  }
  fprintf(f,  "}\n"
          "\n"
          "void rewrite_cleanup()\n"
          "{\n"
#ifndef USE_VARAFUN_VALUE
          "  ATunprotectAFun(varAFun);\n"
          "  ATunprotectAFun(dataapplAFun);\n"
          "  ATunprotectAFun(opidAFun);\n"
          "\n"
#endif
//          "  free(apples);\n"
          "\n");
  for (size_t i=0; i < get_num_opids(); i++)
  {
    if (get_int2term(i)!=atermpp::aterm_appl())
    {
      fprintf(f,  "  ATunprotect(&int2ATerm%ld);\n",i);
      fprintf(f,  "  ATunprotectAppl(&rewrAppl%ld);\n",i);
    }
  }
  fprintf(f,  "\n");
  fprintf(f,  "  free(int2func);\n");
  for (size_t i=0; i<max_arity; i++)
  {
    fprintf(f,  "  free(int2func%ld);\n",i);
  }
  fprintf(f,  "}\n"
          "\n"
          "ATermAppl rewrite(ATermAppl t)\n"
          "{\n"
          // t is an "APPL" or a var
          "  if ( isAppl(t) )\n"
          "  {\n"
          "    ATerm head = ATgetArgument(t,0);\n"
          // if head is int, just apply rewriter
          "    if ( ATisInt(head) )\n"
          "    {\n"
          "      long function_index = ATgetInt((ATermInt)head);\n"
          "      if ( function_index < %ld )\n"
          "      {\n"
          "        return int2func[function_index](t);\n"
          "      } else {\n"
          "        int arity = ATgetArity(ATgetAFun(t));\n"
          "        ATerm args[arity];\n"
          "        args[0] = head;\n"
          "        for (int i=1; i<arity; i++)\n"
          "        {\n"
          "          args[i] = (ATerm) rewrite((ATermAppl) ATgetArgument(t,i));\n"
          "        }\n"
          "        return ATmakeApplArray(ATgetAFun(t),args);\n"
          "      }\n"
          "    } else {\n"
          // head is a var, get value
          "      ATerm u = get_subst((ATermAppl) head);\n"
          "      long arity_t = ATgetArity(ATgetAFun(t));\n"
          "      long arity_u;\n"
          "      if ( isAppl(u) )\n"
          "      {\n"
          // new head is an APPL, get new head and arity of u
          "        head = ATgetArgument((ATermAppl) u,0);\n"
          "        arity_u = ATgetArity(ATgetAFun((ATermAppl) u));\n"
          "      } else {\n"
          // still a var
          "        head = u;\n"
          "        arity_u = 1;\n"
          "      }\n"
          // nead space for head (1), arguments of u (arity_u-1) and
          // arguments of t (arity_t-1)
          "      ATerm args[arity_u+arity_t-1];\n"
          // set new head
          "      args[0] = head;\n"
          "      long function_index;\n"
          // is head a function symbol?
          "      if ( ATisInt(head) && ((function_index = ATgetInt((ATermInt) head)) < %ld) )\n"
          "      {\n"
          // add arguments of u to new args
          "        for (int i=1; i<arity_u; i++)\n"
          "        {\n"
          "          args[i] = ATgetArgument((ATermAppl) u,i);\n"
          "        }\n"
          "        int k = arity_u;\n"
          // add arguments of t to new args
          "        for (int i=1; i<arity_t; i++,k++)\n"
          "        {\n"
          "          args[k] = ATgetArgument((ATermAppl) t,i);\n"
          "        }\n"
          // call rewrite function of head with new term
          "        return int2func[function_index](ATmakeApplArray(mcrl2::data::detail::get_appl_afun_value(arity_u+arity_t-1),args));\n"
          "      } else {\n"
          // add rewritten arguments of u to new args
          "        for (int i=1; i<arity_u; i++)\n"
          "        {\n"
          "          args[i] = (ATerm) rewrite((ATermAppl) ATgetArgument((ATermAppl) u,i));\n"
          "        }\n"
          "        int k = arity_u;\n"
          // add rewritten arguments of t to new args
          "        for (int i=1; i<arity_t; i++,k++)\n"
          "        {\n"
          "          args[k] = (ATerm) rewrite((ATermAppl) ATgetArgument((ATermAppl) t,i));\n"
          "        }\n"
          // done; return new term
          "        return ATmakeApplArray(mcrl2::data::detail::get_appl_afun_value(arity_u+arity_t-1),args);\n"
          "      }\n"
          "    }\n"
          "  } else {\n"
          // t is a var: just return it(s value)
          "    ATermAppl r=(ATermAppl) get_subst(t);\n"
          "    return r;\n"
          "  }\n"
          "}\n"
          "} // extern C \n",
          get_num_opids(),
          get_num_opids()
         );

  fclose(f);

  gsVerboseMsg("compiling rewriter...\n");
  sprintf(t,INNERC_COMPILE_COMMAND,const_cast< char* >(file_base.str().c_str()));
  gsVerboseMsg("%s\n",t);
  if (system(t) != 0)
  {
    // unlink(file_c); In case of compile errors the .c file is not removed.
    throw mcrl2::runtime_error("Could not compile rewriter.");
  }

  gsVerboseMsg("linking rewriter...\n");
  sprintf(t,INNERC_LINK_COMMAND,const_cast< char* >(file_base.str().c_str()), const_cast< char* >(file_base.str().c_str()));
  gsVerboseMsg("%s\n",t);
  if (system(t) != 0)
  {
    unlink(const_cast< char* >(file_o.c_str()));
    // unlink(file_c); In case of link errors the .c file is not removed.
    throw mcrl2::runtime_error("Could not link rewriter.");
  }

  sprintf(t,"./%s.so",const_cast< char* >(file_base.str().c_str()));
  if ((h = so_handle = dlopen(t,RTLD_NOW)) == NULL)
  {
    unlink(const_cast< char* >(file_so.c_str()));
    unlink(const_cast< char* >(file_o.c_str()));
    unlink(const_cast< char* >(file_c.c_str()));
    throw mcrl2::runtime_error(std::string("Cannot load rewriter: ") + dlerror());
  }
  so_rewr_init = (void (*)()) dlsym(h,"rewrite_init");
  if (so_rewr_init    == NULL)
  {
    gsErrorMsg("%s\n",dlerror());
  }
  so_rewr_cleanup = (void (*)()) dlsym(h,"rewrite_cleanup");
  if (so_rewr_cleanup == NULL)
  {
    gsErrorMsg("%s\n",dlerror());
  }
  so_rewr = (ATermAppl(*)(ATermAppl)) dlsym(h,"rewrite");
  if (so_rewr         == NULL)
  {
    gsErrorMsg("%s\n",dlerror());
  }
  so_set_subst = (void (*)(ATermAppl, ATerm)) dlsym(h,"set_subst");
  if (so_set_subst    == NULL)
  {
    gsErrorMsg("%s\n",dlerror());
  }
  so_get_subst = (ATerm(*)(ATermAppl)) dlsym(h,"get_subst");
  if (so_get_subst    == NULL)
  {
    gsErrorMsg("%s\n",dlerror());
  }
  so_clear_subst = (void (*)(ATermAppl)) dlsym(h,"clear_subst");
  if (so_clear_subst  == NULL)
  {
    gsErrorMsg("%s\n",dlerror());
  }
  so_clear_substs = (void (*)()) dlsym(h,"clear_substs");
  if (so_clear_substs == NULL)
  {
    gsErrorMsg("%s\n",dlerror());
  }
  if ((so_rewr_init    == NULL) ||
      (so_rewr_cleanup == NULL) ||
      (so_rewr         == NULL) ||
      (so_set_subst    == NULL) ||
      (so_get_subst    == NULL) ||
      (so_clear_subst  == NULL) ||
      (so_clear_substs == NULL))
  {
    unlink(const_cast< char* >(file_so.c_str()));
    unlink(const_cast< char* >(file_o.c_str()));
    unlink(const_cast< char* >(file_c.c_str()));
    throw mcrl2::runtime_error("Cannot load rewriter functions.");
  }

  so_rewr_init();

  for (ATermList keys=ATtableKeys(subst_store); !ATisEmpty(keys); keys=ATgetNext(keys))
  {
    so_set_subst((ATermAppl) ATgetFirst(keys),ATtableGet(subst_store,ATgetFirst(keys)));
  }

  need_rebuild = false;
#ifndef NDEBUG
  if (!gsDebug)
#endif
  {
    cleanup_file(file_c);
    cleanup_file(file_o);
    cleanup_file(file_so);
#ifndef NDEBUG
  }
  else
  {
    made_files = true;
#endif
  }
}

RewriterCompilingInnermost::RewriterCompilingInnermost(const data_specification& DataSpec, const bool add_rewrite_rules)
{
  initialize_internal_translation_table_rewriter();
  subst_store = ATtableCreate(100,75);
  so_rewr_cleanup = NULL;
  m_data_specification_for_enumeration = DataSpec;
  initialise_common();
  CompileRewriteSystem(DataSpec,add_rewrite_rules);
}

RewriterCompilingInnermost::~RewriterCompilingInnermost()
{
  CleanupRewriteSystem();
  finalise_common();
  ATtableDestroy(tmp_eqns);
  ATtableDestroy(subst_store);
#ifndef NDEBUG
  if (made_files)
  {
    cleanup_file(file_c);
    cleanup_file(file_o);
    cleanup_file(file_so);
  }
#endif
}

ATermList RewriterCompilingInnermost::rewriteInternalList(ATermList l)
{
  if (l==ATempty)
  {
    return ATempty;
  }

  if (need_rebuild)
  {
    BuildRewriteSystem();
  }

  return ATinsertA(
           rewriteInternalList(ATgetNext(l)),
           so_rewr(ATAgetFirst(l)));
}

ATermAppl RewriterCompilingInnermost::rewrite(ATermAppl Term)
{
  if (need_rebuild)
  {
    BuildRewriteSystem();
  }

  return fromRewriteFormat((ATerm) so_rewr((ATermAppl) toRewriteFormat(Term)));
}

ATerm RewriterCompilingInnermost::rewriteInternal(ATerm Term)
{
  if (need_rebuild)
  {
    BuildRewriteSystem();
  }
  ATerm a = (ATerm) so_rewr((ATermAppl) Term);

  return internal_quantifier_enumeration((ATerm) a);
}

void RewriterCompilingInnermost::setSubstitutionInternal(ATermAppl Var, ATerm Expr)
{
  so_set_subst(Var,Expr);
  ATtablePut(subst_store, (ATerm) Var, Expr);
}

ATerm RewriterCompilingInnermost::getSubstitutionInternal(ATermAppl Var)
{
  return ATtableGet(subst_store, (ATerm) Var);
}

void RewriterCompilingInnermost::clearSubstitution(ATermAppl Var)
{
  so_clear_subst(Var);
  ATtableRemove(subst_store, (ATerm) Var);
}

void RewriterCompilingInnermost::clearSubstitutions()
{
  so_clear_substs();
  ATtableReset(subst_store);
}

RewriteStrategy RewriterCompilingInnermost::getStrategy()
{
  return GS_REWR_INNERC;
}
}
}
}

#endif
