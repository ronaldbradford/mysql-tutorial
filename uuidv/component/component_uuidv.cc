/*
  uuidv() - MySQL 8.4 component implementing the loadable function via the
  udf_registration service. The component registers uuidv() on install and
  unregisters it on uninstall, so no CREATE FUNCTION is needed.

  Signature:  uuidv_component([version INT]) RETURNS STRING
  Returns a UUID string of the requested version (1, 4, 6, or 7).
  With no argument, uses the component_uuidv.default_version system variable.

  System variables (GLOBAL and SESSION scope):
    component_uuidv.default_version  INT  default 4  (range 1-7)
    component_uuidv.formatted        BOOL default ON

  Session values are tracked via thread_local mirrors updated by the variable
  update callbacks. New connections inherit the current global value.

  Install/use:
    INSTALL COMPONENT 'file://component_uuidv';
    SELECT uuidv_component();
    SELECT uuidv_component(7);
    SET GLOBAL component_uuidv.default_version = 7;
    SET SESSION component_uuidv.formatted = OFF;
    UNINSTALL COMPONENT 'file://component_uuidv';
*/

#include <climits>
#include <cstring>

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/component_status_var_service.h>

#include "../uuid_gen.h"

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);

namespace {

const char *kFuncName = "uuidv_component";

/* ---- Invocation counters (per UUID version) ------------------------------ */

static long long uuidv_count_v1 = 0;
static long long uuidv_count_v4 = 0;
static long long uuidv_count_v6 = 0;
static long long uuidv_count_v7 = 0;

static SHOW_VAR uuidv_status_vars[] = {
    {"uuidv_component_v1_count", (char *)&uuidv_count_v1,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"uuidv_component_v4_count", (char *)&uuidv_count_v4,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"uuidv_component_v6_count", (char *)&uuidv_count_v6,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"uuidv_component_v7_count", (char *)&uuidv_count_v7,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF, SHOW_SCOPE_GLOBAL}
};

/* ---- Global storage (SET GLOBAL writes here) ----------------------------- */

static int  uuidv_global_default_version = 4;
static bool uuidv_global_formatted       = true;

/*
  Thread-local mirrors: INT_MIN / -1 are sentinels meaning "no session SET has
  been executed on this connection; fall back to the global value".
*/
static thread_local int uuidv_tl_default_version = INT_MIN;
static thread_local int uuidv_tl_formatted       = -1;

/* ---- Update callbacks — maintain thread-local mirrors -------------------- */

static void update_default_version(MYSQL_THD thd, SYS_VAR *,
                                   void *val_ptr, const void *save) {
  int v = *static_cast<const int *>(save);
  *static_cast<int *>(val_ptr) = v;
  if (thd)
    uuidv_tl_default_version = v;     /* SET SESSION */
  else
    uuidv_global_default_version = v; /* SET GLOBAL  */
}

static void update_formatted(MYSQL_THD thd, SYS_VAR *,
                              void *val_ptr, const void *save) {
  bool v = *static_cast<const bool *>(save);
  *static_cast<bool *>(val_ptr) = v;
  if (thd)
    uuidv_tl_formatted = static_cast<int>(v); /* SET SESSION */
  else
    uuidv_global_formatted = v;               /* SET GLOBAL  */
}

/* ---- Helper: read effective (session-or-global) variable values ---------- */

static inline int  effective_version()  {
  return (uuidv_tl_default_version != INT_MIN)
      ? uuidv_tl_default_version
      : uuidv_global_default_version;
}

static inline bool effective_formatted() {
  return (uuidv_tl_formatted != -1)
      ? static_cast<bool>(uuidv_tl_formatted)
      : uuidv_global_formatted;
}

/* ---- UDF callbacks -------------------------------------------------------- */

bool uuidv_udf_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  if (args->arg_count > 1) {
    strcpy(message,
           "uuidv_component() takes zero or one argument: the UUID version");
    return true;
  }
  if (args->arg_count == 1)
    args->arg_type[0] = INT_RESULT;
  initid->maybe_null = true;
  initid->const_item = false;
  initid->max_length = 36;
  return false;
}

