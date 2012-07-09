ext4: fix out-of-date comments in extents.c

From: HaiboLiu <HaiboLiu6@gmail.com>

In this patch, ext4_ext_try_to_merge has been change to merge 
an extent both left and right.  So we need to update the comment
in here.

Signed-off-by: HaiboLiu <HaiboLiu6@gmail.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>

---
v1->v2: update the subject and the patch comment.

fs/ext4/extents.c |    3 +--
 1 file changed, 1 insertion(+), 2 deletions(-)

diff --git a/fs/ext4/extents.c b/fs/ext4/extents.c
index 74f23c2..db671b9 100644
--- a/fs/ext4/extents.c
+++ b/fs/ext4/extents.c
@@ -1807,11 +1807,10 @@ has_space:
 	nearex->ee_len = newext->ee_len;
 
 merge:
-	/* try to merge extents to the right */
+	/* try to merge extents */
 	if (!(flag & EXT4_GET_BLOCKS_PRE_IO))
 		ext4_ext_try_to_merge(inode, path, nearex);
 
-	/* try to merge extents to the left */
 
 	/* time to correct all indexes above */
 	err = ext4_ext_correct_indexes(handle, inode, path);
