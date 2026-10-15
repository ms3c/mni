// recursion, mutual recursion and callbacks into user code
fib = unda(n) { kama (n <= 1) { rudisha n } rudisha fib(n - 1) + fib(n - 2) }
andika(fib(15))

isEven = unda(n) { kama (n == 0) { rudisha kweli } rudisha isOdd(n - 1) }
isOdd = unda(n) { kama (n == 0) { rudisha sikweli } rudisha isEven(n - 1) }
andika(isEven(40), isOdd(7))

sum = unda(n) { kama (n == 0) { rudisha 0 } rudisha n + sum(n - 1) }
andika(sum(100))

compose = unda(f, g) { rudisha unda(x) { rudisha f(g(x)) } }
inc = unda(x) { rudisha x + 1 }
dbl = unda(x) { rudisha x * 2 }
andika(compose(inc, dbl)(5), compose(dbl, inc)(5))

andika([1, 2, 3].map(unda(x) { rudisha [1, 2].map(unda(y) { rudisha x * y }) }))
total = 0;
[1, 2, 3].map(unda(x) { total += x rudisha x })
andika(total)
andika([1, 2, 3, 4, 5].chuja(unda(x) { rudisha x > fib(4) }))

apply = unda(f, n) { rudisha f(f(f(n))) }
andika(apply(unda(x) { rudisha x * 3 }, 2))
