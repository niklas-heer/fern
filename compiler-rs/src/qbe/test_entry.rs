//! Test execution cannot count an explicit process exit as a returned test result.
use super::*;

impl Emitter<'_> {
    /// Redirect the audited runtime exit identity only in test mode, including lifted callables.
    pub(super) fn test_runtime_symbol<'s>(&self, symbol: &'s str) -> &'s str {
        if self.test_mode && symbol == "fern_exit" {
            "fern_rs_test_exit"
        } else {
            symbol
        }
    }

    /// Emit a diagnostic and failing process exit; unused application exit calls remain inert.
    /// The source status is intentionally ignored. Normal compilation emits no helper or message.
    pub(super) fn test_exit_helper(&mut self) {
        if !self.test_mode {
            return;
        }
        const MESSAGE: &str = "fern: System.exit cannot terminate a test";
        let bytes = MESSAGE.len() + 1;
        self.data.push_str(&format!(
            "data $fern_rs_test_exit_message = {{ b \"{MESSAGE}\", b 10 }}\n"
        ));
        self.output.push_str(&format!(
            "function $fern_rs_test_exit(l %status) {{\n@start\n    call $write(w 2, l $fern_rs_test_exit_message, l {bytes})\n    call $fern_exit(l 1)\n    ret\n}}\n"
        ));
    }
}
