ext4: move ext4_ind_migrate() into migrate.c

From: Lukas Czerner <lczerner@redhat.com>

Move ext4_ind_migrate() into migrate.c file since it makes much more
sense and ext4_ext_migrate() is there as well.

Also fix tiny style problem - add spaces around "=" in "i=0".

Signed-off-by: Lukas Czerner <lczerner@redhat.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/ext4.h    |  2 +-
 fs/ext4/extents.c | 57 ------------------------------------------------------
 fs/ext4/migrate.c | 58 ++++++++++++++++++++++++++++++++++++++++++++++++++++++-
 3 files changed, 58 insertions(+), 59 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 12b5604..75b2326 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -2136,6 +2136,7 @@ extern long ext4_compat_ioctl(struct file *, unsigned int, unsigned long);
 
 /* migrate.c */
 extern int ext4_ext_migrate(struct inode *);
+extern int ext4_ind_migrate(struct inode *inode);
 
 /* namei.c */
 extern int ext4_dirent_csum_verify(struct inode *inode,
@@ -2625,7 +2626,6 @@ extern int ext4_find_delalloc_range(struct inode *inode,
 extern int ext4_find_delalloc_cluster(struct inode *inode, ext4_lblk_t lblk);
 extern int ext4_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
 			__u64 start, __u64 len);
-extern int ext4_ind_migrate(struct inode *inode);
 
 
 /* move_extent.c */
diff --git a/fs/ext4/extents.c b/fs/ext4/extents.c
index 34ba222..6fcb375 100644
--- a/fs/ext4/extents.c
+++ b/fs/ext4/extents.c
@@ -4724,60 +4724,3 @@ int ext4_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
 
 	return error;
 }
-
-/*
- * Migrate a simple extent-based inode to use the i_blocks[] array
- */
-int ext4_ind_migrate(struct inode *inode)
-{
-	struct ext4_extent_header	*eh;
-	struct ext4_super_block		*es = EXT4_SB(inode->i_sb)->s_es;
-	struct ext4_inode_info		*ei = EXT4_I(inode);
-	struct ext4_extent		*ex;
-	unsigned int			i, len;
-	ext4_fsblk_t			blk;
-	handle_t			*handle;
-	int				ret;
-
-	if (!EXT4_HAS_INCOMPAT_FEATURE(inode->i_sb,
-				       EXT4_FEATURE_INCOMPAT_EXTENTS) ||
-	    (!ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)))
-		return -EINVAL;
-
-	handle = ext4_journal_start(inode, EXT4_HT_MIGRATE, 1);
-	if (IS_ERR(handle))
-		return PTR_ERR(handle);
-
-	down_write(&EXT4_I(inode)->i_data_sem);
-	ret = ext4_ext_check_inode(inode);
-	if (ret)
-		goto errout;
-
-	eh = ext_inode_hdr(inode);
-	ex  = EXT_FIRST_EXTENT(eh);
-	if (ext4_blocks_count(es) > EXT4_MAX_BLOCK_FILE_PHYS ||
-	    eh->eh_depth != 0 || le16_to_cpu(eh->eh_entries) > 1) {
-		ret = -EOPNOTSUPP;
-		goto errout;
-	}
-	if (eh->eh_entries == 0)
-		blk = len = 0;
-	else {
-		len = le16_to_cpu(ex->ee_len);
-		blk = ext4_ext_pblock(ex);
-		if (len > EXT4_NDIR_BLOCKS) {
-			ret = -EOPNOTSUPP;
-			goto errout;
-		}
-	}
-
-	ext4_clear_inode_flag(inode, EXT4_INODE_EXTENTS);
-	memset(ei->i_data, 0, sizeof(ei->i_data));
-	for (i=0; i < len; i++)
-		ei->i_data[i] = cpu_to_le32(blk++);
-	ext4_mark_inode_dirty(handle, inode);
-errout:
-	ext4_journal_stop(handle);
-	up_write(&EXT4_I(inode)->i_data_sem);
-	return ret;
-}
diff --git a/fs/ext4/migrate.c b/fs/ext4/migrate.c
index 480acf4..d129a4d 100644
--- a/fs/ext4/migrate.c
+++ b/fs/ext4/migrate.c
@@ -426,7 +426,6 @@ static int free_ext_block(handle_t *handle, struct inode *inode)
 			return retval;
 	}
 	return retval;
-
 }
 
 int ext4_ext_migrate(struct inode *inode)
@@ -606,3 +605,60 @@ out:
 
 	return retval;
 }
+
+/*
+ * Migrate a simple extent-based inode to use the i_blocks[] array
+ */
+int ext4_ind_migrate(struct inode *inode)
+{
+	struct ext4_extent_header	*eh;
+	struct ext4_super_block		*es = EXT4_SB(inode->i_sb)->s_es;
+	struct ext4_inode_info		*ei = EXT4_I(inode);
+	struct ext4_extent		*ex;
+	unsigned int			i, len;
+	ext4_fsblk_t			blk;
+	handle_t			*handle;
+	int				ret;
+
+	if (!EXT4_HAS_INCOMPAT_FEATURE(inode->i_sb,
+				       EXT4_FEATURE_INCOMPAT_EXTENTS) ||
+	    (!ext4_test_inode_flag(inode, EXT4_INODE_EXTENTS)))
+		return -EINVAL;
+
+	handle = ext4_journal_start(inode, EXT4_HT_MIGRATE, 1);
+	if (IS_ERR(handle))
+		return PTR_ERR(handle);
+
+	down_write(&EXT4_I(inode)->i_data_sem);
+	ret = ext4_ext_check_inode(inode);
+	if (ret)
+		goto errout;
+
+	eh = ext_inode_hdr(inode);
+	ex  = EXT_FIRST_EXTENT(eh);
+	if (ext4_blocks_count(es) > EXT4_MAX_BLOCK_FILE_PHYS ||
+	    eh->eh_depth != 0 || le16_to_cpu(eh->eh_entries) > 1) {
+		ret = -EOPNOTSUPP;
+		goto errout;
+	}
+	if (eh->eh_entries == 0)
+		blk = len = 0;
+	else {
+		len = le16_to_cpu(ex->ee_len);
+		blk = ext4_ext_pblock(ex);
+		if (len > EXT4_NDIR_BLOCKS) {
+			ret = -EOPNOTSUPP;
+			goto errout;
+		}
+	}
+
+	ext4_clear_inode_flag(inode, EXT4_INODE_EXTENTS);
+	memset(ei->i_data, 0, sizeof(ei->i_data));
+	for (i=0; i < len; i++)
+		ei->i_data[i] = cpu_to_le32(blk++);
+	ext4_mark_inode_dirty(handle, inode);
+errout:
+	ext4_journal_stop(handle);
+	up_write(&EXT4_I(inode)->i_data_sem);
+	return ret;
+}
