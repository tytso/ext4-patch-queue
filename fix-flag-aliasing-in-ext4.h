ext4: fix accidental flag aliasing in ext4_map_blocks flags

Commit b8a8684502a0f introduced an accidental flag aliasing between
EXT4_EX_NOCACHE and EXT4_GET_BLOCKS_CONVERT_UNWRITTEN.

Fortunately, this didn't introduce any untorward side effects --- we
got lucky.  Nevertheless, fix this and leave a warning to hopefully
avoid this from happening in the future.

Signed-off-by: Theodore Ts'o <tytso@mit.edu>
---
 fs/ext4/ext4.h | 5 +++--
 1 file changed, 3 insertions(+), 2 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index cf3ad75..550b4f9 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -569,6 +569,7 @@ enum {
 #define EXT4_GET_BLOCKS_NO_PUT_HOLE		0x0200
 	/* Convert written extents to unwritten */
 #define EXT4_GET_BLOCKS_CONVERT_UNWRITTEN	0x0400
+/* DO NOT ASSIGN ADDITIONAL FLAG VALUES WITHOUT ADJUSTING THE FLAGS BELOW */
 
 /*
  * The bit position of these flags must not overlap with any of the
@@ -579,8 +580,8 @@ enum {
  * caching the extents when reading from the extent tree while a
  * truncate or punch hole operation is in progress.
  */
-#define EXT4_EX_NOCACHE				0x0400
-#define EXT4_EX_FORCE_CACHE			0x0800
+#define EXT4_EX_NOCACHE				0x0800
+#define EXT4_EX_FORCE_CACHE			0x1000
 
 /*
  * Flags used by ext4_free_blocks
