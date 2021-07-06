// Author(s): Maurice Laveaux.
// Copyright: see the accompanying file COPYING or copy at
// https://github.com/mCRL2org/mCRL2/blob/master/COPYING
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ATERMPP_DETAIL_THREAD_ATERM_POOL_H
#define ATERMPP_DETAIL_THREAD_ATERM_POOL_H

#include "mcrl2/atermpp/detail/aterm_pool.h"

#include <atomic>

namespace atermpp
{
namespace detail
{

/// \brief This is a thread's specific access to the global aterm pool which ensures that
///        garbage collection and hash table resizing can proceed.
class thread_aterm_pool final : public thread_aterm_pool_interface, mcrl2::utilities::noncopyable
{
public:
  thread_aterm_pool(aterm_pool& global_pool)
    : m_pool(global_pool)
  {
    m_pool.register_thread_aterm_pool(*this);
  }

  ~thread_aterm_pool() override
  {
    m_pool.remove_thread_aterm_pool(*this);
    print_local_performance_statistics();
  }

  /// \threadsafe
  inline function_symbol create_function_symbol(const std::string& name, const std::size_t arity, const bool check_for_registered_functions = false);

  /// \threadsafe
  inline void create_int(aterm& term, std::size_t val);

  /// \threadsafe
  inline void create_term(aterm& term, const function_symbol& sym);

  /// \threadsafe
  template<class ...Terms>
  inline void create_appl(aterm& term, const function_symbol& sym, const Terms&... arguments);

  /// \threadsafe
  template<typename ForwardIterator>
  inline void create_appl_dynamic(aterm& term,
      const function_symbol& sym,
      ForwardIterator begin,
      ForwardIterator end);

  /// \threadsafe
  template<typename InputIterator, typename ATermConverter>
  inline void create_appl_dynamic(aterm& term,
      const function_symbol& sym,
      ATermConverter convert_to_aterm,
      InputIterator begin,
      InputIterator end);

  // Implementation of thread_aterm_pool_interface
  inline void print_local_performance_statistics() const override;
  inline void wait_for_busy() const override;
  inline void set_forbidden(bool value) override;

private:
  /// \brief Called before entering the global term pool.
  inline void lock_shared();

  /// \brief Called after leaving the global term pool.
  inline void unlock_shared();

  aterm_pool& m_pool;

  /// \brief It can happen that during create_appl with converter the converter generates new terms.
  ///        As such these terms might only be protected after the term_appl was actually created.
  std::size_t m_creation_depth = 0;

  /// \brief A boolean flag indicating whether this thread is working inside the global aterm pool.
  std::atomic<bool> m_busy_flag = false;
  std::atomic<bool> m_forbidden_flag = false;

  std::stack<std::reference_wrapper<_aterm>> m_todo; ///< A reusable todo stack.
};

} // namespace detail
} // namespace atermpp

#include "thread_aterm_pool_implementation.h"

#endif // ATERMPP_DETAIL_ATERM_POOL_H
