ext4: Fix coding style in fs/ext4/move_extent.c

From: Steven Liu <lingjiujianke@gmail.com>

Making sure ee_block is initialized to zero to prevent gcc from
kvetching.  It's harmless (although it's not obvious that it's
harmless) from code inspection:

fs/ext4/move_extent.c:478: warning: 'start_ext.ee_block' may be used
uninitialized in this function

Thanks to Stefan Richter for first bringing this to the attention of
linux-ext4@vger.kernel.org.

Signed-off-by: LiuQi <lingjiujianke@gmail.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
Cc: Stefan Richter <stefanr@s5r6.in-berlin.de>
---
 fs/ext4/move_extent.c |    1 +
 1 files changed, 1 insertions(+), 0 deletions(-)

diff --git a/fs/ext4/move_extent.c b/fs/ext4/move_extent.c
index aa5fe28..b399358 100644
--- a/fs/ext4/move_extent.c
+++ b/fs/ext4/move_extent.c
@@ -481,6 +481,7 @@ mext_leaf_block(handle_t *handle, struct inode *orig_inode,
 	int depth = ext_depth(orig_inode);
 	int ret;

+	start_ext.ee_block = end_ext.ee_block = 0;
 	o_start = o_end = oext = orig_path[depth].p_ext;
 	oext_alen = ext4_ext_get_actual_len(oext);
 	start_ext.ee_len = end_ext.ee_len = 0;
-- 
1.6.0.3

