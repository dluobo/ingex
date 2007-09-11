#include <stdio.h>
#include <stdlib.h>


int main(int argc, const char** argv)
{
    int isHTML;
    
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s (--html|--binary) <filename>\n", argv[0]);
        return 1;
    }
    
    if (strcmp("--html", argv[1]) == 0)
    {
        isHTML = 1;
    }
    else if (strcmp("--binary", argv[1]) == 0)
    {
        isHTML = 0;
    }
    else
    {
        fprintf(stderr, "Usage: %s (--html|--binary) <filename>\n", argv[0]);
        return 1;
    }
    
    
    FILE* file;
    if ((file = fopen(argv[2], "rb")) == NULL)
    {
        perror("fopen");
        return 1;
    }
    
    if (isHTML)
    {
        printf("const char* g_xxxxxx = \n");
        printf("    \"");
    }
    else
    {
        printf("const unsigned char* g_xxxxxx = \n");
        printf("{\n");
    }

    int lineWidth = 80;
    if (!isHTML)
    {
        lineWidth = 20;
    }
    
    int size = 0;
    unsigned char buffer[80];
    int numRead = lineWidth;
    int i;
    int first = 1;
    while (numRead == lineWidth)
    {
        numRead = fread(buffer, 1, lineWidth, file);
        if (numRead > 0)
        {
            if (isHTML)
            {
                for (i = 0; i < numRead; i++)
                {
                    if (buffer[i] == '\"')
                    {
                        printf("\\%c", buffer[i]);
                    }
                    else if (buffer[i] == '\\')
                    {
                        printf("\\%c", buffer[i]);
                    }
                    else if (buffer[i] == '\n')
                    {
                        printf("\\n\"\n    \"", buffer[i]);
                    }
                    else
                    {
                        printf("%c", buffer[i]);
                    }
                }
            }
            else
            {
                size += numRead;
                if (!first)
                {
                    printf(",\n    ");
                }
                else
                {
                    printf("    ");
                    first = 0;
                }
                for (i = 0; i < numRead; i++)
                {
                    if (i != 0)
                    {
                        printf(",");
                    }
                    printf("0x%02x", buffer[i]);
                }
            }
        }
    }
    
    
    if (isHTML)
    {
        printf("\";\n");
    }
    else
    {
        printf("\n};\n");
    }
    
    fclose(file);
    
    return 0;
}
