static void pdo_pglite_handle_closer(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;

	if(handle->einfo.errmsg)
	{
		pefree(handle->einfo.errmsg, dbh->is_persistent);
	}

	if(handle->einfo.sqlstate)
	{
		pefree(handle->einfo.sqlstate, dbh->is_persistent);
	}
}

static bool pdo_pglite_handle_preparer(pdo_dbh_t *dbh, zend_string *sql, pdo_stmt_t *stmt, zval *driver_options)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_stmt *vStmt = emalloc(sizeof(pdo_pglite_stmt));

	int ret;
	zend_string *nsql = NULL;

	stmt->methods = &pdo_pglite_stmt_methods;
	stmt->driver_data = vStmt;
	stmt->supports_placeholders = PDO_PLACEHOLDER_NAMED;
	stmt->named_rewrite_template = "$%d";

	ret = pdo_parse_params(stmt, sql, &nsql);

	if(ret == 1) // query was re-written
	{
		sql = nsql;
	}
	else if(ret == -1) // couldn't grok it
	{
		strcpy(dbh->error_code, stmt->error_code);
		efree(vStmt);
		return false;
	}

	const char *sqlString = ZSTR_VAL(sql);

	zval *prepared = EM_ASM_PTR({
		const db = Module.zvalToJS($0);
		const query  = UTF8ToString($1);
		const supports_placeholders = $2;
		const ret = $3;

		const prepared = (...params) => db.query(query, params);
		prepared.query = query;
		const zval = Module.jsToZval(prepared);

		return zval;

	}, handle->dbPtr, sqlString, stmt->supports_placeholders, ret);

	vStmt->db   = handle;
	vStmt->stmt = vrzno_fetch_object(Z_OBJ_P(prepared));

	return true;
}

static zend_long pdo_pglite_handle_doer(pdo_dbh_t *dbh, const zend_string *sql)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	const char *sqlString = ZSTR_VAL(sql);

	return (zend_long) EM_ASM_INT({
		// console.log('DO', $0, UTF8ToString($1));
		// const db = Module.zvalToJS($0);
		// const query  = UTF8ToString($1);
		return 1;

	}, handle->dbPtr, &sqlString);
}

static zend_string *pdo_pglite_handle_quoter(pdo_dbh_t *dbh, const zend_string *unquoted, enum pdo_param_type paramtype)
{
	// pdo_pglite_db_handle *handle = dbh->driver_data;
	const char *unquotedChar = ZSTR_VAL(unquoted);
	zend_string *quoted = zend_string_init(unquotedChar, strlen(unquotedChar), 0);
	return quoted;
}

EM_ASYNC_JS(int, pdo_pglite_real_handle_begin, (pdo_pglite_db_handle *handle), {
	const db = Module.targets.get(handle);
	await db.query('BEGIN');
});

static bool pdo_pglite_handle_begin(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_real_handle_begin(handle);
	return 1;
}

EM_ASYNC_JS(int, pdo_pglite_real_handle_commit, (pdo_pglite_db_handle *handle), {
	const db = Module.targets.get(handle);
	await db.query('COMMIT');
});

static bool pdo_pglite_handle_commit(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_real_handle_commit(handle);
	return 1;
}

EM_ASYNC_JS(int, pdo_pglite_real_handle_rollback, (pdo_pglite_db_handle *handle), {
	const db = Module.targets.get(handle);
	await db.query('ROLLBACK');
});

static bool pdo_pglite_handle_rollback(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_real_handle_rollback(handle);
	return 1;
}

static bool pdo_pglite_set_attr(pdo_dbh_t *dbh, zend_long attr, zval *val)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;

	return (bool) EM_ASM_INT({
		console.log('SET ATTR', $0, $1, $2);
		return true;
	}, handle->dbPtr, attr, val);
}

EM_ASYNC_JS(int, pdo_pglite_real_last_insert_id, (pdo_pglite_db_handle *handle, const char *namePtr), {
	const db = Module.targets.get(handle);

	if(namePtr)
	{
		const name = UTF8ToString(namePtr);
		console.log('LAST INSERT ID', $0, name);
		const result = await db.query('SELECT CURRVAL($1)', name);
		console.log(result);
	}
	else
	{
		console.log('LAST INSERT ID', $0);
		const result = await db.query('SELECT CURRVAL($1)', name);
		console.log(result);
	}

	return 0;
});

static zend_string *pdo_pglite_last_insert_id(pdo_dbh_t *dbh, const zend_string *name)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	const char *nameStr = NULL;

	if(name != NULL)
	{
		nameStr = ZSTR_VAL(name);
	}

#if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 1
	return zend_ulong_to_str
#else
	return zend_long_to_str
#endif
	(pdo_pglite_real_last_insert_id(handle, nameStr));
}

static void pdo_pglite_fetch_error_func(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;

	if(handle->einfo.errcode)
	{
		add_next_index_long(info, handle->einfo.errcode);
	}
	else
	{
		add_next_index_null(info); // Add null to respect expected info array structure
	}

	if(handle->einfo.errmsg)
	{
		add_next_index_string(info, handle->einfo.errmsg);
	}
}

static int pdo_pglite_get_attr(pdo_dbh_t *dbh, zend_long attr, zval *return_value)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;

	return EM_ASM_INT({
		console.log('GET ATTR', $0, $1, $2);
		return 1;
	}, handle->dbPtr, attr, return_value);
}

static void pdo_pglite_request_shutdown(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;

	EM_ASM({
		console.log('SHUTDOWN', $0);
	}, handle->dbPtr);
}

static void pdo_pglite_get_gc(pdo_dbh_t *dbh, zend_get_gc_buffer *gc_buffer)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;

	EM_ASM({
		console.log('GET GC', $0, $1);
	}, handle->dbPtr, gc_buffer);
}

static const struct pdo_dbh_methods pdo_pglite_db_methods = {
	pdo_pglite_handle_closer,
	pdo_pglite_handle_preparer,
	pdo_pglite_handle_doer,
	pdo_pglite_handle_quoter,
	pdo_pglite_handle_begin,
	pdo_pglite_handle_commit,
	pdo_pglite_handle_rollback,
	pdo_pglite_set_attr,
	pdo_pglite_last_insert_id,
	pdo_pglite_fetch_error_func,
	pdo_pglite_get_attr,
	NULL,  /* check_liveness: not needed */
	NULL,  //get_driver_methods,
	pdo_pglite_request_shutdown,
	NULL,  /* in transaction, use PDO's internal tracking mechanism */
	pdo_pglite_get_gc
};

EM_ASYNC_JS(zval*, pdo_pglite_real_db_handle_factory, (const char *connectionStringPrt), {
	if(!Module.PGlite)
	{
		throw new Error("The PGlite class must be provided as a constructor arg to PHP to use PGlite.");
	}
	const connectionString = UTF8ToString(connectionStringPrt);
	const pglite = new Module.PGlite(connectionString
		? ('idb://' + connectionString)
		: undefined
	);
	return Module.jsToZval(pglite);
});

static int pdo_pglite_db_handle_factory(pdo_dbh_t *dbh, zval *driver_options)
{
	pdo_pglite_db_handle *handle;
	handle = pecalloc(1, sizeof(pdo_pglite_db_handle), dbh->is_persistent);

	handle->einfo.errcode = 0;
	handle->einfo.errmsg = NULL;
	dbh->driver_data = handle;

	handle->dbPtr = pdo_pglite_real_db_handle_factory(dbh->data_source);

	dbh->methods = &pdo_pglite_db_methods;

	return 1;
}

const pdo_driver_t pdo_pglite_driver = {
	PDO_DRIVER_HEADER(pgsql),
	pdo_pglite_db_handle_factory
};
