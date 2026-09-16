/**
 * Converts a PHP zval into a queued statement parameter.
 *
 * @function pdo_pglite_js_bind_value
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @param {number} value Address of the caller-owned PHP zval to convert.
 * @param {number} position Zero-based parameter position.
 * @returns {void}
 */
const statement = Module.targets.get(id);
const paramPosition = position;
const params = Module.PdoParams.get(statement) || [];

params[paramPosition] = Module.zvalToJS(value);
Module.PdoParams.set(statement, params);
