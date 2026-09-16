/**
 * Copies a field name from a retained result into Wasm memory.
 *
 * @function pdo_pglite_js_column_name
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @param {number} column Zero-based field index.
 * @returns {number} Malloc-owned UTF-8 name for the C caller to free, or zero when
 * unavailable.
 */
const result = Module.targets.get(id);
const field = result && result.fields ? result.fields[column] : null;

if(!field || typeof field.name !== 'string')
{
	return 0;
}

const length = lengthBytesUTF8(field.name) + 1;
const location = _malloc(length);

stringToUTF8(field.name, location, length);
return location;
