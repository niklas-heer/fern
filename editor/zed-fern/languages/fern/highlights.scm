; AUTO-GENERATED from scripts/editor/highlights.scm.in. Edit the authored template.
["fn" "type" "newtype" "pub" "import" "as" "let" "if" "else" "match" "return"] @keyword
["and" "or" "not"] @keyword.operator
["true" "false"] @constant.builtin
["+" "-" "*" "/" "%" "**" "==" "!=" "<" "<=" ">" ">=" "|>" "&&&" "|||" "^^^" "<<<" ">>>" "~~~" "=" "->" "?" "|"] @operator
["(" ")" "[" "]" "{" "}"] @punctuation.bracket
["," ":" "." ".."] @punctuation.delimiter
(integer_literal) @number
(float_literal) @number
(string_literal) @string
(triple_string) @string
(line_comment) @comment
(block_comment) @comment
(type_identifier) @type
(type_variable) @type
((type_identifier) @type.builtin (#match? @type.builtin "^(Int|Float|Bool|String|List|Map|Option|Result)$"))
(function_definition name: (identifier) @function)
(call_expression function: (expression (identifier) @function.call))
(parameter pattern: (pattern (identifier) @variable.parameter))
(member_access member: (identifier) @property)
(newtype_definition constructor: (type_identifier) @constructor)
(constructor_pattern (type_identifier) @constructor)
(attribute) @attribute
(module_path) @module
(identifier) @variable
