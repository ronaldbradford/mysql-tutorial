# mysql-tutorial

A collection of MySQL extension examples and tutorials.

## Contents

### `uuidv/` — A `uuidv(version)` loadable function

Returns a UUID string of the requested version. Two parallel implementations
share one self-contained generator (`uuid_gen.h`):

| Path | Interface | Loaded with |
|------|-----------|-------------|
| `uuidv/component/` | MySQL 8.4 component (recommended) | `INSTALL COMPONENT` |
| `uuidv/udf/` | Legacy loadable function (UDF) | `CREATE FUNCTION` |

Supported versions: `uuidv(1)`, `uuidv(4)`, `uuidv(6)`, `uuidv(7)`.
Versions 3 and 5 are name-based and intentionally rejected.

See [`uuidv/README.md`](uuidv/README.md) for build and usage instructions.

## License

GPL-2.0 — consistent with building against the MySQL server source tree.
