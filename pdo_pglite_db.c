EM_ASYNC_JS(void, pdo_pglite_close, (jstarget *targetId), {
	const db = Module.targets.get(targetId);

	try
	{
		if(db && typeof db.close === 'function')
		{
			await db.close();
		}
	}
	catch(exception)
	{
		/* Destructors cannot report a useful PDO error; cleanup must still finish. */
	}
	finally
	{
		if(db)
		{
			Module.tacked.delete(db);
		}

		Module.targets.remove(targetId);
	}
});

static void pdo_pglite_handle_closer(pdo_dbh_t *dbh)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;

	if(!handle)
	{
		return;
	}

	pdo_pglite_clear_error_info(dbh);

	if(handle->dbId)
	{
		pdo_pglite_close(handle->dbId);
	}

	pefree(handle, dbh->is_persistent);
	dbh->driver_data = NULL;
}

static bool pdo_pglite_handle_preparer(
	pdo_dbh_t *dbh,
	zend_string *sql,
	pdo_stmt_t *stmt,
	zval *driver_options
){
	pdo_pglite_db_handle *handle = dbh->driver_data;
	pdo_pglite_stmt *pglite_stmt;
	zend_string *rewritten_sql = NULL;
	char *error = NULL;
	char *sqlstate = NULL;

	if(driver_options && pdo_attr_lval(
		driver_options,
		PDO_ATTR_CURSOR,
		PDO_CURSOR_FWDONLY
	) != PDO_CURSOR_FWDONLY)
	{
		pdo_raise_impl_error(dbh, NULL, "IM001", "Scrollable cursors are not supported");
		return false;
	}

	pglite_stmt = ecalloc(1, sizeof(pdo_pglite_stmt));

	stmt->methods = &pdo_pglite_stmt_methods;
	stmt->driver_data = pglite_stmt;
	stmt->supports_placeholders = PDO_PLACEHOLDER_NAMED;
	stmt->named_rewrite_template = "$%d";

	pglite_stmt->db = handle;

	int parse_status = pdo_parse_params(stmt, sql, &rewritten_sql);

	if(parse_status == -1)
	{
		strcpy(dbh->error_code, stmt->error_code);
		return false;
	}

	if(parse_status == 1)
	{
		sql = rewritten_sql;
	}

	pglite_stmt->stmt = (jstarget*) EM_ASM_PTR({
		const db = Module.targets.get($0);
		const query = UTF8ToString($1);
		const errorPtr = $2;
		const sqlstatePtr = $3;

		const writeError = exception => {
			const message = exception && exception.message
				? String(exception.message)
				: String(exception);
			const state = exception && typeof exception.code === 'string'
				&& /^[0-9A-Z]{5}$/.test(exception.code)
				? exception.code
				: 'HY000';
			const messageLength = lengthBytesUTF8(message) + 1;
			const stateLength = lengthBytesUTF8(state) + 1;
			const messageLocation = _malloc(messageLength);
			const stateLocation = _malloc(stateLength);

			stringToUTF8(message, messageLocation, messageLength);
			stringToUTF8(state, stateLocation, stateLength);
			setValue(errorPtr, messageLocation, '*');
			setValue(sqlstatePtr, stateLocation, '*');
		};

		try
		{
			if(!db)
			{
				throw new Error('The PGlite database handle is no longer available.');
			}

			const textParser = value => value;
			const textTypeIds = (
				'20 114 1082 1114 1184 1186 3802 '
				+ '199 1000 1001 1002 1003 1005 1006 1007 1008 '
				+ '1009 1010 1011 1012 1013 1014 1015 1016 1017 '
				+ '1018 1019 1020 1021 1022 1027 1028 1040 1041 '
				+ '1115 1182 1183 1185 1187 1231 1263 1270 1561 '
				+ '1563 2201 2951 3807'
			).split(' ').map(Number);
			const parsers = {};

			for(const typeId of textTypeIds)
			{
				parsers[typeId] = textParser;
			}

			const prepared = (...params) => db.query(query, params, {
				rowMode: 'array',
				parsers
			});

			prepared.query = query;
			Module.tacked.add(prepared);

			return Module.targets.add(prepared);
		}
		catch(exception)
		{
			writeError(exception);
			return 0;
		}
	}, handle->dbId, ZSTR_VAL(sql), &error, &sqlstate);

	if(rewritten_sql)
	{
		zend_string_release(rewritten_sql);
	}

	if(!pglite_stmt->stmt)
	{
		pdo_pglite_report_bridge_error(dbh, stmt, error, sqlstate, __FILE__, __LINE__);
		return false;
	}

	return true;
}

