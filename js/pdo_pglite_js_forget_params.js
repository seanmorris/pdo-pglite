/**
 * Discards queued parameters when a statement is destroyed.
 *
 * @function pdo_pglite_js_forget_params
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @returns {void}
 */
const statement = Module.targets.get(id);

if(statement)
{
	Module.PdoParams.delete(statement);
}
