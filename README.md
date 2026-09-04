# pdo-pglite

`pdo-pglite` is the PostgreSQL-flavored PDO driver for `php-wasm`, powered by [`@electric-sql/pglite`](https://electric-sql.com/).
It lets PHP talk to a browser-local PGlite database through the normal PDO API.

PHP 8.1+ and the Vrzno extension are required. In a custom `php-wasm` build,
keep `WITH_VRZNO=1` enabled.

## How It Is Enabled

Pass the `PGlite` constructor into the `php-wasm` runtime.
Once that happens, the `pgsql:` PDO driver becomes available inside PHP.

```js
import { PhpWeb } from 'php-wasm/PhpWeb.mjs';
import { PGlite } from '@electric-sql/pglite';

const php = new PhpWeb({
  version: '8.4',
  PGlite,
});
```

You can also load PGlite from a CDN:

```js
import { PhpWeb } from 'php-wasm/PhpWeb.mjs';
import { PGlite } from 'https://cdn.jsdelivr.net/npm/@electric-sql/pglite@0.5.8/dist/index.js';

const php = new PhpWeb({ PGlite });
```

## Opening A Database

Use a normal PDO connection, but with a `pgsql:` DSN.
For example, this DSN opens a PGlite database backed by IndexedDB storage under the name `pdo-pglite-pg18`:

```js
await php.run(`<?php
  $pdo = new PDO('pgsql:idb://pdo-pglite-pg18');
  var_dump($pdo instanceof PDO);
`);
```

If `PGlite` was not passed into the runtime, connection attempts will fail because the driver has no database constructor to instantiate.

## Querying With PDO

Prepared statements work the way you would expect.
Both positional and named placeholders are supported.

```js
await php.run(`<?php
  $pdo = new PDO('pgsql:idb://pdo-pglite-pg18');

  $pdo->exec('
    CREATE TABLE IF NOT EXISTS notes (
      id   SERIAL PRIMARY KEY,
      body TEXT NOT NULL
    )
  ');

  $insert = $pdo->prepare('INSERT INTO notes (body) VALUES (:body)');
  $insert->execute(['body' => 'hello from php']);

  $select = $pdo->prepare('SELECT id, body FROM notes ORDER BY id');
  $select->execute();

  while ($row = $select->fetch(PDO::FETCH_ASSOC)) {
    var_dump($row);
  }
`);
```

## Static HTML Usage

`pdo-pglite` can also be used from `php-tags` in a plain HTML page.
Import `PGlite` through `data-imports`, then connect through PDO from PHP:

```html
<script async type="module" src="./php-tags.mjs"></script>

<script
  type="text/php"
  data-stdout="#output"
  data-stderr="#error"
  data-imports='{
    "https://cdn.jsdelivr.net/npm/@electric-sql/pglite@0.5.8/dist/index.js": ["PGlite"]
  }'
><?php
  $pdo = new PDO('pgsql:idb://pdo-pglite-pg18');
  $pdo->exec('CREATE TABLE IF NOT EXISTS messages (body TEXT NOT NULL)');
  $pdo->prepare('INSERT INTO messages (body) VALUES (?)')->execute(['hello']);

  foreach ($pdo->query('SELECT body FROM messages') as $row) {
    echo $row['body'], PHP_EOL;
  }
?></script>

<pre id="output"></pre>
<pre id="error"></pre>
```

## Notes

- This project targets `php-wasm` runtimes. It is not a general native-PHP PostgreSQL driver.
- `phpinfo()` will report whether the `PGlite` module was detected by the runtime.
- The runtime side is intentionally simple: provide `PGlite`, then use PDO as usual.

### Upgrading Persisted Databases

PGlite 0.5 uses PostgreSQL 18. A database directory created by PGlite 0.2
(PostgreSQL 16) cannot be opened in place. Export it logically with the old
PGlite version, restore it into a new database name such as
`idb://pdo-pglite-pg18`, and switch the PDO DSN only after the restore.
Do not copy a `dumpDataDir()` archive between these PostgreSQL versions.

See the [PGlite upgrade guide](https://pglite.dev/docs/upgrade) and
[PGlite tools documentation](https://pglite.dev/docs/pglite-tools).

## Related

- `php-wasm`: <https://github.com/seanmorris/php-wasm>
- `@electric-sql/pglite`: <https://github.com/electric-sql/pglite>
