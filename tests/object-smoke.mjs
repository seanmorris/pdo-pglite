import assert from 'node:assert/strict';
import load from './bridge.mjs';

const module = await load();
const targets = new Map();
let next = 1;
module.targets = {
	add(value) { const id = next++; targets.set(id, value); return id; }
	, get: id => targets.get(id)
	, remove: id => targets.delete(id)
};
module.tacked = new Set();
module.PdoParams = new WeakMap();
const values = new Map();
module.jsToZval = (value, address) => values.set(address, value);
module.zvalToJS = address => values.get(address);
const nativeCall = module.ccall;
module.ccall = (name, result, types, args, options) => {
	if(name === 'pdo_pglite_create_string')
	{
		values.set(args[2], module.HEAPU8.slice(args[0], args[0] + args[1]));
		return;
	}
	return nativeCall(name, result, types, args, options);
};
const string = value => {
	const length = module.lengthBytesUTF8(value) + 1;
	const address = module._malloc(length);
	module.stringToUTF8(value, address, length);
	return address;
};
const consume = pointer => {
	assert.notEqual(pointer, 0);
	const value = module.UTF8ToString(pointer);
	module._free(pointer);
	return value;
};
const asyncCall = (name, args, result = 'number') => module.ccall('test_' + name,
	result, args.map(() => 'number'), args, {async: true});
const errors = module._malloc(8);
const errorArgs = [errors, errors + 4];
const error = () => errorArgs.map(address => consume(module.getValue(address, '*')));

assert.equal(module._test_pdo_pglite_js_available(), 0);
const dsn = string(''), sql = string('SELECT $1, $2, $3');
assert.equal(await asyncCall('pdo_pglite_open', [dsn, ...errorArgs]), 0);
assert.match(error()[0], /PGlite class must be provided/);
let closed = 0;
module.PGlite = class {
	constructor(directory) {
		assert.equal(directory, undefined);
		this.waitReady = Promise.resolve();
	}
	async close() { closed++; throw new Error('Close failures must still release ownership'); }
	async exec() { return [{affectedRows: 3}, {fields: [{name: 'ignored'}], rows: [[1]]}]; }
	async query(query, params, options) {
		await Promise.resolve();
		if(query === 'SHOW server_version') return {rows: [{server_version: '18.0'}]};
		if(query === 'SELECT LASTVAL() AS id') return {rows: [{id: '9007199254740993'}]};
		if(query === 'SELECT CURRVAL($1) AS id')
		{
			assert.deepEqual(params, ['sequence']);
			return {rows: [{id: '7'}]};
		}
		assert.equal(options.rowMode, 'array');
		assert.equal(options.parsers[20]('9007199254740993'), '9007199254740993');
		if(params[0] === 'fail') throw Object.assign(new Error('query failed'), {code: '23505'});
		return {rows: [params], fields: [{name: 'first'}, {name: 'bytes', dataTypeID: 17}, {name: 'empty'}]};
	}
};
assert.equal(module._test_pdo_pglite_js_available(), 1);
const db = await asyncCall('pdo_pglite_open', [dsn, ...errorArgs]);
assert.notEqual(db, 0);
assert.equal(await asyncCall('pdo_pglite_real_exec', [db, sql, ...errorArgs]), 3);
assert.equal(consume(await asyncCall('pdo_pglite_real_server_version', [db, ...errorArgs])), '18.0');
assert.equal(consume(await asyncCall('pdo_pglite_real_last_insert_id', [db, 0, ...errorArgs])), '9007199254740993');
const sequence = string('sequence');
assert.equal(consume(await asyncCall('pdo_pglite_real_last_insert_id', [db, sequence, ...errorArgs])), '7');
module._free(sequence);

const statement = module._test_pdo_pglite_js_prepare(db, sql, ...errorArgs);
assert.notEqual(statement, 0);
values.set(100, 9007199254740993n);
module._test_pdo_pglite_js_bind_value(statement, 100, 0);
const bytes = module._malloc(3);
module.HEAPU8.set([7, 0, 9], bytes);
module._test_pdo_pglite_js_bind_bytes(statement, bytes, 3, 1);
module.HEAPU8.fill(255, bytes, bytes + 3);
module._free(bytes);
module._test_pdo_pglite_js_bind_null(statement, 2);
const result = await asyncCall('pdo_pglite_real_stmt_execute', [statement, ...errorArgs]);
assert.notEqual(result, 0);
assert.equal(module.PdoParams.has(targets.get(statement)), false);
assert.equal(module._test_pdo_pglite_js_row_count(result), 1);
assert.equal(module._test_pdo_pglite_js_column_count(result), 3);
assert.equal(module._test_pdo_pglite_js_affected_rows(result), 1);
assert.equal(consume(module._test_pdo_pglite_js_column_name(result, 1)), 'bytes');
for(let index = 0; index < 3; index++)
	assert.equal(module._test_pdo_pglite_js_column_value(result, 1, index, 200 + index), 1);
assert.equal(values.get(200), '9007199254740993');
assert.deepEqual(Array.from(values.get(201)), [7, 0, 9]);
assert.equal(values.get(202), null);
assert.equal(module._test_pdo_pglite_js_column_value(result, 2, 0, 200), 0);
module._test_pdo_pglite_js_release(result);
assert.equal(module._test_pdo_pglite_js_row_count(result), 0);
assert.equal(module._test_pdo_pglite_js_column_name(result, 0), 0);

values.set(100, 'fail');
module._test_pdo_pglite_js_bind_value(statement, 100, 0);
assert.equal(await asyncCall('pdo_pglite_real_stmt_execute', [statement, ...errorArgs]), 0);
assert.deepEqual(error(), ['query failed', '23505']);
assert.equal(module.PdoParams.has(targets.get(statement)), false);
module._test_pdo_pglite_js_bind_null(statement, 0);
module._test_pdo_pglite_js_forget_params(statement);
assert.equal(module.PdoParams.has(targets.get(statement)), false);
module._test_pdo_pglite_js_release(statement);
await asyncCall('pdo_pglite_close', [db], null);
assert.equal(closed, 1);
assert.equal(targets.size, 0);
assert.equal(module.tacked.size, 0);
assert.equal(await asyncCall('pdo_pglite_real_exec', [db, sql, ...errorArgs]), -1);
assert.match(error()[0], /no longer available/);
for(const address of [dsn, sql, errors]) module._free(address);
console.log('embedded bridge passed');
