#include <windows.h>
#include <stdio.h>

#define report_error(errorCode, name, message) \
{ \
	wchar_t* msg = get_win_error_message( errorCode ); \
	if (msg) \
	{ \
		fwprintf(stderr, L"wget: %s: %s", name, msg); \
		::LocalFree( msg ); \
	} \
	else \
	{ \
		fwprintf(stderr, L"wget: %s: " message, name); \
	} \
}

bool ignoreCtrl = false;

BOOL WINAPI ctrl_handler(DWORD dwCtrlType)
{
	if (ignoreCtrl)
		return TRUE;
	return FALSE;
}

wchar_t* get_win_error_message(DWORD errorCode)
{
	wchar_t* rc = 0;
	
	::FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		errorCode,
		LANG_SYSTEM_DEFAULT,
		(wchar_t*)(&rc),
		0,
		NULL
	);
	
	return rc;
}

int wmain(int argc , wchar_t* argv[])
{
	::SetConsoleCtrlHandler(ctrl_handler, TRUE);

	wchar_t** outFiles = 0;
	size_t outFilesCount = 0;
	wchar_t* inFile = 0;
	DWORD attempts = 1;
	DWORD pause = 200;
	bool enableStdOut = true;
	bool help = false;
	
	char state = 0;
	for (int i = 1; i < argc; i++)
	{
		if (!argv[i] || 0 == argv[i][0]) continue;

		if (argv[i][0] == L'-' || argv[i][0] == L'/')
		{
			if (argv[i][0] == L'-' && argv[i][1] == L'-')
			{
				if (0 == _wcsicmp(&argv[i][2], L"append"))
				{
					state = 1;
					continue;
				}
				else if (0 == _wcsicmp(&argv[i][2], L"ignore-interrupts"))
				{
					ignoreCtrl = true;
					continue;
				}
				else if (0 == _wcsicmp(&argv[i][2], L"no-output"))
				{
					enableStdOut = false;
					continue;
				}
				else if (0 == _wcsicmp(&argv[i][2], L"file"))
				{
					state = 2;
					continue;
				}
				else if (0 == _wcsicmp(&argv[i][2], L"attempts"))
				{
					state = 3;
					continue;
				}
				else if (0 == _wcsicmp(&argv[i][2], L"pause"))
				{
					state = 4;
					continue;
				}
				else if (0 == _wcsicmp(&argv[i][2], L"help"))
				{
					help = true;
					continue;
				}
				else if (0 == _wcsicmp(&argv[i][2], L"version"))
				{
					_putws(L"wtee utility, version 1.0");
					return 777;
				}
			}
			else if (argv[i][1] == L'a' || argv[i][1] == L'A')
			{
				state = 1;
				continue;
			}
			else if (argv[i][1] == L'i' || argv[i][1] == L'I')
			{
				ignoreCtrl = true;
				continue;
			}
			else if (argv[i][1] == L'o' || argv[i][1] == L'O')
			{
				enableStdOut = false;
				continue;
			}
			else if (argv[i][1] == L'f' || argv[i][1] == L'F')
			{
				state = 2;
				continue;
			}
			else if (argv[i][1] == L'n' || argv[i][1] == L'N')
			{
				state = 3;
				continue;
			}
			else if (argv[i][1] == L'p' || argv[i][1] == L'P')
			{
				state = 4;
				continue;
			}
			else if (argv[i][1] == L'h' || argv[i][1] == L'H' || argv[i][1] == L'?')
			{
				help = true;
				continue;
			}
		}
		
		if (1 == state)
		{
			wchar_t** ptr = outFiles;
			outFiles = new wchar_t*[outFilesCount + 1];
			if (!outFiles)
			{
				if (ptr)
					delete[] ptr;
				return -666;
			}
			if (ptr)
			{
				memcpy_s(outFiles, sizeof(wchar_t*) * outFilesCount, ptr, sizeof(wchar_t*) * outFilesCount);
				delete[] ptr;
			}
			outFiles[outFilesCount++] = argv[i];
		}
		else if (2 == state)
		{
			if (!inFile)
				inFile = argv[i];
			state = 0;
		}
		else if (3 == state)
		{
			__int64 v = _wtoi64( argv[i] );
			if (v >= 0 && v <= UINT_MAX)
				attempts = (DWORD)v;
			state = 0;
		}
		else if (4 == state)
		{
			__int64 v = _wtoi64( argv[i] );
			if (v >= 0 && v <= UINT_MAX)
				pause = (DWORD)v;
			state = 0;
		}
	}

	if (help)
	{
		_putws(L"\n\
Usage: [OPTION]... [FILE]...\n\
\n\
  -a, --append              append to the given FILEs, do not overwrite\n\
  -i, --ignore-interrupts   ignore interrupt signals\n\
  -o, --no-output           no output to stdout\n\
  -f, --file                get input from given FILE rather then stdin\n\
  -n, --attempts            number of attempts to open given FILEs\n\
                            default value is 1 attempt\n\
  -p, --pause               pause between attempts to open given FILEs, ms\n\
                            default value is 200 milliseconds\n\
  -?, --help                display this help and exit\n\
      --version             output version information and exit\n\
"		
		);
		return 777;
	}

	int rc = 0;
	HANDLE hInput = INVALID_HANDLE_VALUE;
	HANDLE* hOutputs = 0;
	DWORD* outputErrors = 0;
	size_t outCount = outFilesCount + 1;
	
	if (inFile)
	{
		DWORD count = attempts;
		do 
		{
			hInput = ::CreateFileW(inFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
			if (INVALID_HANDLE_VALUE != hInput)
				break;
				
			if (pause > 0)
				::Sleep(pause);
		}
		while (count-- > 0);
	}
	else
	{
		hInput = ::GetStdHandle(STD_INPUT_HANDLE);
		
		DWORD mode;
		if (::GetConsoleMode(hInput, &mode))
		{
			fwprintf(stderr, L"wget: stdin: keyboard input not supported");
			rc = -1;
			goto E_x_i_t;
		}
	}
	
	if (INVALID_HANDLE_VALUE == hInput)
	{
		report_error(::GetLastError(), ((inFile) ? inFile : L"stdin"), L"open failed");
		rc = -1;
		goto E_x_i_t;
	}
	
	if (outCount > 0)
	{
		hOutputs = new HANDLE[outCount];
		outputErrors = new DWORD[outCount];
		if (!hOutputs || !outputErrors)
		{
			rc = -666;
			goto E_x_i_t;
		}
		
		for (size_t i = 0; i < outCount; i++)
		{
			hOutputs[i] = INVALID_HANDLE_VALUE;
			outputErrors[i] = ERROR_SUCCESS;
		}
		
		if (enableStdOut)
		{
			hOutputs[0] = ::GetStdHandle(STD_OUTPUT_HANDLE);
			if (INVALID_HANDLE_VALUE == hOutputs[0])
			{
				report_error(::GetLastError(), L"stdout", L"open failed");
			}
		}

		DWORD count = attempts;
		do 
		{
			bool success = true;
			for (size_t i = 0; i < outFilesCount; i++)
			{
				if (INVALID_HANDLE_VALUE == hOutputs[i + 1])
				{
					hOutputs[i + 1] = ::CreateFile(outFiles[i], GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
					if (INVALID_HANDLE_VALUE == hOutputs[i + 1])
					{
						outputErrors[i + 1] = ::GetLastError();
						success = false;
					}
					else if (INVALID_SET_FILE_POINTER == ::SetFilePointer(hOutputs[i + 1], 0, NULL, FILE_END))
					{
						outputErrors[i + 1] = ::GetLastError();
						success = false;
						
						::CloseHandle(hOutputs[i + 1]);
						hOutputs[i + 1] = INVALID_HANDLE_VALUE;
					}
				}
			}
		
			if (success)
				break;
		
			if (pause > 0)
				::Sleep(pause);
		
		} 
		while (count-- > 0);

		for (size_t i = 1; i < outCount; i++)
		{
			if (INVALID_HANDLE_VALUE == hOutputs[i])
			{
				report_error(outputErrors[i], outFiles[i - 1], L"open failed");
			}
		}
	}
	
	char buffer[262144];
	
	while (true)
	{
		DWORD read = 0; 
	
		if (!::ReadFile(hInput, buffer, sizeof(buffer), &read, NULL))
		{
			DWORD errorCode = ::GetLastError();
			if (ERROR_MORE_DATA != errorCode)
			{
				if (ERROR_BROKEN_PIPE != errorCode && ERROR_HANDLE_EOF != errorCode)
				{
					report_error(errorCode, ((inFile) ? inFile : L"stdin"), L"read failed");
					rc = -2;
				}
				break;
			}
		}
		
		if (read <= 0)
		{
			break;
		}
		
		if (hOutputs)
		{
			size_t goodCount = 0;
			for (size_t i = 0; i < outCount; i++)
			{
				if (0 != hOutputs[i] && INVALID_HANDLE_VALUE != hOutputs[i])
				{
					DWORD written = 0;
					if (!::WriteFile(hOutputs[i], buffer, read, &written, NULL))
					{
						report_error(::GetLastError(), ((i > 0) ? outFiles[i - 1] : L"stdout"), L"write failed");
					}
					else if (written != read)
					{
						fwprintf(stderr, L"wget: %s: only %u of %u bytes written", ((i > 0) ? outFiles[i - 1] : L"stdout"), written, read);
					}
					else
					{
						goodCount++;
					}
				}
				else
				{
					goodCount++;
				}
			}
			
			if (0 == goodCount)
			{
				rc = -3;
				break;
			}
		}
	}
	
E_x_i_t:

	if (INVALID_HANDLE_VALUE != hInput)
		::CloseHandle(hInput);
		
	if (hOutputs)
	{
		for (size_t i = 0; i < outCount; i++)
			if (0 != hOutputs[i] && INVALID_HANDLE_VALUE != hOutputs[i])
				::CloseHandle(hOutputs[i]);	
		delete[] hOutputs;
	}
	
	if (outputErrors)
		delete[] outputErrors;
	
	return rc;
}