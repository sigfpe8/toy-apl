⎕←'Testing the dyadic laminate function'

msg←2 6⍴' Error Ok   '
m←3 4⍴⍳12
n←10×m

⍞←'Testing m,[0.5]n'
z←m,[0.5]n
x←2 3 4⍴1 2 3 4 5 6 7 8 9 10 11 12 10 20 30 40 50 60 70 80 90 100 110 120
e←1+∧/,z=x
⎕←msg[e;]

⍞←'Testing m,[1.5]n'
z←m,[1.5]n
x←3 2 4⍴1 2 3 4 10 20 30 40 5 6 7 8 50 60 70 80 9 10 11 12 90 100 110 120
e←1+∧/,z=x
⎕←msg[e;]

⍞←'Testing m,[2.5]n'
z←m,[2.5]n
x←3 4 2⍴1 10 2 20 3 30 4 40 5 50 6 60 7 70 8 80 9 90 10 100 11 110 12 120
e←1+∧/,z=x
⎕←msg[e;]
