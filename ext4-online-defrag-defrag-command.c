/*
 * e4defrag.c - ext4 filesystem defragmenter
 */

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#define _XOPEN_SOURCE	500
#define _GNU_SOURCE
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/statfs.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <mntent.h>
#include <linux/fs.h>
#include <ctype.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#define EXT4_IOC_DEFRAG		_IOW('f', 10, struct ext4_ext_defrag_data)
#define EXT4_IOC_GROUP_INFO	_IOW('f', 11, struct ext4_group_data_info)
#define EXT4_IOC_FREE_BLOCKS_INFO _IOW('f', 12, struct ext4_extents_info)
#define EXT4_IOC_EXTENTS_INFO	_IOW('f', 13, struct ext4_extents_info)
#define EXT4_IOC_RESERVE_BLOCK	_IOW('f', 14, struct ext4_extents_info)
#define EXT4_IOC_MOVE_VICTIM	_IOW('f', 15, struct ext4_extents_info)
#define EXT4_IOC_BLOCK_RELEASE	_IO('f', 16)

#define DEFRAG_MAX_ENT	32

/* Extent status which are used in extent_t */
#define EXT4_EXT_USE		0
#define EXT4_EXT_FREE		1
#define EXT4_EXT_RESERVE	2

/* Insert list2 after list1 */
#define insert(list1, list2)			\
	do {					\
		list2->next = list1->next;	\
		list1->next->prev = list2;	\
		list2->prev = list1;		\
		list1->next = list2;		\
	} while (0)

#define DEFRAG_FORCE_VICTIM	2

/* Magic number for ext4 */
#define EXT4_SUPER_MAGIC	0xEF53

/* Force defrag mode: Max file size in bytes (128MB) */
#define	MAX_FILE_SIZE		(unsigned long)1 << 27

/* Data type for filesystem-wide blocks number */
typedef unsigned long long ext4_fsblk_t;

/* data type for file logical block number */
typedef unsigned int ext4_lblk_t;

/* data type for block offset of block group */
typedef int ext4_grpblk_t;

/* Ioctl command */
#define EXT4_IOC_FIBMAP		_IOW('f', 9, ext4_fsblk_t)

#ifndef __NR_fadvise64
#define __NR_fadvise64		250
#endif

#ifndef __NR_sync_file_range
#define __NR_sync_file_range	314
#endif

#ifndef POSIX_FADV_DONTNEED
#if defined(__s390x__)
#define POSIX_FADV_DONTNEED	6 /* Don't need these pages.  */
#else
#define POSIX_FADV_DONTNEED	4 /* Don't need these pages.  */
#endif
#endif

#ifndef SYNC_FILE_RANGE_WAIT_BEFORE
#define SYNC_FILE_RANGE_WAIT_BEFORE	1
#endif
#ifndef SYNC_FILE_RANGE_WRITE
#define SYNC_FILE_RANGE_WRITE		2
#endif
#ifndef SYNC_FILE_RANGE_WAIT_AFTER
#define SYNC_FILE_RANGE_WAIT_AFTER	4
#endif

#define DEVNAME			0
#define DIRNAME			1
#define FILENAME		2

#define RETURN_OK		0
#define RETURN_NG		-1
#define FTW_CONT		0
#define FTW_OPEN_FD		2000
#define FILE_CHK_OK		0
#define FILE_CHK_NG		-1
#define FS_EXT4			"ext4dev"
#define ROOT_UID		0
#define CHECK_FRAG_COUNT	1

/* Defrag block size, in bytes */
#define DEFRAG_SIZE		67108864

#define min(x, y) (((x) > (y)) ? (y) : (x))

#define PRINT_ERR_MSG(msg)	fprintf(stderr, "%s\n", (msg));
#define PRINT_FILE_NAME(file)	fprintf(stderr, "\t\t    \"%s\"\n", (file));

#define MSG_USAGE		\
"Usage : e4defrag [-v] file...| directory...| device...\n\
      : e4defrag -f file [blocknr] \n\
      : e4defrag -r directory... | device... \n"

#define MSG_R_OPTION		" with regional block allocation mode.\n"
#define NGMSG_MTAB		"\te4defrag  : Can not access /etc/mtab."
#define NGMSG_UNMOUNT		"\te4defrag  : FS is not mounted."
#define NGMSG_EXT4		"\te4defrag  : FS is not ext4 File System."
#define NGMSG_FS_INFO		"\te4defrag  : get FSInfo fail."
#define NGMSG_FILE_INFO		"\te4defrag  : get FileInfo fail."
#define NGMSG_FILE_OPEN		"\te4defrag  : open fail."
#define NGMSG_FILE_SYNC		"\te4defrag  : sync(fsync) fail."
#define NGMSG_FILE_DEFRAG	"\te4defrag  : defrag fail."
#define NGMSG_FILE_BLOCKSIZE	"\te4defrag  : can't get blocksize."
#define NGMSG_FILE_FIBMAP	"\te4defrag  : can't get block number."
#define NGMSG_FILE_UNREG	"\te4defrag  : File is not regular file."

#define NGMSG_FILE_LARGE	\
	"\te4defrag  : Defrag size is larger than FileSystem's free space."

#define NGMSG_FILE_PRIORITY	\
"\te4defrag  : File is not current user's file or current user is not root."

#define NGMSG_FILE_LOCK		"\te4defrag  : File is locked."
#define NGMSG_FILE_BLANK	"\te4defrag  : File size is 0."
#define NGMSG_GET_LCKINFO	"\te4defrag  : get LockInfo fail."
#define NGMSG_TYPE		\
	"e4defrag  : Can not process %s in regional mode.\n"
#define NGMSG_LOST_FOUND	"\te4defrag  : Can not process \"lost+found\"."
#define NGMSG_REALPATH		"\te4defrag  : Can not get full path."
#define NGMSG_FILE_MAP		"\te4defrag  : get file map fail."
#define NGMSG_FILE_DROP_BUFFER	"\te4defrag  : free page fail."
#define NGMSG_FADVISE_SYSCALL	"\tfadvise fail."

struct ext4_extent_data {
	ext4_lblk_t  block;		/* start logical block number */
	ext4_fsblk_t start;		/* start physical block number */
	int len;			/* blocks count */
};

/* Used for defrag */
struct ext4_ext_defrag_data {
	ext4_lblk_t start_offset;	/* start offset to defrag in blocks */
	ext4_lblk_t defrag_size;	/* size of defrag in blocks */
	ext4_fsblk_t goal;		/* block offset for allocation */
	int flag;			/* free space mode flag */
	struct ext4_extent_data ext;
};

struct ext4_group_data_info {
	int s_blocks_per_group;		/* blocks per group */
	int s_inodes_per_group;		/* inodes per group */
};

struct ext4_extents_info {
	unsigned long long ino;		/* inode number */
	int max_entries;		/* maximum extents count */
	int entries; 	 		/* extent number/count */
	ext4_lblk_t  f_offset;		/* file offset */
	ext4_grpblk_t g_offset;		/* group offset */
	ext4_fsblk_t goal;
	struct ext4_extent_data ext[DEFRAG_MAX_ENT];
};

typedef struct extent {
	struct extent *prev;
	unsigned long tag;		/* Extent status */
	unsigned long ino;		/* File's inode number */
	struct ext4_extent_data data;	/* Extent belong to file */
	struct extent *next;
} extent_t;

