#include <stdio.h>
#include <stdlib.h>

#define MAX_PATH 256

int main(int argc, char* argv[])
{
    unsigned long cpu, reg;
    FILE* fin;
    char device[MAX_PATH];
    unsigned long data;

    if (argc < 3 || argc > 4)
    return fprintf(stderr, "Usage:\n\t\t cr cpu reg [value]\n"), 1;

    if (sscanf(argv[1], "cpu%u", &cpu) != 1)
    return fprintf(stderr, "Invalid value '%s' for cpu\n", argv[1]), 2;

    if (sscanf(argv[2], "cr%u", &reg) != 1 && sscanf(argv[2], "xcr%u", &reg) != 1)
    return fprintf(stderr, "Invalid value '%s' for reg\n", argv[2]), 3;

    if (argc == 4 && sscanf(argv[3], "%lu", &data) != 1)
    return fprintf(stderr, "Invalid numeric value '%s'\n", argv[3]), 6;

    snprintf(device, MAX_PATH, "/dev/crs/cpu%u/%s", cpu, argv[2]);

    fin = fopen(device, argc == 4 ? "wb" : "rb");

    if (!fin)
      return fprintf(stderr, "Cannot open device %s\n", device), 4;

    if (argc == 4)
    {
       if (fwrite(&data, sizeof(data), 1, fin) != 1)
    return fprintf(stderr, "Cannot write device %s (%d)\n", device, ferror(fin)), 5;     
    }
    else
    {
      if (fread(&data, sizeof(data), 1, fin) != 1)
    return fprintf(stderr, "Cannot read device %s (%d)\n", device, ferror(fin)), 7;

      printf("%016x\n", data);
    }



    fclose(fin);
    return 0;

}
