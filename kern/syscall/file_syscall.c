
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
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <kern/seek.h>

int
sys_open(userptr_t flname, int rwflag, int *retval)
{
	if(flname==NULL || strlen((char *) flname) == 0 )
	{
		return  EFAULT;
	}	

	if((rwflag & O_ACCMODE)>3){
		return EINVAL;
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

			curthread->filetable[itr]->flags=rwflag;						/*set flags */
			curthread->filetable[itr]->refcnt=1;
			curthread->filetable[itr]->lock=lock_create("open_lock");				/*dummy name for a lock*/
				
			if(curthread->filetable[itr]->lock==NULL)						/*return if lock is not available*/
			{
				kfree(curthread->filetable[itr]);
				return ENOMEM;
			}



			char* tempflname;
			if(itr >= 0 && itr <3){
				tempflname = (char*) flname;
			} else {
				tempflname = kmalloc((strlen((char *)flname)+1)*sizeof(char));

				if(tempflname==NULL)
				{
					return ENOMEM;
				}

				size_t fsize;

				int result = copyinstr((const_userptr_t) flname, tempflname,(strlen((char *)flname)+1) * sizeof(char),&fsize);
				if(result)
				{
					lock_destroy(curthread->filetable[itr]->lock);
					kfree(curthread->filetable[itr]);
					return result;
				}
			}
	                        curthread->filetable[itr]->offset=0;

			
			int result = vfs_open(tempflname, rwflag,0664,&(curthread->filetable[itr]->vn));	/*have no clue what 0664 is, check working later*/
			if(result)
			{
				return result;
			}

			if(rwflag & O_APPEND){
				struct stat st;
				VOP_STAT(curthread->filetable[itr]->vn,&st);
				curthread->filetable[itr]->offset = st.st_size;
			}

			*retval=itr;
			return result;									/*if all steps succeed, return file descriptor*/
		}
	}
	return EMFILE;	
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
		curthread->filetable[fd] = NULL;
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

        int ret = VOP_READ(curthread->filetable[fd]->vn,&uiovar);

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



int
sys_dup2(int oldfd, int newfd, int* retval){
	
	if((oldfd<3 || oldfd>OPEN_MAX) || (newfd < 0 || newfd>OPEN_MAX))
	{
		return EBADF;
	}
	
	if(curthread->filetable[oldfd] == NULL)
	{	
		return EBADF;
	}

	if(newfd == oldfd)
	{
		*retval = newfd;
		return 0;
	}
	
	if(curthread->filetable[newfd] != NULL){
		int close = sys_close(newfd);
		if(close){
			return close;
		}
	}
	lock_acquire(curthread->filetable[oldfd]->lock);
	curthread->filetable[newfd] = curthread->filetable[oldfd];
	curthread->filetable[newfd]->refcnt++;
	lock_release(curthread->filetable[oldfd]->lock);
	*retval = newfd;
	return 0;
}



int
sys_lseek(int fd, off_t *pos, int whence){
	if(fd < 3 || fd > OPEN_MAX){
		*pos = -1;
		return EBADF;
	}

	if(curthread->filetable[fd] == NULL){
		return EBADF;
	}
	
	if(!(whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END )){
		*pos = -1;
		return EINVAL;
	}

	if(*pos<0){
		*pos = -1;
		return EINVAL;
	}
	
	struct stat st;
	lock_acquire(curthread->filetable[fd]->lock);
	VOP_STAT(curthread->filetable[fd]->vn,&st);
	off_t posnew;
	switch(whence)
	{
		case SEEK_SET:
			posnew = *pos;
			break;
		
		case SEEK_CUR:
			posnew = curthread->filetable[fd]->offset + *pos;
			break;
		
		case SEEK_END:
			posnew = st.st_size + *pos;
			break;
		
		default:
			lock_release(curthread->filetable[fd]->lock);
			*pos = -1;
			return EINVAL;
	}

	int ret = VOP_TRYSEEK(curthread->filetable[fd]->vn, posnew);
	if(ret){
		*pos = -1;
		lock_release(curthread->filetable[fd]->lock);
		return ESPIPE;
	}
	
	curthread->filetable[fd]->offset = posnew;	
	lock_release(curthread->filetable[fd]->lock);
	*pos = curthread->filetable[fd]->offset;
	return 0;
	
}



int
sys_chdir(userptr_t path) {
	
	if(path == NULL || strlen((char *) path) == 0){
                return  EFAULT;
        }
	
	char * temppath;
        temppath= kmalloc((strlen((char *)path)+1)*sizeof(char));

        if(temppath==NULL){
                return ENOMEM;
        }

        size_t fsize;
	int result = copyinstr((const_userptr_t) path, temppath,(strlen((char *)path)+1) * sizeof(char),&fsize);

        if(result){
                kfree(temppath);
                return result;
        }
	
	int ret = vfs_chdir(temppath);
	kfree(temppath);
	if(ret){
		return ret;
	}
	return 0;
}


int
sys__getcwd(userptr_t buf, size_t buflen){

	if(buflen <=0)
	{
		return EFAULT;
	}
	
	

	char * tempbuf;
        tempbuf = kmalloc((strlen((char *)buf)+1)*sizeof(char));

        if(tempbuf==NULL)
        {
                return ENOMEM;
        }

        size_t fsize;
        int ret = copyinstr((const_userptr_t) buf, tempbuf,(strlen((char *)buf)+1) * sizeof(char),&fsize);
        if(ret)
        {
                kfree(tempbuf);
                return ret;
        }

	struct uio bufuio;
	struct iovec bufiov;
	bufuio.uio_iov = &bufiov;
	bufuio.uio_iov->iov_kbase = tempbuf;
	bufuio.uio_iov->iov_len = buflen;
	bufuio.uio_iovcnt = 1;
	bufuio.uio_offset = 0;
	bufuio.uio_resid = buflen;
	bufuio.uio_space = NULL;
	bufuio.uio_rw = UIO_READ;
	bufuio.uio_segflg = UIO_SYSSPACE;

	ret = vfs_getcwd(&bufuio);
	if(ret){
		kfree(tempbuf);
		return ret;
	}
	ret = copyout ((userptr_t) tempbuf, buf, buflen);
	kfree(tempbuf);
	if(ret){
		return ret;
	}
	return 0;
}




