/**
 * Retains a query closure using array rows and lossless PostgreSQL text parsers.
 *
 * @function pdo_pglite_js_prepare
 * @param {number} dbId Registered database handle; this is not a Wasm address.
 * @param {number} sql Wasm address of NUL-terminated UTF-8 SQL.
 * @param {number} errorAddress Address receiving a malloc-owned UTF-8 error pointer on
 * failure.
 * @param {number} stateAddress Address receiving a malloc-owned five-character SQLSTATE
 * pointer on failure.
 * @returns {number} Registered statement handle, or zero after allocating error outputs
 * for the C caller to free.
 */
const db = Module.targets.get(dbId);
const query = UTF8ToString(sql);
const errorPtr = errorAddress;
const sqlstatePtr = stateAddress;

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
		rowMode: 'array'
		, parsers
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
