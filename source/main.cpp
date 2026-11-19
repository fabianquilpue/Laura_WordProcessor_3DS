
#include <3ds.h>
#include <citro2d.h>

#include "editor.hpp"

#include <cmath>
#include <cstdint>

namespace
{
    constexpr float TOP_WIDTH = 400.0f;
    constexpr float BOTTOM_WIDTH = 320.0f;
    constexpr float SCREEN_HEIGHT = 240.0f;

    constexpr u32 COLOR_CREMA =
        C2D_Color32(247, 245, 238, 255);

    constexpr u32 COLOR_CARBON =
        C2D_Color32(30, 30, 30, 255);

    constexpr u32 COLOR_MUSGO =
        C2D_Color32(78, 122, 101, 255);

    constexpr u32 COLOR_ROJO =
        C2D_Color32(170, 55, 55, 255);

    constexpr u64 FADE_DURATION = 900;
    constexpr u64 BREATH_DURATION = 3600;

    C3D_RenderTarget* topScreen = nullptr;
    C3D_RenderTarget* bottomScreen = nullptr;

    C2D_SpriteSheet splashSheet = nullptr;
    C2D_TextBuf textBuffer = nullptr;

    C2D_Text titleText;
    C2D_Text spanishText;
    C2D_Text englishText;
    C2D_Text instructionLine1;
    C2D_Text instructionLine2;

    u32 interpolateColor(
        u8 r1, u8 g1, u8 b1,
        u8 r2, u8 g2, u8 b2,
        float amount)
    {
        if (amount < 0.0f) amount = 0.0f;
        if (amount > 1.0f) amount = 1.0f;

        const u8 r = static_cast<u8>(r1 + (r2 - r1) * amount);
        const u8 g = static_cast<u8>(g1 + (g2 - g1) * amount);
        const u8 b = static_cast<u8>(b1 + (b2 - b1) * amount);

        return C2D_Color32(r, g, b, 255);
    }

    void prepareText(C2D_Text* text, const char* content)
    {
        C2D_TextParse(text, textBuffer, content);
        C2D_TextOptimize(text);
    }

    void drawCenteredText(
        const C2D_Text* text,
        float screenWidth,
        float y,
        float scale,
        u32 color)
    {
        float width = 0.0f;

        C2D_TextGetDimensions(
            text, scale, scale, &width, nullptr
        );

        C2D_DrawText(
            text,
            C2D_WithColor,
            (screenWidth - width) / 2.0f,
            y,
            0.0f,
            scale,
            scale,
            color
        );
    }

    void drawFadeOverlay(float screenWidth, u8 alpha)
    {
        C2D_DrawRectSolid(
            0.0f, 0.0f, 1.0f,
            screenWidth, SCREEN_HEIGHT,
            C2D_Color32(0, 0, 0, alpha)
        );
    }

    void drawLogo()
    {
        if (splashSheet == nullptr)
            return;

        const C2D_Image logo =
            C2D_SpriteSheetGetImage(splashSheet, 0);

        const float originalWidth =
            static_cast<float>(logo.subtex->width);

        const float originalHeight =
            static_cast<float>(logo.subtex->height);

        if (originalWidth <= 0.0f || originalHeight <= 0.0f)
            return;

        constexpr float MAX_LOGO_WIDTH = 260.0f;
        constexpr float MAX_LOGO_HEIGHT = 78.0f;

        float scale = MAX_LOGO_WIDTH / originalWidth;

        if (originalHeight * scale > MAX_LOGO_HEIGHT)
            scale = MAX_LOGO_HEIGHT / originalHeight;

        const float width = originalWidth * scale;

        C2D_DrawImageAt(
            logo,
            (TOP_WIDTH - width) / 2.0f,
            28.0f,
            0.0f,
            nullptr,
            scale,
            scale
        );
    }

    void drawWelcome(u64 elapsed, u8 blackAlpha)
    {
        const float breathPhase =
            static_cast<float>(elapsed % BREATH_DURATION) /
            static_cast<float>(BREATH_DURATION);

        const float wave =
            (std::sin(breathPhase * 6.28318530718f) + 1.0f) / 2.0f;

        const float breathIntensity =
            0.025f + wave * 0.975f;

        const u32 breathingColor = interpolateColor(
            247, 245, 238,
            78, 122, 101,
            breathIntensity
        );

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(topScreen, COLOR_CREMA);
        C2D_SceneBegin(topScreen);

        drawLogo();

        drawCenteredText(
            &titleText, TOP_WIDTH, 126.0f, 0.82f, COLOR_CARBON
        );

        drawCenteredText(
            &spanishText, TOP_WIDTH, 166.0f, 0.43f, COLOR_CARBON
        );

        drawCenteredText(
            &englishText, TOP_WIDTH, 190.0f, 0.38f, COLOR_CARBON
        );

        drawFadeOverlay(TOP_WIDTH, blackAlpha);

        C2D_TargetClear(bottomScreen, COLOR_CREMA);
        C2D_SceneBegin(bottomScreen);

        drawCenteredText(
            &instructionLine1,
            BOTTOM_WIDTH, 98.0f, 0.43f, breathingColor
        );

        drawCenteredText(
            &instructionLine2,
            BOTTOM_WIDTH, 124.0f, 0.43f, breathingColor
        );

        drawFadeOverlay(BOTTOM_WIDTH, blackAlpha);

        C3D_FrameEnd(0);
    }

