htmldir = $(prefix)/share/doc/$(PACKAGE)
designdir = $(htmldir)/design

dist_design_DATA = design.html design.pdf sys.svg node.svg docs.css
EXTRA_DIST = design.xml dbdar.sty config.xsl

%.html: %.xml config.xsl docs.css
	$(XMLTO) xhtml-nochunks -m config.xsl $<
design.html: sys.svg node.svg

%.pdf: %.xml config.xsl dbdar.sty
	echo $(DBLATEX)
	$(DBLATEX) -s dbdar.sty -p config.xsl $<
design.pdf: sys.pdf node.pdf

CLEANFILES = design.html design.pdf
