# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Project

`mysql-tutorial` — MySQL extension examples. The first example is `uuidv/`, a
loadable function implemented both as an 8.4 component and a legacy UDF.

## Coding standards

- No trailing whitespace on any line, in any language.
- Python shebang is always `#!/usr/bin/env python` (never `python3`).
- C++ for MySQL extensions follows server source conventions: `extern "C"`
  linkage for UDF entry points, components use the
  `mysql/components/services/*` service APIs.

## Building

The C++ extensions build against the MySQL 8.4 source tree (they rely on
`MYSQL_ADD_COMPONENT` / `MYSQL_ADD_PLUGIN` CMake macros and server headers).
See `uuidv/README.md` for the build steps.

## Layout

```
uuidv/
  uuid_gen.h          shared, self-contained UUID generation (v1/4/6/7)
  component/          MySQL 8.4 component implementation
  udf/                legacy UDF plugin implementation
```
