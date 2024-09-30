dnl config.m4 for extension php-pdo-pglite

PHP_ARG_ENABLE([pdo_pglite],
  [whether to enable pdo_pglite support],
  [AS_HELP_STRING([--enable-pdo-pglite],
    [Enable pdo_pglite support])],
  [no])

if test "$PHP_PDO_PGLITE" != "no"; then
  PHP_NEW_EXTENSION(pdo_pglite, pdo_pglite.c, $ext_shared)
fi
