//! Typed calls into audited native APIs, including explicit representation bridges.
use super::*;
use crate::runtime::{self, Operation, Signature, ValueAbi};

/// Unify a signature scheme with concrete arguments, without accepting untyped fallbacks.
fn bind_type(
    template: &Type,
    actual: &Type,
    bindings: &mut HashMap<String, Type>,
    span: Span,
    depth: usize,
) -> Lowering<()> {
    if depth > MAX_DEPTH {
        return Err(invalid(span, "runtime signature nesting limit exceeded"));
    }
    match (template, actual) {
        (Type::Generic(name), actual) => {
            if let Some(expected) = bindings.get(name) {
                expect_type(actual.clone(), expected.clone(), span)
            } else {
                bindings.insert(name.clone(), actual.clone());
                Ok(())
            }
        }
        (Type::List(a), Type::List(b)) | (Type::Option(a), Type::Option(b)) => {
            bind_type(a, b, bindings, span, depth + 1)
        }
        (Type::Result(a, e), Type::Result(b, f)) => {
            bind_type(a, b, bindings, span, depth + 1)?;
            bind_type(e, f, bindings, span, depth + 1)
        }
        _ => expect_type(actual.clone(), template.clone(), span),
    }
}

/// Resolve the return scheme solely from argument substitutions and checked types.
fn return_type(template: &Type, bindings: &HashMap<String, Type>, span: Span) -> Lowering<Type> {
    match template {
        Type::Generic(name) => bindings
            .get(name)
            .cloned()
            .ok_or_else(|| invalid(span, "unresolved runtime type parameter")),
        Type::List(item) => Ok(Type::List(Box::new(return_type(item, bindings, span)?))),
        Type::Option(item) => Ok(Type::Option(Box::new(return_type(item, bindings, span)?))),
        Type::Result(ok, err) => Ok(Type::Result(
            Box::new(return_type(ok, bindings, span)?),
            Box::new(return_type(err, bindings, span)?),
        )),
        _ => Ok(template.clone()),
    }
}

