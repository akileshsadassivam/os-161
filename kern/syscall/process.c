
#include <types.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/wait.h>
#include <addrspace.h>
#include <synch.h>
#include <current.h>
#include <copyinout.h>
#include <vfs.h>
#include <syscall.h>
#include <process.h>

/* Function to generate pid for the newly created process */
pid_t 
generate_pid(){

	int itr = 1;	//pid should start only from 1 and not 0
	pid_t pid = -1;

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
sys_execv(userptr_t arg1, userptr_t arg2){
	char* progname; 
	char** args = (char**) arg2;

	char** kbuffer;
	int argc = 0;
	int count = 0;
	int result;
	int ksize;
	int usize;
	int argtotalsize = 0;
	size_t actual;
	struct vnode *vnode;
	vaddr_t entrypoint, stackptr;

	if((char*)arg1 == (char*) 0x40000000 || (char*)arg1 >= (char*) 0x80000000 || 
		(char*)arg2 == (char*) 0x40000000 || (char*)arg2 >= (char*) 0x80000000){
                 return EFAULT;
        }

	char temp[128];
	result = copyinstr((const_userptr_t)arg1, temp, 128, &actual);
	if(result){
		return EFAULT;
	}	

	if(arg1 == NULL || args == NULL){
		return EFAULT;
	}

	if(strlen((char*)arg1) == 0){
		return EINVAL;
	}

	progname = (char*) arg1;

	for(count = 0; count < ARG_MAX; count++){
		if((char*)args[count] == (char*) 0x40000000 || (char*)args[count] >= (char*) 0x80000000){
			return EFAULT;
		}

		if(args[count] == NULL){
			break;
		}
	}
	kbuffer = kmalloc(count * sizeof(char));

	for(int var = 0; var < count; var++){
		usize = strlen(args[var]) + 1;
		if(usize == 0){
			return EINVAL;
		}

		if(usize % 4 == 0){
			ksize = usize;
		} else {
			ksize = usize + (4 - (usize % 4));
		}
			
		argtotalsize += ksize;
		kbuffer[var] = kmalloc(sizeof(char) * ksize);
		if((result = copyinstr((const_userptr_t)args[var], kbuffer[var], ksize, &actual)) != 0){
			return result;
		}
	}

	argc = count;
	int32_t* kargv[argc+1];

	kargv[0] = kmalloc(sizeof(int32_t));
	*kargv[0] = (argc+1) * 4;		//'+1' -> since index starts with zero

	for(count = 1; count < argc; count++){
		kargv[count] = kmalloc(sizeof(int32_t));
		*kargv[count] = *kargv[count-1] + strlen(kbuffer[count-1]) + 1;
	}

	if(argc == 0){
		kfree(kargv[argc]);
	}
	kargv[argc] = NULL;

	//load the program
	result = vfs_open(progname, O_RDONLY, 0, &vnode);
	if(result){
		return result;
	}

	//KASSERT(curthread->t_addrspace == NULL);
	as_destroy(curthread->t_addrspace);

	//create an address space
	curthread->t_addrspace = as_create();
	if(curthread->t_addrspace == NULL){
		vfs_close(vnode);
		return ENOMEM;
	}

	//Activate the address space for the executable
	as_activate(curthread->t_addrspace);

	//Load the elf
	result = load_elf(vnode, &entrypoint);
	if(result){
		vfs_close(vnode);
		return result;
	}

	vfs_close(vnode);


	//Get the initial stack pointer
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if(result){
		return result;
	}

	//Calculate the bottom of the stack based on the initial stack pointer and kernel buffer
	vaddr_t bottomstack = stackptr - (vaddr_t)argtotalsize - (vaddr_t)((argc+1) * (sizeof(int32_t)));
	vaddr_t curstack = bottomstack;
	vaddr_t kargstack = bottomstack;

	for(count = 0; count < argc; count++){
		kargstack = bottomstack + (count * sizeof(int32_t));
		curstack = bottomstack + (vaddr_t)*kargv[count];

		result = copyout(&curstack, (userptr_t)kargstack, sizeof(int32_t));
		if(result){
			return result;
		}

		result = copyoutstr(kbuffer[count], (userptr_t)curstack, strlen(kbuffer[count]) + 1, &actual);
		if(result){
			return result;
		}	
	}

	for(int count = 0; count<argc; count++){
		kfree(kargv[count]);
		kfree(kbuffer[count]);
	}
	kfree(kbuffer);

	enter_new_process(argc, (userptr_t)bottomstack, bottomstack, entrypoint);

	panic("enter_new_process returned");
	return EINVAL;
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
		kfree(childtrap);
		return ENOMEM;
	}

	result = thread_fork("process", child_forkentry, childtrap,
				(unsigned long) childaddr, &childthread); 

	if(childthread == NULL){
		return ENOMEM;
	}

	if(result){
		return result;
	}

	*retval = childthread->t_pid;
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
	kfree(childtrap);
	childtrap = NULL;

	curthread->t_addrspace = childaddr;
	as_activate(curthread->t_addrspace);

	mips_usermode(&stacktrapframe);
}

int
sys_waitpid(int32_t* retval, userptr_t arg1, userptr_t arg2, userptr_t arg3){
	pid_t pid = (pid_t)arg1;
	int* status = (int*)arg2;
	int options = (int)arg3;

	pid_t debug = curthread->t_pid;
	(void)debug;

	if(status == NULL || ((int)arg2 & 3) != 0){
		return EFAULT;
	}

	if((int*)arg1 == (int*) 0x40000000 || (int*)arg1 == (int*) 0x80000000
		|| (int*)arg2 == (int*) 0x40000000 || (int*)arg2 == (int*) 0x80000000
		|| (int*)arg3 == (int*) 0x40000000 || (int*)arg3 == (int*) 0x80000000){
		return EFAULT;
	}

	if(options != 0 && options != WNOHANG && options != WUNTRACED){
		return EINVAL;
	}

	if(pid < 1 || pid >= PID_MAX){
		return ESRCH;
	}

	if(process[pid] == NULL){
		return ESRCH;
	}

	if(curthread->t_pid == pid){
		return EINVAL;
	}

	if(process[pid]->ppid != curthread->t_pid){
		return ECHILD;
	}

	lock_acquire(process[pid]->exitlock);	
	if(!process[pid]->exited){
		cv_wait(process[pid]->exitcv, process[pid]->exitlock);
	}
	
	*status = process[pid]->exitcode;
	lock_release(process[pid]->exitlock);
	*retval = pid;

	lock_destroy(process[pid]->exitlock);
	cv_destroy(process[pid]->exitcv);
	kfree(process[pid]);
	process[pid] = NULL;

	return 0;
}

int
sys_exit(userptr_t exitcode){
	pid_t pid = curthread->t_pid;

	process[pid]->exited = true;

	pid_t ppid = process[pid]->ppid;
	lock_acquire(process[pid]->exitlock);
	if(!process[ppid]->exited){
		process[pid]->exitcode = _MKWAIT_EXIT((int)exitcode);
		cv_signal(process[pid]->exitcv,process[pid]->exitlock);
		lock_release(process[pid]->exitlock);
	}else{
		lock_destroy(process[pid]->exitlock);
		cv_destroy(process[pid]->exitcv);
		kfree(process[pid]);
		process[pid] = NULL;
	}

	thread_exit();

	return 0;
}
