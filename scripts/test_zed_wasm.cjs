#!/usr/bin/env node
/* Validate the exact staged grammar and queries without relying on parser caches. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

/** Parse union/control source and require useful captures from every staged Zed query. */
async function main() {
  const [runtime, stage] = process.argv.slice(2);
  assert(runtime && stage, 'usage: test_zed_wasm.cjs WEB_RUNTIME_CJS STAGED_EXTENSION');
  const {Parser, Language, Query} = require(path.resolve(runtime));
  await Parser.init();
  const language = await Language.load(path.join(path.resolve(stage), 'grammars/fern.wasm'));
  const parser = new Parser();
  parser.setLanguage(language);
  const source = 'type Choice = Int | String\nfn size(value:Choice)->Int:\n' +
    '    match value:\n        n:Int -> n\n        _:String -> 0\n' +
    'fn add(left:Int,right:Int)->Int:left+right\n' +
    'fn main():\n    println(add(right:2,left:1))\n    let xs = [1, 2]\n    defer println("done")\n' +
    '    for x in xs:\n        println(size(x))\n';
  const tree = parser.parse(source);
  assert(!tree.rootNode.hasError, tree.rootNode.toString());
  for (const name of ['highlights', 'outline', 'indents', 'brackets']) {
    const text = fs.readFileSync(path.join(stage, 'languages/fern', name + '.scm'), 'utf8');
    const query = new Query(language, text);
    const captures = query.captures(tree.rootNode);
    assert(captures.length > 0, name + ': empty captures');
    if (name === 'highlights') {
      for (const word of ['for', 'defer', 'match']) {
        assert(captures.some(c => c.name === 'keyword' && c.node.text === word), word);
      }
      assert(captures.some(c => c.name === 'operator' && c.node.text === '|'));
      for (const label of ['left', 'right']) {
        assert(captures.some(c => c.name === 'variable.parameter' && c.node.text === label), label);
      }
    }
    if (name === 'outline') {
      assert.deepEqual(captures.filter(c => c.name === 'name').map(c => c.node.text),
        ['Choice', 'size', 'add', 'main']);
    }
    query.delete();
  }
  tree.delete();
  parser.delete();
  console.log('Zed package: exact staged grammar, union/control syntax and four staged queries passed');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