impl Emitter<'_> {
    /// Resolve an audited runtime ID and lower its concrete typed arguments exactly once.
    pub(super) fn runtime_call(
        &mut self,
        id: usize,
        args: &[Expr],
        span: Span,
        locals: &mut Locals,
        depth: usize,
    ) -> Lowering<(Type, String)> {
        let (signature, ty) = self.checked_runtime_signature(id, args, span)?;
        match signature.symbol {
            "fern_list_get" | "fern_list_head" => {
                return self.list_access(
                    args,
                    signature.symbol == "fern_list_head",
                    span,
                    locals,
                    depth,
                )
            }
            "fern_str_repeat" => return self.repeat_string(args, span, locals, depth),
            "fern_str_slice" => return self.slice_string(args, span, locals, depth),
            _ => {}
        }
        if signature.operation == Operation::ScalarContains {
            return self.scalar_contains(args, span, locals, depth);
        }
        let symbol = runtime_symbol(&signature, args, span)?;
        if signature.operation == Operation::JsonObject {
            let map = self.expr(&args[0], locals, depth)?;
            let value = self.assign(
                locals,
                ty.clone(),
                &format!("call $fern_rs_json_object(l {map})"),
            );
            return Ok((ty, value));
        }
        let mut values = Vec::new();
        for (arg, abi) in args.iter().zip(&signature.parameter_abi) {
            let value = self.expr(arg, locals, depth)?;
            values.push(self.runtime_argument(value, &arg.ty, *abi, span, locals)?);
        }
        self.runtime_operation(signature.operation, &mut values, span, locals)?;
        if symbol == "fern_str_split" {
            self.split_guard(&values, locals);
        }
        if signature.return_abi == ValueAbi::PackedOption {
            return Ok((ty, self.packed_option(symbol, &values, span, locals)?));
        }
        let instruction = format!("call ${symbol}({})", values.join(", "));
        let raw = match signature.return_abi {
            ValueAbi::Void => {
                self.output.push_str(&format!("    {instruction}\n"));
                "0".into()
            }
            ValueAbi::Word32 => self.assign(locals, Type::Bool, &instruction),
            _ => self.assign(locals, Type::Int, &instruction),
        };
        let mut value = self.runtime_result(raw, &ty, signature.return_abi, locals)?;
        if signature.operation == Operation::InvertBool {
            value = self.assign(locals, Type::Bool, &format!("ceqw {value}, 0"));
        }
        Ok((ty, value))
    }

    /// Validate the complete public IR signature before evaluating argument effects.
    fn checked_runtime_signature(
        &self,
        id: usize,
        args: &[Expr],
        span: Span,
    ) -> Lowering<(Signature, Type)> {
        let signature =
            runtime::signature(id).ok_or_else(|| invalid(span, "unknown runtime identity"))?;
        if signature.parameters.len() != args.len() {
            return Err(invalid(
                span,
                "runtime argument count differs from signature",
            ));
        }
        if signature.return_abi == ValueAbi::NullableStringList {
            return Err(invalid(span, "nullable directory-list result needs an explicit error contract before native lowering"));
        }
        let mut bindings = HashMap::new();
        for (template, arg) in signature.parameters.iter().zip(args) {
            nominal::resolved(&arg.ty, &self.layouts, arg.span, 0)?;
            bind_type(template, &arg.ty, &mut bindings, arg.span, 0)?;
        }
        let ty = return_type(&signature.return_type, &bindings, span)?;
        Ok((signature, ty))
    }

    /// Apply source/native arity and enum differences after evaluating arguments once.
    fn runtime_operation(
        &mut self,
        operation: Operation,
        values: &mut Vec<String>,
        span: Span,
        locals: &mut Locals,
    ) -> Lowering<()> {
        match operation {
            Operation::UniformPadding => values.push(values[1].clone()),
            Operation::TableBorder => {
                let name = values[1]
                    .strip_prefix("l ")
                    .ok_or_else(|| invalid(span, "border name requires String ABI"))?;
                values[1] = format!("l {}", self.border_style(name, span, locals)?);
            }
            _ => {}
        }
        Ok(())
    }

    /// Map known style names to C enum values; -1 preserves the object's existing style.
    fn border_style(&mut self, name: &str, span: Span, locals: &mut Locals) -> Lowering<String> {
        let merge = locals.label();
        let mut incoming = Vec::new();
        for (index, style) in ["rounded", "square", "double", "heavy", "ascii", "none"]
            .iter()
            .enumerate()
        {
            let literal = self.string(style, span)?;
            let equal = self.assign(
                locals,
                Type::Int,
                &format!("call $fern_str_eq(l {name}, l {literal})"),
            );
            let found = locals.label();
            let next = locals.label();
            self.output
                .push_str(&format!("    jnz {equal}, {found}, {next}\n"));
            self.start_block(locals, &found);
            incoming.push(format!("{found} {index}"));
            self.output.push_str(&format!("    jmp {merge}\n"));
            self.start_block(locals, &next);
        }
        incoming.push(format!("{} -1", locals.current));
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &merge);
        Ok(self.assign(locals, Type::Int, &format!("phi {}", incoming.join(", "))))
    }

    /// Convert a checked Fern argument to its documented native representation.
    fn runtime_argument(
        &mut self,
        value: String,
        ty: &Type,
        abi: ValueAbi,
        span: Span,
        locals: &mut Locals,
    ) -> Lowering<String> {
        match abi {
            ValueAbi::Double64 => {
                expect_type(ty.clone(), Type::Float, span)?;
                Ok(format!("d {value}"))
            }
            ValueAbi::StringList => Ok(format!(
                "l {}",
                self.emit_string_list_argument(&value, locals)
            )),
            ValueAbi::Word64 | ValueAbi::HeapOption | ValueAbi::HeapResult => {
                Ok(format!("l {}", self.payload(locals, ty, value)))
            }
            ValueAbi::Word32 => Ok(format!("w {value}")),
            _ => Err(invalid(span, "unsupported native argument representation")),
        }
    }

    /// Convert native results, retaining signed C int returns and semantic payload widths.
    fn runtime_result(
        &mut self,
        raw: String,
        ty: &Type,
        abi: ValueAbi,
        locals: &mut Locals,
    ) -> Lowering<String> {
        Ok(match abi {
            ValueAbi::Void => "0".into(),
            ValueAbi::Word32 if *ty == Type::Int => {
                self.assign(locals, Type::Int, &format!("extsw {raw}"))
            }
            ValueAbi::Word32 => raw,
            ValueAbi::StringList => self.emit_string_list_result(&raw, locals),
            ValueAbi::HeapJsonMembers => self.assign(
                locals,
                ty.clone(),
                &format!("call $fern_rs_json_members(l {raw})"),
            ),
            ValueAbi::HeapStringListResult => self.native_list_result(&raw, locals),
            ValueAbi::ExecResult => self.native_tuple(&raw, 3, &[1, 2], locals),
            ValueAbi::TermSize => self.native_tuple(&raw, 2, &[], locals),
            ValueAbi::RegexMatch => self.native_match(&raw, locals),
            ValueAbi::RegexCaptures => self.native_captures(&raw, locals),
            ValueAbi::Word64 | ValueAbi::HeapOption | ValueAbi::HeapResult => {
                self.unpack(locals, ty, raw)
            }
            _ => {
                return Err(invalid(
                    Span::default(),
                    "unsupported native return representation",
                ))
            }
        })
    }
}

