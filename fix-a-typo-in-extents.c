ext4: fix a typo in extents.c

From: Yongqiang Yang <xiaoqiangnk@gmail.com>

Signed-off-by: Yongqiang Yang <yangyongqiang01@baidu.com>
Reviewed-by: Carlos Maiolino <cmaiolino@redhat.com>
---
diff --git a/fs/ext4/extents.c b/fs/ext4/extents.c
index 4410cc3..a710b88 100644
--- a/fs/ext4/extents.c
+++ b/fs/ext4/extents.c
@@ -3477,7 +3477,7 @@ static int ext4_ext_convert_to_initialized(handle_t *handle,
 	WARN_ON(map->m_lblk < ee_block);
 	/*
 	 * It is safe to convert extent to initialized via explicit
-	 * zeroout only if extent is fully insde i_size or new_size.
+	 * zeroout only if extent is fully inside i_size or new_size.
 	 */
 	split_flag |= ee_block + ee_len <= eof_block ? EXT4_EXT_MAY_ZEROOUT : 0;
 
