⎕←'Testing the dyadic rotate function'
msg←2 6⍴' Error Ok   '
m←2 3 4⍴⍳24

⍞←'Testing q⌽[1]m'
q←3 4⍴0 1 2 3 ¯1 ¯2 1 2 ¯3 ¯2 ¯1 0
z←q⌽[1]m
x←2 3 4⍴1 14 3 16 17 6 19 8 21 10 23 12 13 2 15 4 5 18 7 20 9 22 11 24
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing q⊖m'
z←q⊖m
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing r⌽[2]m'
r←2 4⍴0 1 2 3 ¯1 ¯2 1 2
z←r⌽[2]m
x←2 3 4⍴1 6 11 4 5 10 3 8 9 2 7 12 21 18 19 24 13 22 23 16 17 14 15 20
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing s⌽[3]m'
s←2 3⍴0 1 2 ¯1 ¯2 0
z←s⌽[3]m
x←2 3 4⍴1 2 3 4 6 7 8 5 11 12 9 10 16 13 14 15 19 20 17 18 21 22 23 24
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing s⌽m'
z←s⌽m
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1⌽m'
z←1⌽m
x←2 3 4⍴2 3 4 1 6 7 8 5 10 11 12 9 14 15 16 13 18 19 20 17 22 23 24 21
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 2⌽5'
z←2⌽5
x←5
e←1+∧/,x=z
⎕←msg[e;]
