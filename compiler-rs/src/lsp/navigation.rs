//! Editor request routing over fresh source snapshots; no stale or generated identities.
use super::index::Index;
use super::*;

impl Server {
    /// Validate cursor coordinates before loading one coherent set of accepted open buffers.
    pub(super) fn navigation(&self, method: &str, params: &Json) -> Result<Json> {
        let uri = document_uri(params)?;
        let document = self
            .documents
            .get(uri)
            .ok_or("request document is not open")?;
        let cursor = byte_position(&document.source, field(params, "position")?)?;
        let sources = self.overlays();
        let path = file_path(uri)?;
        if let Some(path) = path {
            let identity = modules::source_identity(&path).map_err(|e| e.message)?;
            let loaded = modules::load_editor_sources(&path, &sources).ok();
            let index = loaded
                .as_ref()
                .and_then(|l| Index::loaded(l, &identity, cursor));
            self.navigation_result(method, uri, &document.source, cursor, index.as_ref())
        } else {
            let path = PathBuf::from(uri);
            let program = parse::parse(&document.source).ok();
            let index = program
                .as_ref()
                .and_then(|p| Index::single(p, &document.source, &path, cursor));
            self.navigation_result(method, uri, &document.source, cursor, index.as_ref())
        }
    }
    /// Current accepted overlays always win over disk, including new unsaved dependency files.
    fn overlays(&self) -> HashMap<PathBuf, String> {
        self.documents
            .iter()
            .filter_map(|(uri, doc)| {
                let path = file_path(uri).ok().flatten()?;
                let path = modules::source_identity(&path).ok()?;
                Some((path, doc.source.clone()))
            })
            .collect()
    }
    /// Return source locations or completion from the current graph, never stale positions.
    fn navigation_result(
        &self,
        method: &str,
        uri: &str,
        source: &str,
        cursor: usize,
        index: Option<&Index<'_>>,
    ) -> Result<Json> {
        if method == "textDocument/definition" {
            let Some(index) = index else {
                return Ok(Json::Null);
            };
            let Some(target) = index.target else {
                return Ok(Json::Null);
            };
            let Some((path, text, span)) = index.location(target) else {
                return Ok(Json::Null);
            };
            let uri = self.definition_uri(uri, path);
            return Ok(object([
                ("uri", string(uri)),
                ("range", source_range(text, span)),
            ]));
        }
        Ok(completion(source, cursor, index))
    }
    /// Reuse a client's original URI when an imported source is open under that spelling.
    fn definition_uri(&self, current: &str, path: &Path) -> String {
        if file_path(current).ok().flatten().is_none() {
            return current.into();
        }
        self.documents
            .keys()
            .find(|uri| {
                file_path(uri)
                    .ok()
                    .flatten()
                    .and_then(|p| modules::source_identity(&p).ok())
                    .as_deref()
                    == Some(path)
            })
            .cloned()
            .unwrap_or_else(|| path_uri(path))
    }
}

/// Translate both source byte endpoints into the negotiated UTF-16 coordinates.
fn source_range(source: &str, span: Span) -> Json {
    object([
        ("start", position(source, span.start)),
        ("end", position(source, span.end)),
    ])
}

/// Bound deterministic candidates and replace only the current identifier's UTF-16 range.
fn completion(source: &str, cursor: usize, index: Option<&Index<'_>>) -> Json {
    let Some((receiver, prefix, span)) = index::prefix(source, cursor) else {
        return completion_list(Vec::new(), false);
    };
    let mut candidates = BTreeMap::new();
    let shadowed = index.is_some_and(|i| {
        i.locals
            .contains_key(receiver.split('.').next().unwrap_or(&receiver))
    });
    if receiver.is_empty() || !shadowed {
        for name in index::builtins() {
            add_candidate(&mut candidates, &name, 3, &receiver, &prefix);
        }
        if let Some(index) = index {
            for (name, target) in &index.visible {
                let kind = index.globals.get(target).map_or(9, |s| s.kind);
                add_candidate(&mut candidates, name, kind, &receiver, &prefix);
            }
        }
    }
    if receiver.is_empty() {
        for name in [
            "fn", "let", "if", "else", "match", "with", "do", "return", "defer", "import", "type",
            "pub", "module", "for", "in", "break", "continue", "true", "false", "and", "or", "not",
        ] {
            add_candidate(&mut candidates, name, 14, "", &prefix);
        }
        if let Some(index) = index {
            for name in index.locals.keys() {
                if name.starts_with(&prefix) {
                    candidates.insert(name.clone(), 6);
                }
            }
        }
    }
    completion_items(candidates, source_range(source, span))
}

/// Cap both item count and encoded text size, computing the shared UTF-16 range only once.
fn completion_items(candidates: BTreeMap<String, i64>, range: Json) -> Json {
    let mut items = Vec::new();
    let mut bytes = 0usize;
    for (name, kind) in candidates {
        bytes = bytes
            .saturating_add(name.len().saturating_mul(2))
            .saturating_add(256);
        if items.len() >= 256 || bytes > 1024 * 1024 {
            return completion_list(items, true);
        }
        items.push(object([
            ("label", string(&name)),
            ("kind", number(kind)),
            (
                "textEdit",
                object([("range", range.clone()), ("newText", string(name))]),
            ),
        ]));
    }
    completion_list(items, false)
}

/// Namespace members are selected from actual visible spellings; unrelated prefixes are excluded.
fn add_candidate(
    candidates: &mut BTreeMap<String, i64>,
    name: &str,
    kind: i64,
    receiver: &str,
    prefix: &str,
) {
    let member = if receiver.is_empty() {
        Some(name)
    } else {
        name.strip_prefix(receiver)
            .and_then(|s| s.strip_prefix('.'))
    };
    let Some(member) = member else {
        return;
    };
    let first = member.split('.').next().unwrap_or(member);
    if first.starts_with(prefix) && !first.is_empty() {
        candidates
            .entry(first.into())
            .or_insert(if member.contains('.') { 9 } else { kind });
    }
}
/// Report truncation explicitly so clients can request a narrower completion prefix.
fn completion_list(items: Vec<Json>, incomplete: bool) -> Json {
    object([
        ("isIncomplete", Json::Bool(incomplete)),
        ("items", Json::Array(items)),
    ])
}