typedef struct exts_group {
	struct exts_group *prev;
	extent_t *start;		/* Start ext */
	extent_t *end;			/* End ext */
	int len;			/* Length of this continuous region */
	struct exts_group *next;
} exts_group_t;

typedef struct extent_wrap {
	struct extent_wrap *prev, *next;
	struct extent *group_ext;
} extent_wrap_t;

int	force_flag;
int	detail_flag;
int	regional_flag;
int	amount_cnt;
int	succeed_cnt;
char	lost_found_dir[PATH_MAX + 1];
ext4_fsblk_t	goal;
ext4_fsblk_t	fgoal = -1;

/**
 * fadvise() -		advise operater system process page cache.
 *
 * @fd:			file descriptor.
 * @offset:		file offset.
 * @len:		area length.
 * @advise:		process flag.
 */
int fadvise(int fd, loff_t offset, size_t len, int advise)
{
	return syscall(__NR_fadvise64, fd, offset, len, advise);
}

/**
 * sync_file_range() -	sync file region.
 *
 * @fd:			file descriptor.
 * @offset:		file offset.
 * @length:		area length.
 * @advise:		process flag.
 */
int sync_file_range(int fd, loff_t offset, loff_t length, unsigned int flag)
{
	return syscall(__NR_sync_file_range, fd, offset, length, flag);
}

/**
 * page_in_core() -	get information on whether pages are in core.
 *
 * @fd:			file descriptor.
 * @defrag_data:	data used for defrag.
 * @vec:		page state array.
 * @page_num:		page number.
 */
int page_in_core(int fd, struct ext4_ext_defrag_data defrag_data,
		 unsigned char **vec, unsigned long *page_num)
{
	int blocksize;
	int pagesize = getpagesize();
	void *page = NULL;
	loff_t offset, end_offset, length;

	if (vec == NULL || *vec != NULL) {
		return RETURN_NG;
	}

	if (ioctl(fd, FIGETBSZ, &blocksize) < 0) {
		return RETURN_NG;
	}

	/*in mmap, offset should be a multiple of the page size */
	offset = defrag_data.start_offset * blocksize;
	length = defrag_data.defrag_size * blocksize;
	end_offset = offset + length;
	/* round the offset down to the nearest multiple of pagesize */
	offset = (offset / pagesize) * pagesize;
	length = end_offset - offset;

	page = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, offset);
	if (page == MAP_FAILED) {
		return RETURN_NG;
	}

	*page_num = 0;
	*page_num = (length + pagesize - 1) / pagesize;
	*vec = (unsigned char *)calloc(*page_num, 1);
	if (*vec == NULL) {
		return RETURN_NG;
	}

	/* get information on whether pages are in core */
	if (mincore(page, (size_t)length, *vec) == -1) {
		if (*vec) {
			free(*vec);
		}
		return RETURN_NG;
	}

	if (munmap(page, length) == -1) {
		if (*vec) {
			free(*vec);
		}
		return RETURN_NG;
	}

	return RETURN_OK;
}

/**
 * defrag_fadvise() -	predeclare an access pattern for file data.
 *
 * @fd:			file descriptor.
 * @defrag_data:	data used for defrag.
 * @vec:		page state array.
 * @page_num:		page number.
 */
int defrag_fadvise(int fd, struct ext4_ext_defrag_data defrag_data,
		   unsigned char *vec, unsigned long page_num)
{
	int flag = 1;
	int blocksize;
	int pagesize = getpagesize();
	int fadvise_flag = POSIX_FADV_DONTNEED;
	int sync_flag = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE|
			SYNC_FILE_RANGE_WAIT_AFTER;
	unsigned long i;
	loff_t offset;

	if (ioctl(fd, FIGETBSZ, &blocksize) < 0) {
		return RETURN_NG;
	}

	offset = (loff_t)defrag_data.start_offset * blocksize;
	offset = (offset / pagesize) * pagesize;

	/* sync file for fadvise process */
	if (sync_file_range(fd, offset, (loff_t)pagesize*page_num, sync_flag)
	    != 0) {
		return RETURN_NG;
	}

	/* try to release buffer cache this process used,
	 * then other process can use the released buffer */
	for (i = 0; i < page_num; i++) {
		if ((vec[i] & 0x1) == 0) {
			offset += pagesize;
			continue;
		}
		if (fadvise(fd, offset, pagesize, fadvise_flag) != 0) {
			if (detail_flag && flag) {
				perror(NGMSG_FADVISE_SYSCALL);
				flag = 0;
			}
		}
		offset += pagesize;
	}

	return RETURN_OK;
}
/**
 * check_free_size() -	Check if there's enough disk space.
 *
 * @fd:			the file's descriptor.
 * @buf:		the pointer of the struct stat64.
 */
int check_free_size(int fd, const struct stat64 *buf)
{
	off64_t	size = 0;
	off64_t	free_size = 0;
	struct statfs	fsbuf;

	/* target file size */
	size = buf->st_size;

	if (fstatfs(fd, &fsbuf) < 0) {
		if (detail_flag) {
			perror(NGMSG_FS_INFO);
		}
		return RETURN_NG;
	}

	/* Compute free space for root and normal user separately */
	if (getuid() == ROOT_UID) {
		free_size = (off64_t)fsbuf.f_bsize * fsbuf.f_bfree;
	} else {
		free_size = (off64_t)fsbuf.f_bsize * fsbuf.f_bavail;
	}

	if (free_size >= size) {
		return RETURN_OK;
	}

	return RETURN_NG;
}

int file_check(int fd, const struct stat64 *buf, const char *file_name);
int force_defrag(int fd, const struct stat64 *buf, int blocksize);

/**
 * ftw_fn() -           Check file attributes and ioctl call to avoid
 * 			illegal operations.
 *
 * @file:		the file's name.
 * @buf:		the pointer of the struct stat64.
 * @flag:		file type.
 * @ftwbuf:		the pointer of a struct FTW.
 */
