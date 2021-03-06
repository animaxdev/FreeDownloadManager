//
// task_io_service.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2007 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_TASK_IO_SERVICE_HPP
#define ASIO_DETAIL_TASK_IO_SERVICE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/push_options.hpp"

#include "asio/error_code.hpp"
#include "asio/io_service.hpp"
#include "asio/detail/call_stack.hpp"
#include "asio/detail/event.hpp"
#include "asio/detail/handler_alloc_helpers.hpp"
#include "asio/detail/handler_invoke_helpers.hpp"
#include "asio/detail/handler_queue.hpp"
#include "asio/detail/mutex.hpp"
#include "asio/detail/service_base.hpp"
#include "asio/detail/task_io_service_fwd.hpp"

namespace asio {
namespace detail {

template <typename Task>
class task_io_service
  : public asio::detail::service_base<task_io_service<Task> >
{
public:
  // Constructor.
  task_io_service(asio::io_service& io_service)
    : asio::detail::service_base<task_io_service<Task> >(io_service),
      mutex_(),
      task_(use_service<Task>(io_service)),
      task_interrupted_(true),
      outstanding_work_(0),
      stopped_(false),
      shutdown_(false),
      first_idle_thread_(0)
  {
    handler_queue_.push(&task_handler_);
  }

  void init(size_t /*concurrency_hint*/)
  {
  }

  // Destroy all user-defined handler objects owned by the service.
  void shutdown_service()
  {
    asio::detail::mutex::scoped_lock lock(mutex_);
    shutdown_ = true;
    lock.unlock();

    // Destroy handler objects.
    while (!handler_queue_.empty())
    {
      handler_queue::handler* h = handler_queue_.front();
      handler_queue_.pop();
      if (h != &task_handler_)
        h->destroy();
    }

    // Reset handler queue to initial state.
    handler_queue_.push(&task_handler_);
  }

  // Run the event loop until interrupted or no more work.
  size_t run(asio::error_code& ec)
  {
    typename call_stack<task_io_service>::context ctx(this);

    idle_thread_info this_idle_thread;
    this_idle_thread.next = 0;

    asio::detail::mutex::scoped_lock lock(mutex_);

    size_t n = 0;
    while (do_one(lock, &this_idle_thread, ec))
      if (n != (std::numeric_limits<size_t>::max)())
        ++n;
    return n;
  }

  // Run until interrupted or one operation is performed.
  size_t run_one(asio::error_code& ec)
  {
    typename call_stack<task_io_service>::context ctx(this);

    idle_thread_info this_idle_thread;
    this_idle_thread.next = 0;

    asio::detail::mutex::scoped_lock lock(mutex_);

    return do_one(lock, &this_idle_thread, ec);
  }

  // Poll for operations without blocking.
  size_t poll(asio::error_code& ec)
  {
    typename call_stack<task_io_service>::context ctx(this);

    asio::detail::mutex::scoped_lock lock(mutex_);

    size_t n = 0;
    while (do_one(lock, 0, ec))
      if (n != (std::numeric_limits<size_t>::max)())
        ++n;
    return n;
  }

  // Poll for one operation without blocking.
  size_t poll_one(asio::error_code& ec)
  {
    typename call_stack<task_io_service>::context ctx(this);

    asio::detail::mutex::scoped_lock lock(mutex_);

    return do_one(lock, 0, ec);
  }

  // Interrupt the event processing loop.
  void stop()
  {
    asio::detail::mutex::scoped_lock lock(mutex_);
    stop_all_threads(lock);
  }

  // Reset in preparation for a subsequent run invocation.
  void reset()
  {
    asio::detail::mutex::scoped_lock lock(mutex_);
    stopped_ = false;
  }

  // Notify that some work has started.
  void work_started()
  {
    asio::detail::mutex::scoped_lock lock(mutex_);
    ++outstanding_work_;
  }

  // Notify that some work has finished.
  void work_finished()
  {
    asio::detail::mutex::scoped_lock lock(mutex_);
    if (--outstanding_work_ == 0)
      stop_all_threads(lock);
  }

  // Request invocation of the given handler.
  template <typename Handler>
  void dispatch(Handler handler)
  {
    if (call_stack<task_io_service>::contains(this))
      asio_handler_invoke_helpers::invoke(handler, &handler);
    else
      post(handler);
  }

  // Request invocation of the given handler and return immediately.
  template <typename Handler>
  void post(Handler handler)
  {
    // Allocate and construct an operation to wrap the handler.
    handler_queue::scoped_ptr ptr(handler_queue::wrap(handler));

    asio::detail::mutex::scoped_lock lock(mutex_);

    // If the service has been shut down we silently discard the handler.
    if (shutdown_)
      return;

    // Add the handler to the end of the queue.
    handler_queue_.push(ptr.get());
    ptr.release();

    // An undelivered handler is treated as unfinished work.
    ++outstanding_work_;

    // Wake up a thread to execute the handler.
    if (!interrupt_one_idle_thread(lock))
    {
      if (!task_interrupted_)
      {
        task_interrupted_ = true;
        task_.interrupt();
      }
    }
  }

private:
  struct idle_thread_info;

  size_t do_one(asio::detail::mutex::scoped_lock& lock,
      idle_thread_info* this_idle_thread, asio::error_code& ec)
  {
    if (outstanding_work_ == 0 && !stopped_)
    {
      stop_all_threads(lock);
      ec = asio::error_code();
      return 0;
    }

    bool polling = !this_idle_thread;
    bool task_has_run = false;
    while (!stopped_)
    {
      if (!handler_queue_.empty())
      {
        // Prepare to execute first handler from queue.
        handler_queue::handler* h = handler_queue_.front();
        handler_queue_.pop();

        if (h == &task_handler_)
        {
          bool more_handlers = (!handler_queue_.empty());
          task_interrupted_ = more_handlers || polling;
          lock.unlock();

          // If the task has already run and we're polling then we're done.
          if (task_has_run && polling)
          {
            ec = asio::error_code();
            return 0;
          }
          task_has_run = true;
          
          task_cleanup c(lock, *this);

          // Run the task. May throw an exception. Only block if the handler
          // queue is empty and we have an idle_thread_info object, otherwise
          // we want to return as soon as possible.
          task_.run(!more_handlers && !polling);
        }
        else
        {
          lock.unlock();
          handler_cleanup c(lock, *this);

          // Invoke the handler. May throw an exception.
          h->invoke(); // invoke() deletes the handler object

          ec = asio::error_code();
          return 1;
        }
      }
      else if (this_idle_thread)
      {
        // Nothing to run right now, so just wait for work to do.
        this_idle_thread->next = first_idle_thread_;
        first_idle_thread_ = this_idle_thread;
        this_idle_thread->wakeup_event.clear(lock);
        this_idle_thread->wakeup_event.wait(lock);
      }
      else
      {
        ec = asio::error_code();
        return 0;
      }
    }

    ec = asio::error_code();
    return 0;
  }

  // Stop the task and all idle threads.
  void stop_all_threads(
      asio::detail::mutex::scoped_lock& lock)
  {
    stopped_ = true;
    interrupt_all_idle_threads(lock);
    if (!task_interrupted_)
    {
      task_interrupted_ = true;
      task_.interrupt();
    }
  }

  // Interrupt a single idle thread. Returns true if a thread was interrupted,
  // false if no running thread could be found to interrupt.
  bool interrupt_one_idle_thread(
      asio::detail::mutex::scoped_lock& lock)
  {
    if (first_idle_thread_)
    {
      idle_thread_info* idle_thread = first_idle_thread_;
      first_idle_thread_ = idle_thread->next;
      idle_thread->next = 0;
      idle_thread->wakeup_event.signal(lock);
      return true;
    }
    return false;
  }

  // Interrupt all idle threads.
  void interrupt_all_idle_threads(
      asio::detail::mutex::scoped_lock& lock)
  {
    while (first_idle_thread_)
    {
      idle_thread_info* idle_thread = first_idle_thread_;
      first_idle_thread_ = idle_thread->next;
      idle_thread->next = 0;
      idle_thread->wakeup_event.signal(lock);
    }
  }

  // Helper class to perform task-related operations on block exit.
  class task_cleanup
  {
  public:
    task_cleanup(asio::detail::mutex::scoped_lock& lock,
        task_io_service& task_io_svc)
      : lock_(lock),
        task_io_service_(task_io_svc)
    {
    }

    ~task_cleanup()
    {
      // Reinsert the task at the end of the handler queue.
      lock_.lock();
      task_io_service_.task_interrupted_ = true;
      task_io_service_.handler_queue_.push(&task_io_service_.task_handler_);
    }

  private:
    asio::detail::mutex::scoped_lock& lock_;
    task_io_service& task_io_service_;
  };

  // Helper class to perform handler-related operations on block exit.
  class handler_cleanup;
  friend class handler_cleanup;
  class handler_cleanup
  {
  public:
    handler_cleanup(asio::detail::mutex::scoped_lock& lock,
        task_io_service& task_io_svc)
      : lock_(lock),
        task_io_service_(task_io_svc)
    {
    }

    ~handler_cleanup()
    {
      lock_.lock();
      if (--task_io_service_.outstanding_work_ == 0)
        task_io_service_.stop_all_threads(lock_);
    }

  private:
    asio::detail::mutex::scoped_lock& lock_;
    task_io_service& task_io_service_;
  };

  // Mutex to protect access to internal data.
  asio::detail::mutex mutex_;

  // The task to be run by this service.
  Task& task_;

  // Handler object to represent the position of the task in the queue.
  class task_handler
    : public handler_queue::handler
  {
  public:
    task_handler()
      : handler_queue::handler(0, 0)
    {
    }
  } task_handler_;

  // Whether the task has been interrupted.
  bool task_interrupted_;

  // The count of unfinished work.
  int outstanding_work_;

  // The queue of handlers that are ready to be delivered.
  handler_queue handler_queue_;

  // Flag to indicate that the dispatcher has been stopped.
  bool stopped_;

  // Flag to indicate that the dispatcher has been shut down.
  bool shutdown_;

  // Structure containing information about an idle thread.
  struct idle_thread_info
  {
    event wakeup_event;
    idle_thread_info* next;
  };

  // The number of threads that are currently idle.
  idle_thread_info* first_idle_thread_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_TASK_IO_SERVICE_HPP
