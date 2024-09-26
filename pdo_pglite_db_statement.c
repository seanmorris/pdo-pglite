static int pdo_pglite_stmt_dtor(pdo_stmt_t *stmt)
{
	pdo_pglite_stmt *vStmt = (pdo_pglite_stmt*)stmt->driver_data;

	EM_ASM({
		const statement = Module.targets.get($0);
		Module.PdoParams.delete(statement);
	}, vStmt->stmt->targetId);

	efree(vStmt);

	return 1;
}

EM_ASYNC_JS(zval*, pdo_pglite_real_stmt_execute, (long targetId, char **error), {
	const statement = Module.targets.get(targetId);

	if(!Module.PdoParams.has(statement))
	{
		Module.PdoParams.set(statement, []);
	}

	const params = Module.PdoParams.get(statement);

	try
	{
		const result = await statement(...params);
		const rows = result.rows ?? [];
		const _fields = result.fields ?? [];
		const fields = new Map;

		_fields.forEach(field => {
			fields.set(field.name, field);
		});

		const utf8decoder = new TextDecoder();

		const mapped = rows.map(row => {
			const _row = {};
			for(const [key, val] of Object.entries(row))
			{
				if(fields.has(key) && fields.get(key).dataTypeID === 17)
				{
					_row[key] = utf8decoder.decode(val);
					continue;
				}

				_row[key] = val;
			}

			return _row;
		});

		return Module.jsToZval(mapped);
	}
	catch(exception)
	{
		const message = String(exception.message);
		const len     = lengthBytesUTF8(message) + 1;
		const strLoc  = _malloc(len);

		console.error(message, statement.query, exception);

		stringToUTF8(message, strLoc, len);
		setValue(error, strLoc, '*');

		return 0;
	}
});

static int pdo_pglite_stmt_execute(pdo_stmt_t *stmt)
{
	pdo_pglite_stmt *vStmt = (pdo_pglite_stmt*)stmt->driver_data;

	stmt->column_count = 0;
	vStmt->row_count = 0;
	vStmt->curr = 0;
	vStmt->done = 0;

	char *error = NULL;

	zval *results = pdo_pglite_real_stmt_execute(vStmt->stmt->targetId, &error);

	if(results)
	{
		vStmt->results = results;
		vStmt->row_count = EM_ASM_INT({

			const results = Module.targets.get($0);

			if(results)
			{
				return results.length;
			}

			return 0;

		}, vrzno_fetch_object(Z_OBJ_P(results))->targetId);

		if(vStmt->row_count)
		{
			stmt->column_count = EM_ASM_INT({

				const results = Module.targets.get($0);

				if(results.length)
				{
					return Object.keys(results[0]).length;
				}

				return 0;

			}, vrzno_fetch_object(Z_OBJ_P(results))->targetId);
		}
	}
	else if(error)
	{
		pdo_pglite_error(stmt->dbh, stmt, 1, "HY000", error, __FILE__, __LINE__);
		return false;
	}

	stmt->executed = 1;
	return true;
}

static int pdo_pglite_stmt_fetch(pdo_stmt_t *stmt, enum pdo_fetch_orientation ori, zend_long offset)
{
	pdo_pglite_stmt *vStmt = (pdo_pglite_stmt*)stmt->driver_data;

	if(!vStmt->results)
	{
		return false;
	}

	zval *row = EM_ASM_PTR({
		const targetId = $0;
		const target = Module.targets.get(targetId);
		const current = $1;

		if(current >= target.length)
		{
			return null;
		}

		return Module.jsToZval(target[current]);

	}, vrzno_fetch_object(Z_OBJ_P(vStmt->results))->targetId, vStmt->curr);

	if(row)
	{
		vStmt->curr++;
	}
	else
	{
		vStmt->done = 1;
	}

	return row;
}

static int pdo_pglite_stmt_describe_col(pdo_stmt_t *stmt, int colno)
{
	pdo_pglite_stmt *vStmt = (pdo_pglite_stmt*)stmt->driver_data;

	if(colno >= stmt->column_count) {
		return 0;
	}

	char *colName = EM_ASM_PTR({

		const results = Module.targets.get($0);

		if(results.length)
		{
			const jsRet = Object.keys(results[0])[$1];
			const len    = lengthBytesUTF8(jsRet) + 1;
			const strLoc = _malloc(len);

			stringToUTF8(jsRet, strLoc, len);

			return strLoc;
		}

		return 0;

	}, vrzno_fetch_object(Z_OBJ_P(vStmt->results))->targetId, colno);

	if(!colName)
	{
		return 0;
	}

	stmt->columns[colno].name = zend_string_init(colName, strlen(colName), 0);
	stmt->columns[colno].maxlen = SIZE_MAX;
	stmt->columns[colno].precision = 0;

	free(colName);

	return 1;
}

