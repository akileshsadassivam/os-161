
#include <types.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <kern/errno.h>
#include <addrspace.h>
#include <synch.h>
#include <current.h>
#include <process.h>

/* Function to generate pid for the newly created process */
pid_t 
generate_pid(){

	int itr = 1;	//pid should start only from 1 and not 0
	pid_t pid = ENPROC;

	for(; itr < PID_MAX; itr++){
		if(process[itr] == NULL){
			process[itr] = kmalloc(sizeof(struct process));
			if(process[itr] == NULL){
				return ENOMEM;
			}
			
			pid = itr;
			break;
		}
	}

	return pid;
}

/* Function to provide the current process pid */
int
sys_getpid(int32_t* retval){
	*retval = (int32_t) curthread->t_pid;

	return 0;
}

int
sys_fork(int32_t* retval, struct trapframe* tf){
	struct trapframe* childtrap = NULL;
	struct addrspace* childaddr = NULL;
	struct thread* childthread = NULL;
	int result;

	childtrap = kmalloc(sizeof(struct trapframe));
	if(childtrap == NULL){
		return ENOMEM;
	}

	memcpy(childtrap, tf, sizeof(struct trapframe));

	as_copy(curthread->t_addrspace, &childaddr);
	if(childaddr == NULL){
		return ENOMEM;
	}

	result = thread_fork("process", child_forkentry, childtrap,
				(unsigned long) childaddr, &childthread); 

	if(childthread == NULL){
		return ENOMEM;
	}

	if(result){
		return ENOMEM;
	}else{
		*retval = childthread->t_pid;
	}
	
	return result;
}

void
child_forkentry(void* data1, unsigned long data2){
	struct trapframe stacktrapframe;
	struct trapframe* childtrap = (struct trapframe*) data1;
	struct addrspace* childaddr = (struct addrspace*) data2;
	
	childtrap->tf_v0 = 0;
	childtrap->tf_a3 = 0;
	childtrap->tf_epc += 4;

	memcpy(&stacktrapframe, childtrap, sizeof(struct trapframe));

	curthread->t_addrspace = childaddr;
	as_activate(curthread->t_addrspace);

	mips_usermode(&stacktrapframe);
}
