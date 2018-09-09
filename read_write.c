/*************************************************************************//**
 *****************************************************************************
 * @file   read_write.c
 * @brief  
 * @author Forrest Y. Yu
 * @date   2008
 *****************************************************************************
 *****************************************************************************/

#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"


/*****************************************************************************
 *                                do_rdwt
 *****************************************************************************/
/**
 * Read/Write file and return byte count read/written.
 *
 * Sector map is not needed to update, since the sectors for the file have been
 * allocated and the bits are set when the file was created.
 * 
 * @return How many bytes have been read/written.
 *****************************************************************************/
PUBLIC int do_rdwt()
{
	int fd = fs_msg.FD;	/**< file descriptor. */
	void * buf = fs_msg.BUF;/**< r/w buffer */
	int len = fs_msg.CNT;	/**< r/w bytes */

	int src = fs_msg.source;		/* caller proc nr. */

	assert((pcaller->filp[fd] >= &f_desc_table[0]) &&
	       (pcaller->filp[fd] < &f_desc_table[NR_FILE_DESC]));

	if (!(pcaller->filp[fd]->fd_mode & O_RDWR))
		return -1;

	int pos = pcaller->filp[fd]->fd_pos;

	struct inode * pin = pcaller->filp[fd]->fd_inode;

	assert(pin >= &inode_table[0] && pin < &inode_table[NR_INODE]);

	int imode = pin->i_mode & I_TYPE_MASK;

	if (imode == I_CHAR_SPECIAL) {
		int t = fs_msg.type == READ ? DEV_READ : DEV_WRITE;
		fs_msg.type = t;

		int dev = pin->i_start_sect;
		assert(MAJOR(dev) == 4);

		fs_msg.DEVICE	= MINOR(dev);
		fs_msg.BUF	= buf;
		fs_msg.CNT	= len;
		fs_msg.PROC_NR	= src;
		assert(dd_map[MAJOR(dev)].driver_nr != INVALID_DRIVER);
		send_recv(BOTH, dd_map[MAJOR(dev)].driver_nr, &fs_msg);
		assert(fs_msg.CNT == len);

		return fs_msg.CNT;
	}
	else {
		assert(pin->i_mode == I_REGULAR || pin->i_mode == I_DIRECTORY);
		assert((fs_msg.type == READ) || (fs_msg.type == WRITE));

		int pos_end;
		if (fs_msg.type == READ)
			pos_end = min(pos + len, pin->i_size);
		else		/* WRITE */
			pos_end = min(pos + len, pin->i_nr_sects * SECTOR_SIZE);

		int off = pos % SECTOR_SIZE;
		int rw_sect_min=pin->i_start_sect+(pos>>SECTOR_SIZE_SHIFT);
		int rw_sect_max=pin->i_start_sect+(pos_end>>SECTOR_SIZE_SHIFT);

		int chunk = min(rw_sect_max - rw_sect_min + 1,
				FSBUF_SIZE >> SECTOR_SIZE_SHIFT);

		int bytes_rw = 0;
		int bytes_left = len;
		int i;
		for (i = rw_sect_min; i <= rw_sect_max; i += chunk) {
			/* read/write this amount of bytes every time */
			int bytes = min(bytes_left, chunk * SECTOR_SIZE - off);
			rw_sector(DEV_READ,
				  pin->i_dev,
				  i * SECTOR_SIZE,
				  chunk * SECTOR_SIZE,
				  TASK_FS,
				  fsbuf);

			if (fs_msg.type == READ) {
				phys_copy((void*)va2la(src, buf + bytes_rw),
					  (void*)va2la(TASK_FS, fsbuf + off),
					  bytes);
			}
			else {	/* WRITE */
				phys_copy((void*)va2la(TASK_FS, fsbuf + off),
					  (void*)va2la(src, buf + bytes_rw),
					  bytes);

				rw_sector(DEV_WRITE,
					  pin->i_dev,
					  i * SECTOR_SIZE,
					  chunk * SECTOR_SIZE,
					  TASK_FS,
					  fsbuf);
			}
			off = 0;
			bytes_rw += bytes;
			pcaller->filp[fd]->fd_pos += bytes;
			bytes_left -= bytes;
		}

		if (pcaller->filp[fd]->fd_pos > pin->i_size) {
			/* update inode::size */
			pin->i_size = pcaller->filp[fd]->fd_pos;

			/* write the updated i-node back to disk */
			sync_inode(pin);
		}

		return bytes_rw;
	}
}
PUBLIC int is_cross(int fd, int n,int count)//n������������ʼֵ�� pin->i_nr_sects
{
	struct inode * pin = filp[fd]->fd_inode;
	int len = filp[fd]->fd_inode.i_size;//���д�С
	if (len + count > n * SECTOR_SIZE)
		return 1;
	else
		return 0;
}
/*
count:д��Ĵ�С
multiple:��չ�ı���
*/
PUBLIC int copy_and_paste(char*filename1, char*filename2,void*buf, int count)
{
	int n;
	int my_fd;
	int multiple;
	multiple = 2;
	char*filename3;
	/*open*/
	int fd1;
	fd1= open(filename1, O_RDWR);
	struct inode* pin = file[fd1]->fd1_inode;
	int rd_bytes = filp[fd1]->fd1_inode.i_size;
	char bufr[rd_bytes];
	
	assert(fd1 != -1);
	printf("File opened. fd: %d\n", fd1);
	/*create*/
	n= pin->i_nr_sects
	n = n*mutiple;
	
	while (is_cross(fd,n, count))
	{
		mutiple++;
		n = n*mutiple;
	}//���⣺��ͬ·������ԭ����multiple�����ļ�������һ��fd1,




	
	/*read*/
	int fd2;
	fd2 = open(filename1, O_CREAT | O_RDWR);//filename1,�����������
	assert(fd2 != -1);
	n = read(fd1, bufr, rd_bytes);
	assert(n == rd_bytes);
	bufr[n] = 0;
	printf("%d bytes read: %s\n", n, bufr);
	close(fd1);

	/*write*/
	n = write(fd2, bufr, strlen(bufr));
	assert(n == strlen(bufr));
	n = write(fd2, buf, strlen(buf));
	assert(n == strlen(buf));
	filename3 = filename1;
	filename1 = filename2;
	filename2 = filename3;//���⣺���ֿ�����ô�������Ҿ����е�����
	unlink(pathname);//���⣺pathname�ǵ�һ���ļ���·������εõ�
	//�����������ļ�����Ϊfilename


	
}
