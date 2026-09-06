//! Immutable descriptor tables bind runtime callbacks to exact semantic layouts and closure identities.
use super::*;

impl Emitter<'_> {
    /// Describe every known closure identity before runtime registration validates capture layouts.
    pub(in crate::qbe) fn actor_descriptors(&mut self) -> Lowering<()> {
        if !self.actors.active {
            return Ok(());
        }
        let functions: Vec<_> = self.functions.values().copied().collect();
        let mut table = Vec::new();
        for function in functions {
            let id = function.id.0;
            let mut captures = Vec::new();
            for capture in &function.captures {
                captures.push(self.actor_type(&capture.ty)?);
            }
            let captures = self.actor_array(&format!("actor_captures{id}"), &captures);
            let mailbox = function
                .mailbox
                .as_ref()
                .or_else(|| self.actors.steps.get(&id))
                .or_else(|| self.actors.selectors.get(&id))
                .cloned();
            let mailbox = mailbox
                .as_ref()
                .map(|ty| self.actor_type(ty))
                .transpose()?
                .unwrap_or_else(|| "0".into());
            let identity = if self.actors.entries.contains_key(&id) {
                self.data
                    .push_str(&format!("data $actor_identity{id} = {{ l {id} }}\n"));
                format!("$actor_identity{id}")
            } else {
                format!("$f{id}")
            };
            let selector = self.actors.selectors.contains_key(&id);
            let (step, select) = if selector {
                ("0".into(), format!("$actor_callback{id}"))
            } else {
                (format!("$actor_callback{id}"), "0".into())
            };
            self.data.push_str(&format!("data $actor_descriptor{id} = {{ l {identity}, l {step}, l {select}, l {}, l {captures}, l {mailbox} }}\n", function.captures.len()));
            table.push(format!("$actor_descriptor{id}"));
            self.actor_callback(function, selector);
        }
        self.actor_array("actor_functions", &table);
        Ok(())
    }

    /// Bridge only validated step/selector signatures; unrelated callable entries cannot be spawned.
    fn actor_callback(&mut self, function: &Function, selector: bool) {
        let id = function.id.0;
        let target = self.actors.entries.get(&id).copied().unwrap_or(id);
        let generated = self.actors.steps.contains_key(&target);
        self.output.push_str(&format!(
            "function l $actor_callback{id}(l %exec, l %env{}) {{\n@start\n",
            if selector { ", l %payload" } else { "" }
        ));
        if !selector
            && (!function.params.is_empty() || (!generated && function.return_type != Type::Unit))
        {
            self.output.push_str("    ret 3\n}\n");
            return;
        }
        self.output
            .push_str("    %fault =l call $fern_managed_fault(l %exec)\n");
        let mut args = vec!["l %env".to_owned(), "l %fault".to_owned()];
        if self.actors.managed.contains(&target) {
            args.push("l %exec".into());
        }
        if selector {
            let ty = &function.params[0].ty;
            let width = self.width(ty.clone());
            let value = if width == 'l' {
                "%payload"
            } else {
                self.output.push_str(&format!(
                    "    %decoded ={width} {} %payload\n",
                    if width == 'd' { "cast" } else { "copy" }
                ));
                "%decoded"
            };
            args.push(format!("{width} {value}"));
        }
        if generated || selector {
            self.output.push_str(&format!(
                "    %status =l call $f{target}({})\n    ret %status\n}}\n",
                args.join(", ")
            ));
        } else {
            self.output.push_str(&format!(
                "    call $f{target}({})\n    ret 2\n}}\n",
                args.join(", ")
            ));
        }
    }

    /// Deduplicate by semantic type, reserving identity before descending recursive nominal fields.
    pub(super) fn actor_type(&mut self, ty: &Type) -> Lowering<String> {
        if let Some(id) = self.actors.types.get(ty) {
            return Ok(format!("$actor_type{id}"));
        }
        if self.actors.types.len() >= 4096 {
            return Err(invalid(
                Span::default(),
                "actor type descriptor limit exceeded",
            ));
        }
        let id = self.actors.types.len();
        self.actors.types.insert(ty.clone(), id);
        let (kind, groups) = self.actor_shape(ty)?;
        let mut children = Vec::new();
        for group in &groups {
            for child in group {
                children.push(self.actor_type(child)?);
            }
        }
        let child_array = self.actor_array(&format!("actor_children{id}"), &children);
        let count = if kind == 4 {
            groups.len()
        } else {
            children.len()
        };
        let arities = if kind == 4 {
            let lengths: Vec<_> = groups.iter().map(|group| group.len().to_string()).collect();
            self.actor_array(&format!("actor_arities{id}"), &lengths)
        } else {
            "0".into()
        };
        self.data.push_str(&format!(
            "data $actor_type{id} = {{ l {kind}, l {count}, l {child_array}, l {arities} }}\n"
        ));
        Ok(format!("$actor_type{id}"))
    }

    /// Translate approved immutable language layouts, retaining tags and full-width payload identity.
    fn actor_shape(&self, ty: &Type) -> Lowering<(usize, Vec<Vec<Type>>)> {
        Ok(match ty {
            Type::Int | Type::Bool | Type::Unit | Type::Float => (0, vec![]),
            Type::String => (1, vec![]),
            Type::List(item) => (2, vec![vec![*item.clone()]]),
            Type::Tuple(fields) => (3, vec![fields.clone()]),
            Type::Option(item) => (4, vec![vec![*item.clone()], vec![]]),
            Type::Result(ok, err) => (4, vec![vec![*ok.clone()], vec![*err.clone()]]),
            Type::Union(members) => (4, members.iter().map(|ty| vec![ty.clone()]).collect()),
            Type::Named(_, _) => {
                let layout = self
                    .layouts
                    .get(ty)
                    .ok_or_else(|| invalid(Span::default(), "unknown actor nominal layout"))?;
                (
                    if layout.storage == ir::LayoutStorage::Unboxed {
                        5
                    } else {
                        4
                    },
                    layout.variants.clone(),
                )
            }
            Type::Pid(mailbox) => (6, vec![vec![*mailbox.clone()]]),
            Type::Function(_, _) | Type::ActorFunction(_, _) => (7, vec![]),
            Type::Map(key, value) => (9, vec![vec![*key.clone(), *value.clone()]]),
            Type::Range => (10, vec![]),
            Type::Native(_) => (11, vec![]),
            _ => {
                return Err(invalid(
                    Span::default(),
                    "native handle is not supported in actor frame accounting",
                ))
            }
        })
    }

    /// Empty descriptor vectors use null; every nonempty vector has an immutable fixed-length object.
    fn actor_array(&mut self, name: &str, values: &[String]) -> String {
        if values.is_empty() {
            return "0".into();
        }
        let entries = values
            .iter()
            .map(|value| format!("l {value}"))
            .collect::<Vec<_>>()
            .join(", ");
        self.data
            .push_str(&format!("data ${name} = {{ {entries} }}\n"));
        format!("${name}")
    }
}
