
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <lib.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <kern/stat.h>
#include <vnode.h>
#include <vfs.h>
#include <vm.h>
#include <swap.h>

struct vnode* sw_vn;
extern struct spinlock cm_lock;

swapspace sw_space[MAX_VAL];

void
swapspace_init(){

	for(int itr = 0; itr < MAX_VAL; itr++){
		sw_space[itr].sw_vaddr = 0;
	}
	
	int ret = vfs_open((char*)"lhd0raw:", O_RDWR, 0, &sw_vn);
	KASSERT(ret == 0);
	KASSERT(sw_vn != NULL);
}


int
read_page(void* kbuf, off_t sw_offset){
	struct iovec iovectr;
	struct uio uiovar;

	KASSERT(sw_vn != NULL);
	uio_kinit(&iovectr, &uiovar, kbuf, PAGE_SIZE, sw_offset, UIO_READ);

	spinlock_release(&cm_lock);
	int ret = VOP_READ(sw_vn, &uiovar);
	spinlock_acquire(&cm_lock);

	if(ret){
		return ret;
	}

	return 0;
}

int
write_page(void* kbuf, off_t* newoffset, int index){
	(void)index;
	struct iovec iovectr;
	struct uio uiovar;
	off_t offset;

	KASSERT(sw_vn != NULL);

	offset = index * PAGE_SIZE;
	uio_kinit(&iovectr, &uiovar, kbuf, PAGE_SIZE, offset, UIO_WRITE);

	spinlock_release(&cm_lock);
	int ret = VOP_WRITE(sw_vn, &uiovar);
	spinlock_acquire(&cm_lock);

	if(ret){
		return 1;
	}

	*newoffset = offset;
	return 0;
}

int
swap_in(struct addrspace* as, vaddr_t va, void* kbuf){
	(void)kbuf;
	int itr =0;
	
	//spinlock_acquire(&cm_lock);
	for(; itr < MAX_VAL; itr++){
                if(sw_space[itr].sw_addrspace == as && sw_space[itr].sw_vaddr == va){
			if(read_page(kbuf, sw_space[itr].sw_offset)){
				//spinlock_release(&cm_lock);
				panic("Error while swapping in\n");
				return 1;
			}
                        break;
                }
        }
	
	if(itr == MAX_VAL){
		panic("Data lost in swapping\n");
		return 1;
	}else{
		sw_space[itr].sw_addrspace = NULL;
		sw_space[itr].sw_vaddr = 0;
		sw_space[itr].sw_offset = 0;

		//spinlock_release(&cm_lock);
		return 0;
	}
}


int
swap_out(struct addrspace* as, vaddr_t va, void* kbuf){
	(void)kbuf;	
	int itr = 0;

	for(; itr < MAX_VAL; itr++){
		if(sw_space[itr].sw_vaddr == 0){
			sw_space[itr].sw_addrspace = as;
			sw_space[itr].sw_vaddr = va;

			if(write_page(kbuf, &sw_space[itr].sw_offset, itr)){
				panic("Error while swapping out\n");
				return 1;
			}
			
			break;
		}
	}
	
	if(itr == MAX_VAL){
		panic("Running out of memory\n");
		return 1;
	}else{
		return 0;
	}
}

void
swap_clean(struct addrspace* as){
	(void)as;
	spinlock_acquire(&cm_lock);

	for(int itr = 0; itr < MAX_VAL; itr++){
		if(sw_space[itr].sw_addrspace == as){
			sw_space[itr].sw_addrspace = NULL;
			sw_space[itr].sw_vaddr = 0;
			sw_space[itr].sw_offset = 0;
		}
	}
	spinlock_release(&cm_lock);
}
