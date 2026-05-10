
struct FileId {
    int id;
    FILE* file;
};

struct DirId {
    int id;
    DIR* dir;
};

void serviceThread();
extern size_t openedFilesAmount;
extern struct FileId openedFilesArray[FOPEN_MAX-1];
extern size_t openedDirsAmount;
extern struct DirId openedDirsArray[OPEN_MAX-1];