int
ftw_fn(const char *file, const struct stat64 *buf, int flag, struct FTW *ftwbuf)
{
	int	fd;
	int	blocksize;
	int	percent = 0;
	int	defraged_size = 0;
	int	ret = RETURN_NG;
	int	pos, file_frags_start, file_frags_end;
	unsigned long	page_num;
	unsigned char	*vec = NULL;
	loff_t	start = 0;
	struct ext4_ext_defrag_data	df_data;
	struct ext4_extents_info	extents_info;

	if (lost_found_dir[0] != '\0' &&
	    !memcmp(file, lost_found_dir, strnlen(lost_found_dir, PATH_MAX))) {
		if (detail_flag) {
			PRINT_ERR_MSG(NGMSG_LOST_FOUND);
			PRINT_FILE_NAME(file);
		}
		return FTW_CONT;
	}

	if (flag == FTW_F) {
		amount_cnt++;
		if ((fd = open64(file, O_RDONLY)) < 0) {
			if (detail_flag) {
				perror(NGMSG_FILE_OPEN);
				PRINT_FILE_NAME(file);
			}
			return FTW_CONT;
		}

		if (file_check(fd, buf, file) == FILE_CHK_NG) {
			close(fd);
			return FTW_CONT;
		}

		if (fsync(fd) < 0) {
			if (detail_flag) {
				perror(NGMSG_FILE_SYNC);
				PRINT_FILE_NAME(file);
			}
			close(fd);
			return FTW_CONT;
		}
		/* Get blocksize */
		if (ioctl(fd, FIGETBSZ, &blocksize) < 0) {
			if (detail_flag) {
				perror(NGMSG_FILE_BLOCKSIZE);
				PRINT_FILE_NAME(file);
			}
			close(fd);
			return FTW_CONT;
		}
		/* Ioctl call does the actual defragment job. */
		df_data.start_offset = 0;
		df_data.goal = goal;
		df_data.ext.len = 0;

		if (force_flag)
			df_data.flag = 1;

		/* count file frags before defrag if detail_flag set */
		if (detail_flag) {
			pos = 0;
			file_frags_start = 0;
			memset(&extents_info, 0,
				sizeof(struct ext4_extents_info));
			extents_info.ino = buf->st_ino;
			extents_info.max_entries = DEFRAG_MAX_ENT;
			extents_info.entries = 0;

			do {
				extents_info.entries += pos;
				pos = extents_info.entries;
				ret = ioctl(fd, EXT4_IOC_EXTENTS_INFO,
					    &extents_info);
				if (ret < 0) {
					perror(NGMSG_FILE_INFO);
					PRINT_FILE_NAME(file);
					close(fd);
					return FTW_CONT;
				}
				file_frags_start += extents_info.entries;
			} while (extents_info.entries == DEFRAG_MAX_ENT &&
				 ret == 0);
		}

		/* print process progress */
		printf("\tprocessing -------> %s:\n", file);
		percent = (start * 100) / buf->st_size;
		printf("\033[79;16H\033[K progressing ====> %d%%", percent);
		fflush(stdout);

		while (1) {
			df_data.defrag_size =
					(min((buf->st_size - start),
					     DEFRAG_SIZE) + blocksize - 1) /
					blocksize;

			ret = page_in_core(fd, df_data, &vec, &page_num);
			if (ret == RETURN_NG) {
				if (detail_flag) {
					perror(NGMSG_FILE_MAP);
					PRINT_FILE_NAME(file);
				} else {
					printf("\n");
				}
				close(fd);
				return FTW_CONT;
			}

			/* EXT4_IOC_DEFRAG */
			defraged_size = ioctl(fd, EXT4_IOC_DEFRAG, &df_data);

			/* free page */
			ret = defrag_fadvise(fd, df_data, vec, page_num);
			if (vec) {
				free(vec);
				vec = NULL;
			}
			if (ret == RETURN_NG) {
				if (detail_flag) {
					perror(NGMSG_FILE_DROP_BUFFER);
					PRINT_FILE_NAME(file);
				} else {
					printf("\n");
				}
				close(fd);
				return FTW_CONT;
			}

			if ((defraged_size < 0) && (force_flag == 1) &&
			 (errno == ENOSPC) && buf->st_size <= MAX_FILE_SIZE) {
				defraged_size = force_defrag(fd, buf,
							     blocksize);
				if (defraged_size * blocksize >= buf->st_size) {
					/* Whole file is enforcedly defraged */
					break;
				} else {
					defraged_size = RETURN_NG;
				}
			}
			if (defraged_size < 0) {
				if (detail_flag) {
					perror(NGMSG_FILE_DEFRAG);
					PRINT_FILE_NAME(file);
				} else {
					printf("\n");
				}
				close(fd);
				return FTW_CONT;
			}
			df_data.start_offset += defraged_size;
			start = df_data.start_offset * blocksize;

			/* print process progress */
			if (start > ((long long)1 << 56)) {
			/* consider overflow("start * 100" beyond 64bits) */
				start = start >> 8;
				percent = (start * 100) / (buf->st_size >> 8);
			} else {
				percent = (start * 100) / buf->st_size;
			}

			/* disk space file used is bigger than logical size */
			if (percent > 100) {
				percent = 100;
			}
			printf("\033[79;16H\033[K progressing ====> %d%%",
				percent);
			fflush(stdout);

			/* End of file */
			if (start >= buf->st_size) {
				break;
			}
		}

		/* count file frags after defrag and print extents info */
		if (detail_flag) {
			pos = 0;
			ret = RETURN_NG;
			file_frags_end = 0;
			extents_info.entries = 0;

			do {
				extents_info.entries += pos;
				pos = extents_info.entries;
				ret = ioctl(fd, EXT4_IOC_EXTENTS_INFO,
					    &extents_info);
				if (ret < 0) {
					printf("\n");
					perror(NGMSG_FILE_INFO);
					PRINT_FILE_NAME(file);
					close(fd);
					return FTW_CONT;
				}
				file_frags_end += extents_info.entries;
			} while (extents_info.entries == DEFRAG_MAX_ENT &&
				 ret == 0);

			printf("\n\t\textents: %d ==> %d",
				file_frags_start, file_frags_end);
		}
		printf("\n");
		close(fd);
		succeed_cnt++;
	} else {
		if (detail_flag) {
			PRINT_ERR_MSG(NGMSG_FILE_UNREG);
			PRINT_FILE_NAME(file);
		}
	}

	return FTW_CONT;
}

/**
 * file_check() -       Check file's attributes.
 *
 * @fd:			the file's descriptor.
 * @buf:		a pointer of the struct stat64.
 * @file_name:		the file's name.
 */
int file_check(int fd, const struct stat64 *buf, const char *file_name)
{
	struct flock	lock;

	lock.l_type = F_WRLCK; /* Write-lock check is more reliable. */
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	/* Regular file */
	if (S_ISREG(buf->st_mode) == 0) {
		if (detail_flag) {
			PRINT_ERR_MSG(NGMSG_FILE_UNREG);
			PRINT_FILE_NAME(file_name);
		}
		return FILE_CHK_NG;
	}

	/* Free space */
	if (check_free_size(fd, buf) == RETURN_NG) {

		if (detail_flag) {
			PRINT_ERR_MSG(NGMSG_FILE_LARGE);
			PRINT_FILE_NAME(file_name);
		}
		return FILE_CHK_NG;
	}

	/* Priority */
	if (getuid() != ROOT_UID &&
		buf->st_uid != getuid()) {
		if (detail_flag) {
			PRINT_ERR_MSG(NGMSG_FILE_PRIORITY);
			PRINT_FILE_NAME(file_name);
		}
		return FILE_CHK_NG;
	}

	/* Lock status */
	if (fcntl(fd, F_GETLK, &lock) < 0) {
		if (detail_flag) {
			perror(NGMSG_GET_LCKINFO);
			PRINT_FILE_NAME(file_name);
		}
		return FILE_CHK_NG;
	} else if (lock.l_type != F_UNLCK) {
		if (detail_flag) {
			PRINT_ERR_MSG(NGMSG_FILE_LOCK);
			PRINT_FILE_NAME(file_name);
		}
		return FILE_CHK_NG;
	}

	/* Empty file */
	if (buf->st_size == 0) {
		if (detail_flag) {
			PRINT_ERR_MSG(NGMSG_FILE_BLANK);
			PRINT_FILE_NAME(file_name);
		}
		return FILE_CHK_NG;
	}

	return FILE_CHK_OK;
}

/**
 * is_ext4() -		Whether on an ext4 filesystem.
 *
 * @filename:		the file's name.
 */
