# Place under plugin/uuidv_udf/ in the MySQL 8.4 source tree, or build
# standalone against installed dev headers (see README).
MYSQL_ADD_PLUGIN(uuidv_udf
  uuidv_udf.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "uuidv_udf")
