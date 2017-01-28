#include "lock.h"

/**
 * Lock function implementations
 * 
 * In this program, the lock functions are stubs that do not actually perform
 * any locking. Instead, they mock out the correct behaviour of a lock.
 *
 * This implementation should fail its TESLA assertions for using the lock
 * multiple times.
 */

lock_t lock;

void do_work(void) {
  while(!lock_acquire(&lock)) {}
  lock_release(&lock);
  while(!lock_acquire(&lock)) {}
  lock_release(&lock);
}
