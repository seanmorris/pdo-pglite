/**
 * Executes SQL and counts changed rows while excluding result sets with columns.
 *
 * @function pdo_pglite_real_exec
 * @param {number} dbId Registered database handle; this is not a Wasm address.
 * @param {number} sql Wasm address of NUL-terminated UTF-8 SQL.
 * @param {number} errorPtr Address receiving a malloc-owned UTF-8 error pointer on
 * failure.
 * @param {number} sqlstatePtr Address receiving a malloc-owned five-character SQLSTATE
 * pointer on failure.
 * @returns {Promise<number>} Affected row count, or -1 after allocating error outputs
 * for the C caller to free.
 */
/**
 * Allocates message and SQLSTATE outputs owned by the C error handler.
 * @param {unknown} exception Rejected database operation.
 * @returns {void}
 */
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
