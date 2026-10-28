
 // Yaroslava Sirychenko 251ADB151
// https://github.com/<ysiryyych>/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: https://beej.us/guide/bgc/html/split/file-inputoutput.html#file-inputoutput
//https://github.com/WarpRules/recursive_descent_parser_tutorial
// https://chatgpt.com/share/6ae15d1a-78e8-83ed-8a39-31f9e6e6aaf7

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <math.h>

#define NAME "yaroslava"
#define LASTNAME "sirychenko"
#define STUDENTID "251adb151"

static char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;


    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        fclose(f);
        return NULL; // If malloc fails, close the file and report failure by returning NULL.
    }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;

        if (len + 1 >= cap) {
            if (cap > (size_t)-1 / 2) {
                free(buf);
                fclose(f);
                return NULL;
            }

// Double the buffer capacity when full to limit reallocations
// while reading large files.
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) {
                free(buf);
                fclose(f);
                return NULL;
            }
            buf = tmp;
        }
    }

    if (ferror(f)) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    buf[len] = '\0';
    *len_out = len;
    return buf;
}
// Track positions to report the exact location of a parsing error
// pos and end are zero-based offsets in the full input buffer; end is exclusive.
// error_pos is 1-based for output, and 0 means no error has been recorded.
typedef struct {
    const char *s;
    size_t pos;
    size_t end;
    size_t error_pos;
} Parser;



typedef struct {
    long long integer;
    double floating;
    int is_float;
} Value;

static Value make_integer(long long n) {
    Value v = {n, (double)n, 0};
    return v;
}

static Value make_float(double n) {
    Value v = {0, n, 1};
    return v;
}

static double as_double(Value v) {
    return v.is_float ? v.floating : (double)v.integer;
}

static void skip_spaces(Parser *p) {
    while (p->pos < p->end &&
           (p->s[p->pos] == ' ' ||
            p->s[p->pos] == '\t' ||
            p->s[p->pos] == '\r')) {
        p->pos++;
    }
}
// Separate parsing levels enforce operator precedence:
// powers are parsed inside terms, and terms inside expressions.
static Value parse_expression(Parser *p); 
// Recursive parsing of the exponent makes ** right-associative:
// 2**3**2 is evaluated as 2**(3**2).
static Value parse_power(Parser *p); 

static Value parse_primary(Parser *p) {
    skip_spaces(p);

    if (p->pos >= p->end) {
        // No token remains where a value is required, so report the line break or EOF position
        p->error_pos = p->end + 1;
        return make_integer(0);
    }

    // Parsing a full expression inside parentheses lets them override normal precedence
    if (p->s[p->pos] == '(') {
        p->pos++;

        Value value = parse_expression(p); 
        if (p->error_pos != 0) return make_integer(0);

        skip_spaces(p);

        if (p->pos >= p->end) {
            // Report where the missing closing ')' was expected: at the line break or EOF.
            p->error_pos = p->end + 1;
            return make_integer(0);
        }

        if (p->s[p->pos] != ')') {
            // Report the error at the unexpected character
            p->error_pos = p->pos + 1;
            return make_integer(0);
        }

        p->pos++;
        return value;
    }

    if (!isdigit((unsigned char)p->s[p->pos])) {
        p->error_pos = p->pos + 1;
        return make_integer(0);
    }

    long long integer_part = 0;

    while (p->pos < p->end &&
           isdigit((unsigned char)p->s[p->pos])) {
        integer_part = integer_part * 10 +
                       (p->s[p->pos] - '0');
        p->pos++;
    }

    if (p->pos < p->end && p->s[p->pos] == '.') {
        size_t dot_pos = p->pos;
        p->pos++;

        if (p->pos >= p->end ||
            // This character cannot start a number or parenthesized expression, so report it
            !isdigit((unsigned char)p->s[p->pos])) {
            // A decimal point must be followed by a digit, so report the dot itself
            p->error_pos = dot_pos + 1;
            return make_integer(0);
        }

        double value = (double)integer_part;
        double place = 0.1;

        while (p->pos < p->end &&
               isdigit((unsigned char)p->s[p->pos])) {
            value += (p->s[p->pos] - '0') * place;
            place *= 0.1;
            p->pos++;
        }

        return make_float(value);
    }

    return make_integer(integer_part);
}

