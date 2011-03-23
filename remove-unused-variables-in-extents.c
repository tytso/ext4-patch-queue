ext4: unused variables cleanup in fs/ext4/extents.c

From: Sergey Senozhatsky <sergey.senozhatsky@gmail.com>

ext4 extents cleanup:

  . remove unused `*ex' from check_eofblocks_fl
  . remove unused `*eh' from ext4_ext_map_blocks


Signed-off-by: Sergey Senozhatsky <sergey.senozhatsky@gmail.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---

 fs/ext4/extents.c |    5 +----
 1 files changed, 1 insertions(+), 4 deletions(-)

diff --git a/fs/ext4/extents.c b/fs/ext4/extents.c
index 7516fb9..d89a448 100644
--- a/fs/ext4/extents.c
+++ b/fs/ext4/extents.c
@@ -3108,14 +3108,13 @@ static int check_eofblocks_fl(handle_t *handle, struct inode *inode,
 {
 	int i, depth;
 	struct ext4_extent_header *eh;
-	struct ext4_extent *ex, *last_ex;
+	struct ext4_extent *last_ex;
 
 	if (!ext4_test_inode_flag(inode, EXT4_INODE_EOFBLOCKS))
 		return 0;
 
 	depth = ext_depth(inode);
 	eh = path[depth].p_hdr;
-	ex = path[depth].p_ext;
 
 	if (unlikely(!eh->eh_entries)) {
 		EXT4_ERROR_INODE(inode, "eh->eh_entries == 0 and "
@@ -3295,7 +3294,6 @@ int ext4_ext_map_blocks(handle_t *handle, struct inode *inode,
 			struct ext4_map_blocks *map, int flags)
 {
 	struct ext4_ext_path *path = NULL;
-	struct ext4_extent_header *eh;
 	struct ext4_extent newex, *ex;
 	ext4_fsblk_t newblock;
 	int err = 0, depth, ret;
@@ -3352,7 +3350,6 @@ int ext4_ext_map_blocks(handle_t *handle, struct inode *inode,
 		err = -EIO;
 		goto out2;
 	}
-	eh = path[depth].p_hdr;
 
 	ex = path[depth].p_ext;
 	if (ex) {


