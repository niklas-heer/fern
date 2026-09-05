//! Completion-only recovery over one current token-level member hole, never cached type evidence.
use super::*;
impl Server {
    /// Recheck current overlays; a failed proof simply retains ordinary lexical fallback.
    pub(super) fn member_completion(&self, uri: &str, source: &str, cursor: usize) -> Option<Json> {
        let (program, site) = if let Some(path) = file_path(uri).ok()? {
            let loaded = modules::load_member_sources(&path, &self.overlays(), cursor).ok()?;
            (loaded.program, loaded.recovery?)
        } else {
            parse::recover_member(source, cursor).ok()?
        };
        let facts = check::recovery::analyze(&program, &site).ok()?;
        let candidates = facts
            .members
            .iter()
            .filter(|member| member.name.starts_with(site.prefix()))
            .map(|member| (member.name.clone(), 5))
            .collect();
        Some(navigation::completion_items(
            candidates,
            navigation::source_range(source, site.selector()),
            Some(&facts),
        ))
    }
}
