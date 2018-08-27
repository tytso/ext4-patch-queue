ext4: add nonstring annotations to ext4.h

This suppresses some false positives in gcc 8's -Wstringop-truncation

Suggested by Miguel Ojeda (hopefully the __nonstring definition will
eventually get accepted in the compiler-gcc.h header file).

Signed-off-by: Theodore Ts'o <tytso@mit.edu>
Cc: Miguel Ojeda <miguel.ojeda.sandonis@gmail.com>
---
 fs/ext4/ext4.h | 17 ++++++++++++++---
 1 file changed, 14 insertions(+), 3 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 1fc013f3d944..249bcee4d7b2 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -43,6 +43,17 @@
 #define __FS_HAS_ENCRYPTION IS_ENABLED(CONFIG_EXT4_FS_ENCRYPTION)
 #include <linux/fscrypt.h>
 
+#include <linux/compiler.h>
+
+/* Until this gets included into linux/compiler-gcc.h */
+#ifndef __nonstring
+#if defined(GCC_VERSION) && (GCC_VERSION >= 80000)
+#define __nonstring __attribute__((nonstring))
+#else
+#define __nonstring
+#endif
+#endif
+
 /*
  * The fourth extended filesystem constants/structures
  */
@@ -1226,7 +1237,7 @@ struct ext4_super_block {
 	__le32	s_feature_ro_compat;	/* readonly-compatible feature set */
 /*68*/	__u8	s_uuid[16];		/* 128-bit uuid for volume */
 /*78*/	char	s_volume_name[16];	/* volume name */
-/*88*/	char	s_last_mounted[64];	/* directory where last mounted */
+/*88*/	char	s_last_mounted[64] __nonstring;	/* directory where last mounted */
 /*C8*/	__le32	s_algorithm_usage_bitmap; /* For compression */
 	/*
 	 * Performance hints.  Directory preallocation should only
@@ -1277,13 +1288,13 @@ struct ext4_super_block {
 	__le32	s_first_error_time;	/* first time an error happened */
 	__le32	s_first_error_ino;	/* inode involved in first error */
 	__le64	s_first_error_block;	/* block involved of first error */
-	__u8	s_first_error_func[32];	/* function where the error happened */
+	__u8	s_first_error_func[32] __nonstring;	/* function where the error happened */
 	__le32	s_first_error_line;	/* line number where error happened */
 	__le32	s_last_error_time;	/* most recent time of an error */
 	__le32	s_last_error_ino;	/* inode involved in last error */
 	__le32	s_last_error_line;	/* line number where error happened */
 	__le64	s_last_error_block;	/* block involved of last error */
-	__u8	s_last_error_func[32];	/* function where the error happened */
+	__u8	s_last_error_func[32] __nonstring;	/* function where the error happened */
 #define EXT4_S_ERR_END offsetof(struct ext4_super_block, s_mount_opts)
 	__u8	s_mount_opts[64];
 	__le32	s_usr_quota_inum;	/* inode for tracking user quota */
