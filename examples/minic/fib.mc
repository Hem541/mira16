// fib.mc -- print the first 10 Fibonacci numbers
var a = 0;
var b = 1;
var count = 10;
while (count > 0) {
    print a;
    var next = a + b;
    a = b;
    b = next;
    count = count - 1;
}
