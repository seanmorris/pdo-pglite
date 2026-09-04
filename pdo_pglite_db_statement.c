static void pdo_pglite_release_target(jstarget **target_id)
{
	if(!target_id || !*target_id)
	{
		return;
	}

	EM_ASM({
		const targetId = $0;
		const target = Module.targets.get(targetId);

		if(target)
		{
			Module.tacked.delete(target);
		}

		Module.targets.remove(targetId);
	}, *target_id);

	*target_id = NULL;
}

static int pdo_pglite_stmt_dtor(pdo_stmt_t *stmt)
{
	pdo_pglite_stmt *pglite_stmt = (pdo_pglite_stmt*) stmt->driver_data;

	if(!pglite_stmt)
	{
		return 1;
	}

	if(pglite_stmt->stmt)
	{
		EM_ASM({
			const statement = Module.targets.get($0);

			if(statement)
			{
				Module.PdoParams.delete(statement);
			}
		}, pglite_stmt->stmt);
	}

	pdo_pglite_release_target(&pglite_stmt->results);
	pdo_pglite_release_target(&pglite_stmt->stmt);

	efree(pglite_stmt);
	stmt->driver_data = NULL;

	return 1;
}

EM_ASYNC_JS(jstarget*, pdo_pglite_real_stmt_execute, (
	jstarget *targetId,
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

	const statement = Module.targets.get(targetId);
	const params = statement && Module.PdoParams.has(statement)
		? Module.PdoParams.get(statement)
		: [];

	if(statement)
	{
		Module.PdoParams.delete(statement);
	}

	try
	{
		if(!statement)
		{
			throw new Error('The PGlite statement handle is no longer available.');
		}

		const result = await statement(...params);
		const normalized = {};
		normalized.rows = Array.isArray(result.rows) ? result.rows : [];
		normalized.fields = Array.isArray(result.fields) ? result.fields : [];
		normalized.affectedRows = Number(result.affectedRows ?? result.rowCount ?? 0);

		Module.tacked.add(normalized);
		return Module.targets.add(normalized);
	}
	catch(exception)
	{
		writeError(exception);
		return 0;
	}
});

static int pdo_pglite_stmt_execute(pdo_stmt_t *stmt)
{
	pdo_pglite_stmt *pglite_stmt = (pdo_pglite_stmt*) stmt->driver_data;
	char *error = NULL;
	char *sqlstate = NULL;
	int column_count;

	php_pdo_stmt_set_column_count(stmt, 0);
	stmt->row_count = 0;
	pglite_stmt->row_count = 0;
	pglite_stmt->curr = 0;
	pglite_stmt->done = 0;

	pdo_pglite_release_target(&pglite_stmt->results);

	pglite_stmt->results = pdo_pglite_real_stmt_execute(
		pglite_stmt->stmt,
		&error,
		&sqlstate
	);

	if(!pglite_stmt->results)
	{
		pdo_pglite_report_bridge_error(stmt->dbh, stmt, error, sqlstate, __FILE__, __LINE__);
		return false;
	}

	pglite_stmt->row_count = (zend_long) EM_ASM_INT({
		const result = Module.targets.get($0);
		return result && Array.isArray(result.rows) ? result.rows.length : 0;
	}, pglite_stmt->results);

	column_count = EM_ASM_INT({
		const result = Module.targets.get($0);
		return result && Array.isArray(result.fields) ? result.fields.length : 0;
	}, pglite_stmt->results);

	php_pdo_stmt_set_column_count(stmt, column_count);

	stmt->row_count = (zend_long) EM_ASM_INT({
		const result = Module.targets.get($0);

		if(!result)
		{
			return 0;
		}

		return result.fields.length
			? result.rows.length
			: result.affectedRows;
	}, pglite_stmt->results);

	return true;
}

static int pdo_pglite_stmt_fetch(
	pdo_stmt_t *stmt,
	enum pdo_fetch_orientation orientation,
	zend_long offset
){
	pdo_pglite_stmt *pglite_stmt = (pdo_pglite_stmt*) stmt->driver_data;

	(void) offset;

	if(orientation != PDO_FETCH_ORI_NEXT || !stmt->executed || !pglite_stmt->results)
	{
		return 0;
	}

	if(pglite_stmt->curr >= pglite_stmt->row_count)
	{
		pglite_stmt->done = 1;
		return 0;
	}

	pglite_stmt->curr++;
	return 1;
}

