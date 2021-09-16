⍝ Example from the "APL\360 User's Manual", Appendix A
⍝
⍝ E.g. MULTDRILL 10 12
⍝  Enter the product of the displayed numbers
⍝  or 'S' to stop.

∇ MULTDRILL N;Y;X
Y←?N
Y
X←⎕
→0×⍳X='S'
→⍳X=×/Y
'Wrong, try again'
→3
