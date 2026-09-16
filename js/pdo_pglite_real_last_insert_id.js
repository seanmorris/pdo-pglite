/**
 * Queries LASTVAL or a named sequence without converting its value to a JS number.
 *
 * @function pdo_pglite_real_last_insert_id
 * @param {number} dbId Registered database handle; this is not a Wasm address.
 * @param {number} namePtr Wasm address of the sequence name, or zero to query LASTVAL.
 * @param {number} errorPtr Address receiving a malloc-owned UTF-8 error pointer on
 * failure.
 * @param {number} sqlstatePtr Address receiving a malloc-owned five-character SQLSTATE
 * pointer on failure.
 * @returns {Promise<number>} Malloc-owned UTF-8 result for the C caller to free, or
 * zero with allocated error outputs.
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