/// Select equality specialization only after the generic element type is known.
fn runtime_symbol(signature: &Signature, args: &[Expr], span: Span) -> Lowering<&'static str> {
    if signature.operation == Operation::ScalarContains {
        match args.get(1).map(|arg| &arg.ty) {
            Some(Type::String) => Ok("fern_list_contains_str"),
            Some(Type::Int | Type::Bool) => Ok("fern_list_contains"),
            _ => Err(invalid(
                span,
                "List.contains requires Int, Bool, or String elements",
            )),
        }
    } else {
        Ok(signature.symbol)
    }
}

impl Emitter<'_> {
    /// Bridge only audited packed producers; substring indexes bypass truncating C packing.
    fn packed_option(
        &mut self,
        symbol: &str,
        arguments: &[String],
        span: Span,
        locals: &mut Locals,
    ) -> Lowering<String> {
        let joined = arguments.join(", ");
        match symbol {
            "fern_str_index_of" => {
                let base = arguments
                    .first()
                    .and_then(|arg| arg.strip_prefix("l "))
                    .ok_or_else(|| invalid(span, "index_of requires pointer arguments"))?;
                let found = self.assign(locals, Type::Int, &format!("call $strstr({joined})"));
                let present = self.assign(locals, Type::Bool, &format!("cnel {found}, 0"));
                Ok(self.heap_option(&present, &format!("sub {found}, {base}"), locals))
            }
            "fern_str_char_at" => {
                // This C helper only packs bytes 0..255; all payload bits survive.
                let packed = self.assign(
                    locals,
                    Type::Int,
                    &format!("call $fern_str_char_at({joined})"),
                );
                let tag = self.assign(locals, Type::Bool, &format!("copy {packed}"));
                let present = self.assign(locals, Type::Bool, &format!("ceqw {tag}, 1"));
                Ok(self.heap_option(&present, &format!("sar {packed}, 32"), locals))
            }
            _ => Err(invalid(
                span,
                "packed Option producer has no proven lossless adapter",
            )),
        }
    }

    /// Create a heap Option with payload computation restricted to the Some branch.
    fn heap_option(
        &mut self,
        present: &str,
        payload_instruction: &str,
        locals: &mut Locals,
    ) -> String {
        let some = locals.label();
        let none = locals.label();
        let merge = locals.label();
        self.output
            .push_str(&format!("    jnz {present}, {some}, {none}\n"));
        self.start_block(locals, &some);
        let payload = self.assign(locals, Type::Int, payload_instruction);
        let value = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_result_ok(l {payload})"),
        );
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &none);
        let empty = self.assign(locals, Type::Int, "call $fern_result_err(l 0)");
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &merge);
        self.assign(
            locals,
            Type::Int,
            &format!("phi {some} {value}, {none} {empty}"),
        )
    }
}

