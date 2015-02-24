
#include <types.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <addrspace.h>
#include <synch.h>
#include <current.h>
#include <copyinout.h>
#include <vfs.h>
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
sys_execv(userptr_t arg1, userptr_t arg2){
	char* progname = (char*) arg1;
	char** args = (char**) arg2;

	char** kbuffer;
	int argc = 0;
	int count = 0;
	int result;
	int ksize;
	int usize;
	size_t actual;
	struct vnode *vnode;
	vaddr_t entrypoint, stackptr;

	usize = strlen(progname);
	if(usize % 4 == 0){
		ksize = usize;
	}else{
		ksize = usize + (4 - (usize % 4));
	}
	kbuffer[count] = kmalloc(sizeof(char) * ksize);

	if((result = copyinstr((const_userptr_t)progname, kbuffer[count], ksize, &actual)) != 0){
		return result;
	}

	for(count = 1; count < ARG_MAX; count++){
		if(args[count] == NULL){
			break;
		}else{
			usize = strlen(args[count]);
			if(usize % 4 == 0){
				ksize = usize;
			} else {
				ksize = usize + (4 - (usize % 4));
			}
			
			kbuffer[count] = kmalloc(sizeof(char) * ksize);
			if((result = copyinstr((const_userptr_t)args[count], kbuffer[count], ksize, &actual)) != 0){
				return result;
			}
		}
	}

	argc = count;
	char* kargv[argc+1];			//int32_t?????

	kargv[0] = kmalloc(sizeof(int32_t));
	*kargv[0] = (argc+1) * 4;		//'+1' -> since index starts with zero

	for(count = 1; count < argc; count++){
		kargv[count] = kmalloc(sizeof(int32_t));
		*kargv[count] = *kargv[count-1] + strlen(kbuffer[count-1]);
	}

	kargv[argc] = NULL;

	//load the program
	result = vfs_open(kbuffer[0], O_RDONLY, 0, &vnode);
	if(result){
		return result;
	}

	KASSERT(curthread->t_addrspace == NULL);

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
	kfree(childtrap);

	curthread->t_addrspace = childaddr;
	as_activate(curthread->t_addrspace);

	mips_usermode(&stacktrapframe);
}
