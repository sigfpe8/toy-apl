⍝ Return a list of the prime numbers up to 'n'
⍝    primes 17
⍝ 2 3 5 7 11 13 17

∇ z ← primes n
	z←(~n∊n∘.×n)/n←1↓⍳n
∇