static long long integer_power(long long base,
                               long long exponent) {
    long long result = 1;

    // Exponentiation by squaring reduces the number of multiplications to O(log exponent)
    while (exponent > 0) {
        if (exponent % 2 != 0) {
            result *= base;
        }

        exponent /= 2;

        if (exponent > 0) {
            base *= base;
        }
    }

    return result;
}

static Value parse_power(Parser *p) {
    // Parsing the exponent with parse_power() makes ** right-associative: 2**3**2 is evaluated as 2**(3**2)
    Value value = parse_primary(p);
    if (p->error_pos != 0) return make_integer(0);

    skip_spaces(p);

    if (p->pos + 1 >= p->end ||
        p->s[p->pos] != '*' ||
        p->s[p->pos + 1] != '*') {
        return value;
    }

    size_t operator_pos = p->pos;
    p->pos += 2;
    skip_spaces(p);

    size_t exponent_start = p->pos;

    if (p->pos < p->end && p->s[p->pos] == '-') {
        // Unary minus is not allowed here, so report the '-' that prevents parsing the exponent
        p->error_pos = p->pos + 1;
        return make_integer(0);
    }

    Value exponent = parse_power(p);
    if (p->error_pos != 0) return make_integer(0);

    if (!exponent.is_float && exponent.integer < 0) {
        // Report the start of the exponent expression when its evaluated integer value is negative
        p->error_pos = exponent_start + 1;
        return make_integer(0);
    }
// Use integer exponentiation when both operands are integers; otherwise use floating-point pow()
    if (!value.is_float && !exponent.is_float  && exponent.integer < 0) {
        return make_integer(
            integer_power(value.integer, exponent.integer));
    }

    double result = pow(as_double(value), as_double(exponent));

    if (!isfinite(result)) {
        // Report the ** operator when the floating-point result is not finite
        p->error_pos = operator_pos + 1;
        return make_integer(0);
    }

    return make_float(result);
}

static Value parse_term(Parser *p) {
    // Parsing powers as operands makes ** bind more tightly than * and /
    Value value = parse_power(p);
    if (p->error_pos != 0) return make_integer(0);

    while (1) {
        skip_spaces(p);

        if (p->pos >= p->end ||
            (p->s[p->pos] != '*' && p->s[p->pos] != '/')) {
            break;
        }

        size_t operator_pos = p->pos;
        char op = p->s[p->pos++];

        // If ** has no usable exponent, report its second '*' instead of treating it as multiplication
        if (op == '*' && p->pos < p->end &&
            p->s[p->pos] == '*') {
            p->error_pos = p->pos + 1;
            return make_integer(0);
        }

        skip_spaces(p);
        size_t right_start = p->pos;

        Value right = parse_power(p);
        if (p->error_pos != 0) return make_integer(0);

        if (op == '/' &&
            (right.is_float
                ? right.floating == 0.0
                : right.integer == 0)) {
 // Report division by zero at the start of the right operand.
            p->error_pos = right_start + 1;
            return make_integer(0);
        }

        // Keep integer arithmetic when both operands are integers to avoid unnecessary floating-point conversion.
        if (!value.is_float && !right.is_float) {
            if (op == '*') {
                value = make_integer(
                    value.integer * right.integer);
            } else {
                value = make_integer(
                    value.integer / right.integer);
            }
        } else {
            double a = as_double(value);
            double b = as_double(right);
            double result = (op == '*') ? a * b : a / b;

            if (!isfinite(result)) {
                // Report the operator that caused a non-finite floating-point result
                p->error_pos = operator_pos + 1;
                return make_integer(0);
            }

            value = make_float(result);
        }
    }

    return value;
}

