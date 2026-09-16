/**
 * Reads the number of fields from a retained result.
 *
 * @function pdo_pglite_js_column_count
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @returns {number} Field count, or zero for a missing result.
 */
const result = Module.targets.get(id);
return result && Array.isArray(result.fields) ? result.fields.length : 0;