EM_ASYNC_JS(int, pdo_pglite_real_exec, (
	jstarget *dbId,
	const char *sql,
	char **errorPtr,
	char **sqlstatePtr
), {
	const writeError = exception => {
		const message = exception && exception.message
			? String(exception.message)
			: String(exception);
		const state = exception && typeof exception.code === 'string'
			&& /^[0-9A-Z]{5}$/.test(exception.code)
			? exception.code
			: 'HY000';
		const messageLength = lengthBytesUTF8(message) + 1;
		const stateLength = lengthBytesUTF8(state) + 1;
		const messageLocation = _malloc(messageLength);
		const stateLocation = _malloc(stateLength);

		stringToUTF8(message, messageLocation, messageLength);
		stringToUTF8(state, stateLocation, stateLength);
		setValue(errorPtr, messageLocation, '*');
		setValue(sqlstatePtr, stateLocation, '*');
	};

	try
	{
		const db = Module.targets.get(dbId);

		if(!db)
		{
			throw new Error('The PGlite database handle is no longer available.');
		}

		const results = await db.exec(UTF8ToString(sql));

		if(!Array.isArray(results))
		{
			return 0;
		}

		let previousAffectedRows = 0;

		return results.reduce((count, result) => {
			const affectedRows = Number(result.affectedRows ?? previousAffectedRows);
			const affectedRowsDelta = Math.max(0, affectedRows - previousAffectedRows);

			previousAffectedRows = affectedRows;

			if(Array.isArray(result.fields) && result.fields.length)
			{
				return count;
			}

			return count + Number(result.rowCount ?? affectedRowsDelta);
		}, 0);
	}
	catch(exception)
	{
		writeError(exception);
		return -1;
	}
});

static zend_long pdo_pglite_exec_sql(pdo_dbh_t *dbh, const char *sql)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	char *error = NULL;
	char *sqlstate = NULL;
	int affected_rows = pdo_pglite_real_exec(handle->dbId, sql, &error, &sqlstate);

	if(affected_rows < 0)
	{
		pdo_pglite_report_bridge_error(dbh, NULL, error, sqlstate, __FILE__, __LINE__);
		return -1;
	}

	return (zend_long) affected_rows;
}

static zend_long pdo_pglite_handle_doer(pdo_dbh_t *dbh, const zend_string *sql)
{
	return pdo_pglite_exec_sql(dbh, ZSTR_VAL(sql));
}

