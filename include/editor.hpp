
#ifndef LAURA_EDITOR_HPP
#define LAURA_EDITOR_HPP

#include <3ds.h>
#include <citro3d.h>

bool Editor_Init(
    C3D_RenderTarget* topTarget,
    C3D_RenderTarget* bottomTarget
);

void Editor_Run();
void Editor_Fini();

#endif
