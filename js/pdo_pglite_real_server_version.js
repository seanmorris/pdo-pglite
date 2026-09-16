/**
 * Queries the embedded PostgreSQL server version.
 *
 * @function pdo_pglite_real_server_version
 * @param {number} dbId Registered database handle; this is not a Wasm address.
 * @param {number} errorPtr Address receiving a malloc-owned UTF-8 error pointer on
 * failure.
 * @param {number} sqlstatePtr Address receiving a malloc-owned five-character SQLSTATE
 * pointer on failure.
 * @returns {Promise<number>} Malloc-owned UTF-8 version for the C caller to free, or
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
