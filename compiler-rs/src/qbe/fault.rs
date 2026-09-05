//! An explicit invocation context carries numeric faults across typed call boundaries.
use super::*;

impl Emitter<'_> {
    /// A failed call never exposes its neutral ABI payload to the source program.
    pub(super) fn guard_fault(&mut self, locals: &mut Locals) {
        let code = self.assign(locals, Type::Int, "loadl %fault");
        let failed = self.assign(locals, Type::Bool, &format!("cnel {code}, 0"));
        let unwind = locals.label();
        let resume = locals.label();
        self.output
            .push_str(&format!("    jnz {failed}, {unwind}, {resume}\n"));
        self.start_block(locals, &unwind);
        self.output
            .push_str("    storel 0, %return_slot\n    jmp @return\n");
        self.start_block(locals, &resume);
    }
}
