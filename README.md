# mysql-tutorial

A collection of MySQL extension examples and tutorials.

## Contents

### `uuidv/` — A `uuidv(version)` loadable function

Returns a UUID string of the requested version. Three parallel implementations
share one self-contained generator (`uuid_gen.h`) and can all be loaded
simultaneously:

| Path | Function | Interface | Loaded with |
|------|----------|-----------|-------------|
| `uuidv/component/` | `uuidv_component(version)` | MySQL 8.4 component (recommended) | `INSTALL COMPONENT` |
| `uuidv/plugin/` | `uuidv_plugin(version)` | Daemon server plugin | `INSTALL PLUGIN` + `CREATE FUNCTION` |
| `uuidv/udf/` | `uuidv_udf(version)` | Legacy loadable function (UDF) | `CREATE FUNCTION` |

Supported versions: `1` (time-based), `4` (random), `6` (reordered time-based),
`7` (Unix-ms time-ordered). Versions 3 and 5 are name-based and intentionally
rejected.

See [`uuidv/README.md`](uuidv/README.md) for build and usage instructions.

## License

GPL-2.0 — consistent with building against the MySQL server source tree.
