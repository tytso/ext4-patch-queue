ext4: move __ext4_check_blockref to block_validity.c

In preparation for moving the indirect functions to a separate file,
move __ext4_check_blockref() to block_validity.c and rename it to
ext4_check_blockref() which is exported as globally visible function.

Also, rename the cpp macro ext4_check_inode_blockref() to
ext4_ind_check_inode(), to make it clear that it is only valid for use
with non-extent mapped inodes.

Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/block_validity.c |   20 ++++++++++++++++++++
 fs/ext4/ext4.h           |   15 +++++++++++++++
 fs/ext4/inode.c          |   35 +----------------------------------
 3 files changed, 36 insertions(+), 34 deletions(-)

diff --git a/fs/ext4/block_validity.c b/fs/ext4/block_validity.c
index fac90f3..af103be 100644
--- a/fs/ext4/block_validity.c
+++ b/fs/ext4/block_validity.c
@@ -246,3 +246,23 @@ int ext4_data_block_valid(struct ext4_sb_info *sbi, ext4_fsblk_t start_blk,
 	return 1;
 }
 
+int ext4_check_blockref(const char *function, unsigned int line,
+			struct inode *inode, __le32 *p, unsigned int max)
+{
+	struct ext4_super_block *es = EXT4_SB(inode->i_sb)->s_es;
+	__le32 *bref = p;
+	unsigned int blk;
+
+	while (bref < p+max) {
+		blk = le32_to_cpu(*bref++);
+		if (blk &&
+		    unlikely(!ext4_data_block_valid(EXT4_SB(inode->i_sb),
+						    blk, 1))) {
+			es->s_last_error_block = cpu_to_le64(blk);
+			ext4_error_inode(inode, function, line, blk,
+					 "invalid block");
+			return -EIO;
+		}
+	}
+	return 0;
+}
diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 8532dd4..82ba7eb 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -2125,6 +2125,19 @@ static inline void ext4_mark_super_dirty(struct super_block *sb)
 }
 
 /*
+ * Block validity checking
+ */
+#define ext4_check_indirect_blockref(inode, bh)				\
+	ext4_check_blockref(__func__, __LINE__, inode,			\
+			    (__le32 *)(bh)->b_data,			\
+			    EXT4_ADDR_PER_BLOCK((inode)->i_sb))
+
+#define ext4_ind_check_inode(inode)					\
+	ext4_check_blockref(__func__, __LINE__, inode,			\
+			    EXT4_I(inode)->i_data,			\
+			    EXT4_NDIR_BLOCKS)
+
+/*
  * Inodes and files operations
  */
 
@@ -2153,6 +2166,8 @@ extern void ext4_exit_system_zone(void);
 extern int ext4_data_block_valid(struct ext4_sb_info *sbi,
 				 ext4_fsblk_t start_blk,
 				 unsigned int count);
+extern int ext4_check_blockref(const char *, unsigned int,
+			       struct inode *, __le32 *, unsigned int);
 
 /* extents.c */
 extern int ext4_ext_tree_init(handle_t *handle, struct inode *);
diff --git a/fs/ext4/inode.c b/fs/ext4/inode.c
index 6c1d28e..3dca526 100644
--- a/fs/ext4/inode.c
+++ b/fs/ext4/inode.c
@@ -360,39 +360,6 @@ static int ext4_block_to_path(struct inode *inode,
 	return n;
 }
 
-static int __ext4_check_blockref(const char *function, unsigned int line,
-				 struct inode *inode,
-				 __le32 *p, unsigned int max)
-{
-	struct ext4_super_block *es = EXT4_SB(inode->i_sb)->s_es;
-	__le32 *bref = p;
-	unsigned int blk;
-
-	while (bref < p+max) {
-		blk = le32_to_cpu(*bref++);
-		if (blk &&
-		    unlikely(!ext4_data_block_valid(EXT4_SB(inode->i_sb),
-						    blk, 1))) {
-			es->s_last_error_block = cpu_to_le64(blk);
-			ext4_error_inode(inode, function, line, blk,
-					 "invalid block");
-			return -EIO;
-		}
-	}
-	return 0;
-}
-
-
-#define ext4_check_indirect_blockref(inode, bh)                         \
-	__ext4_check_blockref(__func__, __LINE__, inode,		\
-			      (__le32 *)(bh)->b_data,			\
-			      EXT4_ADDR_PER_BLOCK((inode)->i_sb))
-
-#define ext4_check_inode_blockref(inode)                                \
-	__ext4_check_blockref(__func__, __LINE__, inode,		\
-			      EXT4_I(inode)->i_data,			\
-			      EXT4_NDIR_BLOCKS)
-
 /**
  *	ext4_get_branch - read the chain of indirect blocks leading to data
  *	@inode: inode in question
@@ -5010,7 +4977,7 @@ struct inode *ext4_iget(struct super_block *sb, unsigned long ino)
 		   (S_ISLNK(inode->i_mode) &&
 		    !ext4_inode_is_fast_symlink(inode))) {
 		/* Validate block references which are part of inode */
-		ret = ext4_check_inode_blockref(inode);
+		ret = ext4_ind_check_inode(inode);
 	}
 	if (ret)
 		goto bad_inode;
