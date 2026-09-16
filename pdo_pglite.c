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
#include <pdo_pglite_js.h>

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

static void pdo_pglite_clear_error_info(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = (pdo_pglite_db_handle*) dbh->driver_data;

	if(!handle)
	{
		return;
	}

	if(handle->einfo.errmsg)
	{
		pefree(handle->einfo.errmsg, dbh->is_persistent);
		handle->einfo.errmsg = NULL;
	}

	if(handle->einfo.sqlstate)
	{
		pefree(handle->einfo.sqlstate, dbh->is_persistent);
		handle->einfo.sqlstate = NULL;
	}

	handle->einfo.errcode = 0;
	handle->einfo.file = NULL;
	handle->einfo.line = 0;
}

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
	const char *normalized_sqlstate = sqlstate;

	pdo_pglite_clear_error_info(dbh);

	if(normalized_sqlstate == NULL || strlen(normalized_sqlstate) != sizeof(pdo_error_type) - 1)
	{
		normalized_sqlstate = "HY000";
	}

	handle->einfo.errcode = errcode;
	handle->einfo.file = file;
	handle->einfo.line = line;

	if(errmsg)
	{
		handle->einfo.errmsg = pestrdup(errmsg, dbh->is_persistent);
	}

	handle->einfo.sqlstate = pestrdup(normalized_sqlstate, dbh->is_persistent);
	strcpy(*pdo_err, normalized_sqlstate);

	if(!dbh->methods)
	{
		pdo_throw_exception(
			handle->einfo.errcode,
			handle->einfo.errmsg ? handle->einfo.errmsg : "PGlite operation failed",
			pdo_err
		);
	}

	return errcode;
}

void EMSCRIPTEN_KEEPALIVE pdo_pglite_create_string(const char *value, size_t length, zval *return_value)
{
	if(length)
	{
		ZVAL_STRINGL(return_value, value, length);
	}
	else
	{
		ZVAL_EMPTY_STRING(return_value);
	}
}

static void pdo_pglite_report_bridge_error(
	pdo_dbh_t *dbh,
	pdo_stmt_t *stmt,
	char *message,
	char *sqlstate,
	const char *file,
	int line
){
	pdo_pglite_error(
		dbh,
		stmt,
		1,
		sqlstate ? sqlstate : "HY000",
		message ? message : "PGlite operation failed",
		file,
		line
	);

	if(message)
	{
		free(message);
	}

	if(sqlstate)
	{
		free(sqlstate);
	}
}

// PHP_INI_BEGIN()
// PHP_INI_ENTRY("pdo_pglite.prefix", "pgsql", PHP_INI_SYSTEM|PHP_INI_PERDIR, NULL)
// PHP_INI_END()

#include "pdo_pglite_db_statement.c"
#include "pdo_pglite_db.c"

PHP_MINIT_FUNCTION(pdo_pglite)
{
	// REGISTER_INI_ENTRIES();
#if defined(ZTS) && defined(COMPILE_DL_PDO_PGLITE)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return php_pdo_register_driver(&pdo_pglite_driver);
}

PHP_MSHUTDOWN_FUNCTION(pdo_pglite)
{
	php_pdo_unregister_driver(&pdo_pglite_driver);
	// UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_MINFO_FUNCTION(pdo_pglite)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "PGlite support for PDO", "enabled");
	php_info_print_table_row(2, "PGlite module detected",
		pdo_pglite_js_available() ? "yes" : "no"
	);
	php_info_print_table_end();
	// DISPLAY_INI_ENTRIES();
}

static const zend_module_dep pdo_pglite_deps[] = {
	ZEND_MOD_REQUIRED("pdo")
	ZEND_MOD_REQUIRED("vrzno")
	ZEND_MOD_END
};

zend_module_entry pdo_pglite_module_entry = {
	STANDARD_MODULE_HEADER_EX, NULL,
	pdo_pglite_deps,
	"pdo_pglite",
	NULL,                      /* zend_function_entry */
	PHP_MINIT(pdo_pglite),     /* PHP_MINIT - Module initialization */
	PHP_MSHUTDOWN(pdo_pglite), /* PHP_MSHUTDOWN - Module shutdown */
	NULL,                      /* PHP_RINIT - Request initialization */
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
