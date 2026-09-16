/**
 * Releases a statement or result target and its strong retention.
 *
 * @function pdo_pglite_js_release
 * @param {number} id Registered target handle; this is not a Wasm address.
 * @returns {void}
 */
const targetId = id;
const target = Module.targets.get(targetId);

if(target)
{
	Module.tacked.delete(target);
}

Module.targets.remove(targetId);