static zend_string *pdo_pglite_handle_quoter(
	pdo_dbh_t *dbh,
	const zend_string *unquoted,
	enum pdo_param_type paramtype
){
	const char *input = ZSTR_VAL(unquoted);
	size_t input_length = ZSTR_LEN(unquoted);
	size_t escape_count = 0;
	size_t output_index = 0;
	zend_bool use_escape_literal = false;
	zend_string *quoted;
	char *output;

	(void) dbh;

	if(PDO_PARAM_TYPE(paramtype) == PDO_PARAM_LOB)
	{
		static const char hex[] = "0123456789abcdef";

		quoted = zend_string_safe_alloc(2, input_length, 6, false);
		output = ZSTR_VAL(quoted);
		output[output_index++] = 'E';
		output[output_index++] = '\'';
		output[output_index++] = '\\';
		output[output_index++] = '\\';
		output[output_index++] = 'x';

		for(size_t input_index = 0; input_index < input_length; input_index++)
		{
			unsigned char byte = (unsigned char) input[input_index];

			output[output_index++] = hex[byte >> 4];
			output[output_index++] = hex[byte & 0x0f];
		}

		output[output_index++] = '\'';
		output[output_index] = '\0';

		return quoted;
	}

	for(size_t input_index = 0; input_index < input_length; input_index++)
	{
		if(input[input_index] == '\'' || input[input_index] == '\\')
		{
			escape_count++;
		}

		if(input[input_index] == '\\')
		{
			use_escape_literal = true;
		}
	}

	quoted = zend_string_safe_alloc(
		1,
		input_length,
		escape_count + (use_escape_literal ? 3 : 2),
		false
	);
	output = ZSTR_VAL(quoted);

	if(use_escape_literal)
	{
		output[output_index++] = 'E';
	}

	output[output_index++] = '\'';

	for(size_t input_index = 0; input_index < input_length; input_index++)
	{
		if(input[input_index] == '\'' || input[input_index] == '\\')
		{
			output[output_index++] = input[input_index];
		}

		output[output_index++] = input[input_index];
	}

	output[output_index++] = '\'';
	output[output_index] = '\0';

	return quoted;
}

static bool pdo_pglite_transaction(pdo_dbh_t *dbh, const char *command)
{
	return pdo_pglite_exec_sql(dbh, command) >= 0;
}

static bool pdo_pglite_handle_begin(pdo_dbh_t *dbh)
{
	return pdo_pglite_transaction(dbh, "BEGIN");
}

static bool pdo_pglite_handle_commit(pdo_dbh_t *dbh)
{
	return pdo_pglite_transaction(dbh, "COMMIT");
}

static bool pdo_pglite_handle_rollback(pdo_dbh_t *dbh)
{
	return pdo_pglite_transaction(dbh, "ROLLBACK");
}

EM_ASYNC_JS(char*, pdo_pglite_real_last_insert_id, (
	jstarget *dbId,
	const char *namePtr,
	char **errorPtr,
	char **sqlstatePtr
), {
	const writeError = exception => {
		const message = exception && exception.message
			? String(exception.message)
			: String(exception);
		const state = exception && typeof exception.code === 'string'
			&& /^[0-9A-Z]{5}$/.test(exception.code)
			? exception.code
			: 'HY000';
		const messageLength = lengthBytesUTF8(message) + 1;
		const stateLength = lengthBytesUTF8(state) + 1;
		const messageLocation = _malloc(messageLength);
		const stateLocation = _malloc(stateLength);

		stringToUTF8(message, messageLocation, messageLength);
		stringToUTF8(state, stateLocation, stateLength);
		setValue(errorPtr, messageLocation, '*');
		setValue(sqlstatePtr, stateLocation, '*');
	};

	try
	{
		const db = Module.targets.get(dbId);

		if(!db)
		{
			throw new Error('The PGlite database handle is no longer available.');
		}

		const hasName = Boolean(namePtr);
		const name = hasName ? UTF8ToString(namePtr) : null;
		const result = hasName
			? await db.query('SELECT CURRVAL($1) AS id', [name])
			: await db.query('SELECT LASTVAL() AS id');
		const id = result.rows && result.rows[0] ? result.rows[0].id : null;

		if(id === null || typeof id === 'undefined')
		{
			throw new Error('PGlite did not return a sequence value.');
		}

		const value = String(id);
		const valueLength = lengthBytesUTF8(value) + 1;
		const valueLocation = _malloc(valueLength);

		stringToUTF8(value, valueLocation, valueLength);
		return valueLocation;
	}
	catch(exception)
	{
		writeError(exception);
		return 0;
	}
});

