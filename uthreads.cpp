//
// Created by Matanel on 28/03/2022.
//
#include "uthreads.h"
#include <set>
#include <map>
#include <list>
#include <numeric>
#include <iostream>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address (address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
  : "=g" (ret)
  : "0" (addr));
  return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif

using namespace std;

enum state { RUNNING, BLOCKED, READY };
enum action { BLOCKING, TERMINATING, SLEEPING };

class Thread {
 private:

  int _tid, _turns;

  sigjmp_buf _env;
  char *_stack;
  thread_entry_point _entry_point;
  state _curr_state;

 public:

  explicit Thread (int tid, thread_entry_point entry_point)
      : _tid (tid), _entry_point (entry_point),
        _curr_state (READY), _stack (new char[STACK_SIZE]), _turns (0)
  {
    if (tid == 0) return;
    address_t sp = (address_t) _stack + STACK_SIZE - sizeof (address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp (_env, 1);
    (_env->__jmpbuf)[JB_SP] = translate_address (sp);
    (_env->__jmpbuf)[JB_PC] = translate_address (pc);
    sigemptyset (&_env->__saved_mask);
  }

  ~Thread ()
  {
    delete[] _stack;
  }

  int get_turns () const
  {
    return _turns;
  }

  void advance ()
  {
    _turns++;
  }

  int get_id () const
  {
    return _tid;
  }

  state get_state () const
  {
    return _curr_state;
  }

  void set_state (state state_)
  {
    _curr_state = state_;
  }

  void load ()
  {
    siglongjmp(_env, 1);
  }
  void save ()
  {
    sigsetjmp(_env, 1);
  }

} typedef Thread;

/****** UThreads Implementation: ******/

int total_turns;
set<int> available_ids;
list<Thread *> ready_threads_list;
list<Thread *> blocked_threads_list;
map<int, Thread *> all_threads;
list<Thread *> threads_list;
Thread *running_thread;
struct sigaction sa;
struct itimerval timer;

/*
 * todo doc
 */
int get_next_id ()
{
  if (available_ids.empty ()) return -1;
  int nextId = *available_ids.begin ();
  available_ids.erase (nextId);
  return nextId;
}

void SIGVTALRM_handler (int sig)
{
  //todo: handle what happens upon SIGVTALRM
}

int init_helper (int quantum_usecs)
{
  sa.sa_handler = &SIGVTALRM_handler;
  if (sigaction (SIGVTALRM, &sa, NULL) < 0)
    {
      //todo: prints error
      exit (EXIT_FAILURE);
    }

  // Configure the timer to expire after quantum_usecs usec... */
  timer.it_value.tv_sec = 0;        // first time interval, seconds part
  timer.it_value.tv_usec = quantum_usecs;        // first time interval, microseconds part

  // configure the timer to expire every quantum_usecs usec after that.
  timer.it_interval.tv_sec = 0;    // following time intervals, seconds part
  timer.it_interval.tv_usec = quantum_usecs;    // following time intervals, microseconds part

  // Start a virtual timer. It counts down whenever this process is executing.
  if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
    {
      //todo: prints error
      exit (EXIT_FAILURE);
    }
  return 0;
}
/**
 * @brief initializes the Thread library.
 *
 * You may assume that this function is called before any other Thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0) return -1;
  total_turns = 1;
  all_threads[0] = new Thread (0, nullptr);

  for (int i = 1; i < MAX_THREAD_NUM; ++i)
    available_ids.insert (available_ids.end (), i);

  return init_helper (quantum_usecs);
}

/**
 * @brief Creates a new Thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The Thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each Thread should be allocated with a stack of size STACK_SIZE bytes.
 *
 * @return On success, return the ID of the created Thread. On failure, return -1.
*/
int uthread_spawn (thread_entry_point entry_point)
{
  int next_available_id = get_next_id ();
  if (next_available_id == -1)
    {
      //todo: printerror
      return -1;
    }
  auto *spawned_thread = new Thread (next_available_id, entry_point);
  ready_threads_list.push_back (spawned_thread);
  all_threads[next_available_id] = spawned_thread;
  return next_available_id;
}

bool is_valid_id (int tid)
{

  if (tid <= 0 || tid > MAX_THREAD_NUM - 1)
    {
      return false;
    }

  if (available_ids.find (tid) != available_ids.end ())
    {
      return false;
    }

  return true;

}

bool run_next_thread (action action)
{
  auto next_thread = ready_threads_list.empty () ?
                     ready_threads_list.front () : nullptr;
  if (next_thread == nullptr)
    { // no next threads to run
      running_thread->advance ();
      return 0;
    }
  else
    {
      ready_threads_list.pop_front ();
    }
  switch (action)
    {
      case TERMINATING:break;
      case BLOCKING:
        //todo
        break;
      case SLEEPING:
        //todo
        break;
    }
  running_thread = next_thread;
  running_thread->set_state (RUNNING);
  running_thread->load ();
  running_thread->advance ();
  total_turns++;
  return 0;

}

void terminate_thread (int tid)
{
  auto p_thread = all_threads[tid];
  all_threads.erase (tid);
  available_ids.insert (tid);
  ready_threads_list.remove (p_thread);
  delete p_thread;
}
/**
 * @brief Terminates the Thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this Thread should be released. If no Thread with ID tid exists it
 * is considered an error. Terminating the main Thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the Thread was successfully terminated and -1 otherwise. If a Thread terminates
 * itself or the main Thread is terminated, the function does not return.
*/
int uthread_terminate (int tid)
{
  if (tid == 0)
    { //terminating main Thread
      for (auto &thread_p: all_threads)
        {
          delete thread_p.second;
        }
      exit (0);
    }

  if (available_ids.count (tid))
    { //tid not assigned
      //todo: print error.
      return -1;
    }

  if (tid == running_thread->get_id ()) //self-termination
    {
      terminate_thread (tid);
      if (!run_next_thread (TERMINATING))
        {
          //todo: print error.
          return -1;
        }
    }

  terminate_thread (tid);
  return 0;

}

/**
 * @brief Blocks the Thread with ID tid. The Thread may be resumed later using uthread_resume.
 *
 * If no Thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main Thread (tid == 0). If a Thread blocks itself, a scheduling decision should be made. Blocking a Thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block (int tid)
{
  if (!is_valid_id (tid))
    {
      return -1;
    }

  auto it = all_threads.find (tid);
  Thread *curr_thread = it->second;
  if (curr_thread->get_state () != BLOCKED)
    {
      if (curr_thread->get_state () == RUNNING)
        {
          run_next_thread (SLEEPING);
        }
      curr_thread->set_state (BLOCKED);
      // todo blocking for number of seconds?

    }

  return 0;

}

/**
 * @brief Resumes a blocked Thread with ID tid and moves it to the READY state.
 *
 * Resuming a Thread in a RUNNING or READY state has no effect and is not considered as an error. If no Thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume (int tid)
{
  if (!is_valid_id (tid))
    {
      return -1;
    }

  Thread *thread_to_resume = all_threads[tid];
  if (thread_to_resume->get_state () == BLOCKED)
    {
      thread_to_resume->set_state (READY);
      ready_threads_list.push_back (thread_to_resume);
    }

}

/**
 * @brief Blocks the RUNNING Thread for num_quantums quantums.
 *
 * Immediately after the RUNNING Thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the Thread should go back to the end of the READY threads list.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the Thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main Thread (tid==0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep (int num_quantums)
{

}

/**
 * @brief Returns the Thread ID of the calling Thread.
 *
 * @return The ID of the calling Thread.
*/
int uthread_get_tid ()
{
  return running_thread->get_id ();
}

/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums ()
{
  return total_turns;
}

/**
 * @brief Returns the number of quantums the Thread with ID tid was in RUNNING state.
 *
 * On the first time a Thread runs, the function should return 1. Every additional quantum that the Thread starts should
 * increase this value by 1 (so if the Thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no Thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the Thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums (int tid)
{
  return all_threads.count (tid) ? all_threads[tid]->get_turns () : -1;
}
