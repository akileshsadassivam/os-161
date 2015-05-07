
#ifndef _SWAP_H
#define _SWAP_H


#define MAX_VAL 1000


typedef struct{
	struct addrspace* sw_addrspace;
	vaddr_t sw_vaddr;
	off_t sw_offset;
}swapspace;


void
swapspace_init(void);

int
read_page(void* kbuf, off_t sw_offset);

int
write_page(void* kbuf, off_t* newoffset, int index);


int
swap_in(struct addrspace* as, vaddr_t va, void* kbuf);


int
swap_out(struct addrspace* as, vaddr_t va, void* kbuf);


#endif
