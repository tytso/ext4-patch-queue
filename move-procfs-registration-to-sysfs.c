ext4: move procfs registration code to fs/ext4/sysfs.c

This allows us to refactor the procfs code, which saves a bit of
compiled space.  More importantly it isolates most of the procfs
support code into a single file, so it's easier to #ifdef it out if
the proc file system has been disabled.

Signed-off-by: Theodore Ts'o <tytso@mit.edu>
---
 fs/ext4/ext4.h           |  3 +++
 fs/ext4/extents_status.c | 60 ++----------------------------------------------------------
 fs/ext4/extents_status.h |  2 ++
 fs/ext4/mballoc.c        |  9 +--------
 fs/ext4/super.c          | 41 +++--------------------------------------
 fs/ext4/sysfs.c          | 68 ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++--
 6 files changed, 77 insertions(+), 106 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 76daccf..766b7f7 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -2245,6 +2245,7 @@ extern int ext4_init_inode_table(struct super_block *sb,
 extern void ext4_end_bitmap_read(struct buffer_head *bh, int uptodate);
 
 /* mballoc.c */
+extern const struct file_operations ext4_seq_mb_groups_fops;
 extern long ext4_mb_stats;
 extern long ext4_mb_max_to_scan;
 extern int ext4_mb_init(struct super_block *);
@@ -2372,6 +2373,7 @@ extern int ext4_group_extend(struct super_block *sb,
 extern int ext4_resize_fs(struct super_block *sb, ext4_fsblk_t n_blocks_count);
 
 /* super.c */
+extern int ext4_seq_options_show(struct seq_file *seq, void *offset);
 extern int ext4_calculate_overhead(struct super_block *sb);
 extern void ext4_superblock_csum_set(struct super_block *sb);
 extern void *ext4_kvmalloc(size_t size, gfp_t flags);
@@ -2905,6 +2907,7 @@ extern const struct inode_operations ext4_fast_symlink_inode_operations;
 
 /* sysfs.c */
 extern int ext4_register_sysfs(struct super_block *sb);
+extern void ext4_unregister_sysfs(struct super_block *sb);
 extern int __init ext4_init_sysfs(void);
 extern void ext4_exit_sysfs(void);
 
diff --git a/fs/ext4/extents_status.c b/fs/ext4/extents_status.c
index 26724ae..ac748b3 100644
--- a/fs/ext4/extents_status.c
+++ b/fs/ext4/extents_status.c
@@ -1089,20 +1089,9 @@ static unsigned long ext4_es_scan(struct shrinker *shrink,
 	return nr_shrunk;
 }
 
-static void *ext4_es_seq_shrinker_info_start(struct seq_file *seq, loff_t *pos)
+int ext4_seq_es_shrinker_info_show(struct seq_file *seq, void *v)
 {
-	return *pos ? NULL : SEQ_START_TOKEN;
-}
-
-static void *
-ext4_es_seq_shrinker_info_next(struct seq_file *seq, void *v, loff_t *pos)
-{
-	return NULL;
-}
-
-static int ext4_es_seq_shrinker_info_show(struct seq_file *seq, void *v)
-{
-	struct ext4_sb_info *sbi = seq->private;
+	struct ext4_sb_info *sbi = EXT4_SB((struct super_block *) seq->private);
 	struct ext4_es_stats *es_stats = &sbi->s_es_stats;
 	struct ext4_inode_info *ei, *max = NULL;
 	unsigned int inode_cnt = 0;
@@ -1143,45 +1132,6 @@ static int ext4_es_seq_shrinker_info_show(struct seq_file *seq, void *v)
 	return 0;
 }
 
-static void ext4_es_seq_shrinker_info_stop(struct seq_file *seq, void *v)
-{
-}
-
-static const struct seq_operations ext4_es_seq_shrinker_info_ops = {
-	.start = ext4_es_seq_shrinker_info_start,
-	.next  = ext4_es_seq_shrinker_info_next,
-	.stop  = ext4_es_seq_shrinker_info_stop,
-	.show  = ext4_es_seq_shrinker_info_show,
-};
-
-static int
-ext4_es_seq_shrinker_info_open(struct inode *inode, struct file *file)
-{
-	int ret;
-
-	ret = seq_open(file, &ext4_es_seq_shrinker_info_ops);
-	if (!ret) {
-		struct seq_file *m = file->private_data;
-		m->private = PDE_DATA(inode);
-	}
-
-	return ret;
-}
-
-static int
-ext4_es_seq_shrinker_info_release(struct inode *inode, struct file *file)
-{
-	return seq_release(inode, file);
-}
-
-static const struct file_operations ext4_es_seq_shrinker_info_fops = {
-	.owner		= THIS_MODULE,
-	.open		= ext4_es_seq_shrinker_info_open,
-	.read		= seq_read,
-	.llseek		= seq_lseek,
-	.release	= ext4_es_seq_shrinker_info_release,
-};
-
 int ext4_es_register_shrinker(struct ext4_sb_info *sbi)
 {
 	int err;
@@ -1210,10 +1160,6 @@ int ext4_es_register_shrinker(struct ext4_sb_info *sbi)
 	if (err)
 		goto err2;
 
-	if (sbi->s_proc)
-		proc_create_data("es_shrinker_info", S_IRUGO, sbi->s_proc,
-				 &ext4_es_seq_shrinker_info_fops, sbi);
-
 	return 0;
 
 err2:
@@ -1225,8 +1171,6 @@ err1:
 
 void ext4_es_unregister_shrinker(struct ext4_sb_info *sbi)
 {
-	if (sbi->s_proc)
-		remove_proc_entry("es_shrinker_info", sbi->s_proc);
 	percpu_counter_destroy(&sbi->s_es_stats.es_stats_all_cnt);
 	percpu_counter_destroy(&sbi->s_es_stats.es_stats_shk_cnt);
 	unregister_shrinker(&sbi->s_es_shrinker);
diff --git a/fs/ext4/extents_status.h b/fs/ext4/extents_status.h
index 691b526..f7aa24f 100644
--- a/fs/ext4/extents_status.h
+++ b/fs/ext4/extents_status.h
@@ -172,4 +172,6 @@ static inline void ext4_es_store_pblock_status(struct extent_status *es,
 extern int ext4_es_register_shrinker(struct ext4_sb_info *sbi);
 extern void ext4_es_unregister_shrinker(struct ext4_sb_info *sbi);
 
+extern int ext4_seq_es_shrinker_info_show(struct seq_file *seq, void *v);
+
 #endif /* _EXT4_EXTENTS_STATUS_H */
diff --git a/fs/ext4/mballoc.c b/fs/ext4/mballoc.c
index 34b610e..b0f7ee5 100644
--- a/fs/ext4/mballoc.c
+++ b/fs/ext4/mballoc.c
@@ -2333,7 +2333,7 @@ static int ext4_mb_seq_groups_open(struct inode *inode, struct file *file)
 
 }
 
-static const struct file_operations ext4_mb_seq_groups_fops = {
+const struct file_operations ext4_seq_mb_groups_fops = {
 	.owner		= THIS_MODULE,
 	.open		= ext4_mb_seq_groups_open,
 	.read		= seq_read,
@@ -2661,10 +2661,6 @@ int ext4_mb_init(struct super_block *sb)
 	if (ret != 0)
 		goto out_free_locality_groups;
 
-	if (sbi->s_proc)
-		proc_create_data("mb_groups", S_IRUGO, sbi->s_proc,
-				 &ext4_mb_seq_groups_fops, sb);
-
 	return 0;
 
 out_free_locality_groups:
@@ -2705,9 +2701,6 @@ int ext4_mb_release(struct super_block *sb)
 	struct ext4_sb_info *sbi = EXT4_SB(sb);
 	struct kmem_cache *cachep = get_groupinfo_cache(sb->s_blocksize_bits);
 
-	if (sbi->s_proc)
-		remove_proc_entry("mb_groups", sbi->s_proc);
-
 	if (sbi->s_group_info) {
 		for (i = 0; i < ngroups; i++) {
 			grinfo = ext4_get_group_info(sb, i);
diff --git a/fs/ext4/super.c b/fs/ext4/super.c
index 4a574fb..7ef3fa5 100644
--- a/fs/ext4/super.c
+++ b/fs/ext4/super.c
@@ -34,7 +34,6 @@
 #include <linux/namei.h>
 #include <linux/quotaops.h>
 #include <linux/seq_file.h>
-#include <linux/proc_fs.h>
 #include <linux/ctype.h>
 #include <linux/log2.h>
 #include <linux/crc16.h>
@@ -54,7 +53,6 @@
 #define CREATE_TRACE_POINTS
 #include <trace/events/ext4.h>
 
-static struct proc_dir_entry *ext4_proc_root;
 static struct ext4_lazy_init *ext4_li_info;
 static struct mutex ext4_li_mtx;
 static int ext4_mballoc_ready;
@@ -797,6 +795,7 @@ static void ext4_put_super(struct super_block *sb)
 			ext4_abort(sb, "Couldn't clean up the journal");
 	}
 
+	ext4_unregister_sysfs(sb);
 	ext4_es_unregister_shrinker(sbi);
 	del_timer_sync(&sbi->s_err_report);
 	ext4_release_system_zone(sb);
@@ -811,12 +810,6 @@ static void ext4_put_super(struct super_block *sb)
 	if (!(sb->s_flags & MS_RDONLY))
 		ext4_commit_super(sb, 1);
 
-	if (sbi->s_proc) {
-		remove_proc_entry("options", sbi->s_proc);
-		remove_proc_entry(sb->s_id, ext4_proc_root);
-	}
-	kobject_del(&sbi->s_kobj);
-
 	for (i = 0; i < sbi->s_gdb_count; i++)
 		brelse(sbi->s_group_desc[i]);
 	kvfree(sbi->s_group_desc);
@@ -1877,7 +1870,7 @@ static int ext4_show_options(struct seq_file *seq, struct dentry *root)
 	return _ext4_show_options(seq, root->d_sb, 0);
 }
 
-static int options_seq_show(struct seq_file *seq, void *offset)
+int ext4_seq_options_show(struct seq_file *seq, void *offset)
 {
 	struct super_block *sb = seq->private;
 	int rc;
@@ -1888,19 +1881,6 @@ static int options_seq_show(struct seq_file *seq, void *offset)
 	return rc;
 }
 
-static int options_open_fs(struct inode *inode, struct file *file)
-{
-	return single_open(file, options_seq_show, PDE_DATA(inode));
-}
-
-static const struct file_operations ext4_seq_options_fops = {
-	.owner = THIS_MODULE,
-	.open = options_open_fs,
-	.read = seq_read,
-	.llseek = seq_lseek,
-	.release = single_release,
-};
-
 static int ext4_setup_super(struct super_block *sb, struct ext4_super_block *es,
 			    int read_only)
 {
@@ -3616,13 +3596,6 @@ static int ext4_fill_super(struct super_block *sb, void *data, int silent)
 		goto failed_mount;
 	}
 
-	if (ext4_proc_root)
-		sbi->s_proc = proc_mkdir(sb->s_id, ext4_proc_root);
-
-	if (sbi->s_proc)
-		proc_create_data("options", S_IRUGO, sbi->s_proc,
-				 &ext4_seq_options_fops, sb);
-
 	bgl_lock_init(sbi->s_blockgroup_lock);
 
 	for (i = 0; i < db_count; i++) {
@@ -3960,7 +3933,7 @@ cantfind_ext4:
 
 #ifdef CONFIG_QUOTA
 failed_mount8:
-	kobject_del(&sbi->s_kobj);
+	ext4_unregister_sysfs(sb);
 #endif
 failed_mount7:
 	ext4_unregister_li_request(sb);
@@ -4000,10 +3973,6 @@ failed_mount2:
 failed_mount:
 	if (sbi->s_chksum_driver)
 		crypto_free_shash(sbi->s_chksum_driver);
-	if (sbi->s_proc) {
-		remove_proc_entry("options", sbi->s_proc);
-		remove_proc_entry(sb->s_id, ext4_proc_root);
-	}
 #ifdef CONFIG_QUOTA
 	for (i = 0; i < EXT4_MAXQUOTAS; i++)
 		kfree(sbi->s_qf_names[i]);
@@ -5264,7 +5233,6 @@ static int __init ext4_init_fs(void)
 	err = ext4_init_system_zone();
 	if (err)
 		goto out4;
-	ext4_proc_root = proc_mkdir("fs/ext4", NULL);
 
 	err = ext4_init_sysfs();
 	if (err)
@@ -5295,8 +5263,6 @@ out1:
 out2:
 	ext4_exit_sysfs();
 out3:
-	if (ext4_proc_root)
-		remove_proc_entry("fs/ext4", NULL);
 	ext4_exit_system_zone();
 out4:
 	ext4_exit_pageio();
@@ -5316,7 +5282,6 @@ static void __exit ext4_exit_fs(void)
 	destroy_inodecache();
 	ext4_exit_mballoc();
 	ext4_exit_sysfs();
-	remove_proc_entry("fs/ext4", NULL);
 	ext4_exit_system_zone();
 	ext4_exit_pageio();
 	ext4_exit_es();
diff --git a/fs/ext4/sysfs.c b/fs/ext4/sysfs.c
index b8533440..c945efb 100644
--- a/fs/ext4/sysfs.c
+++ b/fs/ext4/sysfs.c
@@ -15,6 +15,9 @@
 #include "ext4.h"
 #include "ext4_jbd2.h"
 
+static const char *proc_dirname = "fs/ext4";
+static struct proc_dir_entry *ext4_proc_root;
+
 struct ext4_attr {
 	struct attribute attr;
 	ssize_t (*show)(struct ext4_attr *, struct ext4_sb_info *, char *);
@@ -352,14 +355,71 @@ static struct kobject ext4_feat = {
 	.kset	= &ext4_kset,
 };
 
+#define PROC_FILE_SHOW_DEFN(name) \
+static int name##_open(struct inode *inode, struct file *file) \
+{ \
+	return single_open(file, ext4_seq_##name##_show, PDE_DATA(inode)); \
+} \
+\
+const struct file_operations ext4_seq_##name##_fops = { \
+	.owner		= THIS_MODULE, \
+	.open		= name##_open, \
+	.read		= seq_read, \
+	.llseek		= seq_lseek, \
+	.release	= single_release, \
+}
+
+#define PROC_FILE_LIST(name) \
+	{ __stringify(name), &ext4_seq_##name##_fops }
+
+PROC_FILE_SHOW_DEFN(es_shrinker_info);
+PROC_FILE_SHOW_DEFN(options);
+
+static struct ext4_proc_files {
+	const char *name;
+	const struct file_operations *fops;
+} proc_files[] = {
+	PROC_FILE_LIST(options),
+	PROC_FILE_LIST(es_shrinker_info),
+	PROC_FILE_LIST(mb_groups),
+	{ NULL, NULL },
+};
+
 int ext4_register_sysfs(struct super_block *sb)
 {
 	struct ext4_sb_info *sbi = EXT4_SB(sb);
+	struct ext4_proc_files *p;
+	int err;
 
 	sbi->s_kobj.kset = &ext4_kset;
 	init_completion(&sbi->s_kobj_unregister);
-	return kobject_init_and_add(&sbi->s_kobj, &ext4_sb_ktype, NULL,
-				    "%s", sb->s_id);
+	err = kobject_init_and_add(&sbi->s_kobj, &ext4_sb_ktype, NULL,
+				   "%s", sb->s_id);
+	if (err)
+		return err;
+
+	if (ext4_proc_root)
+		sbi->s_proc = proc_mkdir(sb->s_id, ext4_proc_root);
+
+	if (sbi->s_proc) {
+		for (p = proc_files; p->name; p++)
+			proc_create_data(p->name, S_IRUGO, sbi->s_proc,
+					 p->fops, sb);
+	}
+	return 0;
+}
+
+void ext4_unregister_sysfs(struct super_block *sb)
+{
+	struct ext4_sb_info *sbi = EXT4_SB(sb);
+	struct ext4_proc_files *p;
+
+	if (sbi->s_proc) {
+		for (p = proc_files; p->name; p++)
+			remove_proc_entry(p->name, sbi->s_proc);
+		remove_proc_entry(sb->s_id, ext4_proc_root);
+	}
+	kobject_del(&sbi->s_kobj);
 }
 
 int __init ext4_init_sysfs(void)
@@ -376,6 +436,8 @@ int __init ext4_init_sysfs(void)
 				   NULL, "features");
 	if (ret)
 		kset_unregister(&ext4_kset);
+	else
+		ext4_proc_root = proc_mkdir(proc_dirname, NULL);
 	return ret;
 }
 
@@ -383,5 +445,7 @@ void ext4_exit_sysfs(void)
 {
 	kobject_put(&ext4_feat);
 	kset_unregister(&ext4_kset);
+	remove_proc_entry(proc_dirname, NULL);
+	ext4_proc_root = NULL;
 }
 
