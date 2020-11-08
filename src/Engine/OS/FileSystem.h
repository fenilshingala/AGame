#pragma once

typedef void* FileHandle;

void InitFileSystem(void* _platformData);

// Creates a directory named _directoryName
// Logs if either already exist or path not found
void CreateDirecroty(const char* _directoryName);

// Opens a file and returns a file pointer
// _filename: Name of the file to open
// _mode: File access mode
FileHandle OpenFile(const char* _filename, const char* _mode);

// Closes a file
// _handle: Handle to the file which is to be closed
void CloseFile(FileHandle _handle);

// Reads a line from file
// _handle: File handle from which line is to be read
// _ppBuffer: character buffer to write the line into; must be pre-allocated
void FileReadLine(FileHandle _handle, char** _ppBuffer);

// Writes a line to the file
// _handle: File handle to which line is to be written
// buffer: character buffer to read the line from
void FileWriteLine(FileHandle _handle, const char* buffer);

// Returns file pointer position
long FileTell(FileHandle _handle);

// Moves file pointer to _origin + _offset position
void FileSeek(FileHandle _handle, long _offset, int _origin);