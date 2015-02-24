
#ifndef _PROCESS_H
#define _PROCESS_H

#include <limits.h>

struct process {
	pid_t ppid;
	struct lock* exitlock;
	bool exited;
	int exitcode;
	struct thread* self;
};

struct process* process[PID_MAX];

pid_t generate_pid(void);
int sys_getpid(int32_t*);
int sys_execv(userptr_t, userptr_t);
int sys_fork(int32_t* retval, struct trapframe* tf);
void child_forkentry(void*, unsigned long);

#endif /*_PROCESS_H*/
