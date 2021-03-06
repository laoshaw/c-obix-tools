<xsl:stylesheet 
    version='1.0'   
    xmlns:xsl='http://www.w3.org/1999/XSL/Transform'  
    xmlns:xlink='http://www.w3.org/1999/xlink'>
  <xsl:output method="html"/>
  <xsl:template match="/">
    <html>
      <head>
        <base href="{//@href}"/>
        <style type='text/css'>
          .elem { display:block; padding-left:10px; }
          .attr { padding-left:15px; }
          .name { color: black; }
          .an   { color: black; padding-left:5px; }
          .av   { color: purple; }
          .sym  { color: red; }
          a:link    { color: #0000ff; }
          a:visited { color: #0000ff; }
          a:hover   { color: #0000ff; }
          a:active  { color: #0000ff; }
        </style>
      </head>

      <body style='font:14px Courier;'>
        <i><a href="..">Up</a></i>
        <p/>
        <xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>
  <!-- make every element and its attributes visible -->

  <xsl:template match="*">  
    <!-- element start -->    
    <div class='elem' style="white-space: nowrap"> 
      <span class='sym'>&lt;</span>
      <span class='name'> <xsl:value-of select="name()"/></span>
      <!-- element attributes --> 
      <xsl:for-each select='@*'>
        <span class='an'> <xsl:value-of select='name()'/></span>
        <span class='sym'>=</span> 
        <xsl:choose>

          <xsl:when test="name()='href'">
            <span class='av'>"<a href="{.}"><xsl:value-of select='.'/></a>"</span>
          </xsl:when>
          <xsl:when test="name()='icon'">
            <span class='av'>"<a href="{.}"><xsl:value-of select='.'/></a>"</span>
          </xsl:when>
          <xsl:when test="name()='range'">

            <span class='av'>"<a href="{.}"><xsl:value-of select='.'/></a>"</span>
          </xsl:when>
          <xsl:when test="name()='unit'">
            <span class='av'>"<a href="{.}"><xsl:value-of select='.'/></a>"</span>
          </xsl:when>
          <xsl:otherwise>
            <span class='av'>"<xsl:value-of select='.'/>"</span>

          </xsl:otherwise> 
        </xsl:choose>
      </xsl:for-each>
      <span class='sym'>&gt;</span>
      <!-- element body -->
      <xsl:apply-templates/>
      <!-- element end -->
      <span class='sym'>&lt;/</span>
      <span class='name'><xsl:value-of select="name()"/></span>

      <span class='sym'>&gt;</span>
    </div>
  </xsl:template>
</xsl:stylesheet>
