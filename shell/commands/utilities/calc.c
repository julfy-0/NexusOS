#include "calc.h"
#include "console.h"

static void print_int(long n) {
    char digits[20];
    int i = 0;

    if (n < 0) {
        console_putchar('-');
        n = -n;
    }

    if (n == 0) {
        console_putchar('0');
        return;
    }

    while (n > 0 && i < 20) {
        digits[i++] = (char)('0' + (n % 10));
        n /= 10;
    }

    while (i > 0) {
        i--;
        console_putchar(digits[i]);
    }
}

static const char *skip_spaces(const char *p) {
    while (*p == ' ') p++;
    return p;
}

/* Парсит знаковое целое число, сдвигает *p за его пределы.
 * Возвращает 1 при успехе, 0 если числа нет. */
static int parse_int(const char **p, long *out) {
    const char *s = *p;
    int negative = 0;
    long value = 0;
    int got_digit = 0;

    if (*s == '-') {
        negative = 1;
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
        got_digit = 1;
    }

    if (!got_digit) {
        return 0;
    }

    *out = negative ? -value : value;
    *p = s;
    return 1;
}

void calc_run(const char *args) {
    const char *p = args;
    long a, b;

    p = skip_spaces(p);
    if (!parse_int(&p, &a)) {
        console_print("calc: expected a number\n");
        console_print("usage: calc <a> <op> <b>, op is one of + - * /\n");
        return;
    }

    p = skip_spaces(p);
    char op = *p;
    if (op != '+' && op != '-' && op != '*' && op != '/') {
        console_print("calc: expected operator (+ - * /)\n");
        return;
    }
    p++;

    p = skip_spaces(p);
    if (!parse_int(&p, &b)) {
        console_print("calc: expected a second number\n");
        return;
    }

    long result;
    switch (op) {
        case '+':
            result = a + b;
            break;
        case '-':
            result = a - b;
            break;
        case '*':
            result = a * b;
            break;
        case '/':
            if (b == 0) {
                console_print("calc: division by zero\n");
                return;
            }
            result = a / b;
            break;
        default:
            return; /* недостижимо, op уже проверен выше */
    }

    print_int(result);
    console_putchar('\n');
}
