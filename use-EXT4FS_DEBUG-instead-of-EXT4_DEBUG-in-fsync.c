ext4: use EXT4FS_DEBUG instead of EXT4_DEBUG in fsync.c

From: Tao Ma <boyu.mt@taobao.com>

We have EXT4FS_DEBUG for some old debug and CONFIG_EXT4_DEBUG
for the new mballoc debug, but there isn't any EXT4_DEBUG.

As CONFIG_EXT4_DEBUG seems to be only used in mballoc, use
EXT4FS_DEBUG in fsync.c.

[ It doesn't really matter; although I'm including this commit for
  consistency's sake.  The whole point of the #ifdef's is to disable
  the debugging code.  In general you're not going to want to enable
  all of the code protected by EXT4FS_DEBUG at the same time.  -- Ted ]

Signed-off-by: Tao Ma <boyu.mt@taobao.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/fsync.c |    2 +-
 1 files changed, 1 insertions(+), 1 deletions(-)

diff --git a/fs/ext4/fsync.c b/fs/ext4/fsync.c
index e9473cb..60fe572 100644
--- a/fs/ext4/fsync.c
+++ b/fs/ext4/fsync.c
@@ -36,7 +36,7 @@
 
 static void dump_completed_IO(struct inode * inode)
 {
-#ifdef	EXT4_DEBUG
+#ifdef	EXT4FS_DEBUG
 	struct list_head *cur, *before, *after;
 	ext4_io_end_t *io, *io0, *io1;
 	unsigned long flags;
