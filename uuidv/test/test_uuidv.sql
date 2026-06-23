-- test_uuidv.sql — functional tests for all three uuidv implementations.
-- Run against a MySQL 8.4 instance that has the plugin-dir pointing to the
-- directory where uuidv_udf.so, uuidv_plugin.so, and component_uuidv.so live.
--
-- Usage:
--   mysql -uroot -f -S /tmp/mysql-8.4-test/mysql.sock < test/test_uuidv.sql
--
-- The -f flag is required: UNINSTALL PLUGIN/COMPONENT have no IF EXISTS and
-- will error harmlessly when the plugin/component is not loaded (e.g. on the
-- first run or after a clean shutdown).

\W

-- ========================================================================
-- Cleanup: drop anything left over from a previous aborted run.
-- UNINSTALL PLUGIN / COMPONENT have no IF EXISTS; errors here are expected
-- on a clean instance and are suppressed by the -f flag.
-- ========================================================================

DROP FUNCTION IF EXISTS uuidv_udf;
DROP FUNCTION IF EXISTS uuidv_plugin;
UNINSTALL PLUGIN uuidv_plugin;
UNINSTALL COMPONENT 'file://component_uuidv';

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

-- System variables: default_version
SELECT 'uuidv_plugin sysvar default_version exists' AS test,
       IF(COUNT(*) = 1, 'PASS', 'FAIL') AS result
FROM   performance_schema.global_variables
WHERE  VARIABLE_NAME = 'uuidv_plugin_default_version';

SELECT 'uuidv_plugin sysvar default_version default' AS test,
       IF(@@uuidv_plugin_default_version = 4, 'PASS', 'FAIL') AS result;

SET SESSION uuidv_plugin_default_version = 7;
SET @d = uuidv_plugin();
SELECT 'uuidv_plugin default_version=7 no-arg call' AS test,
       IF(SUBSTRING(@d, 15, 1) = '7', 'PASS', 'FAIL') AS result;
SET SESSION uuidv_plugin_default_version = 4;

-- System variables: formatted (dashes vs compact)
SELECT 'uuidv_plugin sysvar formatted exists' AS test,
       IF(COUNT(*) = 1, 'PASS', 'FAIL') AS result
FROM   performance_schema.global_variables
WHERE  VARIABLE_NAME = 'uuidv_plugin_formatted';

SELECT 'uuidv_plugin formatted=ON length' AS test,
       IF(LENGTH(uuidv_plugin(4)) = 36, 'PASS', 'FAIL') AS result;

