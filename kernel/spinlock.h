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

#ifdef LAB_LOCK
// Reader-writer lock.
struct rwspinlock {
  volatile int readers;          // number of active readers
  volatile int pending_writers;  // number of waiting writers
  volatile int writer;           // 1 if a writer holds the lock
  struct cpu *cpu;               // writer owner, for debugging
};

#endif
