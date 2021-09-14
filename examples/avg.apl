∇ z ← avg x
⍝ Compute the numeric average of vector <x>
z←(+/x)÷⍴x
∇

∇ z ← sd x
⍝ Compute the numeric std deviation of vector <x>
z ← avg x
z ← ((+/(x-z)*2)÷⍴x)*.5
∇