/// Only these two audited list representations participate in runtime copies.
#[derive(Clone, Copy)]
enum CopyEndpoint<'a> {
    FernList(&'a str),
    StringData(&'a str),
}

impl Emitter<'_> {
    /// Build an independent FernStringList header/data pair from a checked List(String).
    fn emit_string_list_argument(&mut self, value: &str, locals: &mut Locals) -> String {
        let length = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_list_len(l {value})"),
        );
        let capacity = self.assign(locals, Type::Int, &format!("add {length}, 1"));
        let bytes = self.assign(locals, Type::Int, &format!("mul {capacity}, 8"));
        let header = self.assign(locals, Type::Int, "call $fern_alloc(l 24)");
        let data = self.assign(locals, Type::Int, &format!("call $fern_alloc(l {bytes})"));
        // runtime/fern_runtime.h: FernStringList {char** data; i64 len; i64 cap}.
        self.output
            .push_str(&format!("    storel {data}, {header}\n"));
        self.store_header(&header, 8, &length, locals);
        self.store_header(&header, 16, &capacity, locals);
        self.copy_strings(
            CopyEndpoint::FernList(value),
            CopyEndpoint::StringData(&data),
            &length,
            locals,
        );
        header
    }

    /// Copy a non-null native FernStringList into an ordinary immutable FernList.
    fn emit_string_list_result(&mut self, value: &str, locals: &mut Locals) -> String {
        let data = self.assign(locals, Type::Int, &format!("loadl {value}"));
        let length_pointer = self.assign(locals, Type::Int, &format!("add {value}, 8"));
        let length = self.assign(locals, Type::Int, &format!("loadl {length_pointer}"));
        let capacity = self.assign(locals, Type::Int, &format!("add {length}, 1"));
        let list = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_list_with_capacity(l {capacity})"),
        );
        self.copy_strings(
            CopyEndpoint::StringData(&data),
            CopyEndpoint::FernList(&list),
            &length,
            locals,
        );
        list
    }

    /// Store a full-width runtime header field at its audited byte offset.
    fn store_header(&mut self, header: &str, offset: usize, value: &str, locals: &mut Locals) {
        let address = self.assign(locals, Type::Int, &format!("add {header}, {offset}"));
        self.output
            .push_str(&format!("    storel {value}, {address}\n"));
    }

    /// Copy exactly the source length, preserving 64-bit String pointers and fresh ownership.
    fn copy_strings(
        &mut self,
        source: CopyEndpoint<'_>,
        destination: CopyEndpoint<'_>,
        length: &str,
        locals: &mut Locals,
    ) {
        let before = locals.current.clone();
        let test = locals.label();
        let body = locals.label();
        let done = locals.label();
        let index = locals.temporary();
        let next = locals.temporary();
        self.output.push_str(&format!("    jmp {test}\n"));
        self.start_block(locals, &test);
        self.output
            .push_str(&format!("    {index} =l phi {before} 0, {body} {next}\n"));
        let more = self.assign(locals, Type::Bool, &format!("csltl {index}, {length}"));
        self.output
            .push_str(&format!("    jnz {more}, {body}, {done}\n"));
        self.start_block(locals, &body);
        let value = match source {
            CopyEndpoint::FernList(list) => self.assign(
                locals,
                Type::Int,
                &format!("call $fern_list_get(l {list}, l {index})"),
            ),
            CopyEndpoint::StringData(data) => {
                let address = self.string_slot(data, &index, locals);
                self.assign(locals, Type::Int, &format!("loadl {address}"))
            }
        };
        match destination {
            CopyEndpoint::FernList(list) => self.output.push_str(&format!(
                "    call $fern_list_push_mut(l {list}, l {value})\n"
            )),
            CopyEndpoint::StringData(data) => {
                let address = self.string_slot(data, &index, locals);
                self.output
                    .push_str(&format!("    storel {value}, {address}\n"));
            }
        }
        self.output
            .push_str(&format!("    {next} =l add {index}, 1\n    jmp {test}\n"));
        self.start_block(locals, &done);
    }

    /// Address one element in a native array of 64-bit String pointers.
    fn string_slot(&mut self, data: &str, index: &str, locals: &mut Locals) -> String {
        let offset = self.assign(locals, Type::Int, &format!("mul {index}, 8"));
        self.assign(locals, Type::Int, &format!("add {data}, {offset}"))
    }
}

