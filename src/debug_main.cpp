#ifdef _WIN32

#ifdef _WIN32
#include <windows.h>
#else
#include "platform/Platform.h"
#endif
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    MessageBoxA(nullptr, "Hello from Debug Main", "mcpp", MB_OK);
    return 0;
}


#endif // _WIN32