static zend_string *pdo_pglite_last_insert_id(pdo_dbh_t *dbh, const zend_string *name)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
	const char *name_string = name ? ZSTR_VAL(name) : NULL;
	char *error = NULL;
	char *sqlstate = NULL;
	char *last_id = pdo_pglite_real_last_insert_id(
		handle->dbId,
		name_string,
		&error,
		&sqlstate
	);

	if(!last_id)
	{
		pdo_pglite_report_bridge_error(dbh, NULL, error, sqlstate, __FILE__, __LINE__);
		return NULL;
	}

	zend_string *return_value = zend_string_init(last_id, strlen(last_id), false);
	free(last_id);

	return return_value;
}

static void pdo_pglite_fetch_error_func(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info)
{
	pdo_pglite_db_handle *handle = dbh->driver_data;

	(void) stmt;

	if(handle->einfo.errcode)
	{
		add_next_index_long(info, handle->einfo.errcode);
	}
	else
	{
		add_next_index_null(info);
	}

	if(handle->einfo.errmsg)
	{
		add_next_index_string(info, handle->einfo.errmsg);
	}
	else
	{
		add_next_index_null(info);
	}
}

#if PHP_VERSION_ID < 80100
static int pdo_pglite_handle_set_attribute(pdo_dbh_t *dbh, zend_long attr, zval *value)
#else
static bool pdo_pglite_handle_set_attribute(pdo_dbh_t *dbh, zend_long attr, zval *value)
#endif
{
	pdo_pglite_db_handle *handle = dbh->driver_data;
#if PHP_VERSION_ID < 80100
	zend_bool emulate_prepares;
#else
	bool emulate_prepares;
#endif

	if(attr != PDO_ATTR_EMULATE_PREPARES)
	{
		return false;
	}

#if PHP_VERSION_ID < 80100
	emulate_prepares = zval_get_long(value) ? 1 : 0;
#else
	if(!pdo_get_bool_param(&emulate_prepares, value))
	{
		return false;
	}
#endif

	/*
	 * PGlite's query bridge does not retain a server-side prepared statement,
	 * so both settings use the same placeholder-aware execution path.
	 */
	handle->emulate_prepares = emulate_prepares;
	return true;
}

EM_ASYNC_JS(char*, pdo_pglite_real_server_version, (
	jstarget *dbId,
	char **errorPtr,
	char **sqlstatePtr
), {
	const writeError = exception => {
		const message = exception && exception.message
			? String(exception.message)
			: String(exception);
		const state = exception && typeof exception.code === 'string'
			&& /^[0-9A-Z]{5}$/.test(exception.code)
			? exception.code
			: 'HY000';
		const messageLength = lengthBytesUTF8(message) + 1;
		const stateLength = lengthBytesUTF8(state) + 1;
		const messageLocation = _malloc(messageLength);
		const stateLocation = _malloc(stateLength);

		stringToUTF8(message, messageLocation, messageLength);
		stringToUTF8(state, stateLocation, stateLength);
		setValue(errorPtr, messageLocation, '*');
		setValue(sqlstatePtr, stateLocation, '*');
	};

	try
	{
		const db = Module.targets.get(dbId);

		if(!db)
		{
			throw new Error('The PGlite database handle is no longer available.');
		}

		const result = await db.query('SHOW server_version');
		const version = result && result.rows && result.rows[0]
			? result.rows[0].server_version
			: null;

		if(typeof version !== 'string' || !version)
		{
			throw new Error('PGlite did not return its PostgreSQL server version.');
		}

		const versionLength = lengthBytesUTF8(version) + 1;
		const versionLocation = _malloc(versionLength);

		stringToUTF8(version, versionLocation, versionLength);
		return versionLocation;
	}
	catch(exception)
	{
		writeError(exception);
		return 0;
	}
});