SET SESSION uuidv_plugin_formatted = OFF;
SELECT 'uuidv_plugin formatted=OFF length' AS test,
       IF(LENGTH(uuidv_plugin(4)) = 32, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_plugin formatted=OFF no dashes' AS test,
       IF(LOCATE('-', CAST(uuidv_plugin(4) AS CHAR)) = 0, 'PASS', 'FAIL') AS result;
SET SESSION uuidv_plugin_formatted = ON;

-- Status variables: invocation counters
SELECT 'uuidv_plugin v4_count exists' AS test,
       IF(COUNT(*) = 1, 'PASS', 'FAIL') AS result
FROM   performance_schema.global_status
WHERE  VARIABLE_NAME = 'uuidv_plugin_v4_count';

SET @before = (SELECT CAST(VARIABLE_VALUE AS SIGNED)
               FROM performance_schema.global_status
               WHERE VARIABLE_NAME = 'uuidv_plugin_v7_count');
SELECT uuidv_plugin(7), uuidv_plugin(7), uuidv_plugin(7);
SET @after = (SELECT CAST(VARIABLE_VALUE AS SIGNED)
              FROM performance_schema.global_status
              WHERE VARIABLE_NAME = 'uuidv_plugin_v7_count');
SELECT 'uuidv_plugin v7_count increments' AS test,
       IF(@after - @before = 3, 'PASS', 'FAIL') AS result;

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

-- System variables: default_version
SELECT 'uuidv_component sysvar default_version exists' AS test,
       IF(COUNT(*) = 1, 'PASS', 'FAIL') AS result
FROM   performance_schema.global_variables
WHERE  VARIABLE_NAME = 'component_uuidv.default_version';

SELECT 'uuidv_component sysvar default_version default' AS test,
       IF(@@`component_uuidv.default_version` = 4, 'PASS', 'FAIL') AS result;

SET GLOBAL `component_uuidv.default_version` = 7;
SET @d = uuidv_component();
SELECT 'uuidv_component default_version=7 no-arg call' AS test,
       IF(SUBSTRING(@d, 15, 1) = '7', 'PASS', 'FAIL') AS result;
SET GLOBAL `component_uuidv.default_version` = 4;

-- System variables: formatted (dashes vs compact)
SELECT 'uuidv_component sysvar formatted exists' AS test,
       IF(COUNT(*) = 1, 'PASS', 'FAIL') AS result
FROM   performance_schema.global_variables
WHERE  VARIABLE_NAME = 'component_uuidv.formatted';

SELECT 'uuidv_component formatted=ON length' AS test,
       IF(LENGTH(uuidv_component(4)) = 36, 'PASS', 'FAIL') AS result;

SET SESSION `component_uuidv.formatted` = OFF;
SELECT 'uuidv_component formatted=OFF length' AS test,
       IF(LENGTH(uuidv_component(4)) = 32, 'PASS', 'FAIL') AS result;
SELECT 'uuidv_component formatted=OFF no dashes' AS test,
       IF(LOCATE('-', uuidv_component(4)) = 0, 'PASS', 'FAIL') AS result;
SET SESSION `component_uuidv.formatted` = ON;

-- Performance Schema history table
SELECT 'uuidv_history table exists' AS test,
       IF(COUNT(*) = 1, 'PASS', 'FAIL') AS result
FROM   information_schema.TABLES
WHERE  TABLE_SCHEMA = 'performance_schema'
  AND  TABLE_NAME   = 'uuidv_history';

SET @before = (SELECT COUNT(*) FROM performance_schema.uuidv_history);
SELECT uuidv_component(4), uuidv_component(7);
SET @after = (SELECT COUNT(*) FROM performance_schema.uuidv_history);
SELECT 'uuidv_history rows appended' AS test,
       IF(@after - @before = 2, 'PASS', 'FAIL') AS result;

SELECT 'uuidv_history has UUID column' AS test,
       IF(COUNT(*) > 0, 'PASS', 'FAIL') AS result
FROM   performance_schema.uuidv_history
WHERE  LENGTH(UUID) = 36;

SELECT 'uuidv_history has EVENT_TIME' AS test,
       IF(COUNT(*) > 0, 'PASS', 'FAIL') AS result
FROM   performance_schema.uuidv_history
WHERE  EVENT_TIME > '2020-01-01';

TRUNCATE TABLE performance_schema.uuidv_history;
SELECT 'uuidv_history truncate clears rows' AS test,
       IF(COUNT(*) = 0, 'PASS', 'FAIL') AS result
FROM   performance_schema.uuidv_history;

-- Status variables: invocation counters
SELECT 'uuidv_component v4_count exists' AS test,
       IF(COUNT(*) = 1, 'PASS', 'FAIL') AS result
FROM   performance_schema.global_status
WHERE  VARIABLE_NAME = 'uuidv_component_v4_count';

SET @before = (SELECT CAST(VARIABLE_VALUE AS SIGNED)
               FROM performance_schema.global_status
               WHERE VARIABLE_NAME = 'uuidv_component_v7_count');
SELECT uuidv_component(7), uuidv_component(7), uuidv_component(7);
SET @after = (SELECT CAST(VARIABLE_VALUE AS SIGNED)
              FROM performance_schema.global_status
              WHERE VARIABLE_NAME = 'uuidv_component_v7_count');
SELECT 'uuidv_component v7_count increments' AS test,
       IF(@after - @before = 3, 'PASS', 'FAIL') AS result;

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