impl Emitter<'_> {
    /// Reject unexpected allocation failures before accessing a native result pointer.
    fn require_native_pointer(&mut self, pointer: &str, locals: &mut Locals) {
        let valid = locals.label();
        let failure = locals.label();
        let exists = self.assign(locals, Type::Bool, &format!("cnel {pointer}, 0"));
        self.output
            .push_str(&format!("    jnz {exists}, {valid}, {failure}\n"));
        self.start_block(locals, &failure);
        self.output.push_str("    hlt\n");
        self.start_block(locals, &valid);
    }

    /// Read one audited 64-bit C field; callers must first establish a valid object.
    fn native_field(&mut self, object: &str, offset: usize, locals: &mut Locals) -> String {
        let address = self.assign(locals, Type::Int, &format!("add {object}, {offset}"));
        self.assign(locals, Type::Int, &format!("loadl {address}"))
    }

    /// Copy a C record with contiguous 64-bit fields into a separate tagged Rust tuple.
    fn native_tuple(
        &mut self,
        object: &str,
        fields: usize,
        strings: &[usize],
        locals: &mut Locals,
    ) -> String {
        self.require_native_pointer(object, locals);
        let result = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_alloc(l {})", (fields + 1) * 8),
        );
        self.output.push_str(&format!("    storel 0, {result}\n"));
        for index in 0..fields {
            let value = self.native_field(object, index * 8, locals);
            if strings.contains(&index) {
                self.require_native_pointer(&value, locals);
            }
            self.store_header(&result, (index + 1) * 8, &value, locals);
        }
        result
    }

    /// Native Regex.find uses start=-1 for invalid patterns and absence; no text exists then.
    fn native_match(&mut self, object: &str, locals: &mut Locals) -> String {
        self.require_native_pointer(object, locals);
        let start = self.native_field(object, 0, locals);
        let present = self.assign(locals, Type::Bool, &format!("csgel {start}, 0"));
        let some = locals.label();
        let none = locals.label();
        let merge = locals.label();
        self.output
            .push_str(&format!("    jnz {present}, {some}, {none}\n"));
        self.start_block(locals, &some);
        let tuple = self.native_tuple(object, 3, &[2], locals);
        let value = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_result_ok(l {tuple})"),
        );
        let some_end = locals.current.clone();
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &none);
        let absent = self.assign(locals, Type::Int, "call $fern_result_err(l 0)");
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &merge);
        self.assign(
            locals,
            Type::Int,
            &format!("phi {some_end} {value}, {none} {absent}"),
        )
    }

    /// Copy the native capture array without loading its nullable data pointer on empty results.
    fn native_captures(&mut self, object: &str, locals: &mut Locals) -> String {
        self.require_native_pointer(object, locals);
        let count = self.native_field(object, 0, locals);
        let list = self.assign(locals, Type::Int, "call $fern_list_with_capacity(l 10)");
        let before = locals.current.clone();
        let test = locals.label();
        let body = locals.label();
        let latch = locals.label();
        let done = locals.label();
        let index = locals.temporary();
        let next = locals.temporary();
        self.output.push_str(&format!("    jmp {test}\n"));
        self.start_block(locals, &test);
        self.output
            .push_str(&format!("    {index} =l phi {before} 0, {latch} {next}\n"));
        let more = self.assign(locals, Type::Bool, &format!("csltl {index}, {count}"));
        self.output
            .push_str(&format!("    jnz {more}, {body}, {done}\n"));
        self.start_block(locals, &body);
        let data = self.native_field(object, 8, locals);
        self.require_native_pointer(&data, locals);
        let offset = self.assign(locals, Type::Int, &format!("mul {index}, 24"));
        let record = self.assign(locals, Type::Int, &format!("add {data}, {offset}"));
        let tuple = self.native_tuple(&record, 3, &[2], locals);
        self.output.push_str(&format!(
            "    call $fern_list_push_mut(l {list}, l {tuple})\n    jmp {latch}\n"
        ));
        self.start_block(locals, &latch);
        self.output
            .push_str(&format!("    {next} =l add {index}, 1\n    jmp {test}\n"));
        self.start_block(locals, &done);
        list
    }
}

impl Emitter<'_> {
    /// Preserve directory errors and translate only successful native StringList payloads.
    fn native_list_result(&mut self, result: &str, locals: &mut Locals) -> String {
        self.require_native_pointer(result, locals);
        let success = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_result_is_ok(l {result})"),
        );
        let ok = locals.label();
        let err = locals.label();
        let merge = locals.label();
        self.output
            .push_str(&format!("    jnz {success}, {ok}, {err}\n"));
        self.start_block(locals, &ok);
        let native = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_result_unwrap(l {result})"),
        );
        self.require_native_pointer(&native, locals);
        let list = self.emit_string_list_result(&native, locals);
        let wrapped = self.assign(
            locals,
            Type::Int,
            &format!("call $fern_result_ok(l {list})"),
        );
        let ok_end = locals.current.clone();
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &err);
        self.output.push_str(&format!("    jmp {merge}\n"));
        self.start_block(locals, &merge);
        self.assign(
            locals,
            Type::Int,
            &format!("phi {ok_end} {wrapped}, {err} {result}"),
        )
    }
}
