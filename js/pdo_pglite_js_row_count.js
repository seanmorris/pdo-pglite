/**
 * Reads the number of fetched rows from a retained result.
 *
 * @function pdo_pglite_js_row_count
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @returns {number} Row count, or zero for a missing result.
 */
const result = Module.targets.get(id);
return result && Array.isArray(result.rows) ? result.rows.length : 0;
