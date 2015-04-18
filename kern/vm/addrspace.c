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
#include <clock.h>
#include <synch.h>
#include <addrspace.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

t_perm *start,*q;
extern coremap* cm_entry;
struct spinlock cm_lock;

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	
	
	as->as_parent = NULL;
	as->as_segment = NULL;
	as->as_pgtable = NULL;
	as->as_hpstart = as->as_hpend = 0;
	as->as_stop = 0;

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	//Mark the parent in the new address space
	newas->as_parent = old;

	/*
	 * traverse through lisked list, copy contents from one to another
	 */
	
	pagetable *pg,*q,*strt = old->as_pgtable;
	pagetable *pg_start = NULL;
	while(strt != NULL){
		pg = kmalloc(sizeof(pagetable));
		pg->pg_vaddr = strt->pg_vaddr;

		/*if(strt->pg_paddr == 0){
		}else {*/
			int npages = get_page_count(strt->pg_vaddr);
			for(int page = 0; page < npages; page++){
				page_alloc_copy(pg, pg->pg_vaddr + (page * PAGE_SIZE));
				memmove((void*)pg->pg_vaddr + (page * PAGE_SIZE), (void*)strt->pg_vaddr + (page * PAGE_SIZE), PAGE_SIZE);
			}
		//}

		pg->pg_next = NULL;
		strt = (pagetable*)strt->pg_next;

		if(pg_start == NULL){
			pg_start = pg;
			q = pg;
		}
		else{
			q->pg_next = (struct pagetable*)pg;
			q = pg;
		}
	}
	newas->as_pgtable = pg_start;

	segment *sg,*r,*start = old->as_segment;
	segment *sg_start = NULL;

        while(start != NULL){
                //segment *sg;
		sg = kmalloc(sizeof(segment));
        /*      sg->sg_vaddr = start->sg_vaddr;
 	        sg->sg_numpage = start->sg_numpage;
		sg->sg_perm = start->sg_perm;
	*/
		memcpy(sg,start,sizeof(segment));
                sg->sg_next = NULL;
                start = (segment*)start->sg_next;

                if(sg_start == NULL){
                        sg_start = sg;
                        r = sg;
                }
                else{
                        r->sg_next = (struct segment*)sg;
                        r = sg;
                }
        }

        newas->as_segment = sg_start;
	newas->as_hpstart = old->as_hpstart;
	newas->as_hpend = old->as_hpend;
	
	//Update the coremap with the new addrspace
	coremap* temp = cm_entry;
	pg = newas->as_pgtable;
	int totalpagecnt = get_total_page_count();
	
	while(pg != NULL){
		int page = 0;
		//for(; (temp + page)->cm_vaddr != pg->pg_vaddr; page++);

		while(page < totalpagecnt){
			if((temp + page)->cm_vaddr == pg->pg_vaddr){
				if((temp + page)->cm_addrspace == NULL){
					(temp + page)->cm_addrspace = newas;
					break;
				}
			}
			page++;
		}

		pg = (pagetable*)pg->pg_next;
	}

	*ret = newas;
	return 0;
}

