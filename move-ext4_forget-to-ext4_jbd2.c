ext4: move ext4_forget() to ext4_jbd2.c

The ext4_forget() function better belongs in ext4_jbd2.c.  This will
allow us to do some cleanup of the ext4_journal_revoke() and
ext4_journal_forget() functions, as well as giving us better error
reporting since we can report the caller of ext4_forget() when things
go wrong.

Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/ext4.h      |    2 -
 fs/ext4/ext4_jbd2.c |   56 +++++++++++++++++++++++++++++++++++++++++++++++++++
 fs/ext4/ext4_jbd2.h |    7 ++++++
 fs/ext4/inode.c     |   53 ------------------------------------------------
 4 files changed, 63 insertions(+), 55 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 05ce38b..57c4e03 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -1393,8 +1393,6 @@ extern int ext4_mb_get_buddy_cache_lock(struct super_block *, ext4_group_t);
 extern void ext4_mb_put_buddy_cache_lock(struct super_block *,
 						ext4_group_t, int);
 /* inode.c */
-int ext4_forget(handle_t *handle, int is_metadata, struct inode *inode,
-		struct buffer_head *bh, ext4_fsblk_t blocknr);
 struct buffer_head *ext4_getblk(handle_t *, struct inode *,
 						ext4_lblk_t, int, int *);
 struct buffer_head *ext4_bread(handle_t *, struct inode *,
diff --git a/fs/ext4/ext4_jbd2.c b/fs/ext4/ext4_jbd2.c
index 6a94099..913f857 100644
--- a/fs/ext4/ext4_jbd2.c
+++ b/fs/ext4/ext4_jbd2.c
@@ -4,6 +4,8 @@
 
 #include "ext4_jbd2.h"
 
+#include <trace/events/ext4.h>
+
 int __ext4_journal_get_undo_access(const char *where, handle_t *handle,
 				struct buffer_head *bh)
 {
@@ -64,6 +66,60 @@ int __ext4_journal_revoke(const char *where, handle_t *handle,
 	return err;
 }
 
+/*
+ * The ext4 forget function must perform a revoke if we are freeing data
+ * which has been journaled.  Metadata (eg. indirect blocks) must be
+ * revoked in all cases.
+ *
+ * "bh" may be NULL: a metadata block may have been freed from memory
+ * but there may still be a record of it in the journal, and that record
+ * still needs to be revoked.
+ *
+ * If the handle isn't valid we're not journaling, but we still need to
+ * call into ext4_journal_revoke() to put the buffer head.
+ */
+int __ext4_forget(const char *where, handle_t *handle, int is_metadata,
+		  struct inode *inode, struct buffer_head *bh,
+		  ext4_fsblk_t blocknr)
+{
+	int err;
+
+	might_sleep();
+
+	trace_ext4_forget(inode, is_metadata, blocknr);
+	BUFFER_TRACE(bh, "enter");
+
+	jbd_debug(4, "forgetting bh %p: is_metadata = %d, mode %o, "
+		  "data mode %x\n",
+		  bh, is_metadata, inode->i_mode,
+		  test_opt(inode->i_sb, DATA_FLAGS));
+
+	/* Never use the revoke function if we are doing full data
+	 * journaling: there is no need to, and a V1 superblock won't
+	 * support it.  Otherwise, only skip the revoke on un-journaled
+	 * data blocks. */
+
+	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_JOURNAL_DATA ||
+	    (!is_metadata && !ext4_should_journal_data(inode))) {
+		if (bh) {
+			BUFFER_TRACE(bh, "call jbd2_journal_forget");
+			return __ext4_journal_forget(where, handle, bh);
+		}
+		return 0;
+	}
+
+	/*
+	 * data!=journal && (is_metadata || should_journal_data(inode))
+	 */
+	BUFFER_TRACE(bh, "call ext4_journal_revoke");
+	err = __ext4_journal_revoke(where, handle, blocknr, bh);
+	if (err)
+		ext4_abort(inode->i_sb, __func__,
+			   "error %d when attempting revoke", err);
+	BUFFER_TRACE(bh, "exit");
+	return err;
+}
+
 int __ext4_journal_get_create_access(const char *where,
 				handle_t *handle, struct buffer_head *bh)
 {
diff --git a/fs/ext4/ext4_jbd2.h b/fs/ext4/ext4_jbd2.h
index a286598..dc0b34a 100644
--- a/fs/ext4/ext4_jbd2.h
+++ b/fs/ext4/ext4_jbd2.h
@@ -139,6 +139,10 @@ int __ext4_journal_forget(const char *where, handle_t *handle,
 int __ext4_journal_revoke(const char *where, handle_t *handle,
 				ext4_fsblk_t blocknr, struct buffer_head *bh);
 
+int __ext4_forget(const char *where, handle_t *handle, int is_metadata,
+		  struct inode *inode, struct buffer_head *bh,
+		  ext4_fsblk_t blocknr);
+
 int __ext4_journal_get_create_access(const char *where,
 				handle_t *handle, struct buffer_head *bh);
 
@@ -151,6 +155,9 @@ int __ext4_handle_dirty_metadata(const char *where, handle_t *handle,
 	__ext4_journal_get_write_access(__func__, (handle), (bh))
 #define ext4_journal_revoke(handle, blocknr, bh) \
 	__ext4_journal_revoke(__func__, (handle), (blocknr), (bh))
+#define ext4_forget(handle, is_metadata, inode, bh, block_nr) \
+	__ext4_forget(__func__, (handle), (is_metadata), (inode), (bh),\
+		      (block_nr))
 #define ext4_journal_get_create_access(handle, bh) \
 	__ext4_journal_get_create_access(__func__, (handle), (bh))
 #define ext4_journal_forget(handle, bh) \
diff --git a/fs/ext4/inode.c b/fs/ext4/inode.c
index 3673ec7..fa37f95 100644
--- a/fs/ext4/inode.c
+++ b/fs/ext4/inode.c
@@ -71,59 +71,6 @@ static int ext4_inode_is_fast_symlink(struct inode *inode)
 }
 
 /*
- * The ext4 forget function must perform a revoke if we are freeing data
- * which has been journaled.  Metadata (eg. indirect blocks) must be
- * revoked in all cases.
- *
- * "bh" may be NULL: a metadata block may have been freed from memory
- * but there may still be a record of it in the journal, and that record
- * still needs to be revoked.
- *
- * If the handle isn't valid we're not journaling, but we still need to
- * call into ext4_journal_revoke() to put the buffer head.
- */
-int ext4_forget(handle_t *handle, int is_metadata, struct inode *inode,
-		struct buffer_head *bh, ext4_fsblk_t blocknr)
-{
-	int err;
-
-	might_sleep();
-
-	trace_ext4_forget(inode, is_metadata, blocknr);
-	BUFFER_TRACE(bh, "enter");
-
-	jbd_debug(4, "forgetting bh %p: is_metadata = %d, mode %o, "
-		  "data mode %x\n",
-		  bh, is_metadata, inode->i_mode,
-		  test_opt(inode->i_sb, DATA_FLAGS));
-
-	/* Never use the revoke function if we are doing full data
-	 * journaling: there is no need to, and a V1 superblock won't
-	 * support it.  Otherwise, only skip the revoke on un-journaled
-	 * data blocks. */
-
-	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_JOURNAL_DATA ||
-	    (!is_metadata && !ext4_should_journal_data(inode))) {
-		if (bh) {
-			BUFFER_TRACE(bh, "call jbd2_journal_forget");
-			return ext4_journal_forget(handle, bh);
-		}
-		return 0;
-	}
-
-	/*
-	 * data!=journal && (is_metadata || should_journal_data(inode))
-	 */
-	BUFFER_TRACE(bh, "call ext4_journal_revoke");
-	err = ext4_journal_revoke(handle, blocknr, bh);
-	if (err)
-		ext4_abort(inode->i_sb, __func__,
-			   "error %d when attempting revoke", err);
-	BUFFER_TRACE(bh, "exit");
-	return err;
-}
-
-/*
  * Work out how many blocks we need to proceed with the next chunk of a
  * truncate transaction.
  */
