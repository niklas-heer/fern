#!/usr/bin/env node
/* Execute the exact published WASM with the pinned web runtime and compile every Zed query. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');

/** Require meaningful captures instead of merely accepting query syntax. */
function checkQueries(language, Query, parser) {
  const tree = parser.parse(fs.readFileSync(path.join(root, 'editor/tree-sitter-fern/test/parity/queries.fn'), 'utf8'));
  assert(!tree.rootNode.hasError, 'query source must parse completely');
  const directory = path.join(root, 'editor/zed-fern/languages/fern');
  for (const name of ['highlights', 'outline', 'indents', 'brackets']) {
    const query = new Query(language, fs.readFileSync(path.join(directory, name + '.scm'), 'utf8'));
    const captures = query.captures(tree.rootNode);
    assert(captures.length > 0, name + ' must capture a representative source');
    if (name === 'outline') {
      assert.deepEqual(captures.filter(c => c.name === 'name').map(c => c.node.text), ['Name', 'Id', 'unwrap', 'Choice', 'size', 'Entry', 'workflow', 'following', 'label_probe', 'label_usage']);
    }
    if (name === 'highlights') {
      assert.equal(captures.filter(c => c.name === 'variable.parameter' && c.node.text === 'external').length, 2);

      assert(captures.some(c => c.node.text === 'unwrap' && c.name === 'function'));
      assert(captures.some(c => c.node.text === 'Id' && c.name.startsWith('type')));
      assert(captures.some(c => c.node.text === '|' && c.name === 'operator'));
      for (const text of ['for', 'with', 'defer', 'continue', 'break']) {
        assert(captures.some(c => c.name === 'keyword' && c.node.text === text), text);
      }
      for (const text of ['<-', '..=']) {
        assert(captures.some(c => c.name === 'operator' && c.node.text === text), text);
      }
      assert(captures.some(c => c.name === 'property' && c.node.text === 'count'));
      for (const text of ['number', '_']) {
        assert(captures.some(c => c.name === 'variable' && c.node.text === text &&
          c.node.parent?.parent?.type === 'typed_pattern'), text + ': typed pattern binding');
      }
    }
    query.delete();
  }
  tree.delete();
}

/** Require type precedence and typed-pattern structure independently of display serialization. */
function checkTypePaths(tree, test) {
  for (const expected of test.paths || []) {
    const [first, ...rest] = expected.slice(3).split('/');
    let nodes = tree.rootNode.descendantsOfType(first);
    for (const type of rest) nodes = nodes.flatMap(n => n.namedChildren.filter(c => c.type === type));
    assert(nodes.length > 0, test.name + ': ' + expected);
  }
}

/** Match exact error locations while making the three known recovery gaps visible. */
function checkErrors(tree, test) {
  const pending = [tree.rootNode];
  const ranges = new Map();
  const byteIndex = index => Buffer.byteLength(test.source.slice(0, index));
  while (pending.length) {
    const node = pending.pop();
    if (node.isError || node.isMissing) {
      const quoted = node.isNamed ? node.type : JSON.stringify(node.type);
      const kind = node.isMissing ? 'MISSING ' + quoted : 'ERROR';
      const range = [kind, byteIndex(node.startIndex), byteIndex(node.endIndex)];
      ranges.set(JSON.stringify(range), range);
    }
    for (const child of node.children) pending.push(child);
  }
  const actual = [...ranges.values()].sort((a, b) =>
    a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : a[1] - b[1] || a[2] - b[2]);
  assert.deepEqual(actual, test.error_ranges, test.name + ': error byte ranges');
  const names = tree.rootNode.descendantsOfType('function_definition')
    .map(n => n.childForFieldName('name')?.text);
  assert(names.includes('after'), test.name + ': recovery');
}

/** Load an explicit artifact; exercise positive/recovery sources without host FFI or parser caches. */
async function main() {
  const [runtime, artifact] = process.argv.slice(2);
  assert(runtime && artifact, 'usage: test_editor_wasm.cjs WEB_RUNTIME_CJS GRAMMAR_WASM');
  const {Parser, Language, Query} = require(path.resolve(runtime));
  await Parser.init();
  const language = await Language.load(path.resolve(artifact));
  const parser = new Parser();
  parser.setLanguage(language);
  const cases = JSON.parse(fs.readFileSync(path.join(root, 'editor/tree-sitter-fern/test/parity/cases.json')));
  for (const test of cases.valid) {
    const tree = parser.parse(test.source);
    assert(!tree.rootNode.hasError, test.name + ': ' + tree.rootNode.toString());
    for (const node of test.nodes) assert(tree.rootNode.toString().includes('(' + node), test.name + ': ' + node);
    checkTypePaths(tree, test);
    tree.delete();
  }
  for (const test of cases.invalid) {
    const tree = parser.parse(test.source);
    assert(tree.rootNode.hasError, test.name);
    checkErrors(tree, test);
    tree.delete();
  }
  checkQueries(language, Query, parser);
  parser.delete();
  process.stdout.write(`Exact WASM artifact: ${cases.valid.length} accepted, ${cases.invalid.length} malformed (all recovered), 4 executable queries\n`);
}
main().catch(error => { console.error(error); process.exitCode = 1; });