static int pdo_pglite_handle_get_attribute(
	pdo_dbh_t *dbh,
	zend_long attr,
	zval *return_value
){
	pdo_pglite_db_handle *handle = dbh->driver_data;
	char *error = NULL;
	char *sqlstate = NULL;
	char *version;

	switch(attr)
	{
		case PDO_ATTR_EMULATE_PREPARES:
			ZVAL_BOOL(return_value, handle->emulate_prepares);
			return 1;

		case PDO_ATTR_SERVER_VERSION:
		case PDO_ATTR_CLIENT_VERSION:
			/* PGlite embeds the PostgreSQL engine, so both versions are identical. */
			version = pdo_pglite_real_server_version(handle->dbId, &error, &sqlstate);

			if(!version)
			{
				pdo_pglite_report_bridge_error(
					dbh,
					NULL,
					error,
					sqlstate,
					__FILE__,
					__LINE__
				);
				return -1;
			}

			ZVAL_STRING(return_value, version);
			free(version);
			return 1;

		default:
			return 0;
	}
}

static const struct pdo_dbh_methods pdo_pglite_db_methods = {
	pdo_pglite_handle_closer,
	pdo_pglite_handle_preparer,
	pdo_pglite_handle_doer,
	pdo_pglite_handle_quoter,
	pdo_pglite_handle_begin,
	pdo_pglite_handle_commit,
	pdo_pglite_handle_rollback,
	pdo_pglite_handle_set_attribute,
	pdo_pglite_last_insert_id,
	pdo_pglite_fetch_error_func,
	pdo_pglite_handle_get_attribute,
	NULL, /* check_liveness */
	NULL, /* get_driver_methods */
	NULL, /* persistent_shutdown */
	NULL, /* in_transaction: use PDO's internal tracking */
	NULL  /* get_gc */
};

EM_ASYNC_JS(jstarget*, pdo_pglite_open, (
	const char *dataSourcePtr,
	char **errorPtr,
	char **sqlstatePtr
), {
	let pglite = null;

	try
	{
		if(typeof Module.PGlite !== 'function')
		{
			throw new Error('The PGlite class must be provided to the php-wasm constructor.');
		}

		const dataSource = UTF8ToString(dataSourcePtr);
		const dataDir = dataSource
			? (dataSource.indexOf('://') > 0 ? dataSource : 'idb://' + dataSource)
			: undefined;

		pglite = new Module.PGlite(dataDir);
		await pglite.waitReady;

		Module.tacked.add(pglite);
		return Module.targets.add(pglite);
	}
	catch(exception)
	{
		if(pglite && typeof pglite.close === 'function')
		{
			try
			{
				await pglite.close();
			}
			catch(closeException)
			{
				/* Preserve the initialization error. */
			}
		}

		const message = exception && exception.message
			? String(exception.message)
			: String(exception);
		const state = 'HY000';
		const messageLength = lengthBytesUTF8(message) + 1;
		const stateLength = lengthBytesUTF8(state) + 1;
		const messageLocation = _malloc(messageLength);
		const stateLocation = _malloc(stateLength);

		stringToUTF8(message, messageLocation, messageLength);
		stringToUTF8(state, stateLocation, stateLength);
		setValue(errorPtr, messageLocation, '*');
		setValue(sqlstatePtr, stateLocation, '*');

		return 0;
	}
});

static int pdo_pglite_db_handle_factory(pdo_dbh_t *dbh, zval *driver_options)
{
	pdo_pglite_db_handle *handle = pecalloc(
		1,
		sizeof(pdo_pglite_db_handle),
		dbh->is_persistent
	);
	char *error = NULL;
	char *sqlstate = NULL;

	(void) driver_options;

	dbh->driver_data = handle;

	handle->dbId = pdo_pglite_open(dbh->data_source, &error, &sqlstate);

	if(!handle->dbId)
	{
		pdo_pglite_report_bridge_error(dbh, NULL, error, sqlstate, __FILE__, __LINE__);
		pdo_pglite_clear_error_info(dbh);
		pefree(handle, dbh->is_persistent);
		dbh->driver_data = NULL;
		return 0;
	}

	dbh->methods = &pdo_pglite_db_methods;
	dbh->alloc_own_columns = 1;
	dbh->max_escaped_char_length = 2;

	return 1;
}

const pdo_driver_t pdo_pglite_driver = {
	PDO_DRIVER_HEADER(pgsql),
	pdo_pglite_db_handle_factory
};
