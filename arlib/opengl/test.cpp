#if 0
g++ -std=c++11 -O3 -DARLIB_D3DTEST -DARLIB_OPENGL -DARGUI_WINDOWS -DAROPENGL_D3DSYNC *.cpp ..\debug.cpp ..\gui\*.cpp ..\malloc.cpp ..\file-win32.cpp -lgdi32 -lcomctl32 -lcomdlg32 -o test.exe && test.exe && del test.exe
exit
#endif

#ifdef ARLIB_D3DTEST
#include <time.h>
#include <algorithm>
#include <math.h>
#include "../arlib.h"

#include<windows.h>

void math(int* data, int ndata, float& avg, float& stddev)
{
	if (!ndata)
	{
		avg=0;
		stddev=0;
		return;
	}
	
	float sum = 0;
	for (int i=0;i<ndata;i++) sum+=data[i];
	avg = sum/ndata;
	
	float stddevtmp = 0;
	for (int i=0;i<ndata;i++) stddevtmp += (data[i]-avg) * (data[i]-avg);
	stddev = sqrt(stddevtmp / ndata);
}

void process(bool d3d)
{
	widget_viewport* port = widget_create_viewport(300, 200);
	window* wnd = window_create(port);
	wnd->set_visible(true);
	
	uint32_t flags = aropengl::t_ver_3_3 | aropengl::t_debug_context;
	if (d3d) flags |= aropengl::t_direct3d_vsync;
	//flags |= aropengl::t_depth_buffer;
	aropengl gl(port->get_window_handle(), flags);
	if (!gl) return;
	
	gl.enableDefaultDebugger();
	gl.swapInterval(1);
	
	bool black = false;
	
	//int width = 640;
	
#define SKIP 20
#define FRAMES 1800
	int times[SKIP+FRAMES];
	
LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
LARGE_INTEGER Frequency;

QueryPerformanceFrequency(&Frequency); 
QueryPerformanceCounter(&StartingTime);
	
	for (int i=0;i<SKIP+FRAMES;i++)
	{
		window_run_iter();
		
		black = !black;
		
		gl.Viewport(0, 0, 640, 480);
		gl.ClearColor(black, 1-black, 0, 1.0);
		gl.Clear(GL_COLOR_BUFFER_BIT);
		
		gl.swapBuffers();
		window_run_iter();
		
		//width++;
		//if(width>1000)width=500;
		//port->resize(width, 480);
		//gl.notifyResize(width, 480);
		
		
QueryPerformanceCounter(&EndingTime);
ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
StartingTime = EndingTime;

ElapsedMicroseconds.QuadPart *= 1000000;
ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
times[i] = ElapsedMicroseconds.QuadPart;
	}
	
	delete wnd;
	
	float avg;
	float stddev;
	math(times+SKIP, FRAMES, avg, stddev);
	
	printf("d3d=%i avg=%f stddev=%f ", d3d, avg, stddev);
	std::sort(times+SKIP, times+SKIP+FRAMES);
	printf("min=%i,%i ", times[SKIP+0], times[SKIP+1]);
	printf("max=%i,%i,%i,%i,%i\n", times[SKIP+FRAMES-1], times[SKIP+FRAMES-2], times[SKIP+FRAMES-3], times[SKIP+FRAMES-4], times[SKIP+FRAMES-5]);
}

int main(int argc, char * argv[])
{
	window_init(&argc, &argv);
	for (int i=0;i<5;i++) // results are very jittery, even if averaging over half a minute and discarding warmup time
	{
		process(false);
		process(true);
	}
	//sample results (Windows 7, Intel HD Graphics 4400):
	//TODO: regenerate on next reboot
}
#endif