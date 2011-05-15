ext4: move ext4_add_groupblocks() to mballoc.c

From: Amir Goldstein <amir73il@users.sf.net>

In preparation for the next patch, the function ext4_add_groupblocks()
is moved to mballoc.c, where it could use some static functions.

Signed-off-by: Amir Goldstein <amir73il@users.sf.net>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/balloc.c  |  124 -----------------------------------------------------
 fs/ext4/ext4.h    |    4 +-
 fs/ext4/mballoc.c |  124 +++++++++++++++++++++++++++++++++++++++++++++++++++++
 3 files changed, 126 insertions(+), 126 deletions(-)

diff --git a/fs/ext4/balloc.c b/fs/ext4/balloc.c
index 9c6cd51..b2d10da 100644
--- a/fs/ext4/balloc.c
+++ b/fs/ext4/balloc.c
@@ -362,130 +362,6 @@ ext4_read_block_bitmap(struct super_block *sb, ext4_group_t block_group)
 }
 
 /**
- * ext4_add_groupblocks() -- Add given blocks to an existing group
- * @handle:			handle to this transaction
- * @sb:				super block
- * @block:			start physcial block to add to the block group
- * @count:			number of blocks to free
- *
- * This marks the blocks as free in the bitmap. We ask the
- * mballoc to reload the buddy after this by setting group
- * EXT4_GROUP_INFO_NEED_INIT_BIT flag
- */
-void ext4_add_groupblocks(handle_t *handle, struct super_block *sb,
-			 ext4_fsblk_t block, unsigned long count)
-{
-	struct buffer_head *bitmap_bh = NULL;
-	struct buffer_head *gd_bh;
-	ext4_group_t block_group;
-	ext4_grpblk_t bit;
-	unsigned int i;
-	struct ext4_group_desc *desc;
-	struct ext4_sb_info *sbi = EXT4_SB(sb);
-	int err = 0, ret, blk_free_count;
-	ext4_grpblk_t blocks_freed;
-	struct ext4_group_info *grp;
-
-	ext4_debug("Adding block(s) %llu-%llu\n", block, block + count - 1);
-
-	ext4_get_group_no_and_offset(sb, block, &block_group, &bit);
-	grp = ext4_get_group_info(sb, block_group);
-	/*
-	 * Check to see if we are freeing blocks across a group
-	 * boundary.
-	 */
-	if (bit + count > EXT4_BLOCKS_PER_GROUP(sb)) {
-		goto error_return;
-	}
-	bitmap_bh = ext4_read_block_bitmap(sb, block_group);
-	if (!bitmap_bh)
-		goto error_return;
-	desc = ext4_get_group_desc(sb, block_group, &gd_bh);
-	if (!desc)
-		goto error_return;
-
-	if (in_range(ext4_block_bitmap(sb, desc), block, count) ||
-	    in_range(ext4_inode_bitmap(sb, desc), block, count) ||
-	    in_range(block, ext4_inode_table(sb, desc), sbi->s_itb_per_group) ||
-	    in_range(block + count - 1, ext4_inode_table(sb, desc),
-		     sbi->s_itb_per_group)) {
-		ext4_error(sb, "Adding blocks in system zones - "
-			   "Block = %llu, count = %lu",
-			   block, count);
-		goto error_return;
-	}
-
-	/*
-	 * We are about to add blocks to the bitmap,
-	 * so we need undo access.
-	 */
-	BUFFER_TRACE(bitmap_bh, "getting undo access");
-	err = ext4_journal_get_undo_access(handle, bitmap_bh);
-	if (err)
-		goto error_return;
-
-	/*
-	 * We are about to modify some metadata.  Call the journal APIs
-	 * to unshare ->b_data if a currently-committing transaction is
-	 * using it
-	 */
-	BUFFER_TRACE(gd_bh, "get_write_access");
-	err = ext4_journal_get_write_access(handle, gd_bh);
-	if (err)
-		goto error_return;
-	/*
-	 * make sure we don't allow a parallel init on other groups in the
-	 * same buddy cache
-	 */
-	down_write(&grp->alloc_sem);
-	for (i = 0, blocks_freed = 0; i < count; i++) {
-		BUFFER_TRACE(bitmap_bh, "clear bit");
-		if (!ext4_clear_bit_atomic(ext4_group_lock_ptr(sb, block_group),
-						bit + i, bitmap_bh->b_data)) {
-			ext4_error(sb, "bit already cleared for block %llu",
-				   (ext4_fsblk_t)(block + i));
-			BUFFER_TRACE(bitmap_bh, "bit already cleared");
-		} else {
-			blocks_freed++;
-		}
-	}
-	ext4_lock_group(sb, block_group);
-	blk_free_count = blocks_freed + ext4_free_blks_count(sb, desc);
-	ext4_free_blks_set(sb, desc, blk_free_count);
-	desc->bg_checksum = ext4_group_desc_csum(sbi, block_group, desc);
-	ext4_unlock_group(sb, block_group);
-	percpu_counter_add(&sbi->s_freeblocks_counter, blocks_freed);
-
-	if (sbi->s_log_groups_per_flex) {
-		ext4_group_t flex_group = ext4_flex_group(sbi, block_group);
-		atomic_add(blocks_freed,
-			   &sbi->s_flex_groups[flex_group].free_blocks);
-	}
-	/*
-	 * request to reload the buddy with the
-	 * new bitmap information
-	 */
-	set_bit(EXT4_GROUP_INFO_NEED_INIT_BIT, &(grp->bb_state));
-	grp->bb_free += blocks_freed;
-	up_write(&grp->alloc_sem);
-
-	/* We dirtied the bitmap block */
-	BUFFER_TRACE(bitmap_bh, "dirtied bitmap block");
-	err = ext4_handle_dirty_metadata(handle, NULL, bitmap_bh);
-
-	/* And the group descriptor block */
-	BUFFER_TRACE(gd_bh, "dirtied group descriptor block");
-	ret = ext4_handle_dirty_metadata(handle, NULL, gd_bh);
-	if (!err)
-		err = ret;
-
-error_return:
-	brelse(bitmap_bh);
-	ext4_std_error(sb, err);
-	return;
-}
-
-/**
  * ext4_has_free_blocks()
  * @sbi:	in-core super block structure.
  * @nblocks:	number of needed blocks
diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 076c5d2..47986ae 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -1655,8 +1655,6 @@ extern unsigned long ext4_bg_num_gdb(struct super_block *sb,
 extern ext4_fsblk_t ext4_new_meta_blocks(handle_t *handle, struct inode *inode,
 			ext4_fsblk_t goal, unsigned long *count, int *errp);
 extern int ext4_claim_free_blocks(struct ext4_sb_info *sbi, s64 nblocks);
-extern void ext4_add_groupblocks(handle_t *handle, struct super_block *sb,
-				ext4_fsblk_t block, unsigned long count);
 extern ext4_fsblk_t ext4_count_free_blocks(struct super_block *);
 extern void ext4_check_blocks_bitmap(struct super_block *);
 extern struct ext4_group_desc * ext4_get_group_desc(struct super_block * sb,
@@ -1721,6 +1719,8 @@ extern void ext4_free_blocks(handle_t *handle, struct inode *inode,
 			     unsigned long count, int flags);
 extern int ext4_mb_add_groupinfo(struct super_block *sb,
 		ext4_group_t i, struct ext4_group_desc *desc);
+extern void ext4_add_groupblocks(handle_t *handle, struct super_block *sb,
+				ext4_fsblk_t block, unsigned long count);
 extern int ext4_trim_fs(struct super_block *, struct fstrim_range *);
 
 /* inode.c */
