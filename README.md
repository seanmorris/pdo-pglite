# pdo-pglite

PDO driver for PGlite and php-wasm.

PDO-PGlite connects PHP's PDO interface to [PGlite](https://pglite.dev/), an
embedded PostgreSQL engine. It supports PHP 8.1 and newer compiled with
Emscripten, PDO, and Vrzno. The private `pdo-pglite-bridge` npm package supplies
this repository's JavaScript build and test tools.

## Table of Contents

- [Install](#install)
- [Usage](#usage)
	- [Browser JavaScript](#browser-javascript)
	- [Static HTML](#static-html)
	- [Node.js](#nodejs)
- [Upgrading persisted databases](#upgrading-persisted-databases)
- [Building](#building)
- [API](#api)
	- [Connections and storage](#connections-and-storage)
	- [Statements and parameters](#statements-and-parameters)
	- [Values and insert IDs](#values-and-insert-ids)
	- [Transactions and attributes](#transactions-and-attributes)
	- [Errors and limits](#errors-and-limits)
- [Maintainers](#maintainers)
- [Contributing](#contributing)
- [License](#license)

## Install

Standard php-wasm builds include this driver. Install php-wasm 0.2.0 or newer
with PGlite:

```sh
npm install php-wasm@^0.2.0 @electric-sql/pglite@0.5.8
```

The API below describes the current sources. To use a driver revision newer than
the published runtime, build php-wasm as described in [Building](#building) or
obtain matching artifacts from
[php-wasm CI](https://github.com/seanmorris/php-wasm/actions/workflows/build.yaml),
then install the generated package directory in place of `php-wasm@^0.2.0`:

```sh
npm install /absolute/path/to/php-wasm/packages/php-wasm @electric-sql/pglite@0.5.8
```

Keep its JavaScript, Wasm, and support files together. The examples use PGlite 0.5.8.

The PHP runtime must include PDO-PGlite and Vrzno. Passing the `PGlite`
constructor supplies the database engine to that compiled driver. Installing
extension sources alone does not change an existing Wasm binary.

## Usage

### Browser JavaScript

In a browser application that resolves npm imports, attach output listeners and
pass `PGlite` to `PhpWeb`:

```js
import { PhpWeb } from 'php-wasm/PhpWeb.mjs';
import { PGlite } from '@electric-sql/pglite';

const php = new PhpWeb({
	version: '8.4'
	, PGlite
});

php.addEventListener('output', event => console.log(event.detail.join('')));
php.addEventListener('error', event => console.error(event.detail.join('')));

const exitCode = await php.run(`<?php
	$pdo = new PDO('pgsql:', null, null, [
		PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION
	]);
	$pdo->exec('CREATE TABLE notes (id SERIAL PRIMARY KEY, body TEXT NOT NULL)');

	$insert = $pdo->prepare('INSERT INTO notes (body) VALUES (:body)');
	$insert->execute(['body' => 'hello from php']);

	$select = $pdo->prepare('SELECT id, body FROM notes WHERE id = ?');
	$select->execute([1]);
	echo json_encode($select->fetch(PDO::FETCH_ASSOC));
`);

if(exitCode !== 0)
{
	throw new Error('PHP execution failed');
}
```

The console output is `{"id":1,"body":"hello from php"}`. The `pgsql:` DSN
creates an in-memory database. The HTML example below uses IndexedDB for
persistence.

### Static HTML

Copy the complete built `php-wasm` package into a `php-wasm/` directory beside
this page and serve it over HTTP. `php-tags` passes the named `PGlite` export
from `data-imports` to the runtime:

```html
<!doctype html>
<meta charset="utf-8">
<title>PDO-PGlite example</title>

<script async type="module" src="./php-wasm/php-tags.mjs"></script>

<script
	type="text/php"
	data-version="8.4"
	data-stdout="#output"
	data-stderr="#error"
	data-imports='{
		"https://cdn.jsdelivr.net/npm/@electric-sql/pglite@0.5.8/dist/index.js": ["PGlite"]
	}'
><?php
	$pdo = new PDO('pgsql:idb://pdo-pglite-pg18');
	$pdo->exec('CREATE TABLE IF NOT EXISTS messages (body TEXT NOT NULL)');
	$pdo->prepare('INSERT INTO messages (body) VALUES (?)')->execute(['hello']);
	echo $pdo->query('SELECT COUNT(*) FROM messages')->fetchColumn();
?></script>

<pre id="output"></pre>
<pre id="error"></pre>
```

With a new database, the page displays `1`. Reloading it adds another message
and displays `2`. The database persists in IndexedDB for this origin. The CDN
URL can also replace the PGlite import in the JavaScript example.

### Node.js

Use `PhpNode` from `php-wasm/PhpNode.mjs` with the same `PGlite` option, output
events, and `run()` method. Use `pgsql:` for an in-memory database; IndexedDB
storage needs a browser. See [connections and storage](#connections-and-storage)
for DSN handling.

## Upgrading persisted databases

PGlite 0.5 uses PostgreSQL 18. A database directory created by PGlite 0.2
(PostgreSQL 16) cannot be opened in place. Export it logically with the old
PGlite version, restore it into a new database name such as
`idb://pdo-pglite-pg18`, and switch the PDO DSN after the restore. A
`dumpDataDir()` archive cannot migrate between these PostgreSQL versions.

See the [PGlite upgrade guide](https://pglite.dev/docs/upgrade) and
[PGlite tools documentation](https://pglite.dev/docs/pglite-tools).

## Building

From a [php-wasm](https://github.com/seanmorris/php-wasm) checkout with its builder
image available, build the browser runtime with local PDO-PGlite sources:

```sh
npm ci
make web-mjs PHP_VERSION=8.4 WITH_PDO_PGLITE=1 WITH_VRZNO=1 \
  PDO_PGLITE_DEV_PATH=/absolute/path/to/pdo-pglite
```

Use `node-mjs` for the Node runtime. Outputs go into `packages/php-wasm/`.
Omit `PDO_PGLITE_DEV_PATH` to use php-wasm's pinned upstream revision.

| Setting | Effect |
| --- | --- |
| `WITH_PDO_PGLITE=1` | Compiles the driver; enabled by default in the package's build settings. |
| `WITH_VRZNO=1` | Enables the required Vrzno extension. |
| `PDO_PGLITE_REPOSITORY` | Selects the upstream source repository. |
| `PDO_PGLITE_REF` | Selects its revision; php-wasm defaults to an immutable commit pin. |
| `PDO_PGLITE_DEV_PATH` | Uses a local checkout instead of upstream sources. |

Direct PHP extension configuration uses `--enable-pdo-pglite`. Source builds
require GNU Make 4.3 or newer and Emscripten with Asyncify enabled, plus PHP 8.1+,
PDO, and Vrzno. JavaScript database calls suspend PHP through Asyncify.

## API

### Connections and storage

The driver registers as `pgsql`; the extension name is `pdo_pglite`. Check
`extension_loaded('pdo_pglite')` for the compiled extension. `phpinfo()` also
reports whether the runtime received a PGlite module.

| PDO DSN | Storage |
| --- | --- |
| `pgsql:` | A new in-memory database for each PDO connection. |
| `pgsql:idb://pdo-pglite-pg18` | Browser IndexedDB under the supplied name. |
| `pgsql:pdo-pglite-pg18` | The same IndexedDB database; the driver adds `idb://`. |

A nonempty DSN suffix containing `://` is passed directly to the PGlite
constructor. Every other nonempty suffix gets the `idb://` prefix, including
plain filesystem paths. The suffix is a PGlite storage location; PDO username
and password arguments are unused. See [PGlite filesystems](https://pglite.dev/docs/filesystems).

Each PDO connection creates and owns a PGlite instance. Use one connection per
database at a time; the driver does not arrange sharing between browser tabs.
Missing constructors and database initialization failures become PDO errors.

### Statements and parameters

`prepare()` accepts positional `?` and named `:name` placeholders, rewritten to
PostgreSQL's `$1`, `$2`, and subsequent slots. Use `execute([...])` for ordinary
execution; `bindValue()` and `bindParam()` also work. Numeric binding positions
start at 1, while numeric `execute()` array keys start at 0. Choose one placeholder
style per statement.

`query()` fetches results and `exec()` accepts SQL scripts. `exec()` returns the
number of changed rows and excludes SELECT results. A statement's `rowCount()`
returns the number of buffered rows when there are result columns, or the
write's affected-row count otherwise. `columnCount()` works for empty results.

Results are buffered and cursors move forward. Standard PDO fetch modes and
bound columns are available. `PDO::FETCH_NUM` preserves duplicate-named columns
by position. `closeCursor()` releases the buffered result; the statement can be
executed again with new parameters.

### Values and insert IDs

PostgreSQL `bigint`, date/time, JSON, and common array types are returned as text
to preserve their values. `bytea` results become PHP binary strings, including
embedded NUL bytes. Bind binary strings or readable streams using
`PDO::PARAM_LOB`; streams are consumed from their current position and converted
to strings. SQL NULL remains distinct from an empty string.

`quote()` escapes text, including apostrophes and backslashes.
`quote($bytes, PDO::PARAM_LOB)` produces a PostgreSQL hexadecimal `bytea` literal.

`lastInsertId()` returns PostgreSQL `LASTVAL()` as a string.
`lastInsertId('notes_id_seq')` uses `CURRVAL()` for that sequence. These calls
require a sequence value to have been generated in the connection; an unset or
invalid sequence reports a PDO error.

### Transactions and attributes

`beginTransaction()`, `commit()`, and `rollBack()` execute the corresponding
PostgreSQL commands. `inTransaction()` follows PDO's own state, so use those
methods to manage transactions.

`PDO::ATTR_EMULATE_PREPARES` can be read and set. Both values use the same
parameterized PGlite query path; the driver does not retain server-side prepared
statements. `PDO::ATTR_STRINGIFY_FETCHES` controls scalar conversion when
fetching. Both `PDO::ATTR_SERVER_VERSION` and `PDO::ATTR_CLIENT_VERSION` report
the embedded PostgreSQL version.

### Errors and limits

Database failures preserve PostgreSQL's five-character SQLSTATE when available;
other bridge failures use `HY000`. PDO's error mode controls whether an operation
throws `PDOException`, emits a warning, or returns failure with `errorInfo()`.
Database errors clear the previous statement result.

Scrollable cursors, `getColumnMeta()`, and multiple result sets are unsupported.
The PDO bridge accepts a constructor and storage location; PGlite's separate
JavaScript APIs, such as live queries and multi-tab workers, are outside this
interface.

## Maintainers

[Sean Morris](https://github.com/seanmorris).

## Contributing

Use [GitHub issues](https://github.com/seanmorris/pdo-pglite/issues) for questions
and bug reports, and [pull requests](https://github.com/seanmorris/pdo-pglite/pulls)
for changes. Follow `sm-no-saccade-style` in JavaScript, including README examples.

### Edit the bridge

JavaScript bodies and their JSDoc live in [js/](js/).
[pdo_pglite_js.h.in](pdo_pglite_js.h.in) declares their C signatures.
[Makefile.frag](Makefile.frag) invokes the configured compiler with
`-E -P -CC -fdirectives-only` to expand includes before `EM_JS` and `EM_ASYNC_JS`
stringify the JavaScript. The generated code stays inside the native object;
linking it needs no separate JavaScript source files.

The generated header and dependency file live under `generated/` in the build
directory. Make tracks included files for incremental and parallel builds,
including builds outside the source directory. Generated files are excluded
from source imports and commits.

### Run tests

With Node.js, GNU Make 4.3 or newer, and Emscripten 6.0.6 on `PATH`, run:

```sh
npm ci
npm run lint
npm test
```

Lint covers `js/`, tests, and the ESLint configuration. `npm run lint:fix`
applies the formatting rules. PHP's Make build needs no npm dependencies.
[CI](.github/workflows/ci.yml) checks Node 22.23.2 and 24.5.0.

The seven Make tests cover changed and nested includes, parallel and separate
builds, missing inputs, recovery, and clean targets. The compile/link test removes
the sources and generated header, then exercises all 18 bridge calls from the
compiled object, including Asyncify calls.

For PHP integration, build a Node runtime in the php-wasm checkout and run its
[PDO-PGlite tests](https://github.com/seanmorris/php-wasm/blob/develop/packages/pdo-pglite/test/basic.mjs):

```sh
make node-mjs PHP_VERSION=8.4 WITH_PDO_PGLITE=1 WITH_VRZNO=1 \
  PDO_PGLITE_DEV_PATH=/absolute/path/to/pdo-pglite
PHP_VERSION=8.4 node --test packages/pdo-pglite/test/basic.mjs
```

These tests cover queries, parameters, binary and text values, transactions,
PDO attributes, sequence IDs, and SQLSTATE errors.

## License

Dual licensed under the [Apache License, Version 2.0](LICENSE) and the
[GNU General Public License, Version 2](LICENSE-GPL); you may use it under the
terms of either license. [CREDITS](CREDITS) names Sean Morris as the author. See [NOTICE](NOTICE).
