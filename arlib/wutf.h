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

//The above license applies only to this file, not the entire Arlib.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef _WIN32
//This function switches the code page of all Windows ANSI and libc functions
// (for example CreateFileA() and fopen()) to UTF-8.

//Limitations:
//- IMMEDIATELY VOIDS YOUR WARRANTY
//- Possibly makes antivirus software panic
//- Will crash if this code is in a DLL that's unloaded, possibly including program shutdown.
//- Disables support for non-UTF8 code pages in MultiByteToWideChar and WideCharToMultiByte and
//    treats them as UTF-8, even if explicitly requested otherwise.
//- Console input and output remains ANSI. Consoles are very strangely implemented in Windows;
//    judging by struct CHAR_INFO in WriteConsoleOutput's arguments, the consoles don't support
//    UTF-16, but only UCS-2.
//- Did I mention it voids your warranty?
void WuTF_enable();

//Converts argc/argv to UTF-8. (Unlike the above, it uses zero ugly hacks.)
void WuTF_args(int* argc, char** * argv);

#else
//Other OSes already use UTF-8.
static inline void WuTF_enable() {}
static inline void WuTF_args(int* argc, char** * argv) {}
#endif

//This one just combines the above.
static inline void WuTF_enable_args(int* argc, char** * argv) { WuTF_enable(); WuTF_args(argc, argv); }

//Lengths are in code units, and includes the NUL terminator.
//-1 is valid for the input length, and means 'use strlen()+1'.
//Return value is number of code units emitted.
//'strict' means 'return -1 if the input is invalid'; otherwise, it emits an undefined number of U+FFFD for invalid inputs.
//If the output parameters are NULL/0, it discards the output, and only returns the required number of code units.
//In short, it roughly mirrors MultiByteToWideChar().
int WuTF_utf8_to_utf32(bool strict, const char* utf8, int utf8_len, uint32_t* utf32, int utf32_len);
int WuTF_utf32_to_utf8(bool strict, const uint32_t* utf32, int utf32_len, char* utf8, int utf8_len);

//Used internally in WuTF. It's STRONGLY RECOMMENDED to NOT use these; use the above instead.
int WuTF_utf8_to_utf16(bool strict, const char* utf8, int utf8_len, uint16_t* utf16, int utf16_len);
int WuTF_utf16_to_utf8(bool strict, const uint16_t* utf16, int utf16_len, char* utf8, int utf8_len);

#ifdef __cplusplus
}
#endif
