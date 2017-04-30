ext4: copy mpage_readpage() and mpage_readpages() fs/ext4/readpage.c

Move the functions which we need from fs/mpage.c into
fs/ext4/readpage.c.  This will allow us to proceed with the
refactorization of these functions and eventual merger with the
functions in fs/ext4/page_io.c.

Signed-off-by: Theodore Ts'o <tytso@mit.edu>
---
 fs/ext4/readpage.c | 326 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++--
 1 file changed, 320 insertions(+), 6 deletions(-)

diff --git a/fs/ext4/readpage.c b/fs/ext4/readpage.c
index b5249db..3b29da1 100644
--- a/fs/ext4/readpage.c
+++ b/fs/ext4/readpage.c
@@ -23,6 +23,7 @@
 #include <linux/ratelimit.h>
 #include <linux/aio.h>
 #include <linux/bitops.h>
+#include <linux/cleancache.h>
 
 #include "ext4_jbd2.h"
 #include "xattr.h"
@@ -30,31 +31,344 @@
 
 #include <trace/events/ext4.h>
 
-int ext4_readpage(struct file *file, struct page *page)
+/*
+ * I/O completion handler for multipage BIOs.
+ *
+ * The mpage code never puts partial pages into a BIO (except for end-of-file).
+ * If a page does not map to a contiguous run of blocks then it simply falls
+ * back to block_read_full_page().
+ *
+ * Why is this?  If a page's completion depends on a number of different BIOs
+ * which can complete in any order (or at the same time) then determining the
+ * status of that page is hard.  See end_buffer_async_read() for the details.
+ * There is no point in duplicating all that complexity.
+ */
+static void mpage_end_io(struct bio *bio, int err)
+{
+	struct bio_vec *bv;
+	int i;
+
+	bio_for_each_segment_all(bv, bio, i) {
+		struct page *page = bv->bv_page;
+		page_endio(page, bio_data_dir(bio), err);
+	}
+
+	bio_put(bio);
+}
+
+static struct bio *mpage_bio_submit(int rw, struct bio *bio)
+{
+	bio->bi_end_io = mpage_end_io;
+	submit_bio(rw, bio);
+	return NULL;
+}
+
+static struct bio *
+mpage_alloc(struct block_device *bdev,
+		sector_t first_sector, int nr_vecs,
+		gfp_t gfp_flags)
+{
+	struct bio *bio;
+
+	bio = bio_alloc(gfp_flags, nr_vecs);
+
+	if (bio == NULL && (current->flags & PF_MEMALLOC)) {
+		while (!bio && (nr_vecs /= 2))
+			bio = bio_alloc(gfp_flags, nr_vecs);
+	}
+
+	if (bio) {
+		bio->bi_bdev = bdev;
+		bio->bi_iter.bi_sector = first_sector;
+	}
+	return bio;
+}
+
+/*
+ * support function for mpage_readpages.  The fs supplied get_block might
+ * return an up to date buffer.  This is used to map that buffer into
+ * the page, which allows readpage to avoid triggering a duplicate call
+ * to get_block.
+ *
+ * The idea is to avoid adding buffers to pages that don't already have
+ * them.  So when the buffer is up to date and the page size == block size,
+ * this marks the page up to date instead of adding new buffers.
+ */
+static void
+map_buffer_to_page(struct page *page, struct buffer_head *bh, int page_block)
+{
+	struct inode *inode = page->mapping->host;
+	struct buffer_head *page_bh, *head;
+	int block = 0;
+
+	if (!page_has_buffers(page)) {
+		/*
+		 * don't make any buffers if there is only one buffer on
+		 * the page and the page just needs to be set up to date
+		 */
+		if (inode->i_blkbits == PAGE_CACHE_SHIFT &&
+		    buffer_uptodate(bh)) {
+			SetPageUptodate(page);
+			return;
+		}
+		create_empty_buffers(page, 1 << inode->i_blkbits, 0);
+	}
+	head = page_buffers(page);
+	page_bh = head;
+	do {
+		if (block == page_block) {
+			page_bh->b_state = bh->b_state;
+			page_bh->b_bdev = bh->b_bdev;
+			page_bh->b_blocknr = bh->b_blocknr;
+			break;
+		}
+		page_bh = page_bh->b_this_page;
+		block++;
+	} while (page_bh != head);
+}
+
+/*
+ * This is the worker routine which does all the work of mapping the disk
+ * blocks and constructs largest possible bios, submits them for IO if the
+ * blocks are not contiguous on the disk.
+ *
+ * We pass a buffer_head back and forth and use its buffer_mapped() flag to
+ * represent the validity of its disk mapping and to decide when to do the next
+ * get_block() call.
+ */
+static struct bio *
+do_mpage_readpage(struct bio *bio, struct page *page, unsigned nr_pages,
+		sector_t *last_block_in_bio, struct buffer_head *map_bh,
+		unsigned long *first_logical_block, get_block_t get_block)
 {
-	int ret = -EAGAIN;
 	struct inode *inode = page->mapping->host;
+	const unsigned blkbits = inode->i_blkbits;
+	const unsigned blocks_per_page = PAGE_CACHE_SIZE >> blkbits;
+	const unsigned blocksize = 1 << blkbits;
+	sector_t block_in_file;
+	sector_t last_block;
+	sector_t last_block_in_file;
+	sector_t blocks[MAX_BUF_PER_PAGE];
+	unsigned page_block;
+	unsigned first_hole = blocks_per_page;
+	struct block_device *bdev = NULL;
+	int length;
+	int fully_mapped = 1;
+	unsigned nblocks;
+	unsigned relative_block;
+
+	if (page_has_buffers(page))
+		goto confused;
+
+	block_in_file = (sector_t)page->index << (PAGE_CACHE_SHIFT - blkbits);
+	last_block = block_in_file + nr_pages * blocks_per_page;
+	last_block_in_file = (i_size_read(inode) + blocksize - 1) >> blkbits;
+	if (last_block > last_block_in_file)
+		last_block = last_block_in_file;
+	page_block = 0;
+
+	/*
+	 * Map blocks using the result from the previous get_blocks call first.
+	 */
+	nblocks = map_bh->b_size >> blkbits;
+	if (buffer_mapped(map_bh) && block_in_file > *first_logical_block &&
+			block_in_file < (*first_logical_block + nblocks)) {
+		unsigned map_offset = block_in_file - *first_logical_block;
+		unsigned last = nblocks - map_offset;
+
+		for (relative_block = 0; ; relative_block++) {
+			if (relative_block == last) {
+				clear_buffer_mapped(map_bh);
+				break;
+			}
+			if (page_block == blocks_per_page)
+				break;
+			blocks[page_block] = map_bh->b_blocknr + map_offset +
+						relative_block;
+			page_block++;
+			block_in_file++;
+		}
+		bdev = map_bh->b_bdev;
+	}
+
+	/*
+	 * Then do more get_blocks calls until we are done with this page.
+	 */
+	map_bh->b_page = page;
+	while (page_block < blocks_per_page) {
+		map_bh->b_state = 0;
+		map_bh->b_size = 0;
+
+		if (block_in_file < last_block) {
+			map_bh->b_size = (last_block-block_in_file) << blkbits;
+			if (get_block(inode, block_in_file, map_bh, 0))
+				goto confused;
+			*first_logical_block = block_in_file;
+		}
+
+		if (!buffer_mapped(map_bh)) {
+			fully_mapped = 0;
+			if (first_hole == blocks_per_page)
+				first_hole = page_block;
+			page_block++;
+			block_in_file++;
+			continue;
+		}
+
+		/* some filesystems will copy data into the page during
+		 * the get_block call, in which case we don't want to
+		 * read it again.  map_buffer_to_page copies the data
+		 * we just collected from get_block into the page's buffers
+		 * so readpage doesn't have to repeat the get_block call
+		 */
+		if (buffer_uptodate(map_bh)) {
+			map_buffer_to_page(page, map_bh, page_block);
+			goto confused;
+		}
+
+		if (first_hole != blocks_per_page)
+			goto confused;		/* hole -> non-hole */
+
+		/* Contiguous blocks? */
+		if (page_block && blocks[page_block-1] != map_bh->b_blocknr-1)
+			goto confused;
+		nblocks = map_bh->b_size >> blkbits;
+		for (relative_block = 0; ; relative_block++) {
+			if (relative_block == nblocks) {
+				clear_buffer_mapped(map_bh);
+				break;
+			} else if (page_block == blocks_per_page)
+				break;
+			blocks[page_block] = map_bh->b_blocknr+relative_block;
+			page_block++;
+			block_in_file++;
+		}
+		bdev = map_bh->b_bdev;
+	}
+
+	if (first_hole != blocks_per_page) {
+		zero_user_segment(page, first_hole << blkbits, PAGE_CACHE_SIZE);
+		if (first_hole == 0) {
+			SetPageUptodate(page);
+			unlock_page(page);
+			goto out;
+		}
+	} else if (fully_mapped) {
+		SetPageMappedToDisk(page);
+	}
+
+	if (fully_mapped && blocks_per_page == 1 && !PageUptodate(page) &&
+	    cleancache_get_page(page) == 0) {
+		SetPageUptodate(page);
+		goto confused;
+	}
+
+	/*
+	 * This page will go to BIO.  Do we need to send this BIO off first?
+	 */
+	if (bio && (*last_block_in_bio != blocks[0] - 1))
+		bio = mpage_bio_submit(READ, bio);
+
+alloc_new:
+	if (bio == NULL) {
+		if (first_hole == blocks_per_page) {
+			if (!bdev_read_page(bdev, blocks[0] << (blkbits - 9),
+								page))
+				goto out;
+		}
+		bio = mpage_alloc(bdev, blocks[0] << (blkbits - 9),
+				min_t(int, nr_pages, bio_get_nr_vecs(bdev)),
+				GFP_KERNEL);
+		if (bio == NULL)
+			goto confused;
+	}
+
+	length = first_hole << blkbits;
+	if (bio_add_page(bio, page, length, 0) < length) {
+		bio = mpage_bio_submit(READ, bio);
+		goto alloc_new;
+	}
+
+	relative_block = block_in_file - *first_logical_block;
+	nblocks = map_bh->b_size >> blkbits;
+	if ((buffer_boundary(map_bh) && relative_block == nblocks) ||
+	    (first_hole != blocks_per_page))
+		bio = mpage_bio_submit(READ, bio);
+	else
+		*last_block_in_bio = blocks[blocks_per_page - 1];
+out:
+	return bio;
+
+confused:
+	if (bio)
+		bio = mpage_bio_submit(READ, bio);
+	if (!PageUptodate(page))
+	        block_read_full_page(page, get_block);
+	else
+		unlock_page(page);
+	goto out;
+}
+
+int ext4_readpage(struct file *file, struct page *page)
+{
+	unsigned long		first_logical_block = 0;
+	struct buffer_head	map_bh;
+	struct inode		*inode = page->mapping->host;
+	struct bio 		*bio = NULL;
+	sector_t		last_block_in_bio = 0;
+	int			ret = -EAGAIN;
 
 	trace_ext4_readpage(page);
 
 	if (ext4_has_inline_data(inode))
 		ret = ext4_readpage_inline(inode, page);
 
-	if (ret == -EAGAIN)
-		return mpage_readpage(page, ext4_get_block);
+	if (ret != -EAGAIN)
+		return ret;
 
-	return ret;
+	map_bh.b_state = 0;
+	map_bh.b_size = 0;
+	bio = do_mpage_readpage(bio, page, 1, &last_block_in_bio,
+			&map_bh, &first_logical_block, ext4_get_block);
+	if (bio)
+		mpage_bio_submit(READ, bio);
+	return 0;
 }
 
 int ext4_readpages(struct file *file, struct address_space *mapping,
 		   struct list_head *pages, unsigned nr_pages)
 {
 	struct inode *inode = mapping->host;
+	struct bio *bio = NULL;
+	unsigned page_idx;
+	sector_t last_block_in_bio = 0;
+	struct buffer_head map_bh;
+	unsigned long first_logical_block = 0;
 
 	/* If the file has inline data, no need to do readpages. */
 	if (ext4_has_inline_data(inode))
 		return 0;
 
-	return mpage_readpages(mapping, pages, nr_pages, ext4_get_block);
+	map_bh.b_state = 0;
+	map_bh.b_size = 0;
+	for (page_idx = 0; page_idx < nr_pages; page_idx++) {
+		struct page *page = list_entry(pages->prev, struct page, lru);
+
+		prefetchw(&page->flags);
+		list_del(&page->lru);
+		if (!add_to_page_cache_lru(page, mapping,
+					page->index, GFP_KERNEL)) {
+			bio = do_mpage_readpage(bio, page,
+					nr_pages - page_idx,
+					&last_block_in_bio, &map_bh,
+					&first_logical_block,
+					ext4_get_block);
+		}
+		page_cache_release(page);
+	}
+	BUG_ON(!list_empty(pages));
+	if (bio)
+		mpage_bio_submit(READ, bio);
+	return 0;
 }
 
