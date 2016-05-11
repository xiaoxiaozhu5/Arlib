//The following applies only to this file, not the entire Arlib.

// MIT License
//
// Copyright (c) 2016 Alfred Agrell
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifdef _WIN32
#include "wutf.h"
#include <windows.h>

//originally ANSI_STRING and UNICODE_STRING
//renamed to avoid conflicts if some versions of windows.h includes the real ones
struct MS_ANSI_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PCHAR Buffer;
};

struct MS_UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength; // lengths are bytes (cb), not characters (cch)
	PWSTR Buffer;
};


static NTSTATUS RtlAnsiStringToUnicodeString_UTF8(struct MS_UNICODE_STRING * DestinationString,
                                                  const struct MS_ANSI_STRING * SourceString,
                                                  BOOLEAN AllocateDestinationString)
{
	if (AllocateDestinationString)
	{
		int cb = MultiByteToWideChar(CP_UTF8, 0,
		                             SourceString->Buffer, SourceString->Length,
		                             NULL, 0) * 2;
		
		if (cb >= 0x10000) return 0x80000005; // STATUS_BUFFER_OVERFLOW
		DestinationString->MaximumLength = cb*2;
		// this pointer is sent to RtlFreeUnicodeString, which ends up in RtlFreeHeap
		// this is quite clearly mapped up to RtlAllocHeap, but HeapAlloc forwards to that anyways.
		// and GetProcessHeap is verified to return the same thing as RtlFreeUnicodeString uses.
		DestinationString->Buffer = (PWSTR)HeapAlloc(GetProcessHeap(), 0, DestinationString->MaximumLength);
	}
	
	int ret = MultiByteToWideChar(CP_UTF8, 0,
	                              SourceString->Buffer, SourceString->Length,
	                              DestinationString->Buffer, DestinationString->MaximumLength/2);
	if (ret) return 0x00000000; // STATUS_SUCCESS
	else return 0x80000005; // STATUS_BUFFER_OVERFLOW
}

static NTSTATUS RtlUnicodeStringToAnsiString_UTF8(struct MS_ANSI_STRING * DestinationString,
                                                  const struct MS_UNICODE_STRING * SourceString,
                                                  BOOLEAN AllocateDestinationString)
{
	if (AllocateDestinationString)
	{
		int cb = WideCharToMultiByte(CP_UTF8, 0,
		                             SourceString->Buffer, SourceString->Length,
		                             NULL, 0,
		                             NULL, NULL);
		
		if (cb >= 0x10000) return 0x80000005; // STATUS_BUFFER_OVERFLOW
		DestinationString->MaximumLength = cb*2;
		DestinationString->Buffer = (PCHAR)HeapAlloc(GetProcessHeap(), 0, DestinationString->MaximumLength);
	}
	
	int ret = WideCharToMultiByte(CP_UTF8, 0,
	                              SourceString->Buffer, SourceString->Length*2,
	                              DestinationString->Buffer, DestinationString->MaximumLength,
	                              NULL, NULL);
	if (ret) return 0x00000000; // STATUS_SUCCESS
	else return 0x80000005; // STATUS_BUFFER_OVERFLOW
}


//https://sourceforge.net/p/predef/wiki/Architectures/
#if defined(_M_IX86) || defined(__i386__)
static void RedirectFunction_machine(LPBYTE victim, LPBYTE replacement)
{
	LONG_PTR ptrdiff = replacement-victim-5;
	
	victim[0] = 0xE9; // jmp (offset from next instruction)
	*(LONG_PTR*)(victim+1) = ptrdiff;
}

#elif defined(_M_X64) || defined(__x86_64__)
static void RedirectFunction_machine(LPBYTE victim, LPBYTE replacement)
{
	// this destroys %rax, but that register is caller-saved (and the return value).
	// https://msdn.microsoft.com/en-us/library/9z1stfyw.aspx
	victim[0] = 0x48; victim[1] = 0xB8;   // mov %rax, <64 bit constant>
	*(LONG_PTR*)(victim+2) = replacement;
	victim[10] = 0xFF; victim[11] = 0xE0; // jmp %rax
}

#else
#error Not supported
#endif


static void RedirectFunction(FARPROC dst, FARPROC src)
{
	DWORD prot;
	// it's bad to have W+X on the same page, but I don't want to remove X from ntdll.dll.
	// if I hit NtProtectVirtualMemory, I won't be able to fix it
	VirtualProtect((void*)dst, 64, PAGE_EXECUTE_READWRITE, &prot);
	RedirectFunction_machine((LPBYTE)dst, (LPBYTE)src);
	VirtualProtect((void*)dst, 64, prot, &prot);
}

void WUTfEnable()
{
	HMODULE ntdll = GetModuleHandle("ntdll.dll");
	RedirectFunction(GetProcAddress(ntdll, "RtlAnsiStringToUnicodeString"), (FARPROC)RtlAnsiStringToUnicodeString_UTF8);
	RedirectFunction(GetProcAddress(ntdll, "RtlUnicodeStringToAnsiString"), (FARPROC)RtlUnicodeStringToAnsiString_UTF8);
}


void WUTfArgs(int* argc, char** * argv)
{
	
}

#endif
