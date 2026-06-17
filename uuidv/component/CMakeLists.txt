# Place under components/uuidv/ in the MySQL 8.4 source tree.
# Component libraries must be named component_<name>.
MYSQL_ADD_COMPONENT(uuidv
  component_uuidv.cc
  MODULE_ONLY)
