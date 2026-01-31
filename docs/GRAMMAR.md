# Manifast Grammar (EBNF)

```ebnf
program      = { statement } ;
statement    = var_decl | block | if_stmt | while_stmt | for_stmt | try_stmt | func_decl | return_stmt | expr_stmt ;

var_decl     = "lokal" IDENTIFIER [ "=" expression ] [ ";" ] ;
func_decl    = "fungsi" IDENTIFIER "(" [ parameters ] ")" block "tutup" ;
parameters   = IDENTIFIER { "," IDENTIFIER } ;

block        = { statement } ;

if_stmt      = "jika" expression "maka" block [ "kecuali" block ] "tutup" ;
while_stmt   = "sementara" expression "maka" block "tutup" ;
for_stmt     = "untuk" IDENTIFIER "=" expression "ke" expression [ "langkah" expression ] "lakukan" block "tutup" ;
try_stmt     = "coba" block [ "tangkap" [ IDENTIFIER ] [ "maka" ] block ] "tutup" ;

return_stmt  = "kembali" [ expression ] [ ";" ] ;
expr_stmt    = expression [ ";" ] ;

expression   = assignment ;
assignment   = bitwise_or { ( "=" | "+=" | "-=" | "*=" | "/=" | "%=" ) assignment } ;
bitwise_or   = bitwise_xor { "|" bitwise_xor } ;
bitwise_xor  = bitwise_and { "^" bitwise_and } ;
bitwise_and  = equality { "&" equality } ;
equality     = comparison { ( "==" | "!=" ) comparison } ;
comparison   = shift { ( ">" | ">=" | "<" | "<=" ) shift } ;
shift        = term { ( "<<" | ">>" ) term } ;
term         = factor { ( "+" | "-" ) factor } ;
factor       = unary { ( "*" | "/" | "%" ) unary } ;
unary        = [ "!" | "-" | "~" ] call ;
call         = primary { "(" [ arguments ] ")" | "." IDENTIFIER | "[" expression "]" } ;
primary      = NUMBER | STRING | IDENTIFIER | "benar" | "salah" | "null" | group | array | object ;

group        = "(" expression ")" ;
array        = "[" [ arguments ] "]" ;
object       = "{" [ entries ] "}" ;
entries      = entry { "," entry } ;
entry        = IDENTIFIER ":" expression ;
arguments    = expression { "," expression } ;
```
