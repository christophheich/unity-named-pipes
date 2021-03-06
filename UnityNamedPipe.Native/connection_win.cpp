#include "connection.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class NamedPipeClientWindows : public BaseNamedPipeClient
{
public:
	NamedPipeClientWindows()
	{
		handle = INVALID_HANDLE_VALUE;
		isOpened = false;
	}

	~NamedPipeClientWindows() = default;
	
	bool isConnected() override
	{
		return handle != NULL && isOpened && handle != INVALID_HANDLE_VALUE;
	}

	int readFrame(unsigned char* buffer, int length) override
	{
		/*
		if (!PeekNamedPipe(handle)) return false;
		Read(handle, buffer, 0, length);
		*/

		if (length == 0) { return -1; }
		if (!isConnected()) { return -2; }

		//Prepare how many bytes we have read
		DWORD bytesAvailable = 0;

		//Attempt to peek at the available pipe
		if (::PeekNamedPipe(handle, nullptr, 0, nullptr, &bytesAvailable, nullptr))
		{
			//Check if we have bytes available to read
			if (bytesAvailable > 0)
			{
				//Read the bytes. 
				//TODO: Make the Bytes Read appart of the output
				DWORD bytesToRead = (DWORD)length;
				DWORD bytesRead = 0;

				//Attempt to read the bytes
				if (::ReadFile(handle, buffer, bytesToRead, &bytesRead, nullptr) == TRUE)
				{
					return bytesRead;
				}
				else
				{
					//We failed to read anything, close the pipe (broken)
					close();
					return -3;
				}
			}
			else
			{
				//We failed to read as there were no bytes available
				return 0;
			}
		}
		else
		{
			//We have failed to peek. The pipe is probably broken
			close();
			return -5;
		}
	}

	int writeFrame(unsigned char* buffer, int length) override
	{
		if (length == 0) return 0;
		if (!isConnected()) return -1;

		//Prepare the size
		const DWORD bytesLength = (DWORD)length;
		DWORD bytesWritten = 0;

		//Write and return its success
		bool success = ::WriteFile(handle, buffer, bytesLength, &bytesWritten, nullptr);
		if (!success) return -2;
		return bytesWritten;
	}

	int open(char* pipename) override
	{
		//Creates a connection to the pipe
		handle = ::CreateFileA(pipename, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (handle != INVALID_HANDLE_VALUE)
		{
			isOpened = true;
			return 0;
		}

		//We have a error, better find out why.
		auto lasterr = GetLastError();

		//Pipe wasnt found
		if (lasterr == ERROR_FILE_NOT_FOUND)
			return lasterr;

		//The path wasnt found?
		if (lasterr == ERROR_PATH_NOT_FOUND)
			return lasterr;

		//Pipe is just busy
		if (lasterr == ERROR_PIPE_BUSY)
		{
			if (!WaitNamedPipeA(pipename, 1000))
			{
				//Failed to open, terminate
				return lasterr;
			}
			else
			{
				//Attempt to open it again
				return open(pipename);
			}
		}

		//Some other magically error occured
		return lasterr;
	}

	void close(void) override
	{
		::CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
		isOpened = false;
	}

private:
	HANDLE handle;
	bool isOpened;
};

BaseNamedPipeClient* BaseNamedPipeClient::create() { return (BaseNamedPipeClient*)(new NamedPipeClientWindows()); }
