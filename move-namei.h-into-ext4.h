ext4: Move fs/ext4/namei.h into ext4.h

The fs/ext4/namei.h header file had only a single function
declaration, and should have never been a standalone file.  Move it
into ext4.h, where should have been from the beginning.

Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/ext4.h  |    1 +
 fs/ext4/namei.c |    1 -
 fs/ext4/namei.h |    8 --------
 fs/ext4/super.c |    1 -
 4 files changed, 1 insertions(+), 10 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 3b7223d..13238dc 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -1595,6 +1595,7 @@ extern const struct file_operations ext4_file_operations;
 /* namei.c */
 extern const struct inode_operations ext4_dir_inode_operations;
 extern const struct inode_operations ext4_special_inode_operations;
+extern struct dentry *ext4_get_parent(struct dentry *child);
 
 /* symlink.c */
 extern const struct inode_operations ext4_symlink_inode_operations;
diff --git a/fs/ext4/namei.c b/fs/ext4/namei.c
index 8018e49..c9690b2 100644
--- a/fs/ext4/namei.c
+++ b/fs/ext4/namei.c
@@ -37,7 +37,6 @@
 #include "ext4.h"
 #include "ext4_jbd2.h"
 
-#include "namei.h"
 #include "xattr.h"
 #include "acl.h"
 
diff --git a/fs/ext4/namei.h b/fs/ext4/namei.h
deleted file mode 100644
index 5e4dfff..0000000
--- a/fs/ext4/namei.h
+++ /dev/null
@@ -1,8 +0,0 @@
-/*  linux/fs/ext4/namei.h
- *
- * Copyright (C) 2005 Simtec Electronics
- *	Ben Dooks <ben@simtec.co.uk>
- *
-*/
-
-extern struct dentry *ext4_get_parent(struct dentry *child);
diff --git a/fs/ext4/super.c b/fs/ext4/super.c
index 1fbf090..d79e1c4 100644
--- a/fs/ext4/super.c
+++ b/fs/ext4/super.c
@@ -46,7 +46,6 @@
 #include "ext4_jbd2.h"
 #include "xattr.h"
 #include "acl.h"
-#include "namei.h"
 #include "group.h"
 
 struct proc_dir_entry *ext4_proc_root;
