#include <iostream>
#include <string.h>
#include <stdio.h>
#include "../src/tools.h"

using namespace std;
 
int main(int argc, char* argv[])
{ 
    bool* word_s = reinterpret_cast<bool*>(malloc((argc)*sizeof(bool)));
    char** keys = reinterpret_cast<char**>(malloc(2*sizeof(char*)));;
    char* key = strdup("openfirefox");
    *keys = strdup("open");
    *(keys+1) = strdup("firefoxnow");
    printf("Argument: %s\nWords   :", argv[1]);
    for (int i = 2; i < argc; i++) {
      printf(" %s", argv[i]);
    }
    scanWord(argc, argv, word_s, key);
    scanWords(argc, argv, word_s, 2, keys);
    return 0;
}

