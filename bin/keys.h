bool scanWord(int n, char* txt_word[], bool *&status, char *&key){
    std::string remainder;
    printf("\nFinding keywords:");
    for (int i = 2; i < n; i++) {
        std::string words;
        if (!*(status+i)) {
            printf("\n\n(Checking)");
            for (int j = 0; j < n-i; j++) {
                words += txt_word[i+j];
                printf("\n%s", words.c_str());
                if (strcmp(key, words.c_str())==0){
                  printf("\n\nExclude:");
                  for (int k=i; k <= i+j; k++) {
                    *(status+k) = true; 
                    printf(" %s(found)", txt_word[k]);
                  }
                  return true;
                }
            }
            printf("\n(Done)");  
        }
    } 
    return false;
}

unsigned int scanWords(int n, char* txt_word[], bool *&status, int x, char **&keys){
    printf("\n\nFinding keywords:\n");
    for (int i = 2; i < n; i++) {
        std::string words;
        if (!*(status+i)) {
            printf("\n(Checking)");
            for (int j = 0; j < n-i; j++) {
                words += txt_word[i+j];
                printf("\n%s", words.c_str());
                for (int k = 0; k < sizeof(keys)/sizeof(char**); k++) {
                    if (strcmp(*(keys+k), words.c_str())==0){
                      printf("\n\nExclude:");
                      for (int l=i; l <= i+j; l++) {
                        *(status+l) = true; 
                        printf(" %s(found)\n", txt_word[l]);
                      }
                      return k;
                    }
                }
            }
            printf("\n(Done)\n");  
        }
    } 
    return 0;
}
