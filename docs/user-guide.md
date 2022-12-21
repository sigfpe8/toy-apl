## Monadic Functions

A monadic function `f` operates on the single argument to its right `B`: `fB`.

### Monadic Scalar Functions

A monadic scalar function is defined for a scalar value according to the table below. If the argument `B` is an array, the result is an array of the same shape with `f` applied to it element wise.

| Function | Name | Definition |
| --- | --- | --- |
| `+B` | Plus | `0+B` |
| `-B` | Negative | `0-B` |
| `×B` | Signum | `¯1, 0, 1` |
| `÷B` | Reciprocal | `1 ÷ B` |
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

## User Functions

## Function Editor

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


