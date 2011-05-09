ext4: remove redundant #ifdef in super.c

From: Amerigo Wang <amwang@redhat.com>

There is already an #ifdef CONFIG_QUOTA some lines above,
so this one is totally useless.

Signed-off-by: WANG Cong <amwang@redhat.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
diff --git a/fs/ext4/super.c b/fs/ext4/super.c
index 22546ad..a5a73ac 100644
--- a/fs/ext4/super.c
+++ b/fs/ext4/super.c
@@ -1170,9 +1170,7 @@ static ssize_t ext4_quota_write(struct super_block *sb, int type,
 				const char *data, size_t len, loff_t off);
 
 static const struct dquot_operations ext4_quota_operations = {
-#ifdef CONFIG_QUOTA
 	.get_reserved_space = ext4_get_reserved_space,
-#endif
 	.write_dquot	= ext4_write_dquot,
 	.acquire_dquot	= ext4_acquire_dquot,
 	.release_dquot	= ext4_release_dquot,

