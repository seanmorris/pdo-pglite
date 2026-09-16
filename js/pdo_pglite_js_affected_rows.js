/**
 * Reports fetched rows for a result set or affected rows for a write.
 *
 * @function pdo_pglite_js_affected_rows
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @returns {number} PDO row count, or zero for a missing result.
 */
const result = Module.targets.get(id);

if(!result)
{
	return 0;
}

return result.fields.length
	? result.rows.length
	: result.affectedRows;
