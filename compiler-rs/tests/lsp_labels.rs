use fern_prototype::lsp;

fn quote(text: &str) -> String {
    format!(
        "\"{}\"",
        text.replace('\\', "\\\\")
            .replace('"', "\\\"")
            .replace('\n', "\\n")
            .replace('\r', "\\r")
    )
}
fn run(messages: Vec<String>) -> Vec<String> {
    use std::fmt::Write;
    let mut input = String::new();
    for message in messages {
        write!(
            &mut input,
            "Content-Length: {}\r\n\r\n{message}",
            message.len()
        )
        .unwrap();
    }
    let mut output = Vec::new();
    lsp::serve(std::io::Cursor::new(input), &mut output).unwrap();
    let text = String::from_utf8(output).unwrap();
    text.split("Content-Length: ")
        .skip(1)
        .map(|frame| frame.split_once("\r\n\r\n").unwrap().1.to_owned())
        .collect()
}
fn query(marked: &str, method: &str) -> String {
    let offset = marked.find('§').unwrap();
    let before = &marked[..offset];
    let line = before.bytes().filter(|b| *b == b'\n').count();
    let character = before.rsplit('\n').next().unwrap().encode_utf16().count();
    let source = marked.replace('§', "");
    let messages = vec![
        r#"{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}"#.into(),
        format!(
            r#"{{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{{"textDocument":{{"uri":"untitled:test","version":1,"text":{}}}}}}}"#,
            quote(&source)
        ),
        format!(
            r#"{{"jsonrpc":"2.0","id":"query","method":"textDocument/{method}","params":{{"textDocument":{{"uri":"untitled:test"}},"position":{{"line":{line},"character":{character}}}}}}}"#
        ),
        r#"{"jsonrpc":"2.0","id":9,"method":"shutdown"}"#.into(),
        r#"{"jsonrpc":"2.0","method":"exit"}"#.into(),
    ];
    run(messages)
        .into_iter()
        .find(|m| m.contains("\"id\":\"query\""))
        .unwrap()
}
#[test]
fn argument_labels_do_not_resolve_to_same_spelled_caller_locals() {
    let source =
        "fn f(value:Int)->Int:value\nfn main():\n    let value=2\n    println(f(val§ue:3))\n";
    assert!(query(source, "definition").contains("\"result\":null"));
}
#[test]
fn explicit_external_label_does_not_replace_a_pattern_binding() {
    let source = "fn f(input value:Int)->Int:val§ue\nfn main():println(f(input:3))\n";
    let result = query(source, "definition");
    assert!(result.contains("\"character\":11,\"line\":0"), "{result}");
}

#[test]
fn external_label_declarations_do_not_refer_to_unrelated_globals() {
    let source =
        "fn input()->Int:1\nfn f(in§put value:Int)->Int:value\nfn main():println(f(input:3))\n";
    assert!(query(source, "definition").contains("\"result\":null"));
}
