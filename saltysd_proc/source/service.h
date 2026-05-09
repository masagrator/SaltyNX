#define arraySize 16

void serviceThread();
extern size_t openedFilesAmount;
extern FILE* openedFilesArray[arraySize];
extern size_t openedDirsAmount;
extern DIR* openedDirsArray[arraySize];