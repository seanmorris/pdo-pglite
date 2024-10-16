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

	EM_ASM({
		console.log('pdo_pglite_handle_closer DB #' + $0);
		const db = Module.targets.get($0);
		// Module.targets.remove(db);
		Module.tacked.delete(db);
	}, handle->dbId);

	pefree(handle, dbh->is_persistent);
	dbh->driver_data = NULL;
}

static bool pdo_pglite_handle_preparer(pdo_dbh_t *dbh, zend_string *sql, pdo_stmt_t *stmt, zval *driver_options)
{
	EM_ASM({ console.log('pdo_pglite_handle_preparer'); });

	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_stmt *vStmt = emalloc(sizeof(pdo_pglite_stmt));

	zend_string *nsql = NULL;

	stmt->methods = &pdo_pglite_stmt_methods;
	stmt->driver_data = vStmt;
	stmt->supports_placeholders = PDO_PLACEHOLDER_NAMED;
	stmt->named_rewrite_template = "$%d";

	int parseStatus = pdo_parse_params(stmt, sql, &nsql);

	if(parseStatus == 1) // query was re-written
	{
		sql = nsql;
	}
	else if(parseStatus == -1) // couldn't grok it
	{
		strcpy(dbh->error_code, stmt->error_code);
		efree(vStmt);
		return false;
	}

	const char *sqlString = ZSTR_VAL(sql);

	zval prepared;

	vStmt->stmt = (jstarget*) EM_ASM_PTR({
		const db = Module.targets.get($0);
		const query = UTF8ToString($1);
		const zv = $2;

		console.log('Preparing...', query);

		const prepared = (...params) => db.query(query, params);
		prepared.query = query;

		Module.tacked.add(prepared);
		return Module.targets.add(prepared);

	}, handle->dbId, sqlString);

	vStmt->results = NULL;
	vStmt->db = handle;

	return true;
}

// EM_ASYNC_JS(void, pdo_pglite_real_doer, (jstarget *dbId, char *sql), {
// 	const db = Module.targets.get(dbId);
// 	await db.query(UTF8ToString(sql));
// });

static zend_long pdo_pglite_handle_doer(pdo_dbh_t *dbh, const zend_string *sql)
{
	EM_ASM({ console.log('pdo_pglite_handle_doer'); });
	// pdo_pglite_db_handle *handle = dbh->driver_data;
	// const char *sqlString = ZSTR_VAL(sql);
	// pdo_pglite_real_doer(handle->dbId, sqlString);
	return 1;
}

static zend_string *pdo_pglite_handle_quoter(pdo_dbh_t *dbh, const zend_string *unquoted, enum pdo_param_type paramtype)
{
	EM_ASM({ console.log('pdo_pglite_handle_quoter'); });
	const char *unquotedChar = ZSTR_VAL(unquoted);
	zend_string *quoted = zend_string_init(unquotedChar, strlen(unquotedChar), 0);
	return quoted;
}

EM_ASYNC_JS(void, pdo_pglite_real_handle_begin, (jstarget *dbId), {
	console.log('BEGIN');
	const db = Module.targets.get(dbId);
	await db.query('BEGIN');
});

static bool pdo_pglite_handle_begin(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_real_handle_begin(handle->dbId);
	return 1;
}

EM_ASYNC_JS(void, pdo_pglite_real_handle_commit, (jstarget *dbId), {
	console.log('COMMIT');
	const db = Module.targets.get(dbId);
	await db.query('COMMIT');
});

static bool pdo_pglite_handle_commit(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_real_handle_commit(&handle->dbId);
	return 1;
}

EM_ASYNC_JS(void, pdo_pglite_real_handle_rollback, (jstarget *dbId), {
	console.log('ROLLBACK');
	const db = Module.targets.get(dbId);
	await db.query('ROLLBACK');
});

static bool pdo_pglite_handle_rollback(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_real_handle_rollback(handle->dbId);
	return 1;
}

static bool pdo_pglite_set_attr(pdo_dbh_t *dbh, zend_long attr, zval *val)
{
	EM_ASM({ console.log('pdo_pglite_set_attr'); });
	pdo_pglite_db_handle *handle = dbh->driver_data;

	return (bool) EM_ASM_INT({
		console.log('SET ATTR', $1, $2);
		return true;
	}, handle->dbId, attr, val);
}

// EM_ASYNC_JS(uint32_t, pdo_pglite_real_last_insert_id, (jstarget *dbId, const char *namePtr, char *errorPtr), {
// 	try
// 	{
// 		const db = Module.targets.get(dbId);
// 		if(namePtr)
// 		{
// 			const name = UTF8ToString(namePtr);
// 			console.log('SELECT CURRVAL($1)', name);
// 			const result = await db.query('SELECT CURRVAL($1)', name);
// 			console.log(result);
// 			// return result;
// 		}
// 		else
// 		{
// 			console.log('SELECT LASTVAL()');
// 			const result = await db.query('SELECT LASTVAL()');
// 			console.log(result);
// 			// return result;
// 		}
// 	}
// 	catch(error)
// 	{
// 		console.error(error);
// 		const str = String(error);
// 		const len = lengthBytesUTF8(str);
// 		const loc = _malloc(len);
// 		stringToUTF8(str, loc, len);
// 		setValue(errorPtr, loc, '*');
// 	}

// 	return 0;
// });

// static zend_string *pdo_pglite_last_insert_id(pdo_dbh_t *dbh, const zend_string *name)
// {
// 	pdo_pglite_db_handle *handle = dbh->driver_data;
// 	const char *nameStr = NULL;

// 	if(name != NULL)
// 	{
// 		nameStr = ZSTR_VAL(name);
// 	}

// 	char *error;
// 	uint32_t lastId = pdo_pglite_real_last_insert_id(handle->dbId, nameStr, error);

// 	if(error)
// 	{
// 		pdo_pglite_error(dbh, NULL, 1, "HY000", error, __FILE__, __LINE__);
// 		return 0;
// 	}

// #if PHP_MAJOR_VERSION >= 8 && PHP_MINOR_VERSION >= 1
// 	return zend_ulong_to_str((zend_ulong)lastId);
// #else
// 	return zend_long_to_str((zend_long)lastId);
// #endif
// }

static void pdo_pglite_fetch_error_func(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info)
{
	EM_ASM({ console.log('pdo_pglite_fetch_error_func'); });

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

static void pdo_pglite_request_shutdown(pdo_dbh_t *dbh)
{
	EM_ASM({ console.log('pdo_pglite_request_shutdown'); });
	pdo_pglite_db_handle *handle = dbh->driver_data;

	EM_ASM({
		const db = Module.targets.get($0);
		console.log('SHUTDOWN');
	}, handle->dbId);
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
	NULL,  //pdo_pglite_last_insert_id,
	pdo_pglite_fetch_error_func,
	NULL,  //pdo_pglite_get_attr,
	NULL,  /* check_liveness: not needed */
	NULL,  //get_driver_methods,
	pdo_pglite_request_shutdown,
	NULL,  /* in transaction, use PDO's internal tracking mechanism */
	NULL   /* get_gc */
};

static int pdo_pglite_db_handle_factory(pdo_dbh_t *dbh, zval *driver_options)
{
	EM_ASM({ console.log('pdo_pglite_db_handle_factory'); });

	pdo_pglite_db_handle *handle;
	handle = pecalloc(1, sizeof(pdo_pglite_db_handle), dbh->is_persistent);

	dbh->driver_data = handle;
	dbh->methods = &pdo_pglite_db_methods;

	handle->einfo.errcode = 0;
	handle->einfo.errmsg = NULL;
	handle->dbId = (jstarget*) EM_ASM_PTR({
		if(!Module.PGlite)
		{
			throw new Error("The PGlite class must be provided as a constructor arg to PHP to use PGlite.");
		}

		const data_source = UTF8ToString($0);
		const pglite = new Module.PGlite(data_source ? ('idb://' + data_source): undefined);

		Module.tacked.add(pglite);
		return Module.targets.add(pglite);
	}, dbh->data_source);

	return 1;
}

const pdo_driver_t pdo_pglite_driver = {
	PDO_DRIVER_HEADER(pgsql),
	pdo_pglite_db_handle_factory
};
