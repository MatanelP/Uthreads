//
// Created by Matanel on 28/03/2022.
//
#include "uthreads.h"
#include <set>
#include <map>
#include <list>
#include <numeric>
#include <iostream>

using namespace std;

enum state { RUNNING, BLOCKED, READY };

class thread {
 private:

  int _id;
  thread_entry_point _entry_point;
  state _curr_state;

 public:

  explicit thread (int id, thread_entry_point entry_point)
      : _id (id), _entry_point (entry_point), _curr_state (READY)
  {}

  int get_id () const
  {
    return _id;
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
    //todo
  }
} typedef thread;

int getNextId ();
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
int quantum;
int nextAvailableId;
set<int> available_ids;
list<thread *> ready_threads_list;
list<thread *> blocked_threads_list;
map<int, thread *> all_threads;
list<thread *> threads_list;
thread *running_thread;

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

int getNextId ()
{
  if (available_ids.empty ()) return -1;
  int nextId = *available_ids.begin ();
  available_ids.erase (nextId);
  return nextId;
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
      curr_thread->set_state (BLOCKED);
      //todo scheduling decision?
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
