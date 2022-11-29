⎕←'Testing outer product'

msg←2 6⍴' Error Ok   '

⍞←'Testing 1 2 3 ∘.+ 1 2 3'
z←1 2 3 ∘.+ 1 2 3
x←3 3⍴2 3 4 3 4 5 4 5 6
e←1+∧/,z=x
⎕←msg[e;]

⍞←'Testing 1 2 3 ∘.- 1 2 3'
z←1 2 3 ∘.- 1 2 3
x←3 3⍴0 ¯1 ¯2 1 0 ¯1 2 1 0
e←1+∧/,z=x
⎕←msg[e;]

⍞←'Testing 1 2 3 ∘.× 1 2 3'
z←1 2 3 ∘.× 1 2 3
x←3 3⍴1 2 3 2 4 6 3 6 9
e←1+∧/,z=x
⎕←msg[e;]

⍞←'Testing 1 2 4 ∘.÷ 1 2 4'
z←1 2 4 ∘.÷ 1 2 4
x←3 3⍴1 0.5 0.25 2 1 0.5 4 2 1 
e←1+∧/,z=x
⎕←msg[e;]

⍞←'Testing 1 2 3 ∘.* 1 2 3'
z←1 2 3 ∘.* 1 2 3
x←3 3⍴1 1 1 2 4 8 3 9 27
e←1+∧/,z=x
⎕←msg[e;]

⍞←'Testing (⍳5)∘.=⍳5'
z←(⍳5)∘.=⍳5
x←5 5⍴1 0 0 0 0 0 1 0 0 0 0 0 1 0 0 0 0 0 1 0 0 0 0 0 1
e←1+∧/,z=x
⎕←msg[e;]

⍞←'Testing (100×⍳2)∘.+(10×⍳4)∘.+⍳3'
z←(100×⍳2)∘.+(10×⍳4)∘.+⍳3
x←2 4 3⍴111 112 113 121 122 123 131 132 133 141 142 143 211 212 213 221 222 223 231 232 233 241 242 243
e←1+∧/,z=x
⎕←msg[e;]
