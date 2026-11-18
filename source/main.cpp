
#include <3ds.h>
#include <cstdio>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    gfxInitDefault();

    PrintConsole* topConsole = consoleInit(GFX_TOP, nullptr);
    PrintConsole* bottomConsole = consoleInit(GFX_BOTTOM, nullptr);

    consoleSelect(topConsole);

    std::printf("Laura\n");
    std::printf("Procesador de texto\n");
    std::printf("Version v1.0.\n\n");
    std::printf("Inicializacion correcta.\n");

    consoleSelect(bottomConsole);

    std::printf("Nintendo 3DS\n\n");
    std::printf("Prueba inicial del sistema.\n");
    std::printf("Presiona START para salir.\n");

    while (aptMainLoop())
    {
        hidScanInput();

        const u32 keysDown = hidKeysDown();

        if ((keysDown & KEY_START) != 0)
        {
            break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();

    return 0;
}