int is_ext4(const char *filename)
{
	int 	maxlen, len;
	FILE	*fp = NULL;
	char	*mnt_type = NULL;
	char	*mtab = MOUNTED;	/* Refer to /etc/mtab */
	char	file_path[PATH_MAX + 1];
	struct mntent	*mnt = NULL;
	struct statfs	buffs;

	/* Get full path */
	if (realpath(filename, file_path) == NULL) {
		perror(NGMSG_REALPATH);
		PRINT_FILE_NAME(filename);
		return RETURN_NG;
	}

	if (statfs(file_path, &buffs) < 0) {
		perror(NGMSG_FS_INFO);
		PRINT_FILE_NAME(filename);
		return RETURN_NG;
	}

	if (buffs.f_type != EXT4_SUPER_MAGIC) {
		PRINT_ERR_MSG(NGMSG_EXT4);
		return RETURN_NG;
	}

	if ((fp = setmntent(mtab, "r")) == NULL) {
		perror(NGMSG_MTAB);
		return RETURN_NG;
	}

	maxlen = 0;
	while ((mnt = getmntent(fp)) != NULL) {
		len = strlen(mnt->mnt_dir);
		if (memcmp(file_path, mnt->mnt_dir, len) == 0) {
			if (maxlen < len) {
				maxlen = len;
				mnt_type = realloc(mnt_type,
						   strlen(mnt->mnt_type) + 1);
				if (!mnt_type) {
					endmntent(fp);
					return RETURN_NG;
				}
				strcpy(mnt_type, mnt->mnt_type);
				strncpy(lost_found_dir, mnt->mnt_dir, PATH_MAX);
			}
		}
	}

	if (strcmp(mnt_type, FS_EXT4) == 0) {
		endmntent(fp);
		if (mnt_type) {
			free(mnt_type);
		}
		return RETURN_OK;
	} else {
		endmntent(fp);
		if (mnt_type) {
			free(mnt_type);
		}
		PRINT_ERR_MSG(NGMSG_EXT4);
		return RETURN_NG;
	}
}

/**
 * get_mount_point() -	Get device's mount point.
 *
 * @devname:		the device's name.
 * @mount_point:	the mount point.
 * @dir_path_len:	the length of directory.
 */
int get_mount_point(const char *devname, char *mount_point, int dir_path_len)
{
	char	*mtab = MOUNTED;	/* Refer to /etc/mtab */
	FILE	*fp = NULL;
	struct mntent	*mnt = NULL;

	if ((fp = setmntent(mtab, "r")) == NULL) {
		perror(NGMSG_MTAB);
		return RETURN_NG;
	}

	while ((mnt = getmntent(fp)) != NULL) {
		if (strcmp(devname, mnt->mnt_fsname) == 0) {
			endmntent(fp);
			if (strcmp(mnt->mnt_type, FS_EXT4) == 0) {
				strncpy(mount_point, mnt->mnt_dir,
					dir_path_len);
				return RETURN_OK;
			}
			PRINT_ERR_MSG(NGMSG_EXT4);
			return RETURN_NG;
		}
	}
	endmntent(fp);
	PRINT_ERR_MSG(NGMSG_UNMOUNT);
	return RETURN_NG;
}

/**
 * main() -		ext4 online defrag.
 *
 * @argc:		the number of parameter.
 * @argv[]:		the pointer array of parameter.
 */
int main(int argc, char *argv[])
{
	int	fd;
	int	ret;
	int	opt;
	int	i, flags;
	int	arg_type;
	int	detail_tmp;
	int	success_flag;
	char	dir_name[PATH_MAX + 1];
	struct stat64	buf;

	i = 1;
	flags = 0;
	arg_type = -1;
	detail_tmp = -1;
	success_flag = 0;
	flags |= FTW_PHYS;	/* Do not follow symlink */
	flags |= FTW_MOUNT;	/* Stay within the same filesystem */
	/* Parse arguments */
	if (argc == 1 || (argc == 2 && argv[1][0] == '-')) {
		printf(MSG_USAGE);
		exit(1);
	}

	while ((opt = getopt(argc, argv, "rvf")) != EOF) {
		switch (opt) {
		case 'r':
			regional_flag = 1;
			i = 2;
			break;
		case 'v':
			detail_flag = 1;
			i = 2;
			break;
		case 'f':
			force_flag = 1;
			i = 2;

			if (argc > 4) {
				printf("Illegal argument\n\n");
				printf(MSG_USAGE);
				exit(1);
			}

			if (argc == 4) {
				int res_strlen;
				res_strlen = strlen(argv[3]);
				for (res_strlen -= 1; res_strlen >= 0;
								res_strlen--) {
					if (!isdigit(argv[3][res_strlen])) {
						printf("Illegal argument\n\n");
						printf(MSG_USAGE);
						exit(1);
					}
				}

				fgoal = strtoul(argv[3], NULL, 0);
				if (errno) {
					printf("block num shold be < 32bit\n");
					exit(1);
				}
			}
			if (!fgoal)
				fgoal = -1;
			break;
		default:
			printf(MSG_USAGE);
			exit(1);
		}
	}

	/* Main process */
	for (; i < argc; i++) {
		amount_cnt = 0;
		succeed_cnt = 0;
		memset(dir_name, 0, PATH_MAX + 1);
		memset(lost_found_dir, 0, PATH_MAX + 1);

		if (force_flag && i == 3)
			break;

		if (lstat64(argv[i], &buf) < 0) {
			perror(NGMSG_FILE_INFO);
			PRINT_FILE_NAME(argv[i]);
			continue;
		}

		/* Regular file is acceptalbe with force mode */
		if (force_flag && !S_ISREG(buf.st_mode)) {
			printf("Inappropriate file type \n\n");
			printf(MSG_USAGE);
			exit(1);
		}

		/* Block device */
		if (S_ISBLK(buf.st_mode)) {
			arg_type = DEVNAME;
			if (get_mount_point(argv[i], dir_name, PATH_MAX) ==
				RETURN_NG) {
				continue;
			}
			printf("Start defragment for device(%s)\n", argv[i]);
		} else if (S_ISDIR(buf.st_mode)) {
			/* Directory */
			arg_type = DIRNAME;
			if (access(argv[i], R_OK) < 0) {
				perror(argv[i]);
				continue;
			}
			strcpy(dir_name, argv[i]);
		} else if (S_ISREG(buf.st_mode)) {
			/* Regular file */
			arg_type = FILENAME;
		} else {
			/* Irregular file */
			PRINT_ERR_MSG(NGMSG_FILE_UNREG);
			PRINT_FILE_NAME(argv[i]);
			continue;
		}

		/* Device's ext4 check is in get_mount_point() */
		if (arg_type == FILENAME || arg_type == DIRNAME) {
			if (is_ext4(argv[i]) == RETURN_NG) {
				continue;
			}
			if (realpath(argv[i], dir_name) == NULL) {
				perror(NGMSG_REALPATH);
				PRINT_FILE_NAME(argv[i]);
				continue;
			}
		}

		switch (arg_type) {
		case DIRNAME:
			printf("Start defragment for directory(%s)\n",
				argv[i]);

			int mount_dir_len = 0;
			mount_dir_len = strnlen(lost_found_dir, PATH_MAX);

			strncat(lost_found_dir, "/lost+found",
				PATH_MAX - strnlen(lost_found_dir, PATH_MAX));

			/* not the case("e4defrag  mount_piont_dir") */
			if (dir_name[mount_dir_len] != '\0') {
				/* "e4defrag mount_piont_dir/lost+found" */
				/* or "e4defrag mount_piont_dir/lost+found/" */
				if (strncmp(lost_found_dir, dir_name,
					    strnlen(lost_found_dir,
						    PATH_MAX)) == 0 &&
				    (dir_name[strnlen(lost_found_dir,
						      PATH_MAX)] == '\0' ||
				     dir_name[strnlen(lost_found_dir,
						      PATH_MAX)] == '/')) {
					PRINT_ERR_MSG(NGMSG_LOST_FOUND);
					PRINT_FILE_NAME(argv[i]);
					continue;
				}

				/* "e4defrag mount_piont_dir/else_dir" */
				memset(lost_found_dir, 0, PATH_MAX + 1);
			}
		case DEVNAME:
			if (arg_type == DEVNAME) {
				strncpy(lost_found_dir, dir_name,
					strnlen(dir_name, PATH_MAX));
				strncat(lost_found_dir, "/lost+found/",
					PATH_MAX - strnlen(lost_found_dir,
							   PATH_MAX));
			}

			/* Regional block allocation */
			if (regional_flag) {
				printf(MSG_R_OPTION);

				if ((fd = open64(dir_name, O_RDONLY)) < 0) {
					if (detail_flag) {
						perror(NGMSG_FILE_OPEN);
						PRINT_FILE_NAME(dir_name);
					}
					continue;
				}

				goal = 0;
				if ((ret = ioctl(fd, EXT4_IOC_FIBMAP,
							 &goal)) != 0) {
					perror(NGMSG_FILE_FIBMAP);
					PRINT_FILE_NAME(dir_name);
					close(fd);
					continue;
				}
				close(fd);
			}

			/* File tree walk */
			nftw64(dir_name, ftw_fn, FTW_OPEN_FD, flags);
			printf("\tTotal:\t\t%12d\n", amount_cnt);
			printf("\tSuccess:\t%12d\n", succeed_cnt);
			printf("\tFailure:\t%12d\n",
				amount_cnt - succeed_cnt);
			break;
		case FILENAME:
			strncat(lost_found_dir, "/lost+found/",
				PATH_MAX - strnlen(lost_found_dir,
						   PATH_MAX));
			if (strncmp(lost_found_dir, dir_name,
				    strnlen(lost_found_dir,
					    PATH_MAX)) == 0) {
				PRINT_ERR_MSG(NGMSG_LOST_FOUND);
				PRINT_FILE_NAME(argv[i]);
				continue;
			}

			if (regional_flag) {
				fprintf(stderr, NGMSG_TYPE, argv[i]);
				continue;
			}
			detail_tmp = detail_flag;
			detail_flag = 1;
			printf("Start defragment for %s\n", argv[i]);
			/* Single file process */
			ftw_fn(argv[i], &buf, FTW_F, NULL);
			if (succeed_cnt != 0) {
				printf(
				"\tSUCCESS\t:file defrag success.\n"
				);
			}
			detail_flag = detail_tmp;
			break;
		}

		if (succeed_cnt != 0)
			success_flag = 1;
	}

	if (success_flag)
		return RETURN_OK;

	exit(1);
}
/**
 * insert_extent() -	Sequentially insert extent by physical block number.
 *
 * @extlist_head:	the head of an extent list.
 * @ext:		the extent element which will be inserted.
 */
