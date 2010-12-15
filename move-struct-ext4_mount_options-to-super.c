ext4: Move struct ext4_mount_options from ext4.h to super.c

Move the ext4_mount_options structure definition from ext4.h, since it
is only used in super.c.

Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/ext4.h  |   16 ----------------
 fs/ext4/super.c |   15 +++++++++++++++
 2 files changed, 15 insertions(+), 16 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 2d93620..ddae3c43 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -561,22 +561,6 @@ struct ext4_new_group_data {
 #define EXT4_IOC32_SETVERSION_OLD	FS_IOC32_SETVERSION
 #endif
 
-
-/*
- *  Mount options
- */
-struct ext4_mount_options {
-	unsigned long s_mount_opt;
-	uid_t s_resuid;
-	gid_t s_resgid;
-	unsigned long s_commit_interval;
-	u32 s_min_batch_time, s_max_batch_time;
-#ifdef CONFIG_QUOTA
-	int s_jquota_fmt;
-	char *s_qf_names[MAXQUOTAS];
-#endif
-};
-
 /* Max physical block we can addres w/o extents */
 #define EXT4_MAX_BLOCK_FILE_PHYS	0xFFFFFFFF
 
diff --git a/fs/ext4/super.c b/fs/ext4/super.c
index cf7d913..7aa3a79 100644
--- a/fs/ext4/super.c
+++ b/fs/ext4/super.c
@@ -4166,6 +4166,21 @@ static int ext4_unfreeze(struct super_block *sb)
 	return 0;
 }
 
+/*
+ * Structure to save mount options for ext4_remount's benefit
+ */
+struct ext4_mount_options {
+	unsigned long s_mount_opt;
+	uid_t s_resuid;
+	gid_t s_resgid;
+	unsigned long s_commit_interval;
+	u32 s_min_batch_time, s_max_batch_time;
+#ifdef CONFIG_QUOTA
+	int s_jquota_fmt;
+	char *s_qf_names[MAXQUOTAS];
+#endif
+};
+
 static int ext4_remount(struct super_block *sb, int *flags, char *data)
 {
 	struct ext4_super_block *es;
