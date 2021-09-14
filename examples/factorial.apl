⍝ A Course in APL with Applications
⍝ 9.9 Recursive Functions (pag 117)
⍝ Factorial

∇ Z←FACTORIAL N
→(N≠0)/NEXT
→0,Z←1
NEXT: Z←N×FACTORIAL(N-1)
∇
