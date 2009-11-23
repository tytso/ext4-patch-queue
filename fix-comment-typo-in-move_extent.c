ext4: fix spelling typos in move_extent.c

From: Akira Fujita <a-fujita@rs.jp.nec.com>

Fix a few spelling typos in move_extent.c

Signed-off-by: Akira Fujita <a-fujita@rs.jp.nec.co.jp>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
Patch 4/4
 fs/ext4/move_extent.c |    4 ++--
 1 files changed, 2 insertions(+), 2 deletions(-)

diff --git a/fs/ext4/move_extent.c b/fs/ext4/move_extent.c
index 2ca6aa3..5a106e0 100644
--- a/fs/ext4/move_extent.c
+++ b/fs/ext4/move_extent.c
@@ -568,7 +568,7 @@ out:
  * @tmp_oext:		the extent that will belong to the donor inode
  * @orig_off:		block offset of original inode
  * @donor_off:		block offset of donor inode
- * @max_count:		the maximun length of extents
+ * @max_count:		the maximum length of extents
  *
  * Return 0 on success, or a negative error value on failure.
  */
@@ -1073,7 +1073,7 @@ mext_check_arguments(struct inode *orig_inode,
 	}

 	if (!*len) {
-		ext4_debug("ext4 move extent: len shoudld not be 0 "
+		ext4_debug("ext4 move extent: len should not be 0 "
 			"[ino:orig %lu, donor %lu]\n", orig_inode->i_ino,
 			donor_inode->i_ino);
 		return -EINVAL;

