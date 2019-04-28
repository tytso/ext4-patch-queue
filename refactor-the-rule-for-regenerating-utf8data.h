unicode: refactor the rule for regenerating utf8data.h

From: Masahiro Yamada <yamada.masahiro@socionext.com>

scripts/mkutf8data is used only when regenerating utf8data.h,
which never happens in the normal kernel build. However, it is
irrespectively built if CONFIG_UNICODE is enabled.

Moreover, there is no good reason for it to reside in the scripts/
directory since it is only used in fs/unicode/.

Hence, move it from scripts/ to fs/unicode/.

In some cases, we bypass build artifacts in the normal build. The
conventional way to do so is to surround the code with ifdef REGENERATE_*.

For example,

 - 7373f4f83c71 ("kbuild: add implicit rules for parser generation")
 - 6aaf49b495b4 ("crypto: arm,arm64 - Fix random regeneration of S_shipped")

I rewrote the rule in a more kbuild'ish style.

It works like this:

$ make REGENERATE_UTF8DATA=1 fs/unicode/
  [ snip ]
  HOSTCC  fs/unicode/mkutf8data
  GEN     fs/unicode/utf8data.h
  CC      fs/unicode/utf8-norm.o
  CC      fs/unicode/utf8-core.o
  AR      fs/unicode/built-in.a

Also, I added utf8data.h to .gitignore and dontdiff.

Signed-off-by: Masahiro Yamada <yamada.masahiro@socionext.com>
Signed-off-by: Theodore Ts'o <tytso@mit.edu>
---
 Documentation/dontdiff               |  1 +
 fs/unicode/.gitignore                |  1 +
 fs/unicode/Makefile                  | 37 +++++++++++++++++++---------
 fs/unicode/README.utf8data           | 10 ++++----
 {scripts => fs/unicode}/mkutf8data.c |  0
 scripts/Makefile                     |  1 -
 6 files changed, 33 insertions(+), 17 deletions(-)
 create mode 100644 fs/unicode/.gitignore
 rename {scripts => fs/unicode}/mkutf8data.c (100%)

diff --git a/Documentation/dontdiff b/Documentation/dontdiff
index ef25a066d952..bc353adfc996 100644
--- a/Documentation/dontdiff
+++ b/Documentation/dontdiff
@@ -176,6 +176,7 @@ mkprep
 mkregtable
 mktables
 mktree
+mkutf8data
 modpost
 modules.builtin
 modules.order
diff --git a/fs/unicode/.gitignore b/fs/unicode/.gitignore
new file mode 100644
index 000000000000..44811fc4a799
--- /dev/null
+++ b/fs/unicode/.gitignore
@@ -0,0 +1 @@
+mkutf8data
diff --git a/fs/unicode/Makefile b/fs/unicode/Makefile
index 671d31f83006..45955264ac04 100644
--- a/fs/unicode/Makefile
+++ b/fs/unicode/Makefile
@@ -5,15 +5,30 @@ obj-$(CONFIG_UNICODE_NORMALIZATION_SELFTEST) += utf8-selftest.o
 
 unicode-y := utf8-norm.o utf8-core.o
 
-# This rule is not invoked during the kernel compilation.  It is used to
-# regenerate the utf8data.h header file.
-utf8data.h.new: *.txt $(objdir)/scripts/mkutf8data
-	$(objdir)/scripts/mkutf8data \
-		-a DerivedAge.txt \
-		-c DerivedCombiningClass.txt \
-		-p DerivedCoreProperties.txt \
-		-d UnicodeData.txt \
-		-f CaseFolding.txt \
-		-n NormalizationCorrections.txt \
-		-t NormalizationTest.txt \
+
+# To regenerate utf8data.h, run the following in the top directory:
+#   $ make REGENERATE_UTF8DATA=1 fs/unicode/
+ifdef REGENERATE_UTF8DATA
+
+$(obj)/utf8-norm.o: $(obj)/utf8data.h
+
+quiet_cmd_utf8data = GEN     $@
+      cmd_utf8data = $(obj)/mkutf8data \
+		-a $(srctree)/$(src)/DerivedAge.txt \
+		-c $(srctree)/$(src)/DerivedCombiningClass.txt \
+		-p $(srctree)/$(src)/DerivedCoreProperties.txt \
+		-d $(srctree)/$(src)/UnicodeData.txt \
+		-f $(srctree)/$(src)/CaseFolding.txt \
+		-n $(srctree)/$(src)/NormalizationCorrections.txt \
+		-t $(srctree)/$(src)/NormalizationTest.txt \
 		-o $@
+
+$(obj)/utf8data.h: $(filter %.txt, $(cmd_utf8data)) $(obj)/mkutf8data FORCE
+	$(call if_changed,utf8data)
+
+always += utf8data.h
+no-clean-files += utf8data.h
+
+endif
+
+hostprogs-y += mkutf8data
diff --git a/fs/unicode/README.utf8data b/fs/unicode/README.utf8data
index eeb7561526d9..155d56e91a71 100644
--- a/fs/unicode/README.utf8data
+++ b/fs/unicode/README.utf8data
@@ -41,15 +41,15 @@ released version of the UCD can be found here:
 
   http://www.unicode.org/Public/UCD/latest/
 
-To build the utf8data.h file, from a kernel tree that has been built,
-cd to this directory (fs/unicode) and run this command:
+To regenerate utf8data.h in the build process, pass REGENERATE_UTF8DATA=1
+from the command line. The easiest command to update it is this:
 
-	make C=../.. objdir=../.. utf8data.h.new
+	make REGENERATE_UTF8DATA=1 fs/unicode/
 
-After sanity checking the newly generated utf8data.h.new file (the
+After sanity checking the newly generated utf8data.h file (the
 version generated from the 12.1.0 UCD should be 4,109 lines long, and
 have a total size of 324k) and/or comparing it with the older version
-of utf8data.h, rename it to utf8data.h.
+of utf8data.h, check it in.
 
 If you are a kernel developer updating to a newer version of the
 Unicode Character Database, please update this README.utf8data file
diff --git a/scripts/mkutf8data.c b/fs/unicode/mkutf8data.c
similarity index 100%
rename from scripts/mkutf8data.c
rename to fs/unicode/mkutf8data.c
diff --git a/scripts/Makefile b/scripts/Makefile
index b87e3e0ade4d..9d442ee050bd 100644
--- a/scripts/Makefile
+++ b/scripts/Makefile
@@ -20,7 +20,6 @@ hostprogs-$(CONFIG_ASN1)	 += asn1_compiler
 hostprogs-$(CONFIG_MODULE_SIG)	 += sign-file
 hostprogs-$(CONFIG_SYSTEM_TRUSTED_KEYRING) += extract-cert
 hostprogs-$(CONFIG_SYSTEM_EXTRA_CERTIFICATE) += insert-sys-cert
-hostprogs-$(CONFIG_UNICODE) += mkutf8data
 
 HOSTCFLAGS_sortextable.o = -I$(srctree)/tools/include
 HOSTCFLAGS_asn1_compiler.o = -I$(srctree)/include
-- 
2.19.1