void
page_alloc_copy(pagetable* table, vaddr_t va){
	(void)table;
	(void)va;

        bool ispagefree = false;
        unsigned int page;
        spinlock_acquire(&cm_lock);

	unsigned int totalpagecnt = get_total_page_count(); 
        for(page = 0; page < totalpagecnt; page++){
                if((cm_entry + page)->cm_state == FREE){
                        ispagefree = true;
                        break;
                }
        }

        coremap* alloc = cm_entry;
        if(!ispagefree){
                make_page_avail(&alloc, 1);
        }else{
                alloc = cm_entry + page;
        }

        time_t secs;
        pagetable* temp = table;

        while(temp != NULL && temp->pg_vaddr != va){
                temp = (pagetable*) temp->pg_next;
        }

        if(temp == NULL){
                return;         //Error: vaddr not found
        }
        temp->pg_paddr = get_first_page() + (page * PAGE_SIZE);

        alloc->cm_addrspace = NULL;
        alloc->cm_vaddr = va;
        gettime(&secs, &alloc->cm_timestamp);
        alloc->cm_state = DIRTY;
        alloc->cm_npages = 1;

        spinlock_release(&cm_lock);
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	pagetable *pg_prev,*pg;
        pg=as->as_pgtable;
        
        while(pg != NULL){
                pg_prev = pg;
                pg = (pagetable*) pg->pg_next;
                kfree(pg_prev);
        }
        
        segment *sg_prev, *sg;
        sg = as->as_segment;
        
        while(sg != NULL){
                sg_prev = sg;
                sg = (segment*) sg->sg_next;
                kfree(sg_prev);
        }
	
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;  // suppress warning until code gets written
	//vm_tlbshootdown_all();
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable, bool isstack)
{
	/*
	 * Write this.
	 */

	/*these lines are for alignment. check if they have to be put after pageallod or before that*/
	int numpage;
	vaddr_t va;
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;
	sz = (sz + PAGE_SIZE -1) & PAGE_FRAME;
	numpage = sz / PAGE_SIZE;
	/* end of alignment */
	
	segment *sg = kmalloc(sizeof(segment));
	if(sg == NULL){
		return ENOMEM;
	}

	sg->sg_next = NULL;
	
	sg->sg_numpage = numpage;
	sg->sg_vaddr = vaddr;
	
	sg->sg_perm.pm_read=readable;
	sg->sg_perm.pm_write = writeable;
	sg->sg_perm.pm_exec = executable;
	
	if(as->as_segment == NULL){
		as->as_segment = sg;
	}
	else{
		segment *sgmt = as->as_segment;
	 	while(sgmt->sg_next!=NULL){
			sgmt = (segment*)sgmt->sg_next;
		}
		sgmt->sg_next = (struct segment*)sg;
	}
	
	for(int page = 0; page<numpage; page++){
		va = vaddr + page*PAGE_SIZE;
		pagetable *pg = kmalloc(sizeof(pagetable));
        	if(pg == NULL){
        	        return ENOMEM;
	        }
		pg->pg_next = NULL;
		pg->pg_vaddr = va;
		pg->pg_paddr = 0;	
		
		if(as->as_pgtable == NULL){
			as->as_pgtable = pg;
		}
		else{
			pagetable *p = as->as_pgtable;
			while(p->pg_next != NULL){
				p=(pagetable*)p->pg_next;
			}
			p->pg_next = (struct pagetable*)pg;
		}
		page_alloc(as, va, false);
	}

	if(!isstack) {
		as->as_hpstart = as->as_hpend = vaddr + sz;
	}

	return 0;
/*
	(void)as;
	(void)vaddr;
	(void)sz;
	(void)readable;
	(void)writeable;
	(void)executable;
*/
//	return EUNIMP;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	
	if(as == NULL){
		return 0;
	}

	if(start != NULL){
		// we are officially screwed
	}

	as_define_region(as, USERSTACK-(12 * PAGE_SIZE), 12 * PAGE_SIZE, 0x4, 0x2, 0, true); 

	segment *sg = as->as_segment;
	while(sg != NULL){
		t_perm *p = kmalloc(sizeof(t_perm));

		if(p == NULL){
			return ENOMEM;
		}

		p->read = sg->sg_perm.pm_read;
		p->write = sg->sg_perm.pm_write;
		sg->sg_perm.pm_read = 4;
		sg->sg_perm.pm_write = 2;
		p->next = NULL;

		if(start == NULL){
			start = p;
			q = p;
		}
		else{
			q->next = (struct t_perm*)p;
			q=(t_perm*)q->next;
		}
		sg = (segment*)sg->sg_next;
	}

//	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	
	t_perm *p = start;
	segment *sg = as->as_segment;
	while(sg!=NULL){
		sg->sg_perm.pm_read = p->read;
		sg->sg_perm.pm_write = p->write;
		p = (t_perm*)p->next;
		kfree(start);
		start = p;
		sg = (segment*)sg->sg_next;
	}
//	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	
	return 0;
}

