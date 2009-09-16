ext4: Replace BUG_ON() with ext4_error() in move_extents.c

From: Akira Fujita <a-fujita@rs.jp.nec.com>

Replace BUG_ON calls with a call to ext4_error()
to print an error message if EXT4_IOC_MOVE_EXT failed
with some kind of reasons.  This will help to debug.
Ted pointed this out, thanks.

Signed-off-by: Akira Fujita <a-fujita@rs.jp.nec.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
[PATCH 2/4]
 fs/ext4/move_extent.c |  149 ++++++++++++++++++++++++++++++++++++-------------
 1 files changed, 109 insertions(+), 40 deletions(-)

diff --git a/fs/ext4/move_extent.c b/fs/ext4/move_extent.c
index 00888e6..6d126d0 100644
--- a/fs/ext4/move_extent.c
+++ b/fs/ext4/move_extent.c
@@ -128,6 +128,31 @@ mext_next_extent(struct inode *inode, struct ext4_ext_path *path,
 }

 /**
+ * mext_check_null_inode - NULL check for two inodes
+ *
+ * If inode1 or inode2 is NULL, return -EIO. Otherwise, return 0.
+ */
+static int
+mext_check_null_inode(struct inode *inode1, struct inode *inode2,
+		const char *function)
+{
+	int ret = 0;
+
+	if (inode1 == NULL) {
+		ext4_error(inode2->i_sb, function,
+			"Both inodes should not be NULL: "
+			"inode1 NULL inode2 %lu", inode2->i_ino);
+		ret = -EIO;
+	} else if (inode2 == NULL) {
+		ext4_error(inode1->i_sb, function,
+			"Both inodes should not be NULL: "
+			"inode1 %lu inode2 NULL", inode1->i_ino);
+		ret = -EIO;
+	}
+	return ret;
+}
+
+/**
  * mext_double_down_read - Acquire two inodes' read semaphore
  *
  * @orig_inode:		original inode structure
@@ -139,8 +164,6 @@ mext_double_down_read(struct inode *orig_inode, struct inode *donor_inode)
 {
 	struct inode *first = orig_inode, *second = donor_inode;

-	BUG_ON(orig_inode == NULL || donor_inode == NULL);
-
 	/*
 	 * Use the inode number to provide the stable locking order instead
 	 * of its address, because the C language doesn't guarantee you can
@@ -167,8 +190,6 @@ mext_double_down_write(struct inode *orig_inode, struct inode *donor_inode)
 {
 	struct inode *first = orig_inode, *second = donor_inode;

-	BUG_ON(orig_inode == NULL || donor_inode == NULL);
-
 	/*
 	 * Use the inode number to provide the stable locking order instead
 	 * of its address, because the C language doesn't guarantee you can
@@ -193,8 +214,6 @@ mext_double_down_write(struct inode *orig_inode, struct inode *donor_inode)
 static void
 mext_double_up_read(struct inode *orig_inode, struct inode *donor_inode)
 {
-	BUG_ON(orig_inode == NULL || donor_inode == NULL);
-
 	up_read(&EXT4_I(orig_inode)->i_data_sem);
 	up_read(&EXT4_I(donor_inode)->i_data_sem);
 }
@@ -209,8 +228,6 @@ mext_double_up_read(struct inode *orig_inode, struct inode *donor_inode)
 static void
 mext_double_up_write(struct inode *orig_inode, struct inode *donor_inode)
 {
-	BUG_ON(orig_inode == NULL || donor_inode == NULL);
-
 	up_write(&EXT4_I(orig_inode)->i_data_sem);
 	up_write(&EXT4_I(donor_inode)->i_data_sem);
 }
@@ -534,7 +551,15 @@ mext_leaf_block(handle_t *handle, struct inode *orig_inode,
 	 * oext      |-----------|
 	 * new_ext       |-------|
 	 */
