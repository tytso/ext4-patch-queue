ext4: Move the ext4_sb.h header file into ext4.h

There is no longer a reason for a separate ext4_sb.h header file, so
move it into ext4.h just to make life easier for developers to find
the relevant data structures and typedefs.  Should also speed up
compiles slightly, too.

Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/ext4.h    |  144 +++++++++++++++++++++++++++++++++++++++++++++-
 fs/ext4/ext4_sb.h |  163 -----------------------------------------------------
 2 files changed, 140 insertions(+), 167 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index ba57d66..af3c906 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -25,6 +25,10 @@
 #include <linux/rbtree.h>
 #include <linux/seqlock.h>
 #include <linux/mutex.h>
+#include <linux/timer.h>
+#include <linux/wait.h>
+#include <linux/blockgroup_lock.h>
+#include <linux/percpu_counter.h>
 
 /*
  * The fourth extended filesystem constants/structures
@@ -195,9 +199,6 @@ struct flex_groups {
 #define EXT4_BG_BLOCK_UNINIT	0x0002 /* Block bitmap not in use */
 #define EXT4_BG_INODE_ZEROED	0x0004 /* On-disk itable initialized to zero */
 
-#ifdef __KERNEL__
-#include "ext4_sb.h"
-#endif
 /*
  * Macro-instructions used to manage group descriptors
  */
@@ -809,6 +810,136 @@ struct ext4_super_block {
 };
 
 #ifdef __KERNEL__
