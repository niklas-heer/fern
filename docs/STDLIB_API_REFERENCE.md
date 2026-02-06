# Fern Standard Library API Reference (Gate C)

Last updated: 2026-02-06

This file is the canonical signature reference for the Gate C core standard-library modules.
It complements `docs/COMPATIBILITY_POLICY.md` with concrete function-level contracts.

## Scope

The APIs below are treated as stable in Gate C:

1. `fs`
2. `json`
3. `http`
4. `sql`
5. `actors`

Compatibility alias:

1. `File.*` is a compatibility alias for `fs.*`.

## Error Code Type

Current placeholder/runtime-facing APIs use `Int` for error codes.

1. `Result(T, Int)` means `Err(Int)` where integer values map to runtime error constants.

## Module Signatures

### `fs`

```fern
fs.read(path: String) -> Result(String, Int)
fs.write(path: String, content: String) -> Result(Int, Int)
fs.append(path: String, content: String) -> Result(Int, Int)
fs.exists(path: String) -> Bool
fs.delete(path: String) -> Result(Int, Int)
fs.size(path: String) -> Result(Int, Int)
```

### `json`

```fern
json.parse(text: String) -> Result(String, Int)
json.stringify(text: String) -> Result(String, Int)
```

### `http`

```fern
http.get(url: String) -> Result(String, Int)
http.post(url: String, body: String) -> Result(String, Int)
```

### `sql`

```fern
sql.open(path: String) -> Result(Int, Int)
sql.execute(handle: Int, query: String) -> Result(Int, Int)
```

### `actors`

```fern
actors.start(name: String) -> Int
actors.post(actor_id: Int, message: String) -> Result(Int, Int)
actors.next(actor_id: Int) -> Result(String, Int)
```

### `File` compatibility alias

```fern
File.read(path: String) -> Result(String, Int)
File.write(path: String, content: String) -> Result(Int, Int)
File.append(path: String, content: String) -> Result(Int, Int)
File.exists(path: String) -> Bool
File.delete(path: String) -> Result(Int, Int)
File.size(path: String) -> Result(Int, Int)
```

## Stability Enforcement

These API contracts are enforced by type-checker tests:

1. `test_check_fs_api_signatures`
2. `test_check_json_api_signatures`
3. `test_check_http_api_signatures`
4. `test_check_sql_api_signatures`
5. `test_check_actors_api_signatures`
6. `test_check_file_alias_api_signatures`

See `tests/test_checker.c`.
