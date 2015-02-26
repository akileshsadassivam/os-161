
#include <limits.h>
#include <types.h>
#include <lib.h>
#include <uio.h>
#include <kern/errno.h>
#include <copyinout.h>
#include <vfs.h>
#include <synch.h>
#include <current.h>
#include <vnode.h>
#include <file_syscall.h>


int
sys_open(userptr_t flname, int rwflag, int *retval)
{
	if(flname==NULL)
	{
		return  EFAULT;
	}

	for(int itr=0;itr<OPEN_MAX;itr++)
	{
		if(curthread->filetable[itr]==NULL)
		{
			curthread->filetable[itr]=kmalloc(sizeof(struct filehandle));			/*initialize file table entry*/
			if(curthread->filetable[itr]==NULL)
			{
				return ENOMEM;
			}

			curthread->filetable[itr]->flags=rwflag;					/*set flags */
			curthread->filetable[itr]->offset=0;
			curthread->filetable[itr]->refcnt=1;
			curthread->filetable[itr]->lock=lock_create("open_lock");				/*dummy name for a lock*/
				
			if(curthread->filetable[itr]->lock==NULL)						/*return if lock is not available*/
			{
				kfree(curthread->filetable[itr]);
				return ENOMEM;
			}

			char* tempflname;
			int result;

			if(itr >= 0 && itr <3){
				tempflname = (char*) flname;
			} else {
				tempflname = kmalloc((strlen((char *)flname)+1)*sizeof(char));

				if(tempflname==NULL)
				{
					return ENOMEM;
				}

				size_t fsize;

				result = copyinstr((const_userptr_t) flname, tempflname,(strlen((char *)flname)+1) * sizeof(char),&fsize);
				if(result)
				{
					lock_destroy(curthread->filetable[itr]->lock);
					kfree(curthread->filetable[itr]);
					return result;
				}
			}
			
			result = vfs_open(tempflname, rwflag,0664,&(curthread->filetable[itr]->vn));	/*have no clue what 0664 is, check working later*/
			if(result)
			{
				return result;
			}
			*retval=itr;
			return result;									/*if all steps succeed, return file descriptor*/
		}
	}
	return ENOMEM;	
}


int
sys_close(int fd)
{
	if(fd<3||fd>OPEN_MAX)
	{
		return EBADF;
	}

	if(curthread->filetable[fd]==NULL)
	{
		return EBADF;
	}

	if(curthread->filetable[fd]->refcnt==0)
	{
		return EBADF;
	}

	lock_acquire(curthread->filetable[fd]->lock);
	curthread->filetable[fd]->refcnt-=1;
	
	if(curthread->filetable[fd]->refcnt==0)
	{
		vfs_close(curthread->filetable[fd]->vn);
		lock_release(curthread->filetable[fd]->lock);
		lock_destroy(curthread->filetable[fd]->lock);
		kfree(curthread->filetable[fd]);
	}
	else
	{
		lock_release(curthread->filetable[fd]->lock);
	}

	return 0;
}



int
sys_write(int fd, userptr_t buf, size_t count, int* retval)
{
	if(buf == NULL || strlen((char *)buf) == 0)
	{
		return EFAULT;
	}
	if(fd < 0 || fd > OPEN_MAX)
	{
		return EBADF;
	}
	if(count <= 0)
        {
                return 1;
        }
	if(curthread->filetable[fd] == NULL)
	{
		return EBADF;
	}
	
	struct iovec iovctr;
	struct uio uiovar;

	/*use copyinstr here*/
	//size_t cpsize;
	//iovctr.iov_ubase = kmalloc(sizeof(PATH_MAX+NAME_MAX));
	//int copyret = copyinstr((const_userptr_t)buf, iovctr.iov_ubase,PATH_MAX+NAME_MAX,&cpsize);
	//if(copyret)
	//{
	//	return ENOMEM;
	//}
	

	iovctr.iov_ubase = buf;
	iovctr.iov_len = count;

	uiovar.uio_segflg = UIO_USERSPACE;
	uiovar.uio_iov = &iovctr;
	uiovar.uio_iovcnt = 1;
	lock_acquire(curthread->filetable[fd]->lock);
	uiovar.uio_offset = curthread->filetable[fd]->offset;/*is it 0 or the offset from current thread file table*/
	uiovar.uio_resid = count;
	uiovar.uio_space = curthread->t_addrspace;
	uiovar.uio_rw = UIO_WRITE;
	lock_release(curthread->filetable[fd]->lock);
	
	int ret;
	ret = VOP_WRITE(curthread->filetable[fd]->vn,&uiovar);
	
	if(ret)
	{
		return ret;
	}

	lock_acquire(curthread->filetable[fd]->lock);
	int writediff = uiovar.uio_offset-curthread->filetable[fd]->offset;
	curthread->filetable[fd]->offset=uiovar.uio_offset;
	lock_release(curthread->filetable[fd]->lock);
	*retval = writediff;
	return 0;
	
}


int
sys_read(int fd, userptr_t buf, size_t count, int* retval)
{
        if(buf == NULL)
        {
                return EFAULT;
        }
        if(fd < 0 || fd > OPEN_MAX)
        {
                return EBADF;
        }
        if(count <= 0)
        {
                return 1;
        }
        if(curthread->filetable[fd] == NULL)
        {
                return EBADF;
        }
        struct iovec iovctr;
        struct uio uiovar;

        iovctr.iov_ubase = buf;
        iovctr.iov_len = count;

        uiovar.uio_segflg = UIO_USERSPACE;
        uiovar.uio_iov = &iovctr;
        uiovar.uio_iovcnt = 1;
        lock_acquire(curthread->filetable[fd]->lock);
        uiovar.uio_offset = curthread->filetable[fd]->offset;/*is it 0 or the offset from current thread file table*/
        uiovar.uio_resid = count;
        uiovar.uio_space = curthread->t_addrspace;
        uiovar.uio_rw = UIO_READ;
        lock_release(curthread->filetable[fd]->lock);

        int ret;
        ret = VOP_READ(curthread->filetable[fd]->vn,&uiovar);

        if(ret)
        {
                 return ret;
        }

	lock_acquire(curthread->filetable[fd]->lock);
        int readdiff = uiovar.uio_offset-curthread->filetable[fd]->offset;
	curthread->filetable[fd]->offset=uiovar.uio_offset;
        lock_release(curthread->filetable[fd]->lock);
        *retval = readdiff;
	return 0;
}




