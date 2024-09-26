/* pdo_pglite extension for PHP */

#ifndef PHP_PDO_PGLITE_H
# define PHP_PDO_PGLITE_H

extern zend_module_entry pdo_pglite_module_entry;
# define phpext_pdo_pglite_ptr &pdo_pglite_module_entry

# define PHP_PDO_PGLITE_VERSION "0.0.0"

# if defined(ZTS) && defined(COMPILE_DL_PDO_PGLITE)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#include <stdbool.h>
#include "../vrzno/php_vrzno.h"

typedef struct {
	unsigned int errcode;
	int sqlstate;
	char *errmsg;
	const char *file;
	int line;
} pdo_pglite_error_info;

typedef struct {
	zval *dbPtr;
	pdo_pglite_error_info einfo;
} pdo_pglite_db_handle;

typedef struct {
	pdo_pglite_db_handle *db;
	vrzno_object *stmt;
	unsigned long curr;
	unsigned long row_count;
	zval *results;
	unsigned pre_fetched:1;
	unsigned done:1;
} pdo_pglite_stmt;

#endif
