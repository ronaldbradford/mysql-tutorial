/*
  uuidv() - MySQL 8.4 component implementing the loadable function via the
  udf_registration service. The component registers uuidv() on install and
  unregisters it on uninstall, so no CREATE FUNCTION is needed.

  Signature:  uuidv_component([version INT]) RETURNS STRING
  Returns a UUID string of the requested version (1, 4, 6, or 7).
  With no argument, uses the uuidv_component.default_version system variable.

  System variables:
    uuidv_component.default_version  INT  GLOBAL scope  default 4  (range 1-7)
    uuidv_component.formatted        BOOL SESSION scope default ON

  Install/use:
    INSTALL COMPONENT 'file://component_uuidv';
    SELECT uuidv_component();
    SELECT uuidv_component(7);
    SET GLOBAL uuidv_component.default_version = 7;   -- GLOBAL only
    SET SESSION uuidv_component.formatted = OFF;       -- SESSION scope
    UNINSTALL COMPONENT 'file://component_uuidv';
*/

#include <chrono>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/pfs_plugin_table_service.h>

#include "../uuid_gen.h"

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_table_v1);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_tiny_v1);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_bigint_v1);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_string_v2);
REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_timestamp_v2);

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

/* ---- History ring buffer -------------------------------------------------- */

static constexpr unsigned int kHistCap = 100;

struct HistRow {
  unsigned long long thread_id;
  int                version;
  char               uuid[37];
  unsigned long long event_us;   /* microseconds since Unix epoch */
};

static HistRow      g_hist[kHistCap];
static unsigned int g_hist_next  = 0;   /* next slot to write */
static unsigned int g_hist_count = 0;   /* filled entries (0..kHistCap) */
static std::mutex   g_hist_mu;

static void hist_append(int version, const char *uuid36) {
  unsigned long long tid = static_cast<unsigned long long>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
  unsigned long long us = static_cast<unsigned long long>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count());
  std::lock_guard<std::mutex> lk(g_hist_mu);
  unsigned int slot        = g_hist_next % kHistCap;
  g_hist[slot].thread_id  = tid;
  g_hist[slot].version    = version;
  g_hist[slot].event_us   = us;
  memcpy(g_hist[slot].uuid, uuid36, 37);
  ++g_hist_next;
  if (g_hist_count < kHistCap) ++g_hist_count;
}

/* ---- P_S table handle ----------------------------------------------------- */

/*
  The position (m_ref_length bytes) is the first field so that
  *pos = (PSI_pos *)handle correctly points to it.
  The server saves/restores m_ref_length bytes there for rnd_pos.
*/
struct HistHandle {
  unsigned int idx;         /* cursor: 0-based into snapshot; UINT_MAX = before-start */
  unsigned int start_slot;  /* ring buffer slot for row 0 in this scan */
  unsigned int count;       /* snapshot of g_hist_count at rnd_init time */
};

static PSI_table_handle *hist_open(PSI_pos **pos) {
  auto *h = new HistHandle{UINT_MAX, 0, 0};
  *pos = reinterpret_cast<PSI_pos *>(h);
  return reinterpret_cast<PSI_table_handle *>(h);
}

static void hist_close(PSI_table_handle *handle) {
  delete reinterpret_cast<HistHandle *>(handle);
}

static int hist_rnd_init(PSI_table_handle *handle, bool scan) {
  auto *h = reinterpret_cast<HistHandle *>(handle);
  if (scan) {
    std::lock_guard<std::mutex> lk(g_hist_mu);
    h->count      = g_hist_count;
    h->start_slot = (g_hist_next >= kHistCap) ? g_hist_next % kHistCap : 0;
  }
  h->idx = UINT_MAX;
  return 0;
}

static int hist_rnd_next(PSI_table_handle *handle) {
  auto *h        = reinterpret_cast<HistHandle *>(handle);
  unsigned int n = (h->idx == UINT_MAX) ? 0u : h->idx + 1;
  if (n >= h->count) return PFS_HA_ERR_END_OF_FILE;
  h->idx = n;
  return 0;
}

static int hist_rnd_pos(PSI_table_handle *handle) {
  auto *h = reinterpret_cast<HistHandle *>(handle);
  return (h->idx < h->count) ? 0 : PFS_HA_ERR_RECORD_DELETED;
}

static void hist_reset_pos(PSI_table_handle *handle) {
  reinterpret_cast<HistHandle *>(handle)->idx = UINT_MAX;
}

static int hist_read_col(PSI_table_handle *handle, PSI_field *field,
                          unsigned int col) {
  auto *h          = reinterpret_cast<HistHandle *>(handle);
  unsigned int slot = (h->start_slot + h->idx) % kHistCap;
  const HistRow &r  = g_hist[slot];
  switch (col) {
    case 0: {
      PSI_ubigint v{r.thread_id, false};
      mysql_service_pfs_plugin_column_bigint_v1->set_unsigned(field, v);
      break;
    }
    case 1: {
      PSI_tinyint v{r.version, false};
      mysql_service_pfs_plugin_column_tiny_v1->set(field, v);
      break;
    }
    case 2:
      mysql_service_pfs_plugin_column_string_v2->set_varchar_utf8mb4(
          field, r.uuid);
      break;
    case 3:
      mysql_service_pfs_plugin_column_timestamp_v2->set2(field, r.event_us);
      break;
    default:
      return 1;
  }
  return 0;
}

