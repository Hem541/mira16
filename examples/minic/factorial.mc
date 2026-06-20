// factorial.mc -- compute 5! = 120 with a while loop
var n = 5;
var result = 1;
while (n > 0) {
    result = result * n;
    n = n - 1;
}
print result;