int insert_extent(extent_t **extlist_head, extent_t *ext)
{
	extent_t	*ext_tmp = *extlist_head;

	if (ext == NULL) {
		return RETURN_NG;
	}
	/* First element */
	if (*extlist_head == NULL) {
		(*extlist_head) = ext;
		(*extlist_head)->prev = *extlist_head;
		(*extlist_head)->next = *extlist_head;
		return RETURN_OK;
	}

	if (ext->data.start <= ext_tmp->data.start) {
		/* Insert before head */
		if (ext_tmp->data.start < ext->data.start + ext->data.len) {
			/* Overlap */
			return RETURN_NG;
		}
		/* Adjust head */
		*extlist_head = ext;
	} else {
		/* Insert into the middle or last of the list */
		do {
			if (ext->data.start < ext_tmp->data.start) {
				break;
			}
			ext_tmp = ext_tmp->next;
		} while (ext_tmp != (*extlist_head));
		if (ext->data.start <
		    ext_tmp->prev->data.start + ext_tmp->prev->data.len) {
			/* Overlap */
			return RETURN_NG;
		}
		if (ext_tmp != *extlist_head &&
		    ext_tmp->data.start < ext->data.start + ext->data.len) {
			/* Overlap */
			return RETURN_NG;
		}
	}
	ext_tmp = ext_tmp->prev;
	/* Insert "ext" after "ext_tmp" */
	insert(ext_tmp, ext);
	return RETURN_OK;
}

/**
 * insert_exts_group() -	Insert a exts_group in decreasing order of length.
 *
 * @exts_group_list_head:	the head of a exts_group list.
 * @exts_group:			the exts_group element which will be inserted.
 */
int insert_exts_group(exts_group_t **exts_group_list_head,
		      exts_group_t *exts_group)
{
	exts_group_t	*exts_group_tmp = NULL;

	if (exts_group == NULL) {
		return RETURN_NG;
	}

	/* Initialize list */
	if (*exts_group_list_head == NULL) {
		(*exts_group_list_head) = exts_group;
		(*exts_group_list_head)->prev = *exts_group_list_head;
		(*exts_group_list_head)->next = *exts_group_list_head;
		return RETURN_OK;
	}

	if (exts_group->len >= (*exts_group_list_head)->len) {
		/* Insert before exts_group_list_head */
		exts_group_tmp = (*exts_group_list_head)->prev;
		insert(exts_group_tmp, exts_group);
		*exts_group_list_head = exts_group;
		return RETURN_OK;
	}

	/* Find insertion positon */
	for (exts_group_tmp = (*exts_group_list_head)->next;
	     exts_group_tmp != *exts_group_list_head;
	     exts_group_tmp = exts_group_tmp->next) {
		if (exts_group_tmp->len < exts_group->len) {
			break;
		}
	}
	exts_group_tmp = exts_group_tmp->prev;
	insert(exts_group_tmp, exts_group);

	return RETURN_OK;
}

/**
 * get_exts_group() -		Get element from the exts_group list.
 *
 * @exts_group_list_head:	the head of a exts_group list.
 * @exts_group:			the exts_group element which will be geted.
 */
exts_group_t *get_exts_group(exts_group_t **exts_group_list_head,
			      exts_group_t *exts_group)
{
	if (exts_group == NULL || *exts_group_list_head == NULL) {
		return NULL;
	}
	/* Keep "exts_group_list_head" point to the largest extent group*/
	if (exts_group == *exts_group_list_head) {
		*exts_group_list_head = exts_group->next;
	}
	if (*exts_group_list_head == (*exts_group_list_head)->next &&
	    exts_group == *exts_group_list_head) {
		/* Delete the last element in the list */
		*exts_group_list_head = NULL;
	}
	exts_group->prev->next = exts_group->next;
	exts_group->next->prev = exts_group->prev;
	return exts_group;
}

/**
 * free_exts_group() -		Free the exts_group.
 *
 * @*exts_group_list_head:	the exts_group list head which will be free.
 */

 void free_exts_group(exts_group_t *exts_group_list_head)
{
	exts_group_t *exts_group_tmp = NULL;

	if (exts_group_list_head == NULL) {
		return;
	}
	while (exts_group_list_head->next != exts_group_list_head) {
		exts_group_tmp = exts_group_list_head;
		exts_group_list_head->prev->next = exts_group_list_head->next;
		exts_group_list_head->next->prev = exts_group_list_head->prev;
		exts_group_list_head = exts_group_list_head->next;
		free(exts_group_tmp);
	}
	free(exts_group_list_head);
}

/**
 * free_ext() -		Free the extent list.
 *
 * @extent_list_head:	the extent list head of which will be free.
 */
