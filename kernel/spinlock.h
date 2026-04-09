// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
#ifdef LAB_LOCK
  int nts;
  int n;
#endif
};

// #ifdef LAB_LOCK
// Reader-writer lock.
struct rwspinlock {
  struct spinlock l;
  struct spinlock r;
  int readers;
  int writer;
  int pending_writers;
  struct cpu *cpu;
};

// #endif
