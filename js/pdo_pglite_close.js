/**
 * Closes a database and always releases its retained target, including on rejection.
 *
 * @function pdo_pglite_close
 * @param {number} targetId Registered database handle to close.
 * @returns {Promise<void>} Resolves after cleanup; close failures are intentionally
 * ignored by the destructor.
 */
const db = Module.targets.get(targetId);

try
{
	if(db && typeof db.close === 'function')
	{
		await db.close();
	}
}
catch(exception)
{
	/* Destructors cannot report a useful PDO error; cleanup must still finish. */
}
finally
{
	if(db)
	{
		Module.tacked.delete(db);
	}

	Module.targets.remove(targetId);
}