static Value parse_expression(Parser *p) {
    // Parsing terms before + and - gives multiplication and division higher precedence
    Value value = parse_term(p);
    if (p->error_pos != 0) return make_integer(0);

    while (1) {
        skip_spaces(p);

        if (p->pos >= p->end ||
            (p->s[p->pos] != '+' && p->s[p->pos] != '-')) {
            break;
        }

        size_t operator_pos = p->pos;
        char op = p->s[p->pos++];

        Value right = parse_term(p);
        if (p->error_pos != 0) return make_integer(0);


// Keep + and - in integer arithmetic unless either operand is floating-point
        if (!value.is_float && !right.is_float) {
            if (op == '+') {
                value = make_integer(
                    value.integer + right.integer);
            } else {
                value = make_integer(
                    value.integer - right.integer);
            }
        } else {
            double a = as_double(value);
            double b = as_double(right);
            double result = (op == '+') ? a + b : a - b;

            if (!isfinite(result)) {
                // Report the operator that caused a non-finite floating-point result
                p->error_pos = operator_pos + 1;
                return make_integer(0);
            }

            value = make_float(result);
        }
    }

    return value;
}

static Value eval_line(const char *s, size_t start, size_t end,
                       size_t *error_pos) {
    Parser p = {s, start, end, 0};

    Value result = parse_expression(&p);

    if (p.error_pos == 0) {
        skip_spaces(&p);
// After a valid expression, the first remaining non-space character is unexpected
        if (p.pos < p.end) {
            p.error_pos = p.pos + 1;
        }
    }

    *error_pos = p.error_pos;
    return (*error_pos != 0) ? make_integer(0) : result;
}


static void process_text(char *buf, size_t len, FILE *out) {
    // Keep results temporary so a later error can replace all output with ERROR:<pos>
    FILE *temp = tmpfile();

    if (!temp) {
        fprintf(stderr, "cannot create temporary file\n");
        return;
    }

    // Keep offsets in the original buffer so error positions include earlier lines and newlines
    size_t i = 0;
    size_t error_pos = 0;

    while (i < len) {
        char *nl = memchr(buf + i, '\n', len - i);
        size_t end = nl ? (size_t)(nl - buf) : len;

        size_t j = i;
        while (j < end &&
               (buf[j] == ' ' || buf[j] == '\t' || buf[j] == '\r')) {
            j++;
        }

        if (j < end && buf[j] != '#') {
            size_t line_error = 0;
            Value result = eval_line(buf, j, end, &line_error);            if (line_error != 0) {
                error_pos = line_error;
                break;
            }
            
            if (result.is_float) {
    fprintf(temp, "%.15g\n", result.floating);
} else {
    fprintf(temp, "%lld\n", result.integer);
}        }

        i = nl ? end + 1 : len;
    }

    if (error_pos != 0) {
        fprintf(out, "ERROR:%zu\n", error_pos);
    } else {
        rewind(temp);

        int ch;
        while ((ch = fgetc(temp)) != EOF) {
            fputc(ch, out);
        }
    }

    fclose(temp);
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int ends_with_txt(const char *name) {
    size_t len = strlen(name);
    return len >= 4 && strcmp(name + len - 4, ".txt") == 0;
}

static char *make_path(const char *dir, const char *name) {
    size_t size = strlen(dir) + strlen(name) + 2;
    char *path = malloc(size);

    if (path) {
        snprintf(path, size, "%s/%s", dir, name);
    }

    return path;
}

static int ensure_dir(const char *dir) {
    struct stat st;

    if (stat(dir, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }

        fprintf(stderr, "%s is not a directory\n", dir);
        return -1;
    }

    if (mkdir(dir, 0777) == 0) {
        return 0;
    }

    fprintf(stderr, "cannot create folder %s\n", dir);
    return -1;
}

static int process_file(const char *input, const char *outdir) {
    size_t len = 0;
    char *text = read_file(input, &len);

    if (!text) {
        fprintf(stderr, "cannot read %s\n", input);
        return -1;
    }

    const char *base = base_name(input);
    size_t base_len = strlen(base);

    if (ends_with_txt(base)) {
        base_len -= 4;
    }

    int name_len = snprintf(NULL, 0,
        "%.*s_%s_%s_%s.txt",
        (int)base_len, base, NAME, LASTNAME, STUDENTID);

    if (name_len < 0) {
        free(text);
        return -1;
    }

    char *filename = malloc((size_t)name_len + 1);

    if (!filename) {
        free(text);
        return -1;
    }

    snprintf(filename, (size_t)name_len + 1,
        "%.*s_%s_%s_%s.txt",
        (int)base_len, base, NAME, LASTNAME, STUDENTID);

    char *path = make_path(outdir, filename);
    free(filename);

    if (!path) {
        free(text);
        return -1;
    }

    FILE *out = fopen(path, "wb");

    if (!out) {
        fprintf(stderr, "cannot write %s\n", path);
        free(path);
        free(text);
        return -1;
    }

    process_text(text, len, out);

    int failed = ferror(out);
    if (fclose(out) != 0) {
        failed = 1;
    }

    free(path);
    free(text);
    return failed ? -1 : 0;
}

static int process_dir(const char *indir, const char *outdir) {
    DIR *dir = opendir(indir);

    if (!dir) {
        fprintf(stderr, "cannot open folder %s\n", indir);
        return -1;
    }

    char **files = NULL;
    size_t count = 0;
    size_t capacity = 0;
    struct dirent *entry;
    int failed = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!ends_with_txt(entry->d_name)) {
            continue;
        }

        char *path = make_path(indir, entry->d_name);

        if (!path) {
            failed = 1;
            break;
        }

        struct stat st;

        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            free(path);
            continue;
        }

        free(path);

        if (count == capacity) {
            size_t new_capacity = capacity ? capacity * 2 : 8;
            char **tmp = realloc(files,
                                 new_capacity * sizeof(*files));

            if (!tmp) {
                failed = 1;
                break;
            }

            files = tmp;
            capacity = new_capacity;
        }

        size_t name_len = strlen(entry->d_name) + 1;
