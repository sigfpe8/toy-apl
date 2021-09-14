⍝ APL: The Language and its Usage
⍝ Illustration 5.18 (page 180)
⍝ A Histogram

∇ Z←SYM HISTO V
Z←(' ',SYM)[1+(⍒⍳⌈/V)∘.≤V]
∇

