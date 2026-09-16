/**
 * Constructs and awaits a host PGlite database, closing partial instances on failure.
 *
 * @function pdo_pglite_open
 * @param {number} dataSourcePtr Wasm address of the DSN; an empty string selects an in-
 * memory database.
 * @param {number} errorPtr Address receiving a malloc-owned UTF-8 error pointer on
 * failure.
 * @param {number} sqlstatePtr Address receiving a malloc-owned five-character SQLSTATE
 * pointer on failure.
 * @returns {Promise<number>} Retained database handle, or zero with allocated error
 * outputs for the C caller to free.
 */
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
