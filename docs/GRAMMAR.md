# Manifast Grammar (EBNF)

```ebnf
program      = { statement } ;
statement    = var_decl | type_decl | block | if_stmt | while_stmt | for_stmt
             | try_stmt | func_decl | class_decl | return_stmt | expr_stmt ;

(* Variable Declaration *)
var_decl     = ( "lokal" | "tetap" ) IDENTIFIER [ ":" type ] [ "=" expression ] [ ";" ] ;

(* Type Declaration *)
type_decl    = "tipe" IDENTIFIER "=" type ;

(* Function Declaration *)
func_decl    = "fungsi" IDENTIFIER "(" [ parameters ] ")" [ ":" type ] block "tutup" ;
parameters   = param { "," param } ;
param        = IDENTIFIER [ ":" type ] ;

(* Class Declaration *)
class_decl   = "kelas" IDENTIFIER "maka" { func_decl } "tutup" ;

(* Block *)
block        = { statement } ;

(* Control Flow *)
if_stmt      = "jika" expression "maka" block
               { "kalau" expression "maka" block }
               [ ( "kecuali" | "sebaliknya" ) block ] "tutup" ;
while_stmt   = "selama" expression "maka" block "tutup" ;
for_stmt     = "untuk" IDENTIFIER "=" expression "ke" expression
               [ "langkah" expression ] "lakukan" block "tutup" ;
try_stmt     = "coba" block [ "tangkap" [ IDENTIFIER ] [ "maka" ] block ] "tutup" ;

(* Return *)
return_stmt  = "kembali" [ expression ] [ ";" ] ;
expr_stmt    = expression [ ";" ] ;

(* Types *)
type         = primitive_type | array_type | pointer_type | func_type
             | struct_type | IDENTIFIER ;
primitive_type = "i8" | "i16" | "i32" | "i64" | "u8" | "u16" | "u32" | "u64"
               | "f32" | "f64" | "angka" | "char" | "string" | "boolean" ;
array_type   = type "[]" ;
pointer_type = "*" type ;
func_type    = "fungsi" "(" [ type { "," type } ] ")" ":" type ;
struct_type  = "{" field { "," field } "}" ;
field        = IDENTIFIER ":" type ;

(* Expressions *)
expression   = assignment ;
assignment   = bitwise_or { ( "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "<<=" | ">>=" | "&=" | "^=" | "|=" ) assignment } ;
bitwise_or   = bitwise_xor { "|" bitwise_xor } ;
bitwise_xor  = bitwise_and { "^" bitwise_and } ;
bitwise_and  = equality { "&" equality } ;
equality     = comparison { ( "==" | "!=" ) comparison } ;
comparison   = shift { ( ">" | ">=" | "<" | "<=" ) shift } ;
shift        = term { ( "<<" | ">>" ) term } ;
term         = factor { ( "+" | "-" ) factor } ;
factor       = unary { ( "*" | "/" | "%" ) unary } ;
unary        = [ "!" | "-" | "~" | "bukan" ] call ;
call         = primary { "(" [ arguments ] ")" | "." IDENTIFIER | "[" expression "]" } ;
primary      = NUMBER | STRING | IDENTIFIER | "benar" | "salah" | "nil"
             | group | array | object | "fungsi" "(" [ parameters ] ")" block "tutup" ;

group        = "(" expression ")" ;
array        = "[" [ arguments ] "]" ;
object       = "{" [ entries ] "}" ;
entries      = entry { "," entry } ;
entry        = IDENTIFIER ":" expression ;
arguments    = expression { "," expression } ;
```

## Built-in Type Aliases
| Alias    | Resolves to |
|----------|-------------|
| `angka`  | `f64`       |

## Keywords Reference
| Keyword       | Meaning          |
|---------------|------------------|
| `lokal`       | Variable (let)   |
| `tetap`       | Constant         |
| `fungsi`      | Function         |
| `kembali`     | Return           |
| `jika`        | If               |
| `maka`        | Then             |
| `kalau`       | Else If          |
| `kecuali`     | Else             |
| `sebaliknya`  | Else             |
| `tutup`       | End              |
| `selama`      | While            |
| `untuk`       | For              |
| `ke`          | To               |
| `langkah`     | Step             |
| `lakukan`     | Do               |
| `coba`        | Try              |
| `tangkap`     | Catch            |
| `kelas`       | Class            |
| `tipe`        | Type alias       |
| `benar`       | True             |
| `salah`       | False            |
| `nil`         | Null             |
| `dan`         | And              |
| `atau`        | Or               |
| `bukan`       | Not              |
| `self`        | Self             |
| `impor`       | Import module    |
