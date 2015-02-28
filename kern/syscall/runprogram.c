/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <copyinout.h>
#include <synch.h>
#include <file_syscall.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, unsigned long argc, char** args)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	 /*setting up STDIN STDOUT STDERR*/
	for(int count = 0; count < 3; count++){
		int flag;
		if(count == 0){
			flag = O_RDONLY;
		}else{
			flag = O_WRONLY;
		}

		curthread->filetable[count] = kmalloc(sizeof(struct filehandle));
		if(curthread->filetable[count] == NULL){
			return ENOMEM;
		}

		curthread->filetable[count]->flags = flag;                                        /*set flags */
        	curthread->filetable[count]->offset = 0;
        	curthread->filetable[count]->refcnt = 1;
        	curthread->filetable[count]->lock = lock_create("std_lock");

		if(curthread->filetable[count]->lock == NULL){
			kfree(curthread->filetable[count]);
			return ENOMEM;
		}

		char* path = kstrdup("con:");

		result = vfs_open(path, flag, 06664, &curthread->filetable[count]->vn);

        	if(result){
                	return result;
        	}
	}

	/* Open the file. */
	int argtotalsize = 0;
	size_t actual;

	for(unsigned int count = 0; count < argc; count++){
                if(args[count] == NULL){
                        break;
                }else{
                        int usize = strlen(args[count]) + 1;
			int ksize;

                        if(usize == 0){
                                return EINVAL;
                        }

                        if(usize % 4 == 0){
                                ksize = usize;
                        } else {
                                ksize = usize + (4 - (usize % 4));
                        }
			
			argtotalsize += ksize;
                }
        }

	int32_t* kargv[argc+1];
 
        kargv[0] = kmalloc(sizeof(int32_t));
        *kargv[0] = (argc+1) * 4;               //'+1' -> since index starts with zero
 
        for(unsigned int count = 1; count < argc; count++){
                kargv[count] = kmalloc(sizeof(int32_t));
                *kargv[count] = *kargv[count-1] + strlen(args[count-1]) + 1;
        }
 
        if(argc == 0){
               kfree(kargv[argc]);
        }
        kargv[argc] = NULL;

	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	KASSERT(curthread->t_addrspace == NULL);

	/* Create a new address space. */
	curthread->t_addrspace = as_create();
	if (curthread->t_addrspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_addrspace);

	

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		return result;
	}

	//Calculate the bottom of the stack based on the initial stack pointer and kernel buffer
        vaddr_t bottomstack = stackptr - (vaddr_t)argtotalsize - (vaddr_t)((argc + 1) * (sizeof(int32_t)));
        vaddr_t curstack = bottomstack;
        vaddr_t kargstack = bottomstack;

        for(unsigned int count = 0; count < argc; count++){
                kargstack = bottomstack + (count * sizeof(int32_t));
                curstack = bottomstack + (vaddr_t)*kargv[count];

                result = copyout(&curstack, (userptr_t)kargstack, sizeof(int32_t));
                if(result){
                        return result;
                }

                result = copyoutstr(args[count], (userptr_t)curstack, strlen(args[count]) + 1, &actual);
                if(result){
                        return result;
                }
        }


	/* Warp to user mode. */
	enter_new_process(argc /*argc*/, (userptr_t) bottomstack /*userspace addr of argv*/,
			  bottomstack, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

