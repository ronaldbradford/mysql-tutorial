/*
  uuidv() - MySQL 8.4 daemon server plugin.

  This is a hybrid implementation:
    - INSTALL PLUGIN registers it as a daemon server plugin (visible in
      SHOW PLUGINS / information_schema.PLUGINS).
    - CREATE FUNCTION registers the UDF entry points exposed by the same .so.

  Both steps are required, unlike the component which auto-registers.

  Signature:  uuidv_plugin([version INT]) RETURNS STRING
  Returns a UUID string of the requested version (1, 4, 6, or 7).
  With no argument, uses the uuidv_plugin_default_version system variable.

  System variables (GLOBAL and SESSION scope):
    uuidv_plugin_default_version  INT  default 4  (range 1-7)
    uuidv_plugin_formatted        BOOL default ON

  Install/use:
    INSTALL PLUGIN uuidv_plugin SONAME 'uuidv_plugin.so';
    CREATE FUNCTION uuidv_plugin RETURNS STRING SONAME 'uuidv_plugin.so';
    SELECT uuidv_plugin();
    SELECT uuidv_plugin(7);
    SET GLOBAL uuidv_plugin_default_version = 7;
    SET SESSION uuidv_plugin_formatted = OFF;
    DROP FUNCTION uuidv_plugin;
    UNINSTALL PLUGIN uuidv_plugin;
*/

#include <cstring>

#include <mysql/mysql_version.h>  /* MYSQL_VERSION_ID — needed before plugin.h with MYSQL_ABI_CHECK */
#include <mysql/attribute.h>       /* MY_ATTRIBUTE — needed for MYSQL_PLUGIN_EXPORT */
#include <mysql/plugin.h>

#include <mysql/udf_registration_types.h>

#include "../uuid_gen.h"

/* plugin.h forward-declares class THD; thd_get_current_thd() is exported by mysqld */
extern THD *thd_get_current_thd();

/* ---- Session-scoped system variables ------------------------------------- */

static MYSQL_THDVAR_INT(default_version,
    PLUGIN_VAR_RQCMDARG,
    "Default UUID version to generate (1, 4, 6, or 7). "
    "Used when uuidv_plugin() is called with no argument.",
    nullptr, nullptr,
    4,   /* default */
    1,   /* min */
    7,   /* max */
    0);  /* block size */

static MYSQL_THDVAR_BOOL(formatted,
    PLUGIN_VAR_RQCMDARG,
    "Return UUID with dashes (ON) or as a compact 32-character hex string (OFF).",
    nullptr, nullptr, true);

static SYS_VAR *uuidv_sysvars[] = {
    MYSQL_SYSVAR(default_version),
    MYSQL_SYSVAR(formatted),
    nullptr
};

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
    nullptr,       /* check_uninstall */
    daemon_deinit,
    0x0100,        /* version 1.0 */
    nullptr,       /* status variables */
    uuidv_sysvars, /* system variables */
    nullptr,       /* __reserved1 */
    0,             /* flags */
} mysql_declare_plugin_end;

/* ---- UDF entry points (extern "C" for CREATE FUNCTION) ------------------- */

extern "C" {

bool uuidv_plugin_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  if (args->arg_count > 1) {
    strcpy(message,
           "uuidv_plugin() takes zero or one argument: the UUID version");
    return true;
  }
  if (args->arg_count == 1)
    args->arg_type[0] = INT_RESULT;
  initid->maybe_null = true;
  initid->const_item = false;
  initid->max_length = 36;
  return false;
}

void uuidv_plugin_deinit(UDF_INIT *) {}

char *uuidv_plugin(UDF_INIT *, UDF_ARGS *args, char *result,
                   unsigned long *length, char *is_null, char *error) {
  THD *thd = thd_get_current_thd();

  long long version;
  if (args->arg_count == 0) {
    version = THDVAR(thd, default_version);
  } else {
    if (args->args[0] == nullptr) {
      *is_null = 1;
      return nullptr;
    }
    version = *reinterpret_cast<long long *>(args->args[0]);
  }

  if (!uuidv_gen::supported(static_cast<int>(version))) {
    *error = 1;
    *is_null = 1;
    return nullptr;
  }

  char buf[37];
  if (!uuidv_gen::generate(static_cast<int>(version), buf)) {
    *error = 1;
    *is_null = 1;
    return nullptr;
  }

  if (THDVAR(thd, formatted)) {
    memcpy(result, buf, 36);
    *length = 36;
  } else {
    unsigned long n = 0;
    for (int i = 0; i < 36; i++) {
      if (buf[i] != '-') result[n++] = buf[i];
    }
    *length = n;  /* 32 */
  }
  return result;
}

}  /* extern "C" */
