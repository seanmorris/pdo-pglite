/**
 * Snapshots a binary parameter so later heap changes cannot alter the queued value.
 *
 * @function pdo_pglite_js_bind_bytes
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @param {number} address Wasm address of the binary parameter bytes.
 * @param {number} size Number of parameter bytes to copy, including embedded NUL bytes.
 * @param {number} position Zero-based parameter position.
 * @returns {void}
 */
const statement = Module.targets.get(id);
const start = address;
const length = size;
const paramPosition = position;
const params = Module.PdoParams.get(statement) || [];
const value = Module.HEAPU8.slice(start, start + length);

params[paramPosition] = value;
Module.PdoParams.set(statement, params);
