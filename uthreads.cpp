//
// Created by Matanel on 28/03/2022.
//
#include "uthreads.h"
#include <set>
#include <map>
#include <list>
#include <numeric>
#include <iostream>

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

class thread {
 private:

  int _tid;

  sigjmp_buf _env;
  char *_stack;
  thread_entry_point _entry_point;
  state _curr_state;

 public:

  explicit thread (int tid, thread_entry_point entry_point)
      : _tid (tid), _entry_point (entry_point),
        _curr_state (READY), _stack (new char[STACK_SIZE])
  {
    address_t sp = (address_t) _stack + STACK_SIZE - sizeof (address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp (_env[tid], 1);
    (_env[tid]->__jmpbuf)[JB_SP] = translate_address (sp);
    (_env[tid]->__jmpbuf)[JB_PC] = translate_address (pc);
    sigemptyset (&_env[tid]->__saved_mask);
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

  void run ()
  {
    _entry_point ();
  }

  void terminate ()
  {
    //todo: free the buff of size STACK_SIZE allocated in the constructor
  }

} typedef thread;

/****** UThreads Implementation: ******/

int quantum;
int nextAvailableId;
set<int> available_ids;
list<thread *> ready_threads_list;
list<thread *> blocked_threads_list;
map<int, thread *> all_threads;
list<thread *> threads_list;
thread *running_thread;

/*
 * todo doc
 */
int getNextId ()
{
  if (available_ids.empty ()) return -1;
  int nextId = *available_ids.begin ();
  available_ids.erase (nextId);
  return nextId;
}

/**
 * @brief initializes the thread library.
 *
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0) return -1;

  quantum = quantum_usecs;
  for (int i = 0; i < MAX_THREAD_NUM; ++i)
    available_ids.insert (available_ids.end (), i);
  return 0;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn (thread_entry_point entry_point)
{
  int next_available_id = getNextId ();
  if (next_available_id == -1) return -1;
  auto *spawned_thread = new thread (next_available_id, entry_point);
  if (next_available_id == 0) // main thread -> running:
    {
      running_thread = spawned_thread;
    }
  else
    {
      ready_threads_list.push_back (spawned_thread);
    }
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

void run_next_thread ()
{

  running_thread = ready_threads_list.front ();
  running_thread->set_state (RUNNING);
  //todo run thread
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate (int tid)
{
  //todo: test tid.

  if (tid != running_thread->get_id ())
    {
      thread_to_terminate =;
      thread_to_terminate->terminate ();
    }

  if (tid == 0)
    { //terminating main thread
      terminate_all ();
      exit (0);
    }
  running_thread->terminate (); //self-termination
}

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
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
  thread *curr_thread = it->second;
  if (curr_thread->get_state () != BLOCKED)
    {
      if (curr_thread->get_state () == RUNNING)
        {
          run_next_thread ();
        }
      curr_thread->set_state (BLOCKED);
      // todo blocking for number of seconds?

    }

  return 0;

}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
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

  thread *thread_to_resume = all_threads[tid];
  if (thread_to_resume->get_state () == BLOCKED)
    {
      thread_to_resume->set_state (READY);
      ready_threads_list.push_back (thread_to_resume);
    }

}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY threads list.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid==0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep (int num_quantums)
{

}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
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

}

/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums (int tid)
{

}
