⍝ APL: The Language and its Usage
⍝ Illustration 5.17 (page 180)
⍝ A Chessboard

∇ B←CHESSBD
B←' /'[1+(2|⌊(1+⍳26)÷3)∘.=2|⌊(8+⍳42)÷5]
B[1;]←B[26;]←B[;1]←B[;42]←'*'
∇
