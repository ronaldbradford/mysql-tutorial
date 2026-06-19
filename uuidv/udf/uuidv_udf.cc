/*
  uuidv() - legacy loadable function (UDF) plugin for MySQL 8.4.

  Signature:  uuidv(version INT) RETURNS STRING
  Returns a UUID string of the requested version (1, 4, 6, or 7).

  Build/install:
    CREATE FUNCTION uuidv_udf RETURNS STRING SONAME 'uuidv_udf.so';
    SELECT uuidv_udf(4);
    DROP FUNCTION uuidv_udf;
*/

#include <cstring>
#include <cstdlib>

#include <mysql/udf_registration_types.h>

#include "../uuid_gen.h"

extern "C" {

bool uuidv_udf_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  if (args->arg_count != 1) {
    strcpy(message, "uuidv() requires exactly one argument: the UUID version");
    return true;
  }
  /* Coerce the argument to INT so callers may pass it as a numeric literal. */
  args->arg_type[0] = INT_RESULT;

  initid->maybe_null = true;
  initid->const_item = false;
  initid->max_length = 36;
  return false;
}

void uuidv_udf_deinit(UDF_INIT *) {}

char *uuidv_udf(UDF_INIT *, UDF_ARGS *args, char *result, 
		unsigned long *length, char *is_null, char *error) {
  if (args->args[0] == nullptr) {
    *is_null = 1;
    return nullptr;
  }

  long long version = *reinterpret_cast<long long *>(args->args[0]);

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

  memcpy(result, buf, 36);  /* result buffer is 255 bytes */
  *length = 36;
  return result;
}

}  /* extern "C" */
