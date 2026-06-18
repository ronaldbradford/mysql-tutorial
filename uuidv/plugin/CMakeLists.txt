# Place under plugin/uuidv_plugin/ in the MySQL 8.4 source tree.
MYSQL_ADD_PLUGIN(uuidv_plugin
  uuidv_plugin.cc
  MODULE_ONLY
  MODULE_OUTPUT_NAME "uuidv_plugin")
