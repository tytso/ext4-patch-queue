ext4: remove unused #include <linux/version.h>

From: Huang Weiyi <weiyi.huang@gmail.com>

Remove unused #include <linux/version.h>('s) in
  fs/ext4/block_validity.c
  fs/ext4/mballoc.h

Signed-off-by: Huang Weiyi <weiyi.huang@gmail.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/block_validity.c |    1 -
 fs/ext4/mballoc.h        |    1 -
 2 files changed, 0 insertions(+), 2 deletions(-)

diff --git a/fs/ext4/block_validity.c b/fs/ext4/block_validity.c
index 4df8621..a60ab9a 100644
--- a/fs/ext4/block_validity.c
+++ b/fs/ext4/block_validity.c
@@ -16,7 +16,6 @@
 #include <linux/module.h>
 #include <linux/swap.h>
 #include <linux/pagemap.h>
-#include <linux/version.h>
 #include <linux/blkdev.h>
 #include <linux/mutex.h>
 #include "ext4.h"
diff --git a/fs/ext4/mballoc.h b/fs/ext4/mballoc.h
index 0ca8110..436521c 100644
--- a/fs/ext4/mballoc.h
+++ b/fs/ext4/mballoc.h
@@ -17,7 +17,6 @@
 #include <linux/proc_fs.h>
 #include <linux/pagemap.h>
 #include <linux/seq_file.h>
-#include <linux/version.h>
 #include <linux/blkdev.h>
 #include <linux/mutex.h>
 #include "ext4_jbd2.h"