diff --git a/fs/ext4/mballoc.c b/fs/ext4/mballoc.c
index 730c1a7..92aa05d 100644
--- a/fs/ext4/mballoc.c
+++ b/fs/ext4/mballoc.c
@@ -4700,6 +4700,130 @@ error_return:
 }
 
 /**
+ * ext4_add_groupblocks() -- Add given blocks to an existing group
+ * @handle:			handle to this transaction
+ * @sb:				super block
+ * @block:			start physcial block to add to the block group
+ * @count:			number of blocks to free
+ *
+ * This marks the blocks as free in the bitmap. We ask the
+ * mballoc to reload the buddy after this by setting group
+ * EXT4_GROUP_INFO_NEED_INIT_BIT flag
+ */
+void ext4_add_groupblocks(handle_t *handle, struct super_block *sb,
+			 ext4_fsblk_t block, unsigned long count)
+{
+	struct buffer_head *bitmap_bh = NULL;
+	struct buffer_head *gd_bh;
+	ext4_group_t block_group;
+	ext4_grpblk_t bit;
+	unsigned int i;
+	struct ext4_group_desc *desc;
+	struct ext4_sb_info *sbi = EXT4_SB(sb);
+	int err = 0, ret, blk_free_count;
+	ext4_grpblk_t blocks_freed;
+	struct ext4_group_info *grp;
+
+	ext4_debug("Adding block(s) %llu-%llu\n", block, block + count - 1);
+
+	ext4_get_group_no_and_offset(sb, block, &block_group, &bit);
+	grp = ext4_get_group_info(sb, block_group);
+	/*
+	 * Check to see if we are freeing blocks across a group
+	 * boundary.
+	 */
+	if (bit + count > EXT4_BLOCKS_PER_GROUP(sb)) {
+		goto error_return;
+	}
+	bitmap_bh = ext4_read_block_bitmap(sb, block_group);
+	if (!bitmap_bh)
+		goto error_return;
+	desc = ext4_get_group_desc(sb, block_group, &gd_bh);
+	if (!desc)
+		goto error_return;
+
+	if (in_range(ext4_block_bitmap(sb, desc), block, count) ||
+	    in_range(ext4_inode_bitmap(sb, desc), block, count) ||
+	    in_range(block, ext4_inode_table(sb, desc), sbi->s_itb_per_group) ||
+	    in_range(block + count - 1, ext4_inode_table(sb, desc),
+		     sbi->s_itb_per_group)) {
+		ext4_error(sb, "Adding blocks in system zones - "
+			   "Block = %llu, count = %lu",
+			   block, count);
+		goto error_return;
+	}
+
+	/*
+	 * We are about to add blocks to the bitmap,
+	 * so we need undo access.
+	 */
+	BUFFER_TRACE(bitmap_bh, "getting undo access");
+	err = ext4_journal_get_undo_access(handle, bitmap_bh);
+	if (err)
+		goto error_return;
+
+	/*
+	 * We are about to modify some metadata.  Call the journal APIs
+	 * to unshare ->b_data if a currently-committing transaction is
+	 * using it
+	 */
+	BUFFER_TRACE(gd_bh, "get_write_access");
+	err = ext4_journal_get_write_access(handle, gd_bh);
+	if (err)
+		goto error_return;
+	/*
+	 * make sure we don't allow a parallel init on other groups in the
+	 * same buddy cache
+	 */
+	down_write(&grp->alloc_sem);
+	for (i = 0, blocks_freed = 0; i < count; i++) {
+		BUFFER_TRACE(bitmap_bh, "clear bit");
+		if (!ext4_clear_bit_atomic(ext4_group_lock_ptr(sb, block_group),
+						bit + i, bitmap_bh->b_data)) {
+			ext4_error(sb, "bit already cleared for block %llu",
+				   (ext4_fsblk_t)(block + i));
+			BUFFER_TRACE(bitmap_bh, "bit already cleared");
+		} else {
+			blocks_freed++;
+		}
+	}
+	ext4_lock_group(sb, block_group);
+	blk_free_count = blocks_freed + ext4_free_blks_count(sb, desc);
+	ext4_free_blks_set(sb, desc, blk_free_count);
+	desc->bg_checksum = ext4_group_desc_csum(sbi, block_group, desc);
+	ext4_unlock_group(sb, block_group);
+	percpu_counter_add(&sbi->s_freeblocks_counter, blocks_freed);
+
+	if (sbi->s_log_groups_per_flex) {
+		ext4_group_t flex_group = ext4_flex_group(sbi, block_group);
+		atomic_add(blocks_freed,
+			   &sbi->s_flex_groups[flex_group].free_blocks);
+	}
+	/*
+	 * request to reload the buddy with the
+	 * new bitmap information
+	 */
+	set_bit(EXT4_GROUP_INFO_NEED_INIT_BIT, &(grp->bb_state));
+	grp->bb_free += blocks_freed;
+	up_write(&grp->alloc_sem);
+
+	/* We dirtied the bitmap block */
+	BUFFER_TRACE(bitmap_bh, "dirtied bitmap block");
+	err = ext4_handle_dirty_metadata(handle, NULL, bitmap_bh);
+
+	/* And the group descriptor block */
+	BUFFER_TRACE(gd_bh, "dirtied group descriptor block");
+	ret = ext4_handle_dirty_metadata(handle, NULL, gd_bh);
+	if (!err)
+		err = ret;
+
+error_return:
+	brelse(bitmap_bh);
+	ext4_std_error(sb, err);
+	return;
+}
+
+/**
  * ext4_trim_extent -- function to TRIM one single free extent in the group
  * @sb:		super block for the file system
  * @start:	starting block of the free extent in the alloc. group