static int pdo_pglite_stmt_describe_col(pdo_stmt_t *stmt, int colno)
{
	pdo_pglite_stmt *pglite_stmt = (pdo_pglite_stmt*) stmt->driver_data;
	char *column_name;

	if(!pglite_stmt->results || colno < 0 || colno >= stmt->column_count)
	{
		return 0;
	}

	column_name = (char*) EM_ASM_PTR({
		const result = Module.targets.get($0);
		const field = result && result.fields ? result.fields[$1] : null;

		if(!field || typeof field.name !== 'string')
		{
			return 0;
		}

		const length = lengthBytesUTF8(field.name) + 1;
		const location = _malloc(length);

		stringToUTF8(field.name, location, length);
		return location;
	}, pglite_stmt->results, colno);

	if(!column_name)
	{
		return 0;
	}

	stmt->columns[colno].name = zend_string_init(column_name, strlen(column_name), false);
	stmt->columns[colno].maxlen = SIZE_MAX;
	stmt->columns[colno].precision = 0;

	free(column_name);

	return 1;
}

static int pdo_pglite_stmt_get_col(
	pdo_stmt_t *stmt,
	int colno,
	zval *return_value,
	enum pdo_param_type *type
){
	pdo_pglite_stmt *pglite_stmt = (pdo_pglite_stmt*) stmt->driver_data;

	(void) type;

	if(!pglite_stmt->results || colno < 0 || colno >= stmt->column_count)
	{
		return 0;
	}

	return EM_ASM_INT({
		const result = Module.targets.get($0);
		const current = $1 - 1;
		const column = $2;
		const returnValue = $3;

		if(!result || current < 0 || current >= result.rows.length)
		{
			return 0;
		}

		const value = result.rows[current][column];
		const field = result.fields[column];

		if(field && field.dataTypeID === 17 && value !== null)
		{
			const bytes = value instanceof Uint8Array
				? value
				: new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
			const location = bytes.byteLength ? _malloc(bytes.byteLength) : 0;

			if(bytes.byteLength)
			{
				Module.HEAPU8.set(bytes, location);
			}

			Module.ccall(
				'pdo_pglite_create_string',
				null,
				['number', 'number', 'number'],
				[location, bytes.byteLength, returnValue]
			);

			if(location)
			{
				_free(location);
			}

			return 1;
		}

		if(typeof value === 'bigint' || (typeof value === 'number' && !Number.isFinite(value)))
		{
			Module.jsToZval(String(value), returnValue);
			return 1;
		}

		if(value instanceof Date)
		{
			Module.jsToZval(value.toISOString(), returnValue);
			return 1;
		}

		if(value && typeof value === 'object')
		{
			Module.jsToZval(JSON.stringify(value), returnValue);
			return 1;
		}

		Module.jsToZval(value, returnValue);
		return 1;
	}, pglite_stmt->results, pglite_stmt->curr, colno, return_value);
}

