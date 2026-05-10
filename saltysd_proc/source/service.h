void serviceThread();
extern size_t openedFilesAmount;
extern FILE* openedFilesArray[FOPEN_MAX];
extern size_t openedDirsAmount;
extern DIR* openedDirsArray[OPEN_MAX];