htmldir = $(prefix)/share/doc/$(PACKAGE)
manualdir = $(htmldir)/manual

dist_manual_DATA = manual.html manual.pdf docs.css sys.svg node.svg hornet_routing.jpg hornet_vcs.jpg hornet_ban.jpg scaling.jpg
EXTRA_DIST = manual.xml dbdar.sty config.xsl

%.html: %.xml config.xsl docs.css
	$(XMLTO) xhtml-nochunks -m config.xsl $<
manual.html: sys.svg node.svg hornet_routing.jpg hornet_vcs.jpg hornet_ban.jpg scaling.jpg

%.pdf: %.xml config.xsl dbdar.sty
	echo $(DBLATEX)
	$(DBLATEX) -s dbdar.sty -p config.xsl $<
manual.pdf: sys.pdf node.pdf hornet_routing.pdf hornet_vcs.pdf hornet_ban.pdf scaling.pdf

CLEANFILES = manual.html manual.pdf