+/*
+ * fourth extended-fs super-block data in memory
+ */
+struct ext4_sb_info {
+	unsigned long s_desc_size;	/* Size of a group descriptor in bytes */
+	unsigned long s_inodes_per_block;/* Number of inodes per block */
+	unsigned long s_blocks_per_group;/* Number of blocks in a group */
+	unsigned long s_inodes_per_group;/* Number of inodes in a group */
+	unsigned long s_itb_per_group;	/* Number of inode table blocks per group */
+	unsigned long s_gdb_count;	/* Number of group descriptor blocks */
+	unsigned long s_desc_per_block;	/* Number of group descriptors per block */
+	ext4_group_t s_groups_count;	/* Number of groups in the fs */
+	unsigned long s_overhead_last;  /* Last calculated overhead */
+	unsigned long s_blocks_last;    /* Last seen block count */
+	loff_t s_bitmap_maxbytes;	/* max bytes for bitmap files */
+	struct buffer_head * s_sbh;	/* Buffer containing the super block */
+	struct ext4_super_block *s_es;	/* Pointer to the super block in the buffer */
+	struct buffer_head **s_group_desc;
+	unsigned long  s_mount_opt;
+	ext4_fsblk_t s_sb_block;
+	uid_t s_resuid;
+	gid_t s_resgid;
+	unsigned short s_mount_state;
+	unsigned short s_pad;
+	int s_addr_per_block_bits;
+	int s_desc_per_block_bits;
+	int s_inode_size;
+	int s_first_ino;
+	unsigned int s_inode_readahead_blks;
+	spinlock_t s_next_gen_lock;
+	u32 s_next_generation;
+	u32 s_hash_seed[4];
+	int s_def_hash_version;
+	int s_hash_unsigned;	/* 3 if hash should be signed, 0 if not */
+	struct percpu_counter s_freeblocks_counter;
+	struct percpu_counter s_freeinodes_counter;
+	struct percpu_counter s_dirs_counter;
+	struct percpu_counter s_dirtyblocks_counter;
+	struct blockgroup_lock *s_blockgroup_lock;
+	struct proc_dir_entry *s_proc;
+	struct kobject s_kobj;
+	struct completion s_kobj_unregister;
+
+	/* Journaling */
+	struct inode *s_journal_inode;
+	struct journal_s *s_journal;
+	struct list_head s_orphan;
+	struct mutex s_orphan_lock;
+	struct mutex s_resize_lock;
+	unsigned long s_commit_interval;
+	u32 s_max_batch_time;
+	u32 s_min_batch_time;
+	struct block_device *journal_bdev;
+#ifdef CONFIG_JBD2_DEBUG
+	struct timer_list turn_ro_timer;	/* For turning read-only (crash simulation) */
+	wait_queue_head_t ro_wait_queue;	/* For people waiting for the fs to go read-only */
+#endif
+#ifdef CONFIG_QUOTA
+	char *s_qf_names[MAXQUOTAS];		/* Names of quota files with journalled quota */
+	int s_jquota_fmt;			/* Format of quota to use */
+#endif
+	unsigned int s_want_extra_isize; /* New inodes should reserve # bytes */
+
+#ifdef EXTENTS_STATS
+	/* ext4 extents stats */
+	unsigned long s_ext_min;
+	unsigned long s_ext_max;
+	unsigned long s_depth_max;
+	spinlock_t s_ext_stats_lock;
+	unsigned long s_ext_blocks;
+	unsigned long s_ext_extents;
+#endif
+
+	/* for buddy allocator */
+	struct ext4_group_info ***s_group_info;
+	struct inode *s_buddy_cache;
+	long s_blocks_reserved;
+	spinlock_t s_reserve_lock;
+	spinlock_t s_md_lock;
+	tid_t s_last_transaction;
+	unsigned short *s_mb_offsets;
+	unsigned int *s_mb_maxs;
+
+	/* tunables */
+	unsigned long s_stripe;
+	unsigned int s_mb_stream_request;
+	unsigned int s_mb_max_to_scan;
+	unsigned int s_mb_min_to_scan;
+	unsigned int s_mb_stats;
+	unsigned int s_mb_order2_reqs;
+	unsigned int s_mb_group_prealloc;
+	/* where last allocation was done - for stream allocation */
+	unsigned long s_mb_last_group;
+	unsigned long s_mb_last_start;
+
+	/* history to debug policy */
+	struct ext4_mb_history *s_mb_history;
+	int s_mb_history_cur;
+	int s_mb_history_max;
+	int s_mb_history_num;
+	spinlock_t s_mb_history_lock;
+	int s_mb_history_filter;
+
+	/* stats for buddy allocator */
+	spinlock_t s_mb_pa_lock;
+	atomic_t s_bal_reqs;	/* number of reqs with len > 1 */
+	atomic_t s_bal_success;	/* we found long enough chunks */
+	atomic_t s_bal_allocated;	/* in blocks */
+	atomic_t s_bal_ex_scanned;	/* total extents scanned */
+	atomic_t s_bal_goals;	/* goal hits */
+	atomic_t s_bal_breaks;	/* too long searches */
+	atomic_t s_bal_2orders;	/* 2^order hits */
+	spinlock_t s_bal_lock;
+	unsigned long s_mb_buddies_generated;
+	unsigned long long s_mb_generation_time;
+	atomic_t s_mb_lost_chunks;
+	atomic_t s_mb_preallocated;
+	atomic_t s_mb_discarded;
+
+	/* locality groups */
+	struct ext4_locality_group *s_locality_groups;
+
+	/* for write statistics */
+	unsigned long s_sectors_written_start;
+	u64 s_kbytes_written;
+
+	unsigned int s_log_groups_per_flex;
+	struct flex_groups *s_flex_groups;
+};
+
 static inline struct ext4_sb_info *EXT4_SB(struct super_block *sb)
 {
 	return sb->s_fs_info;
@@ -824,7 +955,6 @@ static inline struct timespec ext4_current_time(struct inode *inode)
 		current_fs_time(inode->i_sb) : CURRENT_TIME_SEC;
 }
 
