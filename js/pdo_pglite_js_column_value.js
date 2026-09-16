/**
 * Copies a result cell into a PHP zval, preserving binary bytes and large numeric
 * values.
 *
 * @function pdo_pglite_js_column_value
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @param {number} cursor One-based current row number.
 * @param {number} index Zero-based column index.
 * @param {number} destination Address of the caller-owned PHP zval receiving the value.
 * @returns {number} One when the value was copied, or zero for an unavailable row.
 * Temporary byte storage is freed here.
 */
const result = Module.targets.get(id);
const current = cursor - 1;
const column = index;
const returnValue = destination;

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
