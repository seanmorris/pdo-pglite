import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';
import { parse } from 'espree';

const root = fileURLToPath(new URL('../', import.meta.url));
const inputs = fs.readdirSync(path.join(root, 'js')).filter(name => /^pdo_pglite_[a-z_]+\.js$/.test(name)).map(name => 'js/' + name);

function syntax(source)
{
	const ast = parse(source, { ecmaVersion: 'latest', sourceType: 'module', ecmaFeatures: { globalReturn: true } });
	return JSON.parse(JSON.stringify(ast, (key, value) => ['start', 'end'].includes(key) ? undefined : value));
}

function fixture(t, outOfTree = false)
{
	const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'pdo-pglite-make-'));
	t.after(() => fs.rmSync(temporary, { recursive: true, force: true }));
	const source = path.join(temporary, 'source');
	const build = outOfTree ? path.join(temporary, 'build') : source;
	fs.mkdirSync(source);
	fs.mkdirSync(build, { recursive: true });
	for(const name of ['Makefile.frag', 'pdo_pglite_js.h.in', ...inputs])
	{
		fs.mkdirSync(path.dirname(path.join(source, name)), {recursive: true});
		fs.copyFileSync(path.join(root, name), path.join(source, name));
	}
	// PHP_ADD_MAKEFILE_FRAGMENT substitutes these paths during configure.
	const fragment = fs.readFileSync(path.join(source, 'Makefile.frag'), 'utf8')
		.replaceAll('$(srcdir)', source).replaceAll('$(builddir)', build);
	const header = path.join(build, 'generated/pdo_pglite_js.h');
	const dependencies = path.join(build, 'generated/pdo_pglite_js.d');
	const object = path.join(build, 'pdo_pglite.lo');
	fs.writeFileSync(path.join(build, 'Makefile'), `all: ${object}
distclean: clean
${object}:
\t@cat "${header}" > "$@"
${fragment}`);
	const run = (goal = 'all', success = true, variables = []) => {
		const result = spawnSync('make', ['--no-print-directory', '-j8', `CC=${process.env.CC || 'emcc'}`, ...variables, goal], {
			cwd: build, encoding: 'utf8', timeout: 60000
		});
		if(success) assert.equal(result.status, 0, result.stdout + result.stderr);
		else
		{
			assert.equal(result.error, undefined, 'Make must fail without hanging');
			assert.notEqual(result.status, 0, 'Expected Make to reject missing JS');
		}
		return result;
	};
	const read = () => fs.readFileSync(header, 'utf8');
	const prepareEdit = () => {
		// Only the next edited input may be newer than the outputs. Otherwise a
		// stale template could hide a missing dependency on an individual JS file.
		const inputTime = new Date(Date.now() - 10000);
		const outputTime = new Date(Date.now() - 5000);
		for(const name of ['Makefile.frag', 'pdo_pglite_js.h.in', ...fs.readdirSync(source, { recursive: true }).filter(name => name.endsWith('.js'))])
			fs.utimesSync(path.join(source, name), inputTime, inputTime);
		for(const file of [header, dependencies, object]) fs.utimesSync(file, outputTime, outputTime);
	};
	return { source, build, header, dependencies, object, run, read, prepareEdit };
}

