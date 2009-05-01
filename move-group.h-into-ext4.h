ext4: Move fs/ext4/group.h into ext4.h

Move the function prototypes in group.h into ext4.h so they are all
defined in one place.

Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/balloc.c |    1 -
 fs/ext4/ext4.h   |   17 +++++++++++++++++
 fs/ext4/group.h  |   29 -----------------------------
 fs/ext4/ialloc.c |    1 -
 fs/ext4/resize.c |    1 -
 fs/ext4/super.c  |    1 -
 6 files changed, 17 insertions(+), 33 deletions(-)

diff --git a/fs/ext4/balloc.c b/fs/ext4/balloc.c
index a5ba039..92f557d 100644
--- a/fs/ext4/balloc.c
+++ b/fs/ext4/balloc.c
@@ -19,7 +19,6 @@
 #include <linux/buffer_head.h>
 #include "ext4.h"
 #include "ext4_jbd2.h"
-#include "group.h"
 #include "mballoc.h"
 
 /*
diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 13238dc..05a6860 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -1271,6 +1271,14 @@ extern struct ext4_group_desc * ext4_get_group_desc(struct super_block * sb,
 						    ext4_group_t block_group,
 						    struct buffer_head ** bh);
 extern int ext4_should_retry_alloc(struct super_block *sb, int *retries);
+struct buffer_head *ext4_read_block_bitmap(struct super_block *sb,
+				      ext4_group_t block_group);
+extern unsigned ext4_init_block_bitmap(struct super_block *sb,
+				       struct buffer_head *bh,
+				       ext4_group_t group,
+				       struct ext4_group_desc *desc);
+#define ext4_free_blocks_after_init(sb, group, desc)			\
+		ext4_init_block_bitmap(sb, NULL, group, desc)
 
 /* dir.c */
 extern int ext4_check_dir_entry(const char *, struct inode *,
@@ -1295,6 +1303,11 @@ extern struct inode * ext4_orphan_get(struct super_block *, unsigned long);
 extern unsigned long ext4_count_free_inodes(struct super_block *);
 extern unsigned long ext4_count_dirs(struct super_block *);
 extern void ext4_check_inodes_bitmap(struct super_block *);
+extern unsigned ext4_init_inode_bitmap(struct super_block *sb,
+				       struct buffer_head *bh,
+				       ext4_group_t group,
+				       struct ext4_group_desc *desc);
+extern void mark_bitmap_end(int start_bit, int end_bit, char *bitmap);
 
 /* mballoc.c */
 extern long ext4_mb_stats;
@@ -1418,6 +1431,10 @@ extern void ext4_used_dirs_set(struct super_block *sb,
 				struct ext4_group_desc *bg, __u32 count);
 extern void ext4_itable_unused_set(struct super_block *sb,
 				   struct ext4_group_desc *bg, __u32 count);
+extern __le16 ext4_group_desc_csum(struct ext4_sb_info *sbi, __u32 group,
+				   struct ext4_group_desc *gdp);
+extern int ext4_group_desc_csum_verify(struct ext4_sb_info *sbi, __u32 group,
+				       struct ext4_group_desc *gdp);
 
 static inline ext4_fsblk_t ext4_blocks_count(struct ext4_super_block *es)
 {
diff --git a/fs/ext4/group.h b/fs/ext4/group.h
deleted file mode 100644
index c2c0a8d..0000000
--- a/fs/ext4/group.h
+++ /dev/null
@@ -1,29 +0,0 @@
-/*
- *  linux/fs/ext4/group.h
- *
- * Copyright (C) 2007 Cluster File Systems, Inc
- *
- * Author: Andreas Dilger <adilger@clusterfs.com>
- */
-
-#ifndef _LINUX_EXT4_GROUP_H
-#define _LINUX_EXT4_GROUP_H
-
-extern __le16 ext4_group_desc_csum(struct ext4_sb_info *sbi, __u32 group,
-				   struct ext4_group_desc *gdp);
-extern int ext4_group_desc_csum_verify(struct ext4_sb_info *sbi, __u32 group,
-				       struct ext4_group_desc *gdp);
-struct buffer_head *ext4_read_block_bitmap(struct super_block *sb,
-				      ext4_group_t block_group);
-extern unsigned ext4_init_block_bitmap(struct super_block *sb,
-				       struct buffer_head *bh,
-				       ext4_group_t group,
-				       struct ext4_group_desc *desc);
-#define ext4_free_blocks_after_init(sb, group, desc)			\
-		ext4_init_block_bitmap(sb, NULL, group, desc)
-extern unsigned ext4_init_inode_bitmap(struct super_block *sb,
-				       struct buffer_head *bh,
-				       ext4_group_t group,
-				       struct ext4_group_desc *desc);
-extern void mark_bitmap_end(int start_bit, int end_bit, char *bitmap);
-#endif /* _LINUX_EXT4_GROUP_H */
diff --git a/fs/ext4/ialloc.c b/fs/ext4/ialloc.c
index 55ba419..916d05c 100644
--- a/fs/ext4/ialloc.c
+++ b/fs/ext4/ialloc.c
@@ -27,7 +27,6 @@
 #include "ext4_jbd2.h"
 #include "xattr.h"
 #include "acl.h"
-#include "group.h"
 
 /*
  * ialloc.c contains the inodes allocation and deallocation routines
diff --git a/fs/ext4/resize.c b/fs/ext4/resize.c
index e8ded13..27eb289 100644
--- a/fs/ext4/resize.c
+++ b/fs/ext4/resize.c
@@ -15,7 +15,6 @@
 #include <linux/slab.h>
 
 #include "ext4_jbd2.h"
-#include "group.h"
 
 #define outside(b, first, last)	((b) < (first) || (b) >= (last))
 #define inside(b, first, last)	((b) >= (first) && (b) < (last))
diff --git a/fs/ext4/super.c b/fs/ext4/super.c
index d79e1c4..7903f20 100644
--- a/fs/ext4/super.c
+++ b/fs/ext4/super.c
@@ -46,7 +46,6 @@
 #include "ext4_jbd2.h"
 #include "xattr.h"
 #include "acl.h"
-#include "group.h"
 
 struct proc_dir_entry *ext4_proc_root;
 static struct kset *ext4_kset;
