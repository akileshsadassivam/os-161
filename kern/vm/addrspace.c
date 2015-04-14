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
#include <addrspace.h>
#include <vm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

int t_read;
int t_write;
int t_exec;

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
	
	
	as->as_segment = NULL;
	as->as_pgtable = NULL;
	as->as_hpstart = as->as_hpend = 0;
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

	/*
	 * Write this.
	 */

	(void)old;
	
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;  // suppress warning until code gets written
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
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	/*these lines are for alignment. check if they have to be put after pageallod or before that*/
	size_t numpage;
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;
	sz = (sz + PAGE_FRAME -1) & PAGE_FRAME;
	numpage = sz / PAGE_SIZE;
	/* end of alignment */
	
	segment *sg = kmalloc(sizeof(segment));
	if(sg == NULL){
		return ENOMEM;
	}
	
	pagetable *pg = kmalloc(sizeof(pagetable));
	if(pg == NULL){
		return ENOMEM;
	}

	pg->pg_next = NULL;
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

	pg->pg_vaddr = vaddr;
	
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

	page_alloc(as,vaddr);
	
	as->as_hpstart = as->as_hpend = vaddr + sz;
	
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

	t_read = as->as_segment->sg_perm.pm_read;
	t_write = as->as_segment->sg_perm.pm_write;
//	t_exec = as->as_segment->sg_perm.pm_exec;

	
	as->as_segment->sg_perm.pm_read = 4;
	as->as_segment->sg_perm.pm_write = 2;
//	as->as_segment->sg_perm.pm_exec;

//	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	
	as->as_segment->sg_perm.pm_read = t_read;
	as->as_segment->sg_perm.pm_write = t_write;
	
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

