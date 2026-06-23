cmake_minimum_required(VERSION 3.15)
project(uuidv_extensions CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MySQL 8.4 source tree (for server plugin / component headers).
# Override with: cmake -DMYSQL_SRC=<path> ..
set(MYSQL_SRC "$ENV{HOME}/git/mysql-8.4-src" CACHE PATH
    "MySQL 8.4 source tree (needed for plugin.h and component headers)")

set(MYSQL_INC "${MYSQL_SRC}/include")

# Homebrew install used for plugin_dir default and UDF client headers.
set(BREW_PREFIX "/opt/homebrew/opt/mysql@8.4")
set(MYSQL_PLUGIN_DIR "${BREW_PREFIX}/lib/plugin" CACHE PATH
    "Directory to install .so files into")

# ---- Shared compile flags ------------------------------------------------
# MYSQL_DYNAMIC_PLUGIN  : marks the library as a loadable plugin
# MYSQL_ABI_CHECK       : suppresses sql/sql_plugin.h pull in plugin.h
#                         (we don't use plugin_thdvar_safe_update)
set(COMMON_FLAGS
    -DMYSQL_DYNAMIC_PLUGIN
    -DMYSQL_ABI_CHECK
    -fvisibility=hidden
)
# UDF entry points are plain extern "C" — no MYSQL_PLUGIN_EXPORT macro wraps
# them, so they must remain visible even with -fvisibility=hidden.
set(UDF_FLAGS
    -DMYSQL_DYNAMIC_PLUGIN
    -DMYSQL_ABI_CHECK
)

# ---- UDF (uuidv_udf) -----------------------------------------------------
add_library(uuidv_udf MODULE udf/uuidv_udf.cc)
target_include_directories(uuidv_udf PRIVATE
    "${MYSQL_INC}"
)
target_compile_options(uuidv_udf PRIVATE ${UDF_FLAGS})
set_target_properties(uuidv_udf PROPERTIES
    PREFIX "" SUFFIX ".so" OUTPUT_NAME "uuidv_udf")

# ---- Plugin (uuidv_plugin) -----------------------------------------------
add_library(uuidv_plugin MODULE plugin/uuidv_plugin.cc)
target_include_directories(uuidv_plugin PRIVATE
    "${MYSQL_INC}"
)
target_compile_options(uuidv_plugin PRIVATE ${UDF_FLAGS})
# thd_get_current_thd() is exported by mysqld and resolved at dlopen time;
# -undefined dynamic_lookup suppresses the macOS link-time error.
if(APPLE)
  target_link_options(uuidv_plugin PRIVATE -undefined dynamic_lookup)
endif()
set_target_properties(uuidv_plugin PROPERTIES
    PREFIX "" SUFFIX ".so" OUTPUT_NAME "uuidv_plugin")

# ---- Component (component_uuidv) -----------------------------------------
add_library(component_uuidv MODULE component/component_uuidv.cc)
target_include_directories(component_uuidv PRIVATE
    "${MYSQL_INC}"
)
target_compile_options(component_uuidv PRIVATE ${COMMON_FLAGS})
set_target_properties(component_uuidv PROPERTIES
    PREFIX "" SUFFIX ".so" OUTPUT_NAME "component_uuidv")

# ---- Install -------------------------------------------------------------
install(TARGETS uuidv_udf uuidv_plugin component_uuidv
        DESTINATION "${MYSQL_PLUGIN_DIR}")
