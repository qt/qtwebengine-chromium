noinst_HEADERS=fcgenericfamily.h

NULL =
LIST_FILES = 			\
	serif.txt			\
	sans-serif.txt		\
	monospace.txt		\
	cursive.txt			\
	fantasy.txt			\
	system-ui.txt		\
	ui-serif.txt		\
	ui-sans-serif.txt	\
	ui-monospace.txt	\
	ui-rounded.txt		\
	emoji.txt			\
	math.txt			\
	fangsong.txt		\
	$(NULL)

BUILT_SOURCES =						\
	fcgenericfamily.gperf			\
	$(builddir)/fcgenericfamily.h	\
	$(NULL)

fcgenericfamily.gperf: fc-genericfamily.py $(LIST_FILES) Makefile.am
	-@$(RM) stamp-fcgenericfamily.gperf
	@$(MAKE) stamp-fcgenericfamily.gperf
	@touch -r stamp-fcgenericfamily.gperf $@
stamp-fcgenericfamily.gperf: Makefile.am fc-genericfamily.py
	$(AM_V_GEN) $(PYTHON) $(srcdir)/fc-genericfamily.py -d $(srcdir) -o fcgenericfamily.gperf && touch $@

$(builddir)/fcgenericfamily.h: Makefile.am fcgenericfamily.gperf
	$(AM_V_GEN) $(GPERF) --pic -m 100 fcgenericfamily.gperf > $@.tmp && \
	mv -f $@.tmp $@ || ( $(RM) $@.tmp && false )

CLEANFILES = stamp-fcgenericfamily.gperf
MAINTAINERCLEANFILES = fcgenericfamily.h
