
#ifndef _PROCESS_H
#define _PROCESS_H

#include <limits.h>

struct process {
	pid_t ppid;
	struct lock* exitlock;
	struct cv* exitcv;
	bool exited;
	int exitcode;
	struct thread* self;
};

struct process* process[PID_MAX];

pid_t generate_pid(void);
int sys_getpid(int32_t*);
int sys_execv(userptr_t, userptr_t);
int sys_fork(int32_t*, struct trapframe*);
void child_forkentry(void*, unsigned long);
int sys_waitpid(int32_t*, userptr_t, userptr_t, userptr_t);
int sys_exit(userptr_t);

#endif /*_PROCESS_H*/