void free_ext(extent_t *extent_list_head)
{
	extent_t *extent_tmp = NULL;

	if (extent_list_head == NULL) {
		return;
	}
	while (extent_list_head->next != extent_list_head) {
		extent_tmp = extent_list_head;
		extent_list_head->prev->next = extent_list_head->next;
		extent_list_head->next->prev = extent_list_head->prev;
		extent_list_head = extent_list_head->next;
		free(extent_tmp);
	}
	free(extent_list_head);
}

/**
 * move_wrap() -	Move a ext_wrap from one list to another.
 *
 * @from:		the list which will be moved from.
 * @to:			the list which will be moved to.
 * @entry:		the ext_wrap which will be moved.
 */
int move_wrap(extent_wrap_t **from, extent_wrap_t **to,
		   extent_wrap_t *entry)
{
	if (!to || !entry) {
		return RETURN_NG;
	}
	if (from && *from == entry) {
		if ((*from)->next == *from) {
			*from = NULL;
		} else {
			*from = (*from)->next;
		}
	}
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	if (!(*to)) {
		*to = entry;
		(*to)->prev = (*to)->next = *to;
	} else {
		entry->next = *to;
		entry->prev = (*to)->prev;
		(*to)->prev->next = entry;
		(*to)->prev = entry;
	}
	return RETURN_OK;
}

/**
 * mark_wrap() -	Mark extent status as "EXT4_EXT_RESERVE".
 *
 * @ext_wrap_list:	the ext_wrap list which will be marked.
 */
void mark_wrap(extent_wrap_t *ext_wrap_list)
{
	extent_wrap_t *wrap = ext_wrap_list;

	if (!ext_wrap_list) {
		return;
	}
	do {
		wrap->group_ext->tag |= EXT4_EXT_RESERVE;
		wrap = wrap->next;
	} while (wrap != ext_wrap_list);
}

/**
 * free_wrap_list() -	Free the ext_wrap list.
 *
 * @ext_wrap_head:	the ext_wrap list head which will be free.
 */
void free_wrap_list(extent_wrap_t **ext_wrap_head)
{
	extent_wrap_t *wrap, *ext_wrap_tmp;

	if (!ext_wrap_head || !(*ext_wrap_head)) {
		return;
	}
	wrap = *ext_wrap_head;
	do {
		ext_wrap_tmp = wrap;
		wrap = wrap->next;
		free(ext_wrap_tmp);
	} while (wrap != *ext_wrap_head);
	*ext_wrap_head = NULL;
}

/**
 * do_defrag() -	Execute the defrag program.
 *
 * @fd:			the file's descriptor.
 * @exts_group:		the exts_group which will be defraged.
 * @defrag_data:	the data which will be defraged.
 */
static inline int do_defrag(int fd, exts_group_t *exts_group,
			    struct ext4_ext_defrag_data defrag_data)
{
	int ret = 0;
	int defraged_size = 0;
	int fadvise_ret = 0;
	unsigned long page_num;
	unsigned char *vec = NULL;
	extent_t *extent = NULL;

	/* Defrag */
	defrag_data.ext.start = exts_group->start->data.start;
	defrag_data.ext.len = exts_group->len;
	defrag_data.ext.block = 0;
	defrag_data.defrag_size = exts_group->len;
	defrag_data.flag = DEFRAG_FORCE_VICTIM;
	defrag_data.goal = exts_group->start->data.start;

	if (page_in_core(fd, defrag_data, &vec, &page_num) == RETURN_NG) {
		return RETURN_NG;
	}

	defraged_size = ioctl(fd, EXT4_IOC_DEFRAG, &defrag_data);

	/* free pages */
	fadvise_ret = defrag_fadvise(fd, defrag_data, vec, page_num);
	if (vec) {
		free(vec);
	}
	if (fadvise_ret == RETURN_NG || defraged_size < 0) {
		return RETURN_NG;
	}

	/* Release reserved sign */
	extent = exts_group->start;
	do {
		extent->tag &= ~EXT4_EXT_RESERVE;
		extent = extent->next;
	} while (extent != exts_group->end->next);

	ret += defraged_size;

	return ret;
}

/**
 * get_used_extent() -	Get used extent in the block group.
 *
 * @fd:			the file's descriptor.
 * @ext_list_head:	the head of the extent list.
 * @istart:		the start of the inode.
 * @iend:		the end of the inode.
 * @bstart:		the start of the block.
 * @bend:		the end of the block.
 */
int get_used_extent(int fd, extent_t **ext_list_head,
		    unsigned long long istart, unsigned long long iend,
		    ext4_fsblk_t bstart, ext4_fsblk_t bend)
{
	struct ext4_extents_info	extents_info;
	unsigned long long	inode;
	int	pos = 0;
	int	ret = 0;

	memset(&extents_info, 0, sizeof(struct ext4_extents_info));
	extents_info.max_entries = DEFRAG_MAX_ENT;

	for (inode = istart; inode <= iend; inode++) {
		extents_info.ino = inode;
		extents_info.entries = 0;
		pos = 0;
		do {
			/* Get extents info */
			int i;
			extents_info.entries += pos;/* Offset */
			pos = extents_info.entries;
			memset(extents_info.ext, 0,
			       sizeof(struct ext4_extent_data) *
			       DEFRAG_MAX_ENT);
			ret = ioctl(fd, EXT4_IOC_EXTENTS_INFO, &extents_info);
			if (ret < 0) {
				if (errno == ENOENT) {
					continue;
				} else {
					/* Without ENOENT case*/
					return RETURN_NG;
				}
			}

			for (i = 0; i < extents_info.entries; i++) {
				extent_t	*extent = NULL;
				/* Is this extent in current block group? */
				if (extents_info.ext[i].start < bstart ||
				    extents_info.ext[i].start > bend) {
					continue;
				}
				extent = malloc(sizeof(extent_t));
				if (extent == NULL) {
					return RETURN_NG;
				}
				memset(extent, 0, sizeof(extent_t));
				memcpy(&(extent->data), &extents_info.ext[i],
				       sizeof(struct ext4_extent_data));
				extent->ino = inode;
				if (insert_extent(ext_list_head, extent) < 0) {
					if (extent) {
						free(extent);
					}
					return RETURN_NG;
				}
			}
		} while (extents_info.entries == DEFRAG_MAX_ENT && ret == 0);
	}

	if (ret < 0) {
		if (errno == ENOENT) {
			return RETURN_OK;
		}
	}
	return ret;
}

/**
 * get_free_extent() -	Get used extent in the block group.
 *
 * @fd:			the file's descriptor.
 * @inode:		inode number from struct stat64.
 * @blocks_per_group:	the block number of each block group.
 * @ext_list_head:	the head of the extent list.
 */
