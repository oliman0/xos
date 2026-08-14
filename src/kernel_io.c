#include <kernel/kernel_io.h>

static void print_number(uint64_t num, uint8_t base, bool isSigned)
{
    if (base < 2 || base > 16) return;

    const char* digits = "0123456789ABCDEF";
    char outBuffer[70];

    bool negative = false;
    if (isSigned && (int64_t)num < 0) {
        negative = true;
        num = (uint64_t)(-(int64_t)num);
    }

    int i = 0;
    do {
        outBuffer[i++] = digits[num % base];
        num /= base;
    } while (num != 0);

    if (negative) outBuffer[i++] = '-';
    outBuffer[i--] = '\0';

    for (int j = 0; j < i; j++, i--) {
        char tmp = outBuffer[j];
        outBuffer[j] = outBuffer[i];
        outBuffer[i] = tmp;
    }

    fb_put_str(outBuffer);
}

void kprintf(const char *fmtstr, ...)
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
                    int64_t argNum = va_arg(args, int64_t);
                    print_number(argNum, 10, true);
                }
                break;

                // uint (%u)
                case 'u': {
                    uint64_t argNum = va_arg(args, uint64_t);
                    print_number(argNum, 10, false);
                }
                break;

                // uint as hex (%x)
                case 'x': {
                    uint64_t argNum = va_arg(args, uint64_t);
                    fb_put_str("0x");
                    print_number(argNum, 16, false);
                }
                break;

                // uint as octal (%o)
                case 'o': {
                    uint64_t argNum = va_arg(args, uint64_t);
                    fb_put_str("0o");
                    print_number(argNum, 8, false);
                }
                break;

                // uint as binary (%b)
                case 'b': {
                    uint64_t argNum = va_arg(args, uint64_t);
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