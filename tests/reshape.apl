⎕←'Testing the reshape function'
msg←2 6⍴' Error Ok   '
m←2 3 4⍴⍳24

⍞←'Testing 0⍴1'
z←0⍴1
x←⍴1
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1⍴1'
z←1⍴1
x←,1
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 2⍴1'
z←2⍴1
x←1 1
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0⍴⍳0'
z←0⍴⍳0
x←⍴0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1⍴⍳0'
z←1⍴⍳0
x←0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 2⍴⍳0'
z←2⍴⍳0
x←0 0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 0⍴1 2 3'
z←0⍴1 2 3
x←⍴0
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 1⍴1 2 3'
z←1⍴1 2 3
x←1
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 3⍴1 2 3'
z←3⍴1 2 3
x←1 2 3
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 5⍴1 2 3'
z←5⍴1 2 3
x←1 2 3 1 2
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 2 3⍴⍳6'
z←2 3⍴⍳6
x←1 2 3,[0.5]4 5 6
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 2 3 4⍴⍳24'
z←2 3 4⍴⍳24
p1←1 2 3 4,[1]5 6 7 8,[0.5]9 10 11 12
p2←13 14 15 16,[1]17 18 19 20,[0.5]21 22 23 24
x←p1,[0.5]p2
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 6 4⍴m'
z←6 4⍴m
x←1  2  3  4,[1]5  6  7  8,[1]9 10 11 12,[1]13 14 15 16,[1]17 18 19 20,[0.5]21 22 23 24
e←1+∧/,x=z
⎕←msg[e;]

⍞←'Testing 24⍴m'
z←24⍴m
x←⍳24
e←1+∧/,x=z
⎕←msg[e;]

