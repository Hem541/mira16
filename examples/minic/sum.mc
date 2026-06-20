// sum.mc -- sum of 1..10 = 55, with a conditional
var i = 1;
var total = 0;
while (i <= 10) {
    total = total + i;
    i = i + 1;
}
print total;
if (total > 50) { print 1; } else { print 0; }
