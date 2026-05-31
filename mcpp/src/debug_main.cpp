#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    MessageBoxA(nullptr, "Hello from Debug Main", "mcpp", MB_OK);
    return 0;
}
