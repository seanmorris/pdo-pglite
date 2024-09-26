# pdo-pglite

*pdo driver pglite & php-wasm*

[![Static Badge](https://img.shields.io/badge/reddit-always%20online-336699?style=for-the-badge&logo=reddit)](https://www.reddit.com/r/phpwasm/) [![Discord](https://img.shields.io/discord/1199824765666463835?style=for-the-badge&logo=discord&link=https%3A%2F%2Fdiscord.gg%2Fj8VZzju7gJ)](https://discord.gg/j8VZzju7gJ)

### Join the community: [reddit](https://www.reddit.com/r/phpwasm/) | [discord](https://discord.gg/j8VZzju7gJ)


## Install and Use

Simply pass the PGlite object into the php-wasm constructor to enable pdo_pglite support:

```javascript
import { PGlite } from '@electric-sql/pglite';

const php = new PhpWeb({PGlite});
```

You can even load PGlite from a CDN:

```javascript
import { PGlite } from 'https://cdn.jsdelivr.net/npm/@electric-sql/pglite/dist/index.js';

const php = new PhpWeb({PGlite});
```

Once PGlite is passed in, `pgsql:` will be available as a PDO driver.

```javascript
import { PGlite } from '@electric-sql/pglite';

const php = new PhpWeb({PGlite});

php.run(`<?php
    phpinfo();
    $pdo = new PDO('pgsql:idb-storage');
    $stm = $pdo->prepare('SELECT * FROM pg_catalog.pg_tables');
    $out = fopen('php://stdout', 'w');
    
    while($row = $stm->fetch()) {
        fputcsv($out, $row);
    }
`);
```

PGlite can also be used right from static HTML. Just pass it in the `data-imports` attribute on the php script tag:

```html
<html>
<body>
    <script async type = "module" src = "./php-tags.mjs"></script>
    <script
        type = "text/php"
        data-stdout = "#output"
        data-stderr = "#error"
        data-imports = '{
            "https://cdn.jsdelivr.net/npm/@electric-sql/pglite/dist/index.js": ["PGlite"]
        }'>
    <?php
        $pdo = new PDO('pgsql:idb-storage');
        $stm = $pdo->prepare('SELECT * FROM pg_catalog.pg_tables');
        $out = fopen('php://stdout', 'w');
        
        while($row = $stm->fetch()) {
            fputcsv($out, $row);
        }
    </script>
    <span id = "output"></pre>
    <span id = "error"></pre>
</body>
</html>

```

## @electric-sql/pglite

`pdo_pglite` is powered by [@electric-sql/pglite](https://electric-sql.com/). 

https://github.com/electric-sql/pglite

https://electric-sql.com/


