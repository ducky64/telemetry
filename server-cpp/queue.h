#include <stddef.h>
#include <stdint.h>

#ifndef _QUEUE_H_
#define _QUEUE_H_

namespace telemetry {
/**
 * Statically allocated, lock-free queue.
 * Thread-safe if used in single-producer single-consumer mode.
 */
template <typename T, size_t N> class Queue {
public:
  Queue() : read_ptr(values), write_ptr(values), begin(values), last(values + N) {}

  // Return true if the queue is full (enqueue will return false).
  bool full() const {
    if (read_ptr == begin) {
      // Read pointer at beginning, need to wrap around check.
      if (write_ptr == last) {
        return true;
      } else {
        return false;
      }
    } else {
      if (write_ptr == (read_ptr - 1)) {
        return true;
      } else {
        return false;
      }
    }
  }

  // Return true if the queue is empty (dequeue will return false).
  bool empty() const {
    return (read_ptr == write_ptr);
  }

  /**
   * Puts a new value to the tail of the queue. Returns true if successful,
   * false if not.
   */
  bool enqueue(const T& value) {
    if (full()) {
      return false;
    }

    //memcpy((void*)write_ptr, &value, sizeof(T));  // Make it array-compatible.
    *write_ptr = value;

    if (write_ptr == last) {
      write_ptr = begin;
    } else {
      write_ptr++;
    }

    return true;
  }

  /**
   * Assigns output to the last element in the queue.
   */
  bool dequeue(T* output) {
    if (empty()) {
      return false;
    }

    //memcpy(output, (void*)read_ptr, sizeof(T));  // Make it array-compatible.
    *output = *read_ptr;

    if (read_ptr == last) {
      read_ptr = begin;
    } else {
      read_ptr++;
    }

    return true;
  }

protected:
  // Lots of volatiles to prevent compiler reordering which could corrupt data
  // when accessed by multiple threads. Yes, it's completely overkill, but
  // memory fences aren't in earlier C++ versions.
  volatile T values[N+1];

  // Read pointer, points to next element to be returned by dequeue.
  // Queue is empty if this equals write_ptr. Must never be incremented
  // past write_ptr.
  volatile T* volatile read_ptr;
  // Write pointer, points to next location to be written by enqueue.
  // Must never be incremented to read_ptr. Queue is full when this is one
  // less than read_ptr.
  volatile T* volatile write_ptr;

  // Pointer to beginning of array, cleaner than using values directly.
  volatile T* const begin;
  // Pointer to one past the last element of the array.
  volatile T* const last;
};

}

#endif
