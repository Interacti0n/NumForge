-- Run only through the disposable PostgreSQL fixture. No persistent DDL here.
BEGIN;
SET LOCAL statement_timeout = '3s';
SET LOCAL lock_timeout = '1s';
CREATE EXTENSION IF NOT EXISTS xml2;
DO $checks$
DECLARE
    document xml;
    value text;
    total integer;
BEGIN
    document := XMLPARSE(DOCUMENT '<r><a>výpočet 😀 &amp; XML</a><n>2</n><n>3</n></r>');
    ASSERT document IS DOCUMENT, 'XML document parse';
    ASSERT xml_is_well_formed_document(document::text), 'well-formed document';
    value := (xpath('/r/a/text()', document))[1]::text;
    ASSERT value = 'výpočet 😀 &amp; XML', 'Unicode and entity round trip';
    ASSERT xmlexists('/r/n[text()="3"]' PASSING document), 'XPath predicate';
    SELECT sum(n) INTO total FROM XMLTABLE('/r/n' PASSING document COLUMNS n integer PATH '.');
    ASSERT total = 5, 'XMLTABLE integer mapping';
    ASSERT (xpath('/p:r/p:a/text()', XMLPARSE(DOCUMENT '<r xmlns="urn:nf"><a>7</a></r>'),
                  ARRAY[ARRAY['p','urn:nf']]))[1]::text = '7', 'namespaced XPath';
    ASSERT XMLSERIALIZE(CONTENT xmlelement(NAME r, '<&>') AS text) = '<r>&lt;&amp;&gt;</r>',
           'SQL values are escaped';
    ASSERT xml_is_well_formed_content('<a/><b/>') AND NOT xml_is_well_formed_document('<a/><b/>'),
           'fragment/document distinction';
    ASSERT xpath('/r', NULL::xml) IS NULL, 'NULL handling';
    value := (xpath('/out/text()', xslt_process('<r><v>7</v></r>', $stylesheet$
      <xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
        <xsl:template match="/"><out><xsl:value-of select="/r/v"/></out></xsl:template>
      </xsl:stylesheet>
    $stylesheet$)::xml))[1]::text;
    ASSERT value = '7', 'xml2 extension and XSLT transformation';
    document := XMLPARSE(DOCUMENT '<!DOCTYPE r [<!ENTITY x "small">]><r>&x;</r>');
    ASSERT xmlexists('/r' PASSING document), 'bounded internal entity document';
    BEGIN
        PERFORM XMLPARSE(DOCUMENT '<r><broken></r>');
        RAISE EXCEPTION 'Malformed XML unexpectedly accepted';
    EXCEPTION WHEN invalid_xml_document THEN NULL;
    END;
    ASSERT 6 * 7 = 42, 'session remains usable after XML parse error';
END
$checks$;
ROLLBACK;
SELECT 'XML checks passed';