for(const outOfTree of [false, true])
{
	test(`Make embeds JS before object compilation (${outOfTree ? 'separate build directory' : 'source directory'})`, t => {
		const f = fixture(t, outOfTree);
		f.run();
		const header = f.read();
		assert.doesNotMatch(header, /^#include/m);
		assert.equal([...header.matchAll(/^EM_JS\(/gm)].length, 12);
		assert.equal([...header.matchAll(/^EM_ASYNC_JS\(/gm)].length, 6);
		for(const name of inputs)
		{
			const functionName = path.basename(name).replace('.js', '');
			const body = header.match(new RegExp('^EM_(?:ASYNC_)?JS\\([^\\n]*\\b' + functionName + '\\b[^\\n]*\\{\\n([\\s\\S]*?)^\\}\\);', 'm'))?.[1];
			assert.notEqual(body, undefined, name);
			// The preprocessor normalizes indentation tabs. Check the actual JS
			// syntax, including literal values and control flow, independently of it.
			assert.deepEqual(syntax(body), syntax(fs.readFileSync(path.join(f.source, name), 'utf8')), name);
			assert.ok(fs.readFileSync(f.dependencies, 'utf8').includes(name), name);
		}
		assert.equal(fs.readFileSync(f.object, 'utf8'), header);
		const mtimes = [f.header, f.dependencies, f.object].map(file => fs.statSync(file, { bigint: true }).mtimeNs);
		f.run();
		assert.deepEqual([f.header, f.dependencies, f.object].map(file => fs.statSync(file, { bigint: true }).mtimeNs), mtimes);

		// An edit to any included body must regenerate the header and its object.
		for(const name of inputs)
		{
			f.prepareEdit();
			const marker = `// changed ${name}: $value, \\n, "quotes", \`template\`\n`;
			fs.appendFileSync(path.join(f.source, name), marker);
			f.run();
			assert.ok(f.read().includes(marker));
			assert.equal(fs.readFileSync(f.object, 'utf8'), f.read());
		}
		f.run('clean');
		assert.ok(!fs.existsSync(f.header));
		assert.ok(!fs.existsSync(f.dependencies));
		f.run();
		assert.equal(fs.readFileSync(f.object, 'utf8'), f.read());
	});
}

test('Make tracks nested includes, reports missing files, and recovers when an include is removed', t => {
	const f = fixture(t, true);
	f.run();
	f.prepareEdit();
	const extra = path.join(f.source, 'pdo_pglite_extra.js');
	const leaf = path.join(f.source, 'nested/pdo_pglite_leaf.js');
	fs.mkdirSync(path.dirname(leaf));
	fs.writeFileSync(leaf, '// additional body\n');
	fs.writeFileSync(extra, ' \t# include "nested/pdo_pglite_leaf.js" /* nested include */\n');
	fs.appendFileSync(path.join(f.source, 'pdo_pglite_js.h.in'), ' # include "pdo_pglite_extra.js" // new include\n');
	f.run();
	assert.ok(f.read().includes('// additional body\n'));
	assert.doesNotMatch(f.read(), /^\s*#\s*include/m);
	assert.ok(fs.readFileSync(f.dependencies, 'utf8').includes(leaf));
	f.prepareEdit();
	fs.appendFileSync(leaf, '// changed additional body\n');
	f.run();
	assert.ok(f.read().includes('// changed additional body\n'));
	assert.equal(fs.readFileSync(f.object, 'utf8'), f.read());
	const header = f.read();
	const dependencies = fs.readFileSync(f.dependencies, 'utf8');
	f.prepareEdit();
	fs.unlinkSync(leaf);
	const result = f.run('all', false);
	assert.match(result.stderr, /pdo_pglite_extra\.js:1:\d+: fatal error:.*pdo_pglite_leaf\.js.*file not found/);
	assert.equal(f.read(), header);
	assert.equal(fs.readFileSync(f.dependencies, 'utf8'), dependencies);
	assert.ok(!fs.existsSync(f.header + '.tmp'));
	assert.ok(!fs.existsSync(f.dependencies + '.tmp'));
	// The old depfile still names the deleted leaf. -MP lets the edited parent
	// remove that include instead of leaving Make stuck on a stale dependency.
	fs.writeFileSync(extra, '// nested include removed\n');
	f.run();
	assert.ok(f.read().includes('// nested include removed\n'));
	assert.ok(!fs.readFileSync(f.dependencies, 'utf8').includes(leaf));
});

test('preprocessing preserves macro-like JS names, comments, and template literals', t => {
	const f = fixture(t);
	const body = 'const object = { __LINE__: 17, __FILE__: "kept", __COUNTER__: 23, unix: 29 };\n'
		+ '/* preserved comment */\nconst $value = `literal ${object.__LINE__} \\n`;\n';
	fs.appendFileSync(path.join(f.source, inputs[0]), body);
	f.run();
	assert.ok(f.read().includes(body), f.read());
});

test('Make regenerates either missing output and restores incremental dependency tracking', t => {
	const f = fixture(t, true);
	f.run();
	for(const missing of [f.header, f.dependencies])
	{
		fs.unlinkSync(missing);
		f.run();
		assert.ok(fs.existsSync(f.header));
		assert.ok(fs.existsSync(f.dependencies));
		assert.equal(fs.readFileSync(f.object, 'utf8'), f.read());
		f.prepareEdit();
		const marker = `// edited after removing ${path.basename(missing)}\n`;
		fs.appendFileSync(path.join(f.source, inputs[0]), marker);
		f.run();
		assert.ok(f.read().includes(marker));
		assert.equal(fs.readFileSync(f.object, 'utf8'), f.read());
	}
});

test('clean targets need neither the compiler nor complete JS inputs', t => {
	const f = fixture(t, true);
	for(const goal of ['clean', 'distclean', 'clean-pdo-pglite-js'])
		f.run(goal, true, ['CC=false']);
	f.run();
	fs.unlinkSync(path.join(f.source, inputs[0]));
	for(const file of [f.header, f.dependencies]) fs.writeFileSync(file + '.tmp', 'interrupted generation');
	f.run('clean', true, ['CC=false']);
	for(const file of [f.header, f.dependencies, f.header + '.tmp', f.dependencies + '.tmp'])
		assert.ok(!fs.existsSync(file), file);
});


test('all generated calls link from the object alone and execute through Asyncify', t => {
	const f = fixture(t, true);
	f.run();
	const declarations = [...fs.readFileSync(path.join(root, 'pdo_pglite_js.h.in'), 'utf8')
		.matchAll(/EM_(?:ASYNC_)?JS\(([^,]+), (\w+), \(([^)]*)\), \{/g)];
	assert.equal(declarations.length, 18);
	const wrappers = declarations.map(([, result, name, parameters]) => {
		const args = parameters === 'void' ? '' : parameters.split(',').map(value => value.match(/\w+$/)[0]).join(', ');
		return `EMSCRIPTEN_KEEPALIVE ${result} test_${name}(${parameters}) { ${result === 'void' ? '' : 'return '}${name}(${args}); }\n`;
	}).join('');
	fs.writeFileSync(path.join(f.build, 'bridge.c'), '#include <emscripten.h>\n#include <stdlib.h>\n'
		+ 'typedef void jstarget; typedef void zval;\n#include <pdo_pglite_js.h>\n' + wrappers);
	const run = (command, args) => {
		const result = spawnSync(command, args, {cwd: f.build, encoding: 'utf8', timeout: 60000});
		assert.equal(result.status, 0, String(result.error || '') + result.stdout + result.stderr);
		return result;
	};
	const compiler = process.env.CC || 'emcc';
	run(compiler, ['-Werror', '-I' + path.dirname(f.header), '-c', 'bridge.c', '-o', 'bridge.o']);
	fs.rmSync(f.source, {recursive: true});
	fs.rmSync(path.dirname(f.header), {recursive: true});
	fs.unlinkSync(path.join(f.build, 'bridge.c'));
	run(compiler, ['bridge.o', '--no-entry', '-o', 'bridge.mjs', '-sASYNCIFY=1', '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sENVIRONMENT=node', '-sEXPORTED_FUNCTIONS=["_malloc","_free"]', '-sEXPORTED_RUNTIME_METHODS=["ccall","UTF8ToString","lengthBytesUTF8","stringToUTF8","HEAPU8","getValue","setValue"]']);
	fs.copyFileSync(path.join(root, 'tests/object-smoke.mjs'), path.join(f.build, 'verify.mjs'));
	assert.match(run(process.execPath, ['verify.mjs']).stdout, /embedded bridge passed/);
});