-	BUG_ON(le32_to_cpu(oext->ee_block) + oext_alen - 1 < new_ext_end);
+	if (le32_to_cpu(oext->ee_block) + oext_alen - 1 < new_ext_end) {
+		ext4_error(orig_inode->i_sb, __func__,
+			"new_ext_end(%u) should be less than or equal to "
+			"oext->ee_block(%u) + oext_alen(%d) - 1",
+			new_ext_end, le32_to_cpu(oext->ee_block),
+			oext_alen);
+		ret = -EIO;
+		goto out;
+	}

 	/*
 	 * Case: new_ext is smaller than original extent
@@ -558,6 +583,7 @@ mext_leaf_block(handle_t *handle, struct inode *orig_inode,

 	ret = mext_insert_extents(handle, orig_inode, orig_path, o_start,
 				o_end, &start_ext, &new_ext, &end_ext);
+out:
 	return ret;
 }

@@ -668,7 +694,20 @@ mext_replace_branches(handle_t *handle, struct inode *orig_inode,
 	/* Loop for the donor extents */
 	while (1) {
 		/* The extent for donor must be found. */
-		BUG_ON(!dext || donor_off != le32_to_cpu(tmp_dext.ee_block));
+		if (!dext) {
+			ext4_error(donor_inode->i_sb, __func__,
+				   "The extent for donor must be found");
+			err = -EIO;
+			goto out;
+		} else if (donor_off != le32_to_cpu(tmp_dext.ee_block)) {
+			ext4_error(donor_inode->i_sb, __func__,
+				"Donor offset(%u) and the first block of donor "
+				"extent(%u) should be equal",
+				donor_off,
+				le32_to_cpu(tmp_dext.ee_block));
+			err = -EIO;
+			goto out;
+		}

 		/* Set donor extent to orig extent */
 		err = mext_leaf_block(handle, orig_inode,
@@ -1051,18 +1090,23 @@ mext_check_arguments(struct inode *orig_inode,
  * @inode1:	the inode structure
  * @inode2:	the inode structure
  *
- * Lock two inodes' i_mutex by i_ino order. This function is moved from
- * fs/inode.c.
+ * Lock two inodes' i_mutex by i_ino order.
+ * If inode1 or inode2 is NULL, return -EIO. Otherwise, return 0.
  */
-static void
+static int
 mext_inode_double_lock(struct inode *inode1, struct inode *inode2)
 {
-	if (inode1 == NULL || inode2 == NULL || inode1 == inode2) {
-		if (inode1)
-			mutex_lock(&inode1->i_mutex);
-		else if (inode2)
-			mutex_lock(&inode2->i_mutex);
-		return;
+	int ret = 0;
+
+	BUG_ON(inode1 == NULL && inode2 == NULL);
+
+	ret = mext_check_null_inode(inode1, inode2, __func__);
+	if (ret < 0)
+		goto out;
+
+	if (inode1 == inode2) {
+		mutex_lock(&inode1->i_mutex);
+		goto out;
 	}

 	if (inode1->i_ino < inode2->i_ino) {
@@ -1072,6 +1116,9 @@ mext_inode_double_lock(struct inode *inode1, struct inode *inode2)
 		mutex_lock_nested(&inode2->i_mutex, I_MUTEX_PARENT);
 		mutex_lock_nested(&inode1->i_mutex, I_MUTEX_CHILD);
 	}
+
+out:
+	return ret;
 }

 /**
@@ -1080,17 +1127,28 @@ mext_inode_double_lock(struct inode *inode1, struct inode *inode2)
  * @inode1:     the inode that is released first
  * @inode2:     the inode that is released second
  *
- * This function is moved from fs/inode.c.
+ * If inode1 or inode2 is NULL, return -EIO. Otherwise, return 0.
  */

-static void
+static int
 mext_inode_double_unlock(struct inode *inode1, struct inode *inode2)
 {
+	int ret = 0;
+
+	BUG_ON(inode1 == NULL && inode2 == NULL);
+
+	ret = mext_check_null_inode(inode1, inode2, __func__);
+	if (ret < 0)
+		goto out;
+
 	if (inode1)
 		mutex_unlock(&inode1->i_mutex);

 	if (inode2 && inode2 != inode1)
 		mutex_unlock(&inode2->i_mutex);
+
+out:
+	return ret;
 }

 /**
@@ -1147,21 +1205,23 @@ ext4_move_extents(struct file *o_filp, struct file *d_filp,
 	ext4_lblk_t block_end, seq_start, add_blocks, file_end, seq_blocks = 0;
 	ext4_lblk_t rest_blocks;
 	pgoff_t orig_page_offset = 0, seq_end_page;
-	int ret, depth, last_extent = 0;
+	int ret1, ret2, depth, last_extent = 0;
 	int blocks_per_page = PAGE_CACHE_SIZE >> orig_inode->i_blkbits;
 	int data_offset_in_page;
 	int block_len_in_page;
 	int uninit;

 	/* protect orig and donor against a truncate */
-	mext_inode_double_lock(orig_inode, donor_inode);
+	ret1 = mext_inode_double_lock(orig_inode, donor_inode);
+	if (ret1 < 0)
+		return ret1;

 	mext_double_down_read(orig_inode, donor_inode);
 	/* Check the filesystem environment whether move_extent can be done */
-	ret = mext_check_arguments(orig_inode, donor_inode, orig_start,
+	ret1 = mext_check_arguments(orig_inode, donor_inode, orig_start,
 					donor_start, &len, *moved_len);
 	mext_double_up_read(orig_inode, donor_inode);
-	if (ret)
+	if (ret1)
 		goto out2;

 	file_end = (i_size_read(orig_inode) - 1) >> orig_inode->i_blkbits;
@@ -1169,19 +1229,19 @@ ext4_move_extents(struct file *o_filp, struct file *d_filp,
 	if (file_end < block_end)
 		len -= block_end - file_end;

-	ret = get_ext_path(orig_inode, block_start, &orig_path);
+	ret1 = get_ext_path(orig_inode, block_start, &orig_path);
 	if (orig_path == NULL)
 		goto out2;

 	/* Get path structure to check the hole */
-	ret = get_ext_path(orig_inode, block_start, &holecheck_path);
+	ret1 = get_ext_path(orig_inode, block_start, &holecheck_path);
 	if (holecheck_path == NULL)
 		goto out;

 	depth = ext_depth(orig_inode);
 	ext_cur = holecheck_path[depth].p_ext;
 	if (ext_cur == NULL) {
-		ret = -EINVAL;
+		ret1 = -EINVAL;
 		goto out;
 	}

@@ -1194,13 +1254,13 @@ ext4_move_extents(struct file *o_filp, struct file *d_filp,
 		last_extent = mext_next_extent(orig_inode,
 					holecheck_path, &ext_cur);
 		if (last_extent < 0) {
-			ret = last_extent;
+			ret1 = last_extent;
 			goto out;
 		}
 		last_extent = mext_next_extent(orig_inode, orig_path,
 							&ext_dummy);
 		if (last_extent < 0) {
-			ret = last_extent;
+			ret1 = last_extent;
 			goto out;
 		}
 	}
@@ -1210,7 +1270,7 @@ ext4_move_extents(struct file *o_filp, struct file *d_filp,
 	if (le32_to_cpu(ext_cur->ee_block) > block_end) {
 		ext4_debug("ext4 move extent: The specified range of file "
 							"may be the hole\n");
-		ret = -EINVAL;
+		ret1 = -EINVAL;
 		goto out;
 	}

@@ -1230,7 +1290,7 @@ ext4_move_extents(struct file *o_filp, struct file *d_filp,
 		last_extent = mext_next_extent(orig_inode, holecheck_path,
 						&ext_cur);
 		if (last_extent < 0) {
-			ret = last_extent;
+			ret1 = last_extent;
 			break;
 		}
 		add_blocks = ext4_ext_get_actual_len(ext_cur);
@@ -1282,16 +1342,23 @@ ext4_move_extents(struct file *o_filp, struct file *d_filp,
 		while (orig_page_offset <= seq_end_page) {

 			/* Swap original branches with new branches */
-			ret = move_extent_per_page(o_filp, donor_inode,
+			ret1 = move_extent_per_page(o_filp, donor_inode,
 						orig_page_offset,
 						data_offset_in_page,
 						block_len_in_page, uninit);
-			if (ret < 0)
+			if (ret1 < 0)
 				goto out;
 			orig_page_offset++;
 			/* Count how many blocks we have exchanged */
 			*moved_len += block_len_in_page;
-			BUG_ON(*moved_len > len);
+			if (*moved_len > len) {
+				ext4_error(orig_inode->i_sb, __func__,
+					"We replaced blocks too much! "
+					"sum of replaced: %llu requested: %llu",
+					*moved_len, len);
+				ret1 = -EIO;
+				goto out;
+			}

 			data_offset_in_page = 0;
 			rest_blocks -= block_len_in_page;
@@ -1304,7 +1371,7 @@ ext4_move_extents(struct file *o_filp, struct file *d_filp,
 		/* Decrease buffer counter */
 		if (holecheck_path)
 			ext4_ext_drop_refs(holecheck_path);
-		ret = get_ext_path(orig_inode, seq_start, &holecheck_path);
+		ret1 = get_ext_path(orig_inode, seq_start, &holecheck_path);
 		if (holecheck_path == NULL)
 			break;
 		depth = holecheck_path->p_depth;
@@ -1312,7 +1379,7 @@ ext4_move_extents(struct file *o_filp, struct file *d_filp,
 		/* Decrease buffer counter */
 		if (orig_path)
 			ext4_ext_drop_refs(orig_path);
-		ret = get_ext_path(orig_inode, seq_start, &orig_path);
+		ret1 = get_ext_path(orig_inode, seq_start, &orig_path);
 		if (orig_path == NULL)
 			break;

@@ -1331,10 +1398,12 @@ out:
 		kfree(holecheck_path);
 	}
 out2:
-	mext_inode_double_unlock(orig_inode, donor_inode);
+	ret2 = mext_inode_double_unlock(orig_inode, donor_inode);

-	if (ret)
-		return ret;
+	if (ret1)
+		return ret1;
+	else if (ret2)
+		return ret2;

 	return 0;
 }
--
To unsubscribe from this list: send the line "unsubscribe linux-ext4" in
the body of a message to majordomo@vger.kernel.org
More majordomo info at  http://vger.kernel.org/majordomo-info.html