    void drawTransition(u8 blackAlpha)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(topScreen, COLOR_CREMA);
        C2D_SceneBegin(topScreen);

        drawLogo();

        drawCenteredText(
            &titleText, TOP_WIDTH, 126.0f, 0.82f, COLOR_CARBON
        );

        drawCenteredText(
            &spanishText, TOP_WIDTH, 166.0f, 0.43f, COLOR_CARBON
        );

        drawCenteredText(
            &englishText, TOP_WIDTH, 190.0f, 0.38f, COLOR_CARBON
        );

        drawFadeOverlay(TOP_WIDTH, blackAlpha);

        C2D_TargetClear(bottomScreen, COLOR_CREMA);
        C2D_SceneBegin(bottomScreen);

        drawCenteredText(
            &instructionLine1,
            BOTTOM_WIDTH, 98.0f, 0.43f, COLOR_MUSGO
        );

        drawCenteredText(
            &instructionLine2,
            BOTTOM_WIDTH, 124.0f, 0.43f, COLOR_MUSGO
        );

        drawFadeOverlay(BOTTOM_WIDTH, blackAlpha);

        C3D_FrameEnd(0);
    }

    void runWelcome()
    {
        const u64 fadeStart = osGetTime();

        // Primero se muestra la bienvenida desde negro.
        while (aptMainLoop())
        {
            hidScanInput();

            const u64 elapsed = osGetTime() - fadeStart;

            float progress =
                static_cast<float>(elapsed) /
                static_cast<float>(FADE_DURATION);

            if (progress > 1.0f)
                progress = 1.0f;

            const u8 alpha =
                static_cast<u8>((1.0f - progress) * 255.0f);

            drawWelcome(elapsed, alpha);

            if (progress >= 1.0f)
                break;
        }

        // La bienvenida permanece visible hasta recibir entrada.
        bool leaving = false;
        u64 transitionStart = 0;

        while (aptMainLoop())
        {
            hidScanInput();

            if (!leaving && hidKeysDown() != 0)
            {
                leaving = true;
                transitionStart = osGetTime();
            }

            if (!leaving)
            {
                drawWelcome(osGetTime() - fadeStart, 0);
            }
            else
            {
                const u64 elapsed =
                    osGetTime() - transitionStart;

                float progress =
                    static_cast<float>(elapsed) /
                    static_cast<float>(FADE_DURATION);

                if (progress > 1.0f)
                    progress = 1.0f;

                drawTransition(
                    static_cast<u8>(progress * 255.0f)
                );

                if (progress >= 1.0f)
                    break;
            }
        }
    }

    void showError(const char* message)
    {
        C2D_Text errorText;
        C2D_Text instructionText;

        C2D_TextParse(
            &errorText,
            textBuffer,
            message
        );

        C2D_TextOptimize(&errorText);

        C2D_TextParse(
            &instructionText,
            textBuffer,
            "Presiona cualquier boton para salir."
        );

        C2D_TextOptimize(&instructionText);

        while (aptMainLoop())
        {
            hidScanInput();

            if (hidKeysDown() != 0)
                break;

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

            C2D_TargetClear(topScreen, COLOR_CREMA);
            C2D_SceneBegin(topScreen);

            drawCenteredText(
                &titleText,
                TOP_WIDTH, 75.0f, 0.75f, COLOR_CARBON
            );

            drawCenteredText(
                &errorText,
                TOP_WIDTH, 115.0f, 0.48f, COLOR_ROJO
            );

            C2D_TargetClear(bottomScreen, COLOR_CREMA);
            C2D_SceneBegin(bottomScreen);

            drawCenteredText(
                &instructionText,
                BOTTOM_WIDTH, 110.0f, 0.38f, COLOR_MUSGO
            );

            C3D_FrameEnd(0);
        }
    }
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    romfsInit();
    gfxInitDefault();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    textBuffer = C2D_TextBufNew(512);

    if (textBuffer == nullptr)
    {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        romfsExit();
        return 1;
    }

    prepareText(&titleText, "Laura v1.0");

    prepareText(
        &spanishText,
        "Procesador de texto minimalista para Nintendo 3DS"
    );

    prepareText(
        &englishText,
        "Minimalist word processor for the Nintendo 3DS"
    );

    prepareText(
        &instructionLine1,
        "Toca la pantalla o presiona"
    );

    prepareText(
        &instructionLine2,
        "cualquier boton para continuar."
    );

    splashSheet = C2D_SpriteSheetLoad(
        "romfs:/splash.t3x"
    );

    if (splashSheet == nullptr)
    {
        showError("ERROR: NO SE CARGO SPLASH.T3X");
    }
    else if (!Editor_Init(topScreen, bottomScreen))
    {
        showError("ERROR: EDITOR_INIT FALLO");
    }
    else
    {
        runWelcome();
        Editor_Run();
        Editor_Fini();
    }

    if (splashSheet != nullptr)
        C2D_SpriteSheetFree(splashSheet);

    C2D_TextBufDelete(textBuffer);

    C2D_Fini();
    C3D_Fini();

    gfxExit();
    romfsExit();

    return 0;
}
