/* pdo_pglite extension for PHP */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "../pdo/php_pdo_driver.h"
#include "php_pdo_pglite.h"
#include "zend_API.h"
#include "zend_types.h"
#include "zend_closures.h"
#include <emscripten.h>
#include "zend_hash.h"

#if PHP_MAJOR_VERSION >= 8
# include "zend_attributes.h"
#else
# include <stdbool.h>
#endif

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

int pdo_pglite_error(
	pdo_dbh_t *dbh,
	pdo_stmt_t *stmt,
	int errcode,
	const char *sqlstate,
	const char *errmsg,
	const char *file,
	int line
){
	pdo_pglite_db_handle *handle = (pdo_pglite_db_handle*) dbh->driver_data;
	pdo_error_type *pdo_err = stmt ? &stmt->error_code : &dbh->error_code;
	// pdo_pglite_error_info *einfo = &handle->einfo;

	handle->einfo.errcode = errcode;
	handle->einfo.file = file;
	handle->einfo.line = line;

	if(errmsg)
	{
		handle->einfo.errmsg = pestrdup(errmsg, dbh->is_persistent);
	}

	if(sqlstate)
	{
		handle->einfo.sqlstate = pestrdup(sqlstate, dbh->is_persistent);
	}

	if(sqlstate == NULL || strlen(sqlstate) >= sizeof(pdo_error_type))
	{
		strcpy(*pdo_err, "HY000");
	}
	else
	{
		strcpy(*pdo_err, sqlstate);
	}

	if(!dbh->methods)
	{
		pdo_throw_exception(handle->einfo.errcode, handle->einfo.errmsg, pdo_err);
	}

	return errcode;
}

#include "pdo_pglite_db_statement.c"
#include "pdo_pglite_db.c"

PHP_INI_BEGIN()
PHP_INI_ENTRY("pdo_pglite.prefix", "pgsql", PHP_INI_SYSTEM|PHP_INI_PERDIR, NULL)
PHP_INI_END()

PHP_RINIT_FUNCTION(pdo_pglite)
{
	return SUCCESS;
}

PHP_MINIT_FUNCTION(pdo_pglite)
{
	REGISTER_INI_ENTRIES();
#if defined(ZTS) && defined(COMPILE_DL_PDO_PGLITE)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return php_pdo_register_driver(&pdo_pglite_driver);
}

PHP_MINFO_FUNCTION(pdo_pglite)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "PGlite support for PDO", "enabled");
	php_info_print_table_row(2, "PGlite module detected",
		EM_ASM_INT({ return !!Module.PGlite }) ? "yes" : "no"
	);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

PHP_MSHUTDOWN_FUNCTION(pdo_pglite)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

zend_module_entry pdo_pglite_module_entry = {
	STANDARD_MODULE_HEADER,
	"pdo_pglite",
	NULL,                      /* zend_function_entry */
	PHP_MINIT(pdo_pglite),     /* PHP_MINIT - Module initialization */
	PHP_MSHUTDOWN(pdo_pglite), /* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(pdo_pglite),     /* PHP_RINIT - Request initialization */
	NULL,                      /* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(pdo_pglite),     /* PHP_MINFO - Module info */
	PHP_PDO_PGLITE_VERSION,    /* Version */
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PDO_PGLITE
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(pdo_pglite)
#endif
