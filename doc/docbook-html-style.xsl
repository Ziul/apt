<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <!-- Import our base stylesheet -->
  <xsl:import href="/usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml-1_1/chunk.xsl" />

  <!-- Since we use xsltproc (not saxon), add a workaround to ensure UTF-8 -->
  <xsl:template xmlns="http://www.w3.org/1999/xhtml" name="head.content.generator">
    <xsl:param name="node" select="."/>
    <meta name="generator" content="DocBook {$DistroTitle} V{$VERSION}"/>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
  </xsl:template>

  <xsl:template name="generate.html.title"/>

  <xsl:template match="releaseinfo" mode="titlepage.mode">
    <xsl:apply-imports/>
    <hr/>
  </xsl:template>

  <xsl:param name="root.filename">index</xsl:param>

  <!-- We do not want a title in HTML. -->
  <xsl:param name="generate.meta.abstract" select="0"/>

  <!-- We do not want the first subsection on the same page as content. -->
  <xsl:param name="chunk.first.sections" select="0"/>
  <xsl:param name="chunk.section.depth" select="0"/>
  <xsl:param name="chunker.output.indent" select="'yes'"/>

  <xsl:param name="use.id.as.filename" select="1"/>

  <xsl:param name="toc.section.depth" select="1"/>
  <xsl:param name="generate.section.toc.level" select="0"/>
  <xsl:param name="section.label.includes.component.label" select="1"/>
  <xsl:param name="section.autolabel" select="1"/>

  <xsl:param name="generate.css.header" select="1"/>

</xsl:stylesheet>