int get_free_extent(int fd, unsigned long long inode,
		    int blocks_per_group, extent_t **ext_list_head)
{
	ext4_grpblk_t	pos = 0;
	struct ext4_extents_info	extents_info;

	memset(&extents_info, 0, sizeof(struct ext4_extents_info));
	extents_info.ino = inode;
	extents_info.max_entries = DEFRAG_MAX_ENT;
	while (pos < blocks_per_group) {
		int	i;
		if (ioctl(fd, EXT4_IOC_FREE_BLOCKS_INFO, &extents_info) < 0) {
			return RETURN_NG;
		}
		/*
		 * No free extent after the logical block number "pos".
		 * In other word, offset this time equals to prev recursion.
		 */
		for (i = 0;
		     extents_info.ext[i].len != 0 && i < DEFRAG_MAX_ENT; i++) {
			/* Alloc list node store extent */
			extent_t	*extent = NULL;
			extent = malloc(sizeof(extent_t));
			if (extent == NULL) {
				return RETURN_NG;
			}
			memset(extent, 0, sizeof(extent_t));
			memcpy(&(extent->data), &(extents_info.ext[i]),
			       sizeof(struct ext4_extent_data));
			extent->tag = EXT4_EXT_FREE;/* Free extent */
			if (insert_extent(ext_list_head, extent) < 0) {
				if (extent) {
					free(extent);
				}
				return RETURN_NG;
			}
		}
		/*
		 * No free extent after the logical block number "pos".
		 * In other word, offset this time equals to prev recursion.
		 */
		if (pos == extents_info.g_offset) {
			break;
		}
		if (i < DEFRAG_MAX_ENT) {
			break;
		}
		/* Record the offset of logical block number this time */
		pos = extents_info.g_offset;
		memset(extents_info.ext, 0,
		       sizeof(struct ext4_extent_data) * DEFRAG_MAX_ENT);
	}

	return RETURN_OK;
}

/**
 * join_extents() -		Find continuous region(exts_group).
 *
 * @ext_list_head:		the head of the extent list.
 * @target_exts_group_list_head:the head of the target exts_group list.
 * @exts_group_list_head:	the head of the original exts_group list.
 * @filesize:			the file's descriptor.
 * @max:			the max size of free space.
 */
int join_extents(extent_t *ext_list_head,
		 exts_group_t **target_exts_group_list_head,
		 exts_group_t **exts_group_list_head,
		 unsigned long filesize, int *max)
{
	int len;
	extent_t *ext_start, *extent_tmp;

	ext_start = extent_tmp = ext_list_head;
	*max = 0;
	len = ext_list_head->data.len;
	extent_tmp = extent_tmp->next;
	do {
		if (len >= filesize) {
			/*
			 * Hit on the way,
			 * one extent group is enough for defrag, so return.
			 */
			exts_group_t	*exts_group_tmp = NULL;
			exts_group_tmp = malloc(sizeof(exts_group_t));
			if (!exts_group_tmp) {
				return RETURN_NG;
			}
			exts_group_tmp->prev = exts_group_tmp->next = NULL;
			exts_group_tmp->start = ext_start;
			exts_group_tmp->end = extent_tmp->prev;
			exts_group_tmp->len = len;
			if (insert_exts_group(target_exts_group_list_head,
					      exts_group_tmp) < 0) {
				if (exts_group_tmp) {
					free(exts_group_tmp);
				}
				return RETURN_NG;
			}
			return CHECK_FRAG_COUNT;
		}
		/*
		 * This extent and previous extent is not continuous,
		 * so, all previous extents is treated as an extent group.
		 */
		if ((extent_tmp->prev->data.start + extent_tmp->prev->data.len)
		    != extent_tmp->data.start) {
			exts_group_t	*exts_group_tmp = NULL;
			exts_group_tmp = malloc(sizeof(exts_group_t));
			if (exts_group_tmp == NULL) {
				return RETURN_NG;
			}
			memset(exts_group_tmp, 0, sizeof(exts_group_t));
			exts_group_tmp->len = len;
			exts_group_tmp->start = ext_start;
			exts_group_tmp->end = extent_tmp->prev;

			if (insert_exts_group(exts_group_list_head,
					      exts_group_tmp) < 0) {
				if (exts_group_tmp) {
					free(exts_group_tmp);
				}
				return RETURN_NG;
			}
			*max += len;
			ext_start = extent_tmp;
			len = extent_tmp->data.len;
			extent_tmp = extent_tmp->next;
			continue;
		}
		/*
		 * This extent and previous extent is continuous,
		 * so, they belong to the same extent group, and we check
		 * if the next extent belong to the same extent group.
		 */
		len += extent_tmp->data.len;
		extent_tmp = extent_tmp->next;
	} while (extent_tmp != ext_list_head->next);

	return RETURN_OK;
}

/**
 *find_exts_group() -			Find target exts_group.
 *
 * @ext_count:				the number of extents.
 * @filesize:				the file's size.
 * @exts_group_list_head:		the head of the original exts_group list
 * @target_exts_group_list_head:	the head of the target exts_group list.
 */
int find_exts_group(int	*ext_count, unsigned long filesize,
		    exts_group_t **exts_group_list_head,
		    exts_group_t **target_exts_group_list_head)
{
	int len;

	len = 0;/* Blocks we found for target file */

	if (!(*exts_group_list_head)) {
		return RETURN_NG;
	}

	while (*exts_group_list_head) {
		exts_group_t	*exts_group_tmp;
		if ((*exts_group_list_head)->len + len >= filesize) {
			/*
			 * Search from the smallest extent group
			 * to avoid waste of space
			 */
			exts_group_tmp = (*exts_group_list_head)->prev;
			do {
				if (exts_group_tmp->len + len >= filesize) {
					len += exts_group_tmp->len;
					exts_group_tmp =
					get_exts_group(exts_group_list_head,
						       exts_group_tmp);
					if (insert_exts_group
						(target_exts_group_list_head,
						 exts_group_tmp) < 0) {
						if (exts_group_tmp) {
							free(exts_group_tmp);
						}
						return RETURN_NG;
					}
					(*ext_count)++;
					/* The only entry go out normally*/
					return RETURN_OK;
				}
				exts_group_tmp = exts_group_tmp->prev;
			} while (exts_group_tmp !=
				 (*exts_group_list_head)->prev);
		}
		len += (*exts_group_list_head)->len;
		exts_group_tmp = get_exts_group(exts_group_list_head,
						*exts_group_list_head);
		if (insert_exts_group(target_exts_group_list_head,
				      exts_group_tmp) < 0) {
			if (exts_group_tmp) {
				free(exts_group_tmp);
			}
			return RETURN_NG;
		}
		(*ext_count)++;
	}

	return RETURN_NG;
}

/**
 * check_frag_count() -		Check file frag.
 *
 * @fd:				the file's discriptor.
 * @inode:			inode number from struct stat64.
 * @extent_count:		the number of extents.
 */
int check_frag_count(int fd, unsigned long long inode, int extent_count)
{
	int ret, pos, file_extent_count;
	struct ext4_extents_info	extents_info;

	/* Count file exts */
	memset(&extents_info, 0, sizeof(struct ext4_extents_info));
	file_extent_count = 0;/* Extents count of file */
	extents_info.ino = inode;
	extents_info.max_entries = DEFRAG_MAX_ENT;
	extents_info.entries = 0;
	pos = 0;
	ret = 0;

	do {
		extents_info.entries += pos;
		pos = extents_info.entries;
		ret = ioctl(fd, EXT4_IOC_EXTENTS_INFO, &extents_info);
		if (ret < 0) {
			return RETURN_NG;
		}
		file_extent_count += extents_info.entries;
	} while (extents_info.entries == DEFRAG_MAX_ENT && ret == 0);

	if (extent_count >= file_extent_count) {
		/* No improvment */
		errno = ENOSPC;
		return RETURN_NG;
	}

	return RETURN_OK;
}

/**
 * defrag_proc() -		Reserve extent group and execute the defrag program
 *
 * @fd:				the file's discriptor.
 * @target_exts_group_head:	the head of the original exts_group list.
 * @inode:			inode number from struct stat64.
 */
