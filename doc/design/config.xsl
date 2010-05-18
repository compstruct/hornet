<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:fo="http://www.w3.org/1999/XSL/Format"
                version="1.0">
<xsl:param name="html.stylesheet" select="'docs.css'"/>
<xsl:param name="section.autolabel" select="1"/>
<!-- dblatex configuration -->
<xsl:param name="latex.class.options">letterpaper,10pt</xsl:param>
<xsl:param name="doc.collab.show" select="0"/>
<xsl:param name="doc.pdfcreator.show" select="0"/>
<xsl:param name="latex.encoding" select="utf8"/>
<xsl:template match="releaseinfo" mode="docinfo">
  <xsl:text>\renewcommand{\DBKreleaseinfo}{</xsl:text>
  <xsl:apply-templates select="."/>
  <xsl:text>}&#10;</xsl:text>
</xsl:template>
</xsl:stylesheet>