static int pdo_pglite_stmt_get_col(pdo_stmt_t *stmt, int colno, zval *zv, enum pdo_param_type *type)
{
	pdo_pglite_stmt *vStmt = (pdo_pglite_stmt*)stmt->driver_data;

	if(!vStmt->stmt)
	{
		return 0;
	}

	if(colno >= stmt->column_count)
	{
		return 0;
	}

	zval *jeRet = EM_ASM_PTR({
		const results = Module.targets.get($0);
		const current = -1 + $1;
		const colno   = $2;

		if(current >= results.length)
		{
			return null;
		}

		const result = results[current];
		const key = Object.keys(result)[$2];
		const zval = Module.jsToZval(result[key]);

		return zval;

	}, vrzno_fetch_object(Z_OBJ_P(vStmt->results))->targetId, vStmt->curr, colno);

	if(!zv)
	{
		return 0;
	}

	ZVAL_COPY(zv, jeRet);

	return 1;
}

static int pdo_pglite_stmt_param_hook(pdo_stmt_t *stmt, struct pdo_bound_param_data *param, enum pdo_param_event event_type)
{
	pdo_pglite_stmt *vStmt = (pdo_pglite_stmt*) stmt->driver_data;

	switch(event_type)
	{
		case PDO_PARAM_EVT_FREE:
			if(param->driver_data)
			{
				efree(param->driver_data);
			}
			break;

		case PDO_PARAM_EVT_EXEC_PRE:
		case PDO_PARAM_EVT_EXEC_POST:
		case PDO_PARAM_EVT_FETCH_PRE:
		case PDO_PARAM_EVT_FETCH_POST:
			/* work is handled by EVT_NORMALIZE */
			return 1;

		case PDO_PARAM_EVT_NORMALIZE:
			/* decode name from $1, $2 into 0, 1 etc. */
			if(param->name)
			{
				if(ZSTR_VAL(param->name)[0] == '$')
				{
					param->paramno = ZEND_ATOL(ZSTR_VAL(param->name) + 1);
				}
				else
				{
					/* resolve parameter name to rewritten name */
					zend_string *namevar;

					if(stmt->bound_param_map && (namevar = zend_hash_find_ptr(stmt->bound_param_map, param->name)) != NULL)
					{
						param->paramno = -1 + ZEND_ATOL(ZSTR_VAL(namevar) + 1);
					}
					else
					{
						pdo_pglite_error(stmt->dbh, stmt, 1 + param->paramno, "HY093", ZSTR_VAL(param->name), __FILE__, __LINE__);
						return 0;
					}
				}
			}

			EM_ASM({
				const stmtTgtId = $0;
				const paramPtr  = $1;
				const paramPos  = $2;
				const statement = Module.targets.get(stmtTgtId);
				const paramVal  = Module.zvalToJS(paramPtr);

				if(!Module.PdoParams.has(statement))
				{
					Module.PdoParams.set(statement, []);
				}

				const paramList = Module.PdoParams.get(statement);

				paramList[paramPos] = paramVal;

			}, vStmt->stmt->targetId, param->parameter, param->paramno);
			break;
		case PDO_PARAM_EVT_ALLOC:
			if(!stmt->bound_param_map)
			{
				return 1;
			}
			if(!zend_hash_index_exists(stmt->bound_param_map, param->paramno))
			{
				pdo_pglite_error(stmt->dbh, stmt, 1 + param->paramno, "HY093", "parameter was not defined", __FILE__, __LINE__);
				return 0;
			}
			break;
	}

	return true;
}

static int pdo_pglite_stmt_get_attribute(pdo_stmt_t *stmt, zend_long attr, zval *val)
{
	EM_ASM({ console.log('GET ATTR', $0, $1, $2); }, stmt, attr, val);
	return 1;
}

static int pdo_pglite_stmt_col_meta(pdo_stmt_t *stmt, zend_long colno, zval *return_value)
{
	EM_ASM({ console.log('COL META', $0, $1, $2); }, stmt, colno, return_value);
	return 1;
}

static int pdo_pglite_stmt_cursor_closer(pdo_stmt_t *stmt)
{
	EM_ASM({ console.log('CLOSE', $0, $1, $2); }, stmt);
	return 1;
}

const struct pdo_stmt_methods pdo_pglite_stmt_methods = {
	pdo_pglite_stmt_dtor,
	pdo_pglite_stmt_execute,
	pdo_pglite_stmt_fetch,
	pdo_pglite_stmt_describe_col,
	pdo_pglite_stmt_get_col,
	pdo_pglite_stmt_param_hook,
	NULL, /* set_attr */
	pdo_pglite_stmt_get_attribute, /* get_attr */
	pdo_pglite_stmt_col_meta,
	NULL, /* next_rowset */
	pdo_pglite_stmt_cursor_closer
};
