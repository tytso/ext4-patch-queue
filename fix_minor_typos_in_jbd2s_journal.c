jbd2: Fix minor typos in comments in fs/jbd2/journal.c

From: Alberto Bertogli <albertito@blitiri.com.ar>

Signed-off-by: Alberto Bertogli <albertito@blitiri.com.ar>
Signed-off-by: Theodore Ts'o <tytso@mit.edu>

diff --git a/fs/jbd2/journal.c b/fs/jbd2/journal.c
index 5814410..85af8ea 100644
--- a/fs/jbd2/journal.c
+++ b/fs/jbd2/journal.c
@@ -1781,7 +1781,7 @@ int jbd2_journal_wipe(journal_t *journal, int write)
  * Journal abort has very specific semantics, which we describe
  * for journal abort.
  *
- * Two internal function, which provide abort to te jbd layer
+ * Two internal functions, which provide abort to the jbd layer
  * itself are here.
  */
 
@@ -1879,7 +1879,7 @@ void jbd2_journal_abort(journal_t *journal, int errno)
  * int jbd2_journal_errno () - returns the journal's error state.
  * @journal: journal to examine.
  *
- * This is the errno numbet set with jbd2_journal_abort(), the last
+ * This is the errno number set with jbd2_journal_abort(), the last
  * time the journal was mounted - if the journal was stopped
  * without calling abort this will be 0.
  *
@@ -1903,7 +1903,7 @@ int jbd2_journal_errno(journal_t *journal)
  * int jbd2_journal_clear_err () - clears the journal's error state
  * @journal: journal to act on.
  *
- * An error must be cleared or Acked to take a FS out of readonly
+ * An error must be cleared or acked to take a FS out of readonly
  * mode.
  */
 int jbd2_journal_clear_err(journal_t *journal)
@@ -1923,7 +1923,7 @@ int jbd2_journal_clear_err(journal_t *journal)
  * void jbd2_journal_ack_err() - Ack journal err.
  * @journal: journal to act on.
  *
- * An error must be cleared or Acked to take a FS out of readonly
+ * An error must be cleared or acked to take a FS out of readonly
  * mode.
  */
 void jbd2_journal_ack_err(journal_t *journal)