int defrag_proc(int fd, exts_group_t *target_exts_group_head,
		unsigned long long inode)
{
	int ret = 0;
	int percent = 0;
	int blocksize = 0;
	int data_len = 0;
	struct stat64	buf;
	exts_group_t 			*exts_group;
	extent_t			*extent;
	struct ext4_extents_info	extents_info;
	struct ext4_ext_defrag_data	defrag_data;
	extent_wrap_t *wrap_list = NULL;

	/* Reserve free extents */
	if (!target_exts_group_head) {
		/* Fault */
		return RETURN_NG;
	}

	/* get file size */
	memset(&buf, 0, sizeof(struct stat64));
	ret = fstat64(fd, &buf);
	if (ret < 0) {
		perror(NGMSG_FILE_INFO);
		return RETURN_NG;
	}
	/* get block size */
	ret = ioctl(fd, FIGETBSZ, &blocksize);
	if (ret < 0) {
		perror(NGMSG_FILE_BLOCKSIZE);
		return RETURN_NG;
	}
	memset(&extents_info, 0, sizeof(extents_info));
	memset(&defrag_data, 0, sizeof(struct ext4_ext_defrag_data));

	extents_info.ino = 0;
	exts_group = target_exts_group_head;
	extents_info.max_entries = DEFRAG_MAX_ENT;
	extents_info.ino = inode;
	ext4_fsblk_t data_block = 0;
	ext4_fsblk_t data_start = 0;
	defrag_data.start_offset = 0;

	do {
		extent_wrap_t	*wrap;
		extent = exts_group->start;
		data_len = 0;
		data_start = extent->data.start;
		data_block = extent->data.block;
		do {
			data_len += extent->data.len;
			if (extent->tag != EXT4_EXT_USE) {
				extent->tag = EXT4_EXT_RESERVE;
				extent = extent->next;
				continue;
			}
			extents_info.ino = extent->ino;
			extents_info.goal = fgoal;
			memcpy(extents_info.ext, &extent->data,
			       sizeof(struct ext4_extent_data));
			wrap = malloc(sizeof(extent_wrap_t));
			if (!wrap) {
				goto release_blocks;
			}
			wrap->group_ext = extent;
			wrap->next = wrap->prev = wrap;
			if (move_wrap(NULL, &wrap_list, wrap) < 0) {
				if (wrap) {
					free(wrap);
				}
				goto release_blocks;
			}
			extent = extent->next;
			extents_info.entries = 1;
			ret = ioctl(fd, EXT4_IOC_MOVE_VICTIM, &extents_info);
			if (ret < 0) {
				goto release_blocks;
			}
			mark_wrap(wrap_list);
			free_wrap_list(&wrap_list);
		} while (extent != exts_group->end->next);

		if (fsync(fd) < 0) {
			if (detail_flag) {
				perror(NGMSG_FILE_SYNC);
			}
			return ret;
		}

		extents_info.entries = 1;
		extents_info.ext[0].block = data_block;
		extents_info.ext[0].start = data_start;
		extents_info.ext[0].len = exts_group->len;
		ret = ioctl(fd, EXT4_IOC_RESERVE_BLOCK, &extents_info);
		if (ret < 0) {
			printf("RESERVE_ERROR ret = %d\n", ret);
			printf("block is already used\n");
			goto release_blocks;
		}
		ret = do_defrag(fd, exts_group, defrag_data);
		if (ret < 0) {
			printf("DEFRAG_ERROR ret = %d\n", ret);
			goto release_blocks;
		}
		defrag_data.start_offset += ret;
		ret = defrag_data.start_offset;

		/* print process progress */
		if (detail_flag) {
			percent = ((long long)ret * blocksize * 100) /
				  buf.st_size;
			if (percent > 100) {
				percent = 100;
			}
			printf("\033[79;16H\033[K progressing ====> %d%%",
			percent);
			fflush(stdout);
		}

		exts_group = exts_group->next;
	} while (exts_group != target_exts_group_head);
	return ret;

release_blocks:
	free_wrap_list(&wrap_list);
	ret = ioctl(fd, EXT4_IOC_BLOCK_RELEASE);
	if (ret < 0) {
		return RETURN_NG;
	}

	return ret;
}

/**
 * force_defrag() -	Execute the defrag program in force mode.
 *
 * @fd:			the file's descriptor.
 * @buf:		a pointer of the struct stat64.
 * @blocksize:		block size in byte.
 */
int force_defrag(int fd, const struct stat64 *buf, int blocksize)
{
	int     ret = 0;
	int     exts = 0;
	int     maxlen = 0;
	unsigned int    gnumber;
	unsigned long   filesize;
	unsigned long long	istart, iend;
	ext4_fsblk_t	bstart, bend;
	extent_t	*extlist_head = NULL;
	exts_group_t	*exts_group_list_head, *exts_group_list_target_head;
	struct ext4_group_data_info	ext4_group_data;

	exts_group_list_head = exts_group_list_target_head = NULL;

	/* Get group info */
	memset(&ext4_group_data, 0, sizeof(struct ext4_group_data_info));
	if (ioctl(fd, EXT4_IOC_GROUP_INFO, &ext4_group_data) < 0) {
		return RETURN_NG;
	}

	gnumber = (buf->st_ino - 1) / ext4_group_data.s_inodes_per_group;
	istart = gnumber * ext4_group_data.s_inodes_per_group;
	iend = istart + ext4_group_data.s_inodes_per_group - 1;
	bstart = gnumber * ext4_group_data.s_blocks_per_group;
	bend = bstart + ext4_group_data.s_blocks_per_group - 1;

	/* Compute filesize in block */
	filesize = (buf->st_size + blocksize - 1) / blocksize;

	/* Get used extents in the block group */
	ret = get_used_extent(fd, &extlist_head, istart, iend, bstart, bend);
	if (ret == RETURN_NG) {
		goto freelist;
	}

	/* Get free extents in the group */
	ret = get_free_extent(fd, buf->st_ino,
			     ext4_group_data.s_blocks_per_group, &extlist_head);
	if (ret == RETURN_NG) {
		goto freelist;
	}

	/* All space in this group is used by other groups' inodes */
	if (extlist_head == NULL) {
		ret = RETURN_NG;
		goto freelist;
	}

	/* Get continuous region(extents group) */
	ret = join_extents(extlist_head, &exts_group_list_target_head,
				  &exts_group_list_head, filesize, &maxlen);
	if (ret == RETURN_NG) {
		goto freelist;
	}
	if (ret == CHECK_FRAG_COUNT) {
		exts = 1;
		goto frag_check;
	}

	if (maxlen < filesize) {
		/* No enough space */
		errno = ENOSPC;
		ret = RETURN_NG;
		goto freelist;
	}

	if (!exts_group_list_head) {
		ret = RETURN_NG;
		goto freelist;
	}

	/* Find target extents group */
	ret = find_exts_group(&exts, filesize, &exts_group_list_head,
				      &exts_group_list_target_head);
	if (ret == RETURN_NG) {
		goto freelist;
	}

frag_check:
	/* Check file extent count*/
	ret = check_frag_count(fd, buf->st_ino, exts);
	if (ret == RETURN_NG) {
		goto freelist;
	}

	/* Reserve extent group and execute the defrag program */
	ret = defrag_proc(fd, exts_group_list_target_head, buf->st_ino);

freelist:
	free_exts_group(exts_group_list_target_head);
	free_exts_group(exts_group_list_head);
	free_ext(extlist_head);
	return ret;
}
