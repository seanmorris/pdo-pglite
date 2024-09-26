dnl config.m4 for extension php-pdo-pglite

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary.

dnl If your extension references something external, use 'with':

dnl PHP_ARG_WITH([vrzno],
dnl   [for vrzno support],
dnl   [AS_HELP_STRING([--with-vrzno],
dnl     [Include vrzno support])])

dnl Otherwise use 'enable':

PHP_ARG_ENABLE([pdo_pglite],
  [whether to enable pdo_pglite support],
  [AS_HELP_STRING([--enable-pdo-pglite],
    [Enable pdo_pglite support])],
  [no])

if test "$PHP_PDO_PGLITE" != "no"; then
  PHP_NEW_EXTENSION(pdo_pglite, pdo_pglite.c, $ext_shared)
fi
