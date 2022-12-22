## Monadic Functions

A monadic function `f` operates on the single argument to its right `B`: `fB`.

### Monadic Scalar Functions

A monadic scalar function is defined for a scalar value according to the table below. If the argument `B` is an array, the result is an array of the same shape with `f` applied to it element wise.

| Function | Name | Definition |
| --- | --- | --- |
| `+B` | Plus | `0+B` |
| `-B` | Negative | `0-B` |
| `×B` | Signum | `¯1, 0, 1` |
| `÷B` | Reciprocal | `1÷B` |
| `⌈B` | Ceiling | `smallest integer ≥ B` |
| `⌊B` | Floor | `largest integer ≤ B` |
| `*B` | Exponential | `Exp(B)` |
| `⍟B` | Natural log | `Ln(B)` |
| `\|B` | Magnitude | `Abs(B)` |
| `!B` | Factorial | `1×2×...×B` if `B` is integer |
| `!B` | Gamma | `Gamma(B + 1)` if `B` is not integer |
| `?B` | Roll | Random choice from `⍳B` |
| `○B` | Pi times | `π × B` |
| `~B` | Not | `~0 -> 1`; `~1 -> 0` |


## Dyadic Functions

A dyadic function `f` operates on two arguments, one on its left, `A`, and one on its right, `B`: `AfB`. Since evaluation proceeds from right to left, `B` is evaluated before `A`.

### Dyadic Scalar Functions

A dyadic scalar function is defined for scalar values according to the table below. If both operands are arrays they need to be of the same shape. The result is an array of the same shape where each element is the result of the function operated on the corresponding elements of `A` and `B`.

If one of the operands is a scalar and the other is an array, the result is an array of the same shape where each element is the result of the function operated between the scalar and the corresponding element in the source array.

| Function | Name | Definition |
| --- | --- | --- |
| `A+B` | Plus | `A+B` |
| `A-B` | Negative | `A-B` |
| `A×B` | Times | `A×B` |
| `A÷B` | Divide | `A÷B` |
| `A⌈B` | Maximum | `Max(A,B)` |
| `A⌊B` | Minimum | `Min(A,B)` |
| `A*B` | Power | `Pow(A,B)` |
| `A⍟B` | Logarithm | `Log(B)` in base A |
| `A\|B` | Residue | `Mod(A,B)` |
| `A!B` | Binomial | See table below |
| `A?B` | Deal | See table below |
| `A○B` | Circular | See circular functions table below |
| `A<B` | Less | 1 if `A<B` else 0 |
| `A≤B` | Not greater | 1 if `A≤B` else 0 |
| `A=B` | Equal | 1 if `A=B` else 0 |
| `A≥B` | Not less | 1 if `A≥B` else 0 |
| `A>B` | Greater | 1 if `A>B` else 0 |
| `A≠B` | Not equal | 1 if `A≠B` else 0 |
| `A∧B` | And | See logical functions table below |
| `A∨B` | Or | See logical functions table below |
| `A⍲B` | Nand | See logical functions table below |
| `A⍱B` | Nor | See logical functions table below |

#### Circular functions

| `(-A)○B` | `A` | `A○B` |
| ---: | :---: | :--- |
| `(1-B*2)*.5` | 0 | `(1-B*2)*.5` |
| `Arcsin B` | 1 | `Sine B` |
| `Arccos B` | 2 | `Cosine B` |
| `Arctan B` | 3 | `Tangent B` |
| `(¯1+B*2)*.5` | 4 | `(1+B*2)*.5` |
| `Arcsinh B` | 5 | `Sinh B` |
| `Arccosh B` | 6 | `Cosh B` |
| `Arctanh B` | 7 | `Tanh B` |


#### Logical functions

| `A` | `B` | `A∧B` | `A∨B` | `A⍲B` | `A⍱B` |
| :---: | :---: | :-----: | :-----: | :-----: | :-----: |
|  0  |  0  |   0   |   0   |   1   |   1   |
|  0  |  1  |   0   |   1   |   1   |   0   |
|  1  |  0  |   0   |   1   |   1   |   0   |
|  1  |  1  |   1   |   1   |   0   |   0   |


## User Functions

## Function Editor

Traditional APL functions can be created and edited outside toyAPL using a text editor that can encode files in UTF-8. Such files can then be loaded using the system command `)LOAD filename`.

In addition to this mechanism, there is a built-in line editor that can also be used to create and edit functions.

To create a new function, in the REPL type `∇` followed by the function header. For example

    ∇ ret ← x myfun y ; a; b; c

To edit an existing function, type `∇` followed just by the function name:

    ∇ myfun

In both cases, after editing the function, type `∇` again to exit the editor and return to the REPL.

### Valid editor commands

Most of these commands behave as in other APL implementations, but some are different. A major difference from other editors is that toyAPL doesn't use fractional line numbers. To insert a new line before or after an existing line, use the `<` and `>` commands. The line numbers are always automatically adjusted.

| Command | Action |
| --- | --- |
| `[⎕]`   | Display all lines, insert after end |
| `[N⎕]`  | Display from line N to end, insert after end |
| `[⎕N]`  | Display from line 1 to line N, insert after line N |
| `[M⎕N]` | Display from line M to line N, insert after line N |
| `[N]`   | Replace line N |
| `[!N]`  | Delete line N (bang!) |
| `[<N]`  | Insert before line N |
| `[>N]`  | Insert after line N |

It is possible to add a command in the invocation to edit a function. For example

    ∇ myfun [⎕]

will open `myfun` for editing and display all its lines. If the function has 'N' lines, the editor will then display `[N+1]` and wait for a new line to be entered.

## System Variables

| Variable | Mutable? | Description |
| --- | :---: | --- |
| `⎕A` | N | Alphabet (26 uppercase letters) |
| `⎕D` | N | Digits (0 to 9) |
| `⎕IO` | Y | Index origin |
| `⎕PID` | N | Process id |
| `⎕PP` | Y | Print precision |
| `⎕TS` | N | Timestamp |
| `⎕VER` | N | Version |
| `⎕WSID` | Y | Workspace ID |


## System Commands

| Command | Description |
| --- | --- |
| `)CLEAR` | Clear the workspace |
| `)DIGITS` | Set/get print precision |
| `)ERASE` | Erase variable or function |
| `)FNS` | Show defined functions |
| `)HEAP` | Show heap statistics |
| `)LOAD` | Load source file or workspace |
| `)MEM` | Show memory usage |
| `)OFF` | Exit toyAPL |
| `)ORIGIN` | Set/get the system origin |
| `)SAVE` | Save function source or workspace |
| `)VARS` | Show defined variables |
| `)WSID` | Show/change workspace ID |
| `)?` | Show quick help |