-
 static inline int ext4_valid_inum(struct super_block *sb, unsigned long ino)
 {
 	return ino == EXT4_ROOT_INO ||
@@ -833,6 +963,12 @@ static inline int ext4_valid_inum(struct super_block *sb, unsigned long ino)
 		(ino >= EXT4_FIRST_INO(sb) &&
 		 ino <= le32_to_cpu(EXT4_SB(sb)->s_es->s_inodes_count));
 }
+
+static inline spinlock_t *
+sb_bgl_lock(struct ext4_sb_info *sbi, unsigned int block_group)
+{
+	return bgl_lock_ptr(sbi->s_blockgroup_lock, block_group);
+}
 #else
 /* Assume that user mode programs are passing in an ext4fs superblock, not
  * a kernel struct super_block.  This will allow us to call the feature-test
diff --git a/fs/ext4/ext4_sb.h b/fs/ext4/ext4_sb.h
deleted file mode 100644
index 2d36223..0000000
--- a/fs/ext4/ext4_sb.h
+++ /dev/null
@@ -1,163 +0,0 @@
-/*
- *  ext4_sb.h
- *
- * Copyright (C) 1992, 1993, 1994, 1995
- * Remy Card (card@masi.ibp.fr)
- * Laboratoire MASI - Institut Blaise Pascal
- * Universite Pierre et Marie Curie (Paris VI)
- *
- *  from
- *
- *  linux/include/linux/minix_fs_sb.h
- *
- *  Copyright (C) 1991, 1992  Linus Torvalds
- */
-
-#ifndef _EXT4_SB
-#define _EXT4_SB
-
-#ifdef __KERNEL__
-#include <linux/timer.h>
-#include <linux/wait.h>
-#include <linux/blockgroup_lock.h>
-#include <linux/percpu_counter.h>
-#endif
-#include <linux/rbtree.h>
-
-/*
- * fourth extended-fs super-block data in memory
- */
-struct ext4_sb_info {
-	unsigned long s_desc_size;	/* Size of a group descriptor in bytes */
-	unsigned long s_inodes_per_block;/* Number of inodes per block */
-	unsigned long s_blocks_per_group;/* Number of blocks in a group */
-	unsigned long s_inodes_per_group;/* Number of inodes in a group */
-	unsigned long s_itb_per_group;	/* Number of inode table blocks per group */
-	unsigned long s_gdb_count;	/* Number of group descriptor blocks */
-	unsigned long s_desc_per_block;	/* Number of group descriptors per block */
-	ext4_group_t s_groups_count;	/* Number of groups in the fs */
-	unsigned long s_overhead_last;  /* Last calculated overhead */
-	unsigned long s_blocks_last;    /* Last seen block count */
-	loff_t s_bitmap_maxbytes;	/* max bytes for bitmap files */
-	struct buffer_head * s_sbh;	/* Buffer containing the super block */
-	struct ext4_super_block *s_es;	/* Pointer to the super block in the buffer */
-	struct buffer_head **s_group_desc;
-	unsigned long  s_mount_opt;
-	ext4_fsblk_t s_sb_block;
-	uid_t s_resuid;
-	gid_t s_resgid;
-	unsigned short s_mount_state;
-	unsigned short s_pad;
-	int s_addr_per_block_bits;
-	int s_desc_per_block_bits;
-	int s_inode_size;
-	int s_first_ino;
-	unsigned int s_inode_readahead_blks;
-	spinlock_t s_next_gen_lock;
-	u32 s_next_generation;
-	u32 s_hash_seed[4];
-	int s_def_hash_version;
-	int s_hash_unsigned;	/* 3 if hash should be signed, 0 if not */
-	struct percpu_counter s_freeblocks_counter;
-	struct percpu_counter s_freeinodes_counter;
-	struct percpu_counter s_dirs_counter;
-	struct percpu_counter s_dirtyblocks_counter;
-	struct blockgroup_lock *s_blockgroup_lock;
-	struct proc_dir_entry *s_proc;
-	struct kobject s_kobj;
-	struct completion s_kobj_unregister;
-
-	/* Journaling */
-	struct inode *s_journal_inode;
-	struct journal_s *s_journal;
-	struct list_head s_orphan;
-	struct mutex s_orphan_lock;
-	struct mutex s_resize_lock;
-	unsigned long s_commit_interval;
-	u32 s_max_batch_time;
-	u32 s_min_batch_time;
-	struct block_device *journal_bdev;
-#ifdef CONFIG_JBD2_DEBUG
-	struct timer_list turn_ro_timer;	/* For turning read-only (crash simulation) */
-	wait_queue_head_t ro_wait_queue;	/* For people waiting for the fs to go read-only */
-#endif
-#ifdef CONFIG_QUOTA
-	char *s_qf_names[MAXQUOTAS];		/* Names of quota files with journalled quota */
-	int s_jquota_fmt;			/* Format of quota to use */
-#endif
-	unsigned int s_want_extra_isize; /* New inodes should reserve # bytes */
-
-#ifdef EXTENTS_STATS
-	/* ext4 extents stats */
-	unsigned long s_ext_min;
-	unsigned long s_ext_max;
-	unsigned long s_depth_max;
-	spinlock_t s_ext_stats_lock;
-	unsigned long s_ext_blocks;
-	unsigned long s_ext_extents;
-#endif
-
-	/* for buddy allocator */
-	struct ext4_group_info ***s_group_info;
-	struct inode *s_buddy_cache;
-	long s_blocks_reserved;
-	spinlock_t s_reserve_lock;
-	spinlock_t s_md_lock;
-	tid_t s_last_transaction;
-	unsigned short *s_mb_offsets;
-	unsigned int *s_mb_maxs;
-
-	/* tunables */
-	unsigned long s_stripe;
-	unsigned int s_mb_stream_request;
-	unsigned int s_mb_max_to_scan;
-	unsigned int s_mb_min_to_scan;
-	unsigned int s_mb_stats;
-	unsigned int s_mb_order2_reqs;
-	unsigned int s_mb_group_prealloc;
-	/* where last allocation was done - for stream allocation */
-	unsigned long s_mb_last_group;
-	unsigned long s_mb_last_start;
-
-	/* history to debug policy */
-	struct ext4_mb_history *s_mb_history;
-	int s_mb_history_cur;
-	int s_mb_history_max;
-	int s_mb_history_num;
-	spinlock_t s_mb_history_lock;
-	int s_mb_history_filter;
-
-	/* stats for buddy allocator */
-	spinlock_t s_mb_pa_lock;
-	atomic_t s_bal_reqs;	/* number of reqs with len > 1 */
-	atomic_t s_bal_success;	/* we found long enough chunks */
-	atomic_t s_bal_allocated;	/* in blocks */
-	atomic_t s_bal_ex_scanned;	/* total extents scanned */
-	atomic_t s_bal_goals;	/* goal hits */
-	atomic_t s_bal_breaks;	/* too long searches */
-	atomic_t s_bal_2orders;	/* 2^order hits */
-	spinlock_t s_bal_lock;
-	unsigned long s_mb_buddies_generated;
-	unsigned long long s_mb_generation_time;
-	atomic_t s_mb_lost_chunks;
-	atomic_t s_mb_preallocated;
-	atomic_t s_mb_discarded;
-
-	/* locality groups */
-	struct ext4_locality_group *s_locality_groups;
-
-	/* for write statistics */
-	unsigned long s_sectors_written_start;
-	u64 s_kbytes_written;
-
-	unsigned int s_log_groups_per_flex;
-	struct flex_groups *s_flex_groups;
-};
-
-static inline spinlock_t *
-sb_bgl_lock(struct ext4_sb_info *sbi, unsigned int block_group)
-{
-	return bgl_lock_ptr(sbi->s_blockgroup_lock, block_group);
-}
-
-#endif	/* _EXT4_SB */
