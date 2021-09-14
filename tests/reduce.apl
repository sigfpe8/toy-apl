⎕←'Testing the reduce operator'
msg←2 6⍴' Error Ok   '
m←2 3 4⍴⍳24

⍞←'Testing +⌿m'
z←+⌿m
x←3 4⍴14 16 18 20 22 24 26 28 30 32 34 36
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/[1]m'
z←+/[1]m
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/[2]m'
z←+/[2]m
x←2 4⍴15 18 21 24 51 54 57 60
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/[3]m'
z←+/[3]m
x←2 3⍴10 26 42 58 74 90
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/m'
z←+/m
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing -/[1]m'
z←-/[1]m
x←3 4⍴¯12 ¯12 ¯12 ¯12 ¯12 ¯12 ¯12 ¯12 ¯12 ¯12 ¯12 ¯12
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing -/[2]m'
z←-/[2]m
x←2 4⍴5 6 7 8 17 18 19 20
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing -/[3]m'
z←-/[3]m
x←2 3⍴¯2 ¯2 ¯2 ¯2 ¯2 ¯2
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/[1]n with ⍴[1]=1'
n←1 3 4⍴⍳12
z←+/[1]n
x←3 4⍴⍳12
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/[2]n with ⍴[2]=1'
n←2 1 4⍴⍳8
z←+/[2]n
x←2 4⍴⍳12
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/[3]n with ⍴[3]=1'
n←2 3 1⍴⍳6
z←+/[3]n
x←2 3⍴⍳6
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/[1]n with ⍴[2]=0'
n←2 0 4⍴⍳8
z←+/[1]n
x←0 4⍴⍳0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing +/[2]n with ⍴[3]=0'
n←2 3 0⍴⍳6
z←+/[2]n
x←2 0⍴⍳0
e←1+∧/,x=z
⎕←msg[e;]

