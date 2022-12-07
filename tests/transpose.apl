⎕←'Testing the dyadic transpose function'
msg←2 6⍴' Error Ok   '
m←2 3 4⍴⍳24
t←(100×⍳2)∘.+(10×⍳4)∘.+(⍳3)

⍞←'Testing 1 2 3⍉t'
z←1 2 3⍉t
x←2 4 3⍴111 112 113 121 122 123 131 132 133 141 142 143 211 212 213 221 222 223 231 232 233 241 242 243
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1 3 2⍉t'
z←1 3 2⍉t
x←2 3 4⍴111 121 131 141 112 122 132 142 113 123 133 143 211 221 231 241 212 222 232 242 213 223 233 243
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 2 3 1⍉t'
z←2 3 1⍉t
x←3 2 4⍴111 121 131 141 211 221 231 241 112 122 132 142 212 222 232 242 113 123 133 143 213 223 233 243
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 2 1 3⍉t'
z←2 1 3⍉t
x←4 2 3⍴111 112 113 211 212 213 121 122 123 221 222 223 131 132 133 231 232 233 141 142 143 241 242 243
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 3 1 2⍉t'
z←3 1 2⍉t
x←4 3 2⍴111 211 112 212 113 213 121 221 122 222 123 223 131 231 132 232 133 233 141 241 142 242 143 243
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 3 2 1⍉t'
z←3 2 1⍉t
x←3 4 2⍴111 211 121 221 131 231 141 241 112 212 122 222 132 232 142 242 113 213 123 223 133 233 143 243
e←1+∧/,x=z
⎕←msg[e;]