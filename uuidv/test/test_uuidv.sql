-- test_uuidv.sql — functional tests for all three uuidv implementations.
-- Run against a MySQL 8.4 instance that has the plugin-dir pointing to the
-- directory where uuidv_udf.so, uuidv_plugin.so, and component_uuidv.so live.
--
-- Usage:
--   mysql -uroot -S /tmp/mysql-8.4-test/mysql.sock < test/test_uuidv.sql

\W

-- ========================================================================
-- UDF (uuidv_udf): loaded with CREATE FUNCTION / SONAME
-- ========================================================================

SELECT '=== UDF ===' AS section;

CREATE FUNCTION uuidv_udf RETURNS STRING SONAME 'uuidv_udf.so';

-- Length: every UUID is 36 chars (32 hex + 4 dashes)
SELECT 'uuidv_udf length v1' AS test, IF(LENGTH(uuidv_udf(1)) = 36, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_udf length v4' AS test, IF(LENGTH(uuidv_udf(4)) = 36, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_udf length v6' AS test, IF(LENGTH(uuidv_udf(6)) = 36, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_udf length v7' AS test, IF(LENGTH(uuidv_udf(7)) = 36, 'PASS', 'FAIL') AS result;

-- Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (CONVERT needed: UDF returns binary)
SELECT 'uuidv_udf format v4' AS test,
       IF(CONVERT(uuidv_udf(4) USING utf8mb4)
              REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$',
          'PASS', 'FAIL') AS result;

-- Version bit: character at position 15 is the version digit
SELECT 'uuidv_udf version bit v1' AS test, IF(SUBSTRING(uuidv_udf(1), 15, 1) = '1', 'PASS', 'FAIL') AS result;
SELECT 'uuidv_udf version bit v4' AS test, IF(SUBSTRING(uuidv_udf(4), 15, 1) = '4', 'PASS', 'FAIL') AS result;
SELECT 'uuidv_udf version bit v6' AS test, IF(SUBSTRING(uuidv_udf(6), 15, 1) = '6', 'PASS', 'FAIL') AS result;
SELECT 'uuidv_udf version bit v7' AS test, IF(SUBSTRING(uuidv_udf(7), 15, 1) = '7', 'PASS', 'FAIL') AS result;

-- NULL input → NULL output
SELECT 'uuidv_udf null input' AS test, IF(uuidv_udf(NULL) IS NULL, 'PASS', 'FAIL') AS result;

-- Unsupported version → error (returns NULL + error flag)
SELECT 'uuidv_udf bad version' AS test, IF(uuidv_udf(5) IS NULL, 'PASS', 'FAIL') AS result;

-- Uniqueness: three independent calls must all differ
SET @u1 = uuidv_udf(4);
SET @u2 = uuidv_udf(4);
SET @u3 = uuidv_udf(4);
SELECT 'uuidv_udf uniqueness v4' AS test,
       IF(@u1 <> @u2 AND @u2 <> @u3 AND @u1 <> @u3, 'PASS', 'FAIL') AS result;

DROP FUNCTION uuidv_udf;

-- ========================================================================
-- Plugin (uuidv_plugin): loaded with INSTALL PLUGIN / SONAME
-- ========================================================================

SELECT '=== Plugin ===' AS section;

INSTALL PLUGIN uuidv_plugin SONAME 'uuidv_plugin.so';
CREATE FUNCTION uuidv_plugin RETURNS STRING SONAME 'uuidv_plugin.so';

SELECT 'uuidv_plugin in SHOW PLUGINS' AS test,
       IF(COUNT(*) = 1, 'PASS', 'FAIL') AS result
FROM   information_schema.PLUGINS WHERE PLUGIN_NAME = 'uuidv_plugin';

SELECT 'uuidv_plugin length v4' AS test, IF(LENGTH(uuidv_plugin(4)) = 36, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_plugin length v7' AS test, IF(LENGTH(uuidv_plugin(7)) = 36, 'PASS', 'FAIL') AS result;

SELECT 'uuidv_plugin version bit v4' AS test, IF(SUBSTRING(uuidv_plugin(4), 15, 1) = '4', 'PASS', 'FAIL') AS result;
SELECT 'uuidv_plugin version bit v7' AS test, IF(SUBSTRING(uuidv_plugin(7), 15, 1) = '7', 'PASS', 'FAIL') AS result;

SELECT 'uuidv_plugin null input' AS test, IF(uuidv_plugin(NULL) IS NULL, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_plugin bad version' AS test, IF(uuidv_plugin(5) IS NULL, 'PASS', 'FAIL') AS result;

SET @p1 = uuidv_plugin(7);
SET @p2 = uuidv_plugin(7);
SET @p3 = uuidv_plugin(7);
SELECT 'uuidv_plugin uniqueness v7' AS test,
       IF(@p1 <> @p2 AND @p2 <> @p3 AND @p1 <> @p3, 'PASS', 'FAIL') AS result;

DROP FUNCTION uuidv_plugin;
UNINSTALL PLUGIN uuidv_plugin;

-- ========================================================================
-- Component (uuidv_component): loaded with INSTALL COMPONENT
-- ========================================================================

SELECT '=== Component ===' AS section;

INSTALL COMPONENT 'file://component_uuidv';

SELECT 'uuidv_component length v4' AS test, IF(LENGTH(uuidv_component(4)) = 36, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_component length v7' AS test, IF(LENGTH(uuidv_component(7)) = 36, 'PASS', 'FAIL') AS result;

SELECT 'uuidv_component version bit v4' AS test, IF(SUBSTRING(uuidv_component(4), 15, 1) = '4', 'PASS', 'FAIL') AS result;
SELECT 'uuidv_component version bit v7' AS test, IF(SUBSTRING(uuidv_component(7), 15, 1) = '7', 'PASS', 'FAIL') AS result;

SELECT 'uuidv_component null input' AS test, IF(uuidv_component(NULL) IS NULL, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_component bad version' AS test, IF(uuidv_component(5) IS NULL, 'PASS', 'FAIL') AS result;

SET @c1 = uuidv_component(4);
SET @c2 = uuidv_component(4);
SET @c3 = uuidv_component(4);
SELECT 'uuidv_component uniqueness v4' AS test,
       IF(@c1 <> @c2 AND @c2 <> @c3 AND @c1 <> @c3, 'PASS', 'FAIL') AS result;

UNINSTALL COMPONENT 'file://component_uuidv';

-- ========================================================================
-- All three simultaneously
-- ========================================================================

SELECT '=== All three simultaneously ===' AS section;

CREATE FUNCTION uuidv_udf RETURNS STRING SONAME 'uuidv_udf.so';
INSTALL PLUGIN uuidv_plugin SONAME 'uuidv_plugin.so';
CREATE FUNCTION uuidv_plugin RETURNS STRING SONAME 'uuidv_plugin.so';
INSTALL COMPONENT 'file://component_uuidv';

SELECT 'all three coexist' AS test,
       IF(LENGTH(uuidv_udf(4)) = 36
          AND LENGTH(uuidv_plugin(4)) = 36
          AND LENGTH(uuidv_component(4)) = 36,
          'PASS', 'FAIL') AS result;

DROP FUNCTION uuidv_udf;
DROP FUNCTION uuidv_plugin;
UNINSTALL PLUGIN uuidv_plugin;
UNINSTALL COMPONENT 'file://component_uuidv';
