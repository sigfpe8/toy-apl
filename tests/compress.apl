⎕←'Testing the compress function'
msg←2 6⍴' Error Ok   '
m←2 3 4⍴⍳24

⍞←'Testing 1 0 1 0 1/⍳5'
z←1 0 1 0 1/⍳5
x←1 3 5
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0 1 0 1 0/⍳5'
z←0 1 0 1 0/⍳5
x←2 4
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0 0 0 0 0/⍳5'
z←0 0 0 0 0/⍳5
x←⍳0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1 0 3 0 5/⍳5'
z←1 0 3 0 5/⍳5
x←1 3 3 3 5 5 5 5 5
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1 ¯2 3 ¯4 5/⍳5'
z←1 ¯2 3 ¯4 5/⍳5
x←1 0 0 3 3 3 0 0 0 0 5 5 5 5 5
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0 0/[1]m'
z←0 0/[1]m
x←0 3 4⍴⍳0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0 1/[1]m'
z←0 1/[1]m
x←1 3 4⍴13 14 15 16 17 18 19 20 21 22 23 24
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1 0/[1]m'
z←1 0/[1]m
x←1 3 4⍴⍳12
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1 1/[1]m'
z←1 1/[1]m
x←m
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0 1⌿m'
z←0 1⌿m
x←1 3 4⍴13 14 15 16 17 18 19 20 21 22 23 24
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1 0 1/[2]m'
z←1 0 1/[2]m
x←2 2 4⍴1 2 3 4 9 10 11 12 13 14 15 16 21 22 23 24
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1 2 3/[2]m'
z←1 2 3/[2]m
x←2 6 4⍴1 2 3 4 5 6 7 8 5 6 7 8 9 10 11 12 9 10 11 12 9 10 11 12 13 14 15 16 17 18 19 20 17 18 19 20 21 22 23 24 21 22 23 24 21 22 23 24
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1 0 1 0/[3]m'
z←1 0 1 0/[3]m
x←2 3 2⍴1 3 5 7 9 11 13 15 17 19 21 23
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0 1 ¯2 3/[3]m'
z←0 1 ¯2 3/[3]m
x←2 3 6⍴2 0 0 4 4 4 6 0 0 8 8 8 10 0 0 12 12 12 14 0 0 16 16 16 18 0 0 20 20 20 22 0 0 24 24 24
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0 1 ¯2 3/m'
z←0 1 ¯2 3/m
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0/[1]m'
z←0/[1]m
x←0 3 4⍴0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1/[1]m'
z←1/[1]m
x←m
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0/[2]m'
z←0/[2]m
x←2 0 4⍴0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1/[2]m'
z←1/[2]m
x←m
e←1+∧/,x=z
⎕←msg[e;]

x←0
z←0
