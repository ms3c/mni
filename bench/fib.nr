// recursive calls
fib = unda(n) {
    kama (n <= 1) { rudisha n }
    rudisha fib(n - 1) + fib(n - 2)
}
andika(fib(30))
