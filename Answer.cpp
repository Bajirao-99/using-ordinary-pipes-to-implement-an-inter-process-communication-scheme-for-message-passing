#include <bits/stdc++.h>
#include <dirent.h>
#include <unistd.h>
#define NUMBER_OF_FILES 1000
#define FILENAME_LENGTH 256
#define FILE_LENGTH 1024


// Pipes
// One way communication from 1 process to another

// Directories need to be on the same level and where the program is run

using namespace std;

// This function validates the success of pipe creation.
int validateSuccess(int fd1[2], int fd2[2])
{

    // -1 = Failure of pipe creation
    if (pipe(fd1) == -1 || pipe(fd2) == -1)
    {
        perror("Error opening a pipe");
        exit(EXIT_FAILURE);
    }

    return 0;
}

// This function returns the name of the current working directory
char *cwd_name()
{
    char buffer[PATH_MAX];
    char *cwd;
    if (getcwd(buffer, sizeof(buffer)) != NULL)
    {
        cwd = buffer;
        return cwd;
    }
    else
    {
        perror("getcwd() error");
        exit(EXIT_FAILURE);
    }
}

// This function converts a string to a character pointer
char *string_to_char_ptr(string str)
{
    char *directory_name_char = new char[str.length() + 1];
    strcpy(directory_name_char, str.c_str());
    return directory_name_char;
}

// This function creates files with specified contents in a directory
void createDirectory(char files[NUMBER_OF_FILES][FILENAME_LENGTH], char contents[NUMBER_OF_FILES][FILE_LENGTH], int file_count, string directory_name)
{
    int FILE_PATH_LENGTH = FILE_LENGTH + directory_name.length() + 2;

    char *cwd = cwd_name();

    char *directory_name_char = string_to_char_ptr(directory_name);

    for (int i = 0; i < file_count; i++)
    {
        char filepath[FILE_PATH_LENGTH];
        snprintf(filepath, FILE_PATH_LENGTH, "%s/%s/%s", cwd, directory_name_char, files[i]);

        // Create files in child 2
        FILE *file = fopen(filepath, "w");
        if (file == NULL)
        {
            perror("Error creating file");
            return;
        }

        // Write file contents
        fprintf(file, "%s", contents[i]);

        fclose(file);
    }
}

// This function creates files with specified contents in a directory
void copyDirectory(string directory_name, int fd[2])
{

    char files[NUMBER_OF_FILES][FILENAME_LENGTH];
    char contents[NUMBER_OF_FILES][FILE_LENGTH];
    int file_count = 0;

    char *directory_name_char = new char[directory_name.length() + 1];
    strcpy(directory_name_char, directory_name.c_str());

    // Reading from directory stream
    DIR *directory_stream;
    dirent *entry;
    directory_stream = opendir(directory_name_char);
    int file_path_length = FILENAME_LENGTH + directory_name.length() + 2;

    if (directory_stream)
    {
        while ((entry = readdir(directory_stream)))
        {

            if (entry->d_type == DT_REG)
            { 
                char file_path[file_path_length];
                snprintf(file_path, sizeof(file_path), "%s/%s", directory_name_char, entry->d_name);

                FILE *file = fopen(file_path, "r");

                if (!file)
                {
                    perror("Error in opening file to read");
                    exit(EXIT_FAILURE);
                }
                strncpy(files[file_count], entry->d_name, FILENAME_LENGTH);
                int position = fread(contents[file_count], 1, FILE_LENGTH, file);

                contents[file_count][position] = '\0';
                fclose(file);
                file_count++;
            }
        }
        closedir(directory_stream);
    }
    else
    {
        perror("Failure opening directory stream");
        exit(EXIT_FAILURE);
    }

    // copy directory contents to pipe 1
    for (int i = 0; i < file_count; i++)
    {
        write(fd[1], files[i], strlen(files[i]) + 1);
        write(fd[1], "\n", 1);
        write(fd[1], contents[i], strlen(contents[i]) + 1);
        write(fd[1], "\n", 1);
    }
}

// This function reads file names and file contents from a pipe
void readFromPipe(char (&files)[NUMBER_OF_FILES][FILENAME_LENGTH], char (&contents)[NUMBER_OF_FILES][FILE_LENGTH], int &file_count, int fd[])
{

    int bytes_read;
    bool is_file_name = true;
    char temp;
    char *buffer = new char[FILE_LENGTH];
    int current_pos = 0;
    while ((bytes_read = read(fd[0], &temp, 1)) > 0)
    {
        if (temp == '\n')
        {
            buffer[current_pos++] = '\0';
            if (is_file_name)
            {
                strncpy(files[file_count], buffer, current_pos);
            }
            else
            {
                strncpy(contents[file_count], buffer, current_pos);
                file_count++;
            }
            is_file_name = !is_file_name;
            current_pos = 0;
            delete buffer;
            buffer = new char[FILE_LENGTH];
        }
        else
        {
            buffer[current_pos++] = temp;
        }
    }
}

// This function copies the contents from d1 to d2
void copyContentsFromChild1(int fd1[2], int fd2[2], pid_t pid1, string directory_name)
{

    if (pid1 == -1)
    {
        perror("Problem forking child1");
        exit(EXIT_FAILURE);
    }

    // Parent process
    if (pid1 != 0)
        return;

    // closing reading/writing descriptors
    close(fd1[0]);
    close(fd2[1]);

    // Step 1: Read files in current directory and write to pipe1
    copyDirectory(directory_name, fd1);
    close(fd1[1]);

    // Step 2: Read files from pipe2 and create files
    char files[NUMBER_OF_FILES][FILENAME_LENGTH];
    char contents[NUMBER_OF_FILES][FILE_LENGTH];
    int file_count = 0;

    readFromPipe(files, contents, file_count, fd2);

    for (int i = 0; i < file_count; i++)
    {
        cout << files[i] << " ";
        cout << contents[i] << endl;
    }

    createDirectory(files, contents, file_count, directory_name);

    close(fd2[0]);
}

// This function copies the contents from d2 to d1
void copyContentsFromChild2(int fd1[2], int fd2[2], pid_t pid2, string directory_name)
{

    if (pid2 == -1)
    {
        perror("Problem forking child1");
        exit(EXIT_FAILURE);
    }

    // Parent process
    if (pid2 != 0)
        return;

    // closing reading descriptors, cause only reading from fd1
    // close(fd1[0]);
    close(fd1[1]);
    close(fd2[0]);

    // Step 1: Read files in current directory and write to pipe2
    copyDirectory(directory_name, fd2);
    close(fd2[1]);

    // Step 2: Read data from pipe1 and create directories
    char files[NUMBER_OF_FILES][FILENAME_LENGTH];
    char contents[NUMBER_OF_FILES][FILE_LENGTH];
    int file_count = 0;

    readFromPipe(files, contents, file_count, fd1);

    for (int i = 0; i < file_count; i++)
    {
        cout << files[i] << " ";
        cout << contents[i] << endl;
    }

    createDirectory(files, contents, file_count, directory_name);

    // Close read end of pipe1
    close(fd1[0]);
}


int main()
{
    // fd1[0] - read end, fd1[1] - write end
    int fd1[2], fd2[2];

    validateSuccess(fd1, fd2);

    pid_t pid1, pid2;

    pid1 = fork();

    copyContentsFromChild1(fd1, fd2, pid1, "d1");

    if (pid1 > 0)
    {

        pid2 = fork();
    }

    copyContentsFromChild2(fd1, fd2, pid2, "d2");

    return 0;
}