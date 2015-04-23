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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <clock.h>
#include <synch.h>
#include <vm.h>

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
coremap* cm_entry;
bool bootstrapped = false;
unsigned int totalpagecnt;
struct spinlock cm_lock;// = SPINLOCK_INITIALIZER;
paddr_t firstaddr;

void
vm_bootstrap(void)
{
	paddr_t lastaddr, freeaddr, buf;

	//cm_lock = lock_create("cm_lock");
	spinlock_init(&cm_lock);
	ram_getsize(&firstaddr, &lastaddr);
	totalpagecnt = (unsigned int) (lastaddr - firstaddr) / PAGE_SIZE;

	cm_entry = (coremap*) PADDR_TO_KVADDR(firstaddr);
	freeaddr = firstaddr + totalpagecnt * sizeof(coremap);

	buf = firstaddr;
	for(unsigned int page = 0; page < totalpagecnt; page++){
		(cm_entry+page)->cm_addrspace = NULL;
		(cm_entry+page)->cm_vaddr = PADDR_TO_KVADDR(buf);
		(cm_entry+page)->cm_npages = 0;
		(cm_entry+page)->cm_timestamp = 4294967295; 	

		if(buf < freeaddr){
			(cm_entry+page)->cm_state = FIXED;
		}else {
			(cm_entry+page)->cm_state = FREE;
		}

		buf += PAGE_SIZE;
	}

	bootstrapped = true;
}

int
get_page_count(vaddr_t address)
{
	spinlock_acquire(&cm_lock);

	coremap* temp = cm_entry;

	unsigned int count = 0;
	while((temp+count)->cm_vaddr != address){
		count++;
		if(count >= totalpagecnt){
			return 0;
		}
	}

	int result = (temp+count)->cm_npages;
	spinlock_release(&cm_lock);

	return result;
}

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);
	
	spinlock_release(&stealmem_lock);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	
	if(!bootstrapped){
		pa = getppages(npages);
		if (pa==0) {
			return 0;
		}
		return PADDR_TO_KVADDR(pa);
	}else {
		return page_nalloc(npages);	
	}
}

unsigned int
get_total_page_count(void)
{
	return totalpagecnt;
}

paddr_t
get_first_page(void)
{
	return firstaddr;
}

void
delete_coremap(struct addrspace* as){
	spinlock_acquire(&cm_lock);

	coremap* temp = cm_entry;
	paddr_t buf = firstaddr;
	for(unsigned int page = 0; page < totalpagecnt;page++){
		if((temp + page)->cm_addrspace == as){
			(temp + page)->cm_addrspace = NULL;
			(temp + page)->cm_state = FREE;

			(temp + page)->cm_vaddr = PADDR_TO_KVADDR(buf);
		}
		
		buf += PAGE_SIZE;
	}
	spinlock_release(&cm_lock);
}

void
page_alloc(struct addrspace* as, vaddr_t va, bool forstack)
{
	bool ispagefree = false;
	unsigned int page;
	spinlock_acquire(&cm_lock);
	
	for(page = 0; page < totalpagecnt; page++){
		if((cm_entry + page)->cm_state == FREE){
			ispagefree = true;
			break;
		}
	}

	coremap* alloc = cm_entry;
	if(!ispagefree){
		page = make_page_avail(&alloc, 1);
	}else{
		alloc = cm_entry + page;
	}

	//bzero((int*)alloc->cm_vaddr, PAGE_SIZE);
	time_t secs;
	pagetable* temp = as->as_pgtable;

	if(!forstack){
		while(temp != NULL && temp->pg_vaddr != va){
			temp = (pagetable*) temp->pg_next;
		}
	
		if(temp == NULL){
			spinlock_release(&cm_lock);
			return;		//Error: vaddr not found
		}
		temp->pg_paddr = firstaddr + (page * PAGE_SIZE);
	}else{
		as->as_stop = firstaddr + (page * PAGE_SIZE);
	}

	alloc->cm_addrspace = as;
	alloc->cm_vaddr = va;
	gettime(&secs, &alloc->cm_timestamp);
	alloc->cm_state = DIRTY;
	alloc->cm_npages = 1;

	spinlock_release(&cm_lock);
}

vaddr_t
page_nalloc(int npages)
{
	int freepages = 0;
	unsigned int start = 0;
	spinlock_acquire(&cm_lock);

	for(unsigned int page = 0; page < totalpagecnt; page++){
		if((cm_entry + page)->cm_state == FREE){
			freepages = 1;
			for(int cont = 2; cont <= npages; cont++){
				if((cm_entry + page + cont)->cm_state == FREE){
					freepages++;
				}else{
					break;
				}
			}

			if(freepages == npages){
				start = page;
				break;
			}
			page += freepages - 1;
		}
	}

	coremap* allock = cm_entry;
	if(freepages != npages){
		start = make_page_avail(&allock, npages);
	}else {
		allock = cm_entry + start;
	}

	//bzero((int*) allock->cm_vaddr, npages * PAGE_SIZE);
	vaddr_t result = allock->cm_vaddr;

	/*pagetable* table;
	if(curthread->t_addrspace != NULL){
		table = curthread->t_addrspace->as_pgtable;
	}*/

	for(int page = 0; page < npages; page++){
		(allock+page)->cm_state = DIRTY;

		time_t secs;
		gettime(&secs, &(allock+page)->cm_timestamp);

	/*	if(curthread->t_addrspace != NULL){
		pagetable* prev = NULL;
		while(table != NULL){
			prev = table;
			table = (pagetable*) table->pg_next;
		}

		table = kmalloc(sizeof(pagetable));
		
		if(table == NULL){
			spinlock_release(&cm_lock);
			return 0;
		}

		table->pg_vaddr = allock->cm_vaddr + (page * PAGE_SIZE);
		table->pg_paddr = firstaddr + ((start + page) * PAGE_SIZE);
		table->pg_next = NULL;

		prev->pg_next = (struct pagetable*) table;
		}*/
	}
	
	allock->cm_addrspace = curthread->t_addrspace;
	allock->cm_npages = npages;
	spinlock_release(&cm_lock);
	return result;
}

