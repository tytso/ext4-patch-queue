ext4: remove no longer used functions in inode.c

From: Zheng Liu <wenqing.lz@taobao.com>

The functions ext4_block_truncate_page() and ext4_block_zero_page_range()
are no longer used, so remove them.

Signed-off-by: Zheng Liu <wenqing.lz@taobao.com>
Signed-off-by: "Theodore Ts'o" <tytso@mit.edu>
---
 fs/ext4/ext4.h  |    4 --
 fs/ext4/inode.c |  120 -------------------------------------------------------
 2 files changed, 0 insertions(+), 124 deletions(-)

diff --git a/fs/ext4/ext4.h b/fs/ext4/ext4.h
index 5b0e26a..ae2407f 100644
--- a/fs/ext4/ext4.h
+++ b/fs/ext4/ext4.h
@@ -1880,10 +1880,6 @@ extern int ext4_alloc_da_blocks(struct inode *inode);
 extern void ext4_set_aops(struct inode *inode);
 extern int ext4_writepage_trans_blocks(struct inode *);
 extern int ext4_chunk_trans_blocks(struct inode *, int nrblocks);
-extern int ext4_block_truncate_page(handle_t *handle,
-		struct address_space *mapping, loff_t from);
-extern int ext4_block_zero_page_range(handle_t *handle,
-		struct address_space *mapping, loff_t from, loff_t length);
 extern int ext4_discard_partial_page_buffers(handle_t *handle,
 		struct address_space *mapping, loff_t from,
 		loff_t length, int flags);
diff --git a/fs/ext4/inode.c b/fs/ext4/inode.c
index fffec40..636d1ec 100644
--- a/fs/ext4/inode.c
+++ b/fs/ext4/inode.c
@@ -3335,126 +3335,6 @@ next:
 	return err;
 }
 
-/*
- * ext4_block_truncate_page() zeroes out a mapping from file offset `from'
- * up to the end of the block which corresponds to `from'.
- * This required during truncate. We need to physically zero the tail end
- * of that block so it doesn't yield old data if the file is later grown.
- */
-int ext4_block_truncate_page(handle_t *handle,
-		struct address_space *mapping, loff_t from)
-{
-	unsigned offset = from & (PAGE_CACHE_SIZE-1);
-	unsigned length;
-	unsigned blocksize;
-	struct inode *inode = mapping->host;
-
-	blocksize = inode->i_sb->s_blocksize;
-	length = blocksize - (offset & (blocksize - 1));
-
-	return ext4_block_zero_page_range(handle, mapping, from, length);
-}
-
-/*
- * ext4_block_zero_page_range() zeros out a mapping of length 'length'
- * starting from file offset 'from'.  The range to be zero'd must
- * be contained with in one block.  If the specified range exceeds
- * the end of the block it will be shortened to end of the block
- * that cooresponds to 'from'
- */
-int ext4_block_zero_page_range(handle_t *handle,
-		struct address_space *mapping, loff_t from, loff_t length)
-{
-	ext4_fsblk_t index = from >> PAGE_CACHE_SHIFT;
-	unsigned offset = from & (PAGE_CACHE_SIZE-1);
-	unsigned blocksize, max, pos;
-	ext4_lblk_t iblock;
-	struct inode *inode = mapping->host;
-	struct buffer_head *bh;
-	struct page *page;
-	int err = 0;
-
-	page = find_or_create_page(mapping, from >> PAGE_CACHE_SHIFT,
-				   mapping_gfp_mask(mapping) & ~__GFP_FS);
-	if (!page)
-		return -ENOMEM;
-
-	blocksize = inode->i_sb->s_blocksize;
-	max = blocksize - (offset & (blocksize - 1));
-
-	/*
-	 * correct length if it does not fall between
-	 * 'from' and the end of the block
-	 */
-	if (length > max || length < 0)
-		length = max;
-
-	iblock = index << (PAGE_CACHE_SHIFT - inode->i_sb->s_blocksize_bits);
-
-	if (!page_has_buffers(page))
-		create_empty_buffers(page, blocksize, 0);
-
-	/* Find the buffer that contains "offset" */
-	bh = page_buffers(page);
-	pos = blocksize;
-	while (offset >= pos) {
-		bh = bh->b_this_page;
-		iblock++;
-		pos += blocksize;
-	}
-
-	err = 0;
-	if (buffer_freed(bh)) {
-		BUFFER_TRACE(bh, "freed: skip");
-		goto unlock;
-	}
-
-	if (!buffer_mapped(bh)) {
-		BUFFER_TRACE(bh, "unmapped");
-		ext4_get_block(inode, iblock, bh, 0);
-		/* unmapped? It's a hole - nothing to do */
-		if (!buffer_mapped(bh)) {
-			BUFFER_TRACE(bh, "still unmapped");
-			goto unlock;
-		}
-	}
-
-	/* Ok, it's mapped. Make sure it's up-to-date */
-	if (PageUptodate(page))
-		set_buffer_uptodate(bh);
-
-	if (!buffer_uptodate(bh)) {
-		err = -EIO;
-		ll_rw_block(READ, 1, &bh);
-		wait_on_buffer(bh);
-		/* Uhhuh. Read error. Complain and punt. */
-		if (!buffer_uptodate(bh))
-			goto unlock;
-	}
-
-	if (ext4_should_journal_data(inode)) {
-		BUFFER_TRACE(bh, "get write access");
-		err = ext4_journal_get_write_access(handle, bh);
-		if (err)
-			goto unlock;
-	}
-
-	zero_user(page, offset, length);
-
-	BUFFER_TRACE(bh, "zeroed end of block");
-
-	err = 0;
-	if (ext4_should_journal_data(inode)) {
-		err = ext4_handle_dirty_metadata(handle, inode, bh);
-	} else
-		mark_buffer_dirty(bh);
-
-unlock:
-	unlock_page(page);
-	page_cache_release(page);
-	return err;
-}
-
 int ext4_can_truncate(struct inode *inode)
 {
 	if (S_ISREG(inode->i_mode))
-- 
1.7.4.1


