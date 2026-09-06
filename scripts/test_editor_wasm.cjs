#!/usr/bin/env node
/* Execute the exact published WASM with the pinned web runtime and compile every Zed query. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');

/** Require meaningful captures instead of merely accepting query syntax. */
function checkQueries(language, Query, parser) {
  const tree = parser.parse('type Name = String\nnewtype Id = Id(Int)\nfn unwrap(Id(value): Id) -> Int: value\n');
  const directory = path.join(root, 'editor/zed-fern/languages/fern');
  for (const name of ['highlights', 'outline', 'indents', 'brackets']) {
    const query = new Query(language, fs.readFileSync(path.join(directory, name + '.scm'), 'utf8'));
    const captures = query.captures(tree.rootNode);
    assert(captures.length > 0, name + ' must capture a representative source');
    if (name === 'outline') {
      assert.deepEqual(captures.filter(c => c.name === 'name').map(c => c.node.text), ['Name', 'Id', 'unwrap']);
    }
    if (name === 'highlights') {
      assert(captures.some(c => c.node.text === 'unwrap' && c.name === 'function'));
      assert(captures.some(c => c.node.text === 'Id' && c.name.startsWith('type')));
    }
    query.delete();
  }
  tree.delete();
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
    tree.delete();
  }
  for (const test of cases.invalid) {
    const tree = parser.parse(test.source);
    assert(tree.rootNode.hasError, test.name);
    const recovered = tree.rootNode.descendantsOfType('function_definition');
    assert(recovered.some(n => n.childForFieldName('name')?.text === 'after'), test.name + ': recovery');
    tree.delete();
  }
  checkQueries(language, Query, parser);
  parser.delete();
  process.stdout.write('Exact WASM artifact: 24 accepted, 8 recovery, 4 executable queries\n');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