unsigned int
make_page_avail(coremap** temp, int npages)
{
	uint32_t oldertimestamp = 4294967295;
        unsigned int victimpage = 0;

        for(unsigned int page = 0; page < totalpagecnt; page++){
        	if((cm_entry + page)->cm_state != FIXED && (cm_entry + page)->cm_timestamp < oldertimestamp){
                        oldertimestamp = (cm_entry + page)->cm_timestamp;
                        victimpage = page;
                }
        }

	//Inform the caller about the index of coremap that is to be changed
        *temp = cm_entry + victimpage;

	//vm_tlbshootdown_all();

	if((cm_entry + victimpage)->cm_addrspace != NULL){
		pagetable* pg = (cm_entry + victimpage)->cm_addrspace->as_pgtable;
		while(pg != NULL){
			if(pg->pg_vaddr == (cm_entry + victimpage)->cm_vaddr){
				pg->pg_paddr = 0;
				break;
			}
	
			pg = (pagetable*) pg->pg_next;
		}
	}

	if(npages > 1){
		//TODO: logic for swapping	
	}

	return victimpage;
}

void 
free_kpages(vaddr_t addr)
{
	//lock_acquire(cm_lock);
	spinlock_acquire(&cm_lock);
	
	for(unsigned int page = 0; page < totalpagecnt; page++){
		if((cm_entry + page)->cm_vaddr == addr){
			if((cm_entry + page)->cm_addrspace == curthread->t_addrspace ||
				(cm_entry + page)->cm_addrspace == NULL){
				coremap* temp = cm_entry;

				for(int npages = 0; npages < (cm_entry + page)->cm_npages; npages++){
					if((temp + page + npages)->cm_addrspace != NULL){
						pagetable* table = (temp + page + npages)->cm_addrspace->as_pgtable;
						pagetable* prev = NULL;
						
						while(table != NULL){
							if(table->pg_vaddr == addr){
								if(prev != NULL){
									prev->pg_next = table->pg_next;
								}else{
									prev = (pagetable*) table->pg_next;
								}
								kfree(table);
								break;
								
							}else{							
								prev = table;
								table = (pagetable*)table->pg_next;
							}
						}

						(temp + page + npages)->cm_addrspace = NULL;	
						//vm_tlbshootdown_all();
					}
					(temp + page + npages)->cm_state = FREE;
				}
				break;
			}
		}
	}
	//lock_release(cm_lock);
	spinlock_release(&cm_lock);
}

void
vm_tlbshootdown_all(void)
{
	//panic("dumbvm tried to do tlb shootdown?!\n");

//	spinlock_acquire(&cm_lock);
	int spl = splhigh();

	for(int cnt = 0; cnt < NUM_TLB; cnt++){
		tlb_write(TLBHI_INVALID(cnt), TLBLO_INVALID(), cnt);
	}

	splx(spl);
//	spinlock_release(&cm_lock);
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	uint32_t ehi, elo;	
	int spl;
	paddr_t paddr;
	bool invalidvaddr = true;

	faultaddress &= PAGE_FRAME;

	if(curthread->t_addrspace == NULL){
		return EFAULT;
	}

	switch (faulttype) {
		case VM_FAULT_READONLY:
			panic("Read only");
		break;
		case VM_FAULT_READ:
		case VM_FAULT_WRITE:
		{
			pagetable* table = curthread->t_addrspace->as_pgtable;
			
			while(table != NULL){
				if(table->pg_vaddr == faultaddress){
					if(table->pg_paddr == 0){
						page_alloc(curthread->t_addrspace, faultaddress, false);
						/*if(curthread->t_addrspace->as_parent != NULL){
							pagetable* pg = curthread->t_addrspace->as_parent->as_pgtable;
							memmove((void*)PADDR_TO_KVADDR(table->pg_paddr), (void*)PADDR_TO_KVADDR(pg->pg_paddr), PAGE_SIZE);
						}*/
					}
					paddr = table->pg_paddr;
					invalidvaddr = false;
					break;
				}
				table = (pagetable*) table->pg_next;
			}
			//}

			if(invalidvaddr){
				return EFAULT;
			}
		}
		break;
		default:
			return EFAULT;
	}

	spl = splhigh();

        ehi = faultaddress;
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
        tlb_random(ehi, elo);

        splx(spl);
	return 0;

	/*vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		* We always create pages read-write, so we can't get this 
		panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	as = curthread->t_addrspace;
	if (as == NULL) {
		*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 *
		return EFAULT;
	}

	* Assert that the address space has been set up properly. *
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	* make sure it's page-aligned *
	KASSERT((paddr & PAGE_FRAME) == paddr);

	* Disable interrupts on this CPU while frobbing the TLB. *
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;*/
}
