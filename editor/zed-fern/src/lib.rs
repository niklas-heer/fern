use zed_extension_api as zed;

struct FernExtension;

/// Choose a literal configured command or the Rust frontend in the worktree PATH.
fn select_command(
    settings: Option<zed::settings::CommandSettings>,
    mut which: impl FnMut(&str) -> Option<String>,
) -> zed::Result<zed::Command> {
    let (path, arguments, environment) = match settings {
        Some(settings) => (settings.path, settings.arguments, settings.env),
        None => (None, None, None),
    };
    let command = match path {
        Some(path) if path.is_empty() => return Err("Fern binary.path must not be empty".into()),
        Some(path) => path,
        None => which("fern-rs").ok_or_else(|| {
            "fern-rs was not found in the worktree PATH. Build compiler-rs with cargo build \
             --release, then set lsp.fern-lsp.binary.path to the resulting executable."
                .to_string()
        })?,
    };
    let mut env: Vec<_> = environment.unwrap_or_default().into_iter().collect();
    env.sort();
    Ok(zed::Command {
        command,
        args: arguments.unwrap_or_else(|| vec!["lsp".into()]),
        env,
    })
}

impl zed::Extension for FernExtension {
    fn new() -> Self {
        Self
    }

    fn language_server_command(
        &mut self,
        language_server_id: &zed::LanguageServerId,
        worktree: &zed::Worktree,
    ) -> zed::Result<zed::Command> {
        if language_server_id.as_ref() != "fern-lsp" {
            return Err("Unknown Fern language server".into());
        }
        let settings = zed::settings::LspSettings::for_worktree("fern-lsp", worktree)?;
        select_command(settings.binary, |name| worktree.which(name))
    }
}

zed::register_extension!(FernExtension);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn explicit_literal_path_wins_without_searching() {
        let settings = zed::settings::CommandSettings {
            path: Some("/tmp/Fern tool/fern-rs".into()),
            arguments: None,
            env: None,
        };
        let command = select_command(Some(settings), |_| panic!("unexpected discovery")).unwrap();
        assert_eq!(command.command, "/tmp/Fern tool/fern-rs");
        assert_eq!(command.args, ["lsp"]);
    }

    #[test]
    fn discovery_uses_only_rust_frontend() {
        let command = select_command(None, |name| {
            assert_eq!(name, "fern-rs");
            Some("/tools/fern-rs".into())
        })
        .unwrap();
        assert_eq!(command.command, "/tools/fern-rs");
        assert_eq!(command.args, ["lsp"]);
    }

    #[test]
    fn missing_binary_is_actionable_without_downloads() {
        let error = select_command(None, |_| None).err().unwrap();
        assert!(error.contains("fern-rs"));
        assert!(error.contains("binary.path"));
    }

    #[test]
    fn configured_arguments_and_environment_are_literal() {
        let settings = zed::settings::CommandSettings {
            path: Some("/tools/wrapper".into()),
            arguments: Some(vec!["lsp".into(), "$(literal)".into()]),
            env: Some([("FERN_TEST".into(), "two words".into())].into()),
        };
        let command = select_command(Some(settings), |_| None).unwrap();
        assert_eq!(command.args, ["lsp", "$(literal)"]);
        assert_eq!(command.env, [("FERN_TEST".into(), "two words".into())]);
    }

    #[test]
    fn empty_override_is_an_error_instead_of_fallback() {
        let settings = zed::settings::CommandSettings {
            path: Some(String::new()),
            arguments: None,
            env: None,
        };
        assert!(select_command(Some(settings), |_| Some("fallback".into())).is_err());
    }
}
