/**
 * Queues a null parameter for the next statement execution.
 *
 * @function pdo_pglite_js_bind_null
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @param {number} position Zero-based parameter position.
 * @returns {void}
 */
const statement = Module.targets.get(id);
const paramPosition = position;
const params = Module.PdoParams.get(statement) || [];

params[paramPosition] = null;
Module.PdoParams.set(statement, params);