void uuidv_udf_deinit(UDF_INIT *) {}

char *uuidv_udf(UDF_INIT *, UDF_ARGS *args, char *result,
                unsigned long *length, unsigned char *is_null,
                unsigned char *error) {
  long long version;

  if (args->arg_count == 0) {
    version = effective_version();
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

  switch (static_cast<int>(version)) {
    case 1: ++uuidv_count_v1; break;
    case 4: ++uuidv_count_v4; break;
    case 6: ++uuidv_count_v6; break;
    case 7: ++uuidv_count_v7; break;
  }

  if (effective_formatted()) {
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

/* ---- Registration helpers ------------------------------------------------ */

bool register_uuidv() {
  return mysql_service_udf_registration->udf_register(
      kFuncName, STRING_RESULT, reinterpret_cast<Udf_func_any>(uuidv_udf),
      uuidv_udf_init, uuidv_udf_deinit);
}

bool unregister_uuidv() {
  int was_present = 0;
  return mysql_service_udf_registration->udf_unregister(kFuncName,
                                                        &was_present);
}

bool register_sysvars() {
  INTEGRAL_CHECK_ARG(int) version_arg;
  version_arg.def_val = 4;
  version_arg.min_val = 1;
  version_arg.max_val = 7;
  version_arg.blk_sz  = 0;
  if (mysql_service_component_sys_variable_register->register_variable(
          "component_uuidv", "default_version",
          PLUGIN_VAR_INT,
          "Default UUID version to generate (1, 4, 6, or 7). "
          "Used when uuidv_component() is called with no argument.",
          nullptr, update_default_version,
          reinterpret_cast<void *>(&version_arg),
          reinterpret_cast<void *>(&uuidv_global_default_version)))
    return true;

  BOOL_CHECK_ARG(bool) formatted_arg;
  formatted_arg.def_val = true;
  if (mysql_service_component_sys_variable_register->register_variable(
          "component_uuidv", "formatted",
          PLUGIN_VAR_BOOL | PLUGIN_VAR_THDLOCAL,
          "Return UUID with dashes (ON) or as a compact 32-character "
          "hex string (OFF).",
          nullptr, update_formatted,
          reinterpret_cast<void *>(&formatted_arg),
          reinterpret_cast<void *>(&uuidv_global_formatted))) {
    mysql_service_component_sys_variable_unregister->unregister_variable(
        "component_uuidv", "default_version");
    return true;
  }
  return false;
}

void unregister_sysvars() {
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "component_uuidv", "formatted");
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "component_uuidv", "default_version");
}

mysql_service_status_t component_init() {
  if (register_sysvars()) return 1;
  if (register_uuidv()) {
    unregister_sysvars();
    return 1;
  }
  mysql_service_status_variable_registration->register_variable(
      uuidv_status_vars);
  return 0;
}

mysql_service_status_t component_deinit() {
  mysql_service_status_variable_registration->unregister_variable(
      uuidv_status_vars);
  unregister_uuidv();
  unregister_sysvars();
  return 0;
}

}  // namespace

/* No services exported by this component. */
BEGIN_COMPONENT_PROVIDES(component_uuidv)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(component_uuidv)
REQUIRES_SERVICE(udf_registration),
REQUIRES_SERVICE(component_sys_variable_register),
REQUIRES_SERVICE(component_sys_variable_unregister),
REQUIRES_SERVICE(status_variable_registration),
END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(component_uuidv)
METADATA("mysql.author", "Ronald Bradford"),
METADATA("mysql.license", "GPL"),
METADATA("uuidv.version", "1.0"),
END_COMPONENT_METADATA();

DECLARE_COMPONENT(component_uuidv, "mysql:component_uuidv")
component_init, component_deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(component_uuidv)
END_DECLARE_LIBRARY_COMPONENTS