static int pdo_pglite_stmt_param_hook(
	pdo_stmt_t *stmt,
	struct pdo_bound_param_data *param,
	enum pdo_param_event event_type
){
	pdo_pglite_stmt *pglite_stmt = (pdo_pglite_stmt*) stmt->driver_data;

	if(!param->is_param)
	{
		return 1;
	}

	switch(event_type)
	{
		case PDO_PARAM_EVT_ALLOC:
			if(!stmt->bound_param_map)
			{
				return 1;
			}

			if(!zend_hash_index_exists(stmt->bound_param_map, param->paramno))
			{
				pdo_pglite_error(
					stmt->dbh,
					stmt,
					1 + param->paramno,
					"HY093",
					"parameter was not defined",
					__FILE__,
					__LINE__
				);
				return 0;
			}
			break;

		case PDO_PARAM_EVT_NORMALIZE:
			if(param->name)
			{
				if(ZSTR_VAL(param->name)[0] == '$')
				{
					param->paramno = ZEND_ATOL(ZSTR_VAL(param->name) + 1);
				}
				else
				{
					zend_string *rewritten_name = stmt->bound_param_map
						? zend_hash_find_ptr(stmt->bound_param_map, param->name)
						: NULL;

					if(!rewritten_name)
					{
						pdo_pglite_error(
							stmt->dbh,
							stmt,
							1,
							"HY093",
							"parameter was not defined",
							__FILE__,
							__LINE__
						);
						return 0;
					}

					param->paramno = ZEND_ATOL(ZSTR_VAL(rewritten_name) + 1) - 1;
				}
			}
			break;

		case PDO_PARAM_EVT_EXEC_PRE:
		{
			zval *parameter = &param->parameter;

			if(Z_ISREF_P(parameter))
			{
				parameter = Z_REFVAL_P(parameter);
			}

			if(PDO_PARAM_TYPE(param->param_type) == PDO_PARAM_NULL || Z_TYPE_P(parameter) == IS_NULL)
			{
				EM_ASM({
					const statement = Module.targets.get($0);
					const paramPosition = $1;
					const params = Module.PdoParams.get(statement) || [];

					params[paramPosition] = null;
					Module.PdoParams.set(statement, params);
				}, pglite_stmt->stmt, param->paramno);
				break;
			}

			if(Z_TYPE_P(parameter) == IS_RESOURCE)
			{
				php_stream *stream = NULL;
				zend_string *contents;

				php_stream_from_zval_no_verify(stream, parameter);

				if(!stream)
				{
					pdo_raise_impl_error(stmt->dbh, stmt, "HY105", "Expected a stream resource");
					return 0;
				}

				contents = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, false);
				zval_ptr_dtor(parameter);
				ZVAL_STR(parameter, contents ? contents : ZSTR_EMPTY_ALLOC());
			}

			if(PDO_PARAM_TYPE(param->param_type) == PDO_PARAM_LOB)
			{
				if(Z_TYPE_P(parameter) != IS_STRING && !try_convert_to_string(parameter))
				{
					pdo_raise_impl_error(stmt->dbh, stmt, "HY105", "LOB parameter could not be converted to a string");
					return 0;
				}

				EM_ASM({
					const statement = Module.targets.get($0);
					const start = $1;
					const length = $2;
					const paramPosition = $3;
					const params = Module.PdoParams.get(statement) || [];
					const value = Module.HEAPU8.slice(start, start + length);

					params[paramPosition] = value;
					Module.PdoParams.set(statement, params);
				},
					pglite_stmt->stmt,
					Z_STRVAL_P(parameter),
					Z_STRLEN_P(parameter),
					param->paramno
				);
			}
			else
			{
				EM_ASM({
					const statement = Module.targets.get($0);
					const paramPosition = $2;
					const params = Module.PdoParams.get(statement) || [];

					params[paramPosition] = Module.zvalToJS($1);
					Module.PdoParams.set(statement, params);
				}, pglite_stmt->stmt, parameter, param->paramno);
			}
			break;
		}

		case PDO_PARAM_EVT_FREE:
			if(param->driver_data)
			{
				efree(param->driver_data);
				param->driver_data = NULL;
			}
			break;

		case PDO_PARAM_EVT_EXEC_POST:
		case PDO_PARAM_EVT_FETCH_PRE:
		case PDO_PARAM_EVT_FETCH_POST:
			return 1;
	}

	return 1;
}

static int pdo_pglite_stmt_cursor_closer(pdo_stmt_t *stmt)
{
	pdo_pglite_stmt *pglite_stmt = (pdo_pglite_stmt*) stmt->driver_data;

	pdo_pglite_release_target(&pglite_stmt->results);
	pglite_stmt->curr = 0;
	pglite_stmt->row_count = 0;
	pglite_stmt->done = 1;

	return 1;
}

const struct pdo_stmt_methods pdo_pglite_stmt_methods = {
	pdo_pglite_stmt_dtor,
	pdo_pglite_stmt_execute,
	pdo_pglite_stmt_fetch,
	pdo_pglite_stmt_describe_col,
	pdo_pglite_stmt_get_col,
	pdo_pglite_stmt_param_hook,
	NULL, /* set_attribute */
	NULL, /* get_attribute */
	NULL, /* get_column_meta */
	NULL, /* next_rowset */
	pdo_pglite_stmt_cursor_closer
};
