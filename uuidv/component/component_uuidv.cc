/*
  uuidv() - MySQL 8.4 component implementing the loadable function via the
  udf_registration service. The component registers uuidv() on install and
  unregisters it on uninstall, so no CREATE FUNCTION is needed.

  Signature:  uuidv(version INT) RETURNS STRING
  Returns a UUID string of the requested version (1, 4, 6, or 7).

  Install/use:
    INSTALL COMPONENT 'file://component_uuidv';
    SELECT uuidv(4);
    UNINSTALL COMPONENT 'file://component_uuidv';
*/

#include <cstring>

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/components/services/mysql_runtime_error_service.h>
#include <mysqld_error.h>

#include "../uuid_gen.h"

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);

namespace {

const char *kFuncName = "uuidv";

bool uuidv_udf_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  if (args->arg_count != 1) {
    strcpy(message, "uuidv() requires exactly one argument: the UUID version");
    return true;
  }
  args->arg_type[0] = INT_RESULT;
  initid->maybe_null = false;
  initid->const_item = false;
  initid->max_length = 36;
  return false;
}

void uuidv_udf_deinit(UDF_INIT *) {}

char *uuidv_udf(UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
                unsigned char *is_null, unsigned char *error) {
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

mysql_service_status_t component_init() {
  return register_uuidv() ? 1 : 0;  /* nonzero == failure */
}

mysql_service_status_t component_deinit() {
  return unregister_uuidv() ? 1 : 0;
}

}  // namespace

/* No services exported by this component. */
BEGIN_COMPONENT_PROVIDES(component_uuidv)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(component_uuidv)
REQUIRES_SERVICE(udf_registration),
REQUIRES_SERVICE(mysql_runtime_error),
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