static int hist_delete_all() {
  std::lock_guard<std::mutex> lk(g_hist_mu);
  g_hist_next  = 0;
  g_hist_count = 0;
  return 0;
}

static unsigned long long hist_row_count() { return g_hist_count; }

/* ---- P_S table share ------------------------------------------------------ */

static PFS_engine_table_share_proxy  hist_share;
static PFS_engine_table_share_proxy *hist_share_list[] = {&hist_share};

static void hist_share_setup() {
  hist_share.m_table_name        = "uuidv_history";
  hist_share.m_table_name_length = 13;
  hist_share.m_table_definition  =
      "`THREAD_ID` BIGINT UNSIGNED NOT NULL,"
      "`VERSION` TINYINT NOT NULL,"
      "`UUID` VARCHAR(36) NOT NULL,"
      "`EVENT_TIME` TIMESTAMP(6) NOT NULL";
  hist_share.m_ref_length    = sizeof(unsigned int);
  hist_share.m_acl           = TRUNCATABLE;
  hist_share.delete_all_rows = hist_delete_all;
  hist_share.get_row_count   = hist_row_count;
  auto &p = hist_share.m_proxy_engine_table;
  p.open_table        = hist_open;
  p.close_table       = hist_close;
  p.rnd_init          = hist_rnd_init;
  p.rnd_next          = hist_rnd_next;
  p.rnd_pos           = hist_rnd_pos;
  p.reset_position    = hist_reset_pos;
  p.read_column_value = hist_read_col;
}

/* ---- Global storage ------------------------------------------------------ */

/*
  default_version is a GLOBAL-only variable (PLUGIN_VAR_INT without
  PLUGIN_VAR_THDLOCAL). The server uses this pointer as the live backing
  store, so SET GLOBAL writes here directly.
*/
static int  uuidv_global_default_version = 4;

/*
  formatted is SESSION-scoped (PLUGIN_VAR_BOOL | PLUGIN_VAR_THDLOCAL).
  uuidv_global_formatted holds the current global default; uuidv_tl_formatted
  mirrors the per-session value: -1 = "use global", 0/1 = session override.
*/
static bool uuidv_global_formatted = true;
static thread_local int uuidv_tl_formatted = -1;

/* ---- Update callback for formatted --------------------------------------- */

static void update_formatted(MYSQL_THD thd, SYS_VAR *,
                              void *val_ptr, const void *save) {
  bool v = *static_cast<const bool *>(save);
  *static_cast<bool *>(val_ptr) = v;
  if (thd)
    uuidv_tl_formatted = static_cast<int>(v); /* SET SESSION */
  else
    uuidv_global_formatted = v;               /* SET GLOBAL  */
}

/* ---- Helper: read effective formatted value ------------------------------ */

static inline int  effective_version()   { return uuidv_global_default_version; }

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
  hist_append(static_cast<int>(version), buf);

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
          "uuidv_component", "default_version",
          PLUGIN_VAR_INT,
          "Default UUID version to generate (1, 4, 6, or 7). "
          "Used when uuidv_component() is called with no argument.",
          nullptr, nullptr,
          reinterpret_cast<void *>(&version_arg),
          reinterpret_cast<void *>(&uuidv_global_default_version)))
    return true;

  BOOL_CHECK_ARG(bool) formatted_arg;
  formatted_arg.def_val = true;
  if (mysql_service_component_sys_variable_register->register_variable(
          "uuidv_component", "formatted",
          PLUGIN_VAR_BOOL | PLUGIN_VAR_THDLOCAL,
          "Return UUID with dashes (ON) or as a compact 32-character "
          "hex string (OFF).",
          nullptr, update_formatted,
          reinterpret_cast<void *>(&formatted_arg),
          reinterpret_cast<void *>(&uuidv_global_formatted))) {
    mysql_service_component_sys_variable_unregister->unregister_variable(
        "uuidv_component", "default_version");
    return true;
  }
  return false;
}

bool register_history() {
  hist_share_setup();
  return mysql_service_pfs_plugin_table_v1->add_tables(hist_share_list, 1) != 0;
}

void unregister_history() {
  mysql_service_pfs_plugin_table_v1->delete_tables(hist_share_list, 1);
}

void unregister_sysvars() {
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "uuidv_component", "formatted");
  mysql_service_component_sys_variable_unregister->unregister_variable(
      "uuidv_component", "default_version");
}

mysql_service_status_t component_init() {
  if (register_sysvars()) return 1;
  if (register_uuidv()) {
    unregister_sysvars();
    return 1;
  }
  if (register_history()) {
    unregister_uuidv();
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
  unregister_history();
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
REQUIRES_SERVICE(pfs_plugin_table_v1),
REQUIRES_SERVICE(pfs_plugin_column_tiny_v1),
REQUIRES_SERVICE(pfs_plugin_column_bigint_v1),
REQUIRES_SERVICE(pfs_plugin_column_string_v2),
REQUIRES_SERVICE(pfs_plugin_column_timestamp_v2),
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
