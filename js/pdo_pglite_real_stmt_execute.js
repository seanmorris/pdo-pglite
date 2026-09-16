/**
 * Consumes queued parameters once and retains the normalized query result.
 *
 * @function pdo_pglite_real_stmt_execute
 * @param {number} targetId Registered prepared-statement handle; this is not a Wasm
 * address.
 * @param {number} errorPtr Address receiving a malloc-owned UTF-8 error pointer on
 * failure.
 * @param {number} sqlstatePtr Address receiving a malloc-owned five-character SQLSTATE
 * pointer on failure.
 * @returns {Promise<number>} Result handle, or zero with allocated error outputs for
 * the C caller to free.
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