files[count] = malloc(name_len);

if (files[count] != NULL) {
    memcpy(files[count], entry->d_name, name_len);
}

        if (!files[count]) {
            failed = 1;
            break;
        }

        count++;
    }

    closedir(dir);


    for (size_t i = 0; i < count; i++) {
        if (!failed) {
            char *input = make_path(indir, files[i]);

            if (!input || process_file(input, outdir) != 0) {
                failed = 1;
            }

            free(input);
        }

        free(files[i]);
    }

    free(files);
    return failed ? -1 : 0;
}

int main(int argc, char **argv) {
    const char *indir = NULL;
    const char *input = NULL;
    const char *outdir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 ||
            strcmp(argv[i], "--dir") == 0) {
            if (indir || i + 1 >= argc ||
                argv[i + 1][0] == '-') {
                goto usage_error;
            }

            indir = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 ||
                   strcmp(argv[i], "--output-dir") == 0) {
            if (outdir || i + 1 >= argc ||
                argv[i + 1][0] == '-') {
                goto usage_error;
            }

            outdir = argv[++i];
        } else if (argv[i][0] == '-') {
            goto usage_error;
        } else {
            if (input) {
                goto usage_error;
            }

            input = argv[i];
        }
    }

    if ((indir && input) || (!indir && !input)) {
        goto usage_error;
    }

    char *default_outdir = NULL;

    if (!outdir) {
        const char *base = base_name(indir ? indir : input);
        size_t len = strlen(base);

        if (!indir && ends_with_txt(base)) {
            len -= 4;
        }

        int n = snprintf(NULL, 0, "%.*s_%s_%s",
                         (int)len, base, NAME, STUDENTID);

        if (n < 0) {
            return 1;
        }

        default_outdir = malloc((size_t)n + 1);

        if (!default_outdir) {
            return 1;
        }

        snprintf(default_outdir, (size_t)n + 1,
                 "%.*s_%s_%s",
                 (int)len, base, NAME, STUDENTID);

        outdir = default_outdir;
    }

    if (ensure_dir(outdir) != 0) {
        free(default_outdir);
        return 1;
    }

    int result = indir
        ? process_dir(indir, outdir)
        : process_file(input, outdir);

    free(default_outdir);
    return result == 0 ? 0 : 1;

usage_error:
    fprintf(stderr,
        "usage: calc [-d DIR | --dir DIR] "
        "[-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
    return 1;
}