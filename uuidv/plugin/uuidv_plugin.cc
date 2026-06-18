/*
  uuidv() - MySQL 8.4 daemon server plugin.

  This is a hybrid implementation:
    - INSTALL PLUGIN registers it as a daemon server plugin (visible in
      SHOW PLUGINS / information_schema.PLUGINS).
    - CREATE FUNCTION registers the UDF entry points exposed by the same .so.

  Both steps are required, unlike the component which auto-registers.

  Signature:  uuidv_plugin(version INT) RETURNS STRING
  Returns a UUID string of the requested version (1, 4, 6, or 7).

  Install/use:
    INSTALL PLUGIN uuidv_plugin SONAME 'uuidv_plugin.so';
    CREATE FUNCTION uuidv_plugin RETURNS STRING SONAME 'uuidv_plugin.so';
    SELECT uuidv_plugin(4);
    DROP FUNCTION uuidv_plugin;
    UNINSTALL PLUGIN uuidv_plugin;
*/

#include <cstring>

#include <mysql/mysql_version.h>  /* MYSQL_VERSION_ID — needed before plugin.h with MYSQL_ABI_CHECK */
#include <mysql/attribute.h>       /* MY_ATTRIBUTE — needed for MYSQL_PLUGIN_EXPORT */
#include <mysql/plugin.h>

#include <mysql/udf_registration_types.h>

#include "../uuid_gen.h"

/* ---- Daemon plugin lifecycle --------------------------------------------- */

static int daemon_init(void *) { return 0; }
static int daemon_deinit(void *) { return 0; }

static struct st_mysql_daemon uuidv_daemon = {MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(uuidv_plugin){
    MYSQL_DAEMON_PLUGIN,
    &uuidv_daemon,
    "uuidv_plugin",
    "Ronald Bradford",
    "UUID version 1/4/6/7 generator daemon plugin",
    PLUGIN_LICENSE_GPL,
    daemon_init,
    nullptr,  /* check_uninstall */
    daemon_deinit,
    0x0100,  /* version 1.0 */
    nullptr, /* status variables */
    nullptr, /* system variables */
    nullptr, /* __reserved1 */
    0,       /* flags */
} mysql_declare_plugin_end;

/* ---- UDF entry points (extern "C" for CREATE FUNCTION) ------------------- */

extern "C" {

bool uuidv_plugin_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  if (args->arg_count != 1) {
    strcpy(message, "uuidv_plugin() requires exactly one argument: the UUID version");
    return true;
  }
  args->arg_type[0] = INT_RESULT;
  initid->maybe_null = true;
  initid->const_item = false;
  initid->max_length = 36;
  return false;
}

void uuidv_plugin_deinit(UDF_INIT *) {}

char *uuidv_plugin(UDF_INIT *, UDF_ARGS *args, char *result,
                   unsigned long *length, char *is_null, char *error) {
  if (args->args[0] == nullptr) {
    *is_null = 1;
    return nullptr;
  }

  long long version = *reinterpret_cast<long long *>(args->args[0]);

  char buf[37];
  if (!uuidv_gen::supported(static_cast<int>(version)) ||
      !uuidv_gen::generate(static_cast<int>(version), buf)) {
    *error = 1;
    *is_null = 1;
    return nullptr;
  }

  memcpy(result, buf, 36);  /* result buffer is 255 bytes */
  *length = 36;
  return result;
}

}  /* extern "C" */
