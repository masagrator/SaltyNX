void serviceThread();
extern size_t openedFilesAmount;
extern FILE* openedFilesArray[FOPEN_MAX-1];
extern size_t openedDirsAmount;
extern DIR* openedDirsArray[OPEN_MAX-1];