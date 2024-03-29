#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//Function to check if a langford sequence exists for a given n
bool langford_exists(int n) {
    int rem = n % 4;

    if (n < 1 || rem == 1 || rem == 2) {
        return false;
    }

    return true;
}

//Function to recursively mutate array into langford pairing. The inputs should be an array of length size, where size is
//2k, where k is a positive integer giving a viable langford pairing. n is a positive integer denoting the number of
//values yet to be placed in the array. The array should begin at all zeros, or should only contain pairs of values
//that have already been placed in the array at the correct distance apart. Always returns true if called correctly.
bool recursive_langmutate(int *lang_array, int n, int size) {
    if (n == 0) {
        return true;
    } else {
        //Choosing random integer to start at (for runtime purposes)
        int start = rand() % (size - n - 1);

        for(int i = 0; i < (size - n - 1); i++){
            int curr = (start + i) % (size - n - 1);
            if (lang_array[curr] == 0 && lang_array[curr + n + 1] == 0) {
                lang_array[curr] = n;
                lang_array[curr + n + 1] = n;

                if (recursive_langmutate(lang_array, n - 1, size)) {
                    return true;
                }

                lang_array[curr] = 0;
                lang_array[curr + n + 1] = 0;
            }
        }
    }
    //This should never be reached, but is necessary to avoid a warning
    return false;
}

//code to print the sequence in the format specified in the assignment
void printseq(const int *lang_array, int size) {
    printf("[");
    for (int i = 0; i < size; i++) {
        if (i == 0) {
        printf("%d", lang_array[i]);
        }
        else {
            printf(", %d", lang_array[i]);
        }
    }
    printf("]\n");
}

//Function to create a langford pairing for a given n
int *create_langford_pairing(int n) {
    if (!langford_exists(n)) {
        return NULL;
    }
    //Create an array of size 2n integers
    int size = 2 * n;
    int *lang_array = malloc(size * sizeof(int));

    //Initialize all values to 0
    for (int i = 0; i < size; i++) {
        lang_array[i] = 0;
    }

    recursive_langmutate(lang_array, n, size);
    //print the sequence after mutating.
    return lang_array;
}

//Function to check if the sequence is a langford pairing
bool is_langford_pairing(int size, const int *in) {
    if (size % 2 != 0) {
        return false;
    }
    //Create an array to record the values that have been seen
    int n = size / 2;

    //Check if langford exists for n
    if (!langford_exists(n)) {
        return false;
    }
    //Initialize all values to 0
    int valueRecord[n];
    for (int i = 0; i < n; i++) {
        valueRecord[i] = 0;
    }

    for (int i = 0; i < size; i++) {
        //Check if value is in bounds.
        int val = in[i];
        if (val > n || val < 1) {
            return false;
        }
        //Check if value has been seen before, if it hasn't, check if the value is in the correct position ahead in the array.
        if (valueRecord[val - 1] == 0) {
            if (i + val + 1 >= size) {
                return false;
            }
            if (in[i + val + 1] != val) {
                return false;
            }
            //recording that the value has been seen once, if it has been seen twice, mark it as seen twice.
            valueRecord[val - 1] = 1;
        } else if (valueRecord[val - 1] == 1) {
            valueRecord[val - 1] = 2;
        } else {
            //If the value has been seen twice already, return false.
            return false;
        }
    }
    return true;
}


int main(int argc, char **argv) {
    //Handling for no argument, and -h cases
    if ((argc == 1) || (strcmp(argv[1], "-h") == 0)) {
        fprintf(stderr, "usage: %s [-h] -c n | num....\n", argv[0]);
        return 0;
    }

    //Handling for -c (create mode) case
    else if (strcmp(argv[1], "-c") == 0) {
        //Check if -c has correct number of arguments, of the right form
        if (argc == 2) {
        fprintf(stderr, "%s: -c option requires an argument.\n", argv[0]);
        return 1;
        }
        if (argc > 3) {
            fprintf(stderr, "%s: -c option received too many arguments.\n", argv[0]);
            return 1;
        }
        char *endptr;
        int value = strtol(argv[2], &endptr, 10);
        if (endptr && endptr[0] != '\0') {
            fprintf(stderr, "error: %s is not an integer.\n", argv[2]);
            return 1;
        }
        printf("Creating a langford pairing with n=%d\n", value);

        //Check if langford exists for n, and if it does, create the sequence
        if (!langford_exists(value)) {
            printf("No results found.\n");
        } else {
            int *lang_array = create_langford_pairing(value);
            printseq(lang_array, 2 * value);
            free(lang_array);
        }

        return 0;
    }

    //Handling for check mode case
    if ((strcmp(argv[1], "-c") != 0 && strcmp(argv[1], "-h") != 0)) {
        char *endptr;
        int lang_array[argc - 1];
        //Check if all values are integers, and store them in an array
        for (int i = 0; i < (argc - 1); i++) {
            int value = strtol(argv[i + 1], &endptr, 10);
            if (endptr && endptr[0] != '\0') {
                fprintf(stderr, "error: %s is not an integer.\n", argv[i + 1]);
                return 1;
            }
            lang_array[i] = value;
        }
        //print the sequence
        printf("Your sequence: ");
        printseq(lang_array, argc - 1);

        //Check if it is a langford pairing
        if (is_langford_pairing(argc - 1, lang_array)) {
            printf("It is a langford pairing!\n");
        }
        else {
            printf("It is NOT a langford pairing.\n");
        }
    }

    return 0;
}