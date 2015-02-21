
#ifndef _PROCESS_H
#define _PROCESS_H

struct process {
	pid_t ppid;
	struct lock* exitlock;
	bool exited;
	int exitcode;
	struct thread* self;
}

struct dummy{
	int kkk;
}

#endif /*_PROCESS_H*/
