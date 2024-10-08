dnl config.m4 for extension php-pdo-pglite
if test -z "$SED"; then
  PHP_PGLITE_SED="sed";
else
  PHP_PGLITE_SED="$SED";
fi

PHP_ARG_ENABLE([pdo_pglite],
  [whether to enable pdo_pglite support],
  [AS_HELP_STRING([--enable-pdo-pglite],
    [Enable pdo_pglite support])],
  [no])

if test "$PHP_PDO_PGLITE" != "no"; then
  tmp_version=$PHP_VERSION
  if test -z "$tmp_version"; then
    if test -z "$PHP_CONFIG"; then
      AC_MSG_ERROR([php-config not found])
    fi
    php_version=`$PHP_CONFIG --version 2>/dev/null|head -n 1|$PHP_PGLITE_SED -e 's#\([0-9]\.[0-9]*\.[0-9]*\)\(.*\)#\1#'`
  else
    php_version=`echo "$tmp_version"|$PHP_PGLITE_SED -e 's#\([0-9]\.[0-9]*\.[0-9]*\)\(.*\)#\1#'`
  fi

  ac_IFS=$IFS
  IFS="."
  set $php_version
  IFS=$ac_IFS
  pglite_php_version=`expr [$]1 \* 1000000 + [$]2 \* 1000 + [$]3`

  if test -z "$php_version"; then
    AC_MSG_ERROR([failed to detect PHP version, please report])
  fi

  if test "$pglite_php_version" -lt "8001000"; then
    AC_MSG_ERROR([You need at least PHP 8.1 to be able to use pdo_pglite])
  fi

  PHP_NEW_EXTENSION(pdo_pglite, pdo_pglite.c, $ext_shared)
fi
