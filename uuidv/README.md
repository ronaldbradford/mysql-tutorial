# uuidv() for MySQL 8.4

`uuidv(version)` returns a UUID string of the requested version.

| Argument | Result |
|----------|--------|
| `uuidv(1)` | time-based (Gregorian ticks + random node) |
| `uuidv(4)` | random |
| `uuidv(6)` | reordered time-based (sortable) |
| `uuidv(7)` | Unix-ms time-ordered |

Versions 3 and 5 are name-based (need a namespace + name) and are
intentionally rejected — they cannot be produced from a version number alone.
Any unsupported version raises an error.

Two implementations are provided:

* `udf/` — legacy loadable function (UDF) plugin, loaded with `CREATE FUNCTION`.
* `component/` — modern component, loaded with `INSTALL COMPONENT`. Preferred
  for new work.

`uuid_gen.h` contains the shared, self-contained generation logic.

---

## Component (recommended)

Build inside the MySQL 8.4 source tree:

```bash
cp -r component <mysql-src>/components/uuidv
cp uuid_gen.h <mysql-src>/components/uuidv/../uuid_gen.h   # or adjust the include
cd <mysql-src>
mkdir -p build && cd build
cmake .. -DWITH_DEBUG=0
make component_uuidv
```

Copy `component_uuidv.so` into the directory named by `plugin_dir`, then:

```sql
INSTALL COMPONENT 'file://component_uuidv';
SELECT uuidv(4);
SELECT uuidv(7);
UNINSTALL COMPONENT 'file://component_uuidv';
```

The function is registered automatically on install and removed on uninstall;
no `CREATE FUNCTION` is needed.

## UDF plugin

Build inside the MySQL 8.4 source tree:

```bash
cp -r udf <mysql-src>/plugin/uuidv_udf
cd <mysql-src> && mkdir -p build && cd build
cmake .. && make uuidv_udf
```

Copy `uuidv_udf.so` into `plugin_dir`, then:

```sql
CREATE FUNCTION uuidv RETURNS STRING SONAME 'uuidv_udf.so';
SELECT uuidv(4);
DROP FUNCTION uuidv;
```

## Notes

* Both versions coerce the argument to `INT_RESULT`, so `uuidv(4)`,
  `uuidv('4')`, and `uuidv(col)` all work.
* `const_item` is false so each row gets a distinct value.
* The component is the forward-looking interface; the UDF interface is retained
  for compatibility but Oracle recommends components for new functions.
