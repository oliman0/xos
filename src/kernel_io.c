#include <kernel/kernel_io.h>

static void print_number(unsigned int num, unsigned short int base, bool isSigned)
{
    if (base > 16) return;

    const char* digits = "0123456789ABCDEF";
    char outBuffer[22];

    // If number is signed we check if negative, else false
    bool negative = isSigned ? (int)num < 0 : false;

    // If negative make positive so that we can print
    if (negative) num = (int)num * -1;

    // Iterate backwards over each digit of number
    int i = 0;
    do {
        // Remainder will be current digit, convert to char and store in buffer
        outBuffer[i++] = digits[num % base];
        num /= base;
    } while (num != 0);

    // Prepend negative sign
    if (negative) outBuffer[i++] = '-';

    // Null terminate the string
    outBuffer[i--] = '\0';

    // Reverse string before printing
    for (int j = 0; j < i; j++, i--) {
        // i starts from the top of the buffer
        // j starts from bottom
        char tmp = outBuffer[j]; // Store char at j
        outBuffer[j] = outBuffer[i]; // Set the char at j to char at i
        outBuffer[i] = tmp; // Set char i to char at j
    }

    fb_put_str(outBuffer);
}

void kprintf(char *fmtstr, ...)
{
    va_list args;
    va_start(args, fmtstr);

    char out[2];
    out[0] = '\0', out[1] = '\0';

    for (int i = 0; fmtstr[i] != '\0'; i++)
    {
        if (fmtstr[i] == '%')
        {
            i++;

            switch (fmtstr[i])
            {
                // char string (%s)
                case 's': {
                    const char *argStr = va_arg(args, const char*);
                    fb_put_str(argStr);
                }
                break;

                // char (%c)
                case 'c': {
                    out[0] = va_arg(args, int); // dumbass fucking compiler wants int instead of char16 and it works so fuck me ig
                    fb_put_str(out);
                }
                break;

                // int (%d)
                case 'd': {
                    unsigned int argNum = va_arg(args, unsigned int);
                    print_number(argNum, 10, true);
                }
                break;

                // uint (%u)
                case 'u': {
                    unsigned int argNum = va_arg(args, unsigned int);
                    print_number(argNum, 10, false);
                }
                break;

                // uint as hex (%x)
                case 'x': {
                    unsigned int argNum = va_arg(args, unsigned int);
                    fb_put_str("0x");
                    print_number(argNum, 16, false);
                }
                break;

                // uint as octal (%o)
                case 'o': {
                    unsigned int argNum = va_arg(args, unsigned int);
                    fb_put_str("0o");
                    print_number(argNum, 8, false);
                }
                break;

                // uint as binary (%b)
                case 'b': {
                    unsigned int argNum = va_arg(args, unsigned int);
                    fb_put_str("0b");
                    print_number(argNum, 2, false);
                }
                break;

                // pointer address as hex (%p)
                case 'p': {
                    void* argVoidP = va_arg(args, void*);
                    fb_put_str("0x");
                    print_number((unsigned long long int)argVoidP, 16, false);
                }
                break;

                // % symbol (%%)
                case '%': {
                    out[0] = u'%';
                    fb_put_str(out);
                }
                break;

                default:
                    fb_put_str("[INVALID FORMATTING SPECIFIER]");
            }
        } else
        {
            // not a format specifer
            out[0] = fmtstr[i];
            fb_put_str(out);
        }
    }

    va_end(args);
}