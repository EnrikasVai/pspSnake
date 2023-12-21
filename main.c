#include <pspkernel.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <stdbool.h>
#include <stdlib.h>


PSP_MODULE_INFO("snake", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_VFPU | THREAD_ATTR_USER);

//constants
#define BUFFER_WIDTH 512
#define BUFFER_HEIGHT 272
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT BUFFER_HEIGHT
#define MOVE_SPEED 2.0f

char list[0x20000] __attribute__((aligned(64)));

void initGu(){
    sceGuInit();

    //Set up buffers
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH,SCREEN_HEIGHT,(void*)0x88000,BUFFER_WIDTH);
    sceGuDepthBuffer((void*)0x110000,BUFFER_WIDTH);

    //Set up viewport
    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    //Set some stuff
    sceGuDepthRange(65535, 0); //Use the full buffer for depth testing - buffer is reversed order

    sceGuDepthFunc(GU_GEQUAL); //Depth buffer is reversed, so GEQUAL instead of LEQUAL
    sceGuEnable(GU_DEPTH_TEST); //Enable depth testing

    sceGuFinish();
    sceGuDisplay(GU_TRUE);
}

void endGu(){
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void startFrame(){
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(0xFF000000); // White background
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void endFrame(){
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

typedef struct {
    unsigned short u, v;
    short x, y, z;
} Vertex;

void drawSnakeHead(float x, float y, float w, float h) {

    Vertex* vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(Vertex));

    vertices[0].x = x;
    vertices[0].y = y;

    vertices[1].x = x + w;
    vertices[1].y = y + h;

    sceGuColor(0xFF0000FF); // Red, colors are ABGR
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
}

void drawFood(float x, float y, float size) {

    Vertex* vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(Vertex));

    vertices[0].x = x;
    vertices[0].y = y;

    vertices[1].x = x + size;
    vertices[1].y = y + size;

    sceGuColor(0x00FF00FF);  // Green, colors are ABGR
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
}

int exit_callback(int arg1, int arg2, void *common)
{
    sceKernelExitGame();
    return 0;
}

int callback_thread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);

    if(thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}

float getRandomFloat(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

int main() {
    setup_callbacks();

    // Controls
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    struct SceCtrlData ctrlData;

    initGu();

    // Initial position of the snake head
    float rectX = 32.0f;
    float rectY = 32.0f;
    float rectWidth = 30.0f;
    float rectHeight = 30.0f;

    // Snake direction
    bool snakeDown = true; // Starting point down
    bool snakeUp = false;
    bool snakeLeft = false;
    bool snakeRight = false;

    // Food variables
    float foodX = 100.0f;
    float foodY = 150.0f;
    float foodSize = 15.0f;
    bool foodAvailable = true;

    while (1) {
        // Read controller input
        sceCtrlReadBufferPositive(&ctrlData, 1);

        // Update the snake head direction
        if (ctrlData.Buttons & PSP_CTRL_LEFT) {
            if (snakeRight != true) {
                snakeLeft = true;
                snakeUp = false;
                snakeDown = false;
                snakeRight = false;
            }
        }
        if (ctrlData.Buttons & PSP_CTRL_RIGHT) {
            if (snakeLeft != true) {
                snakeLeft = false;
                snakeUp = false;
                snakeDown = false;
                snakeRight = true;
            }
        }
        if (ctrlData.Buttons & PSP_CTRL_UP) {
            if (snakeDown != true) {
                snakeLeft = false;
                snakeUp = true;
                snakeDown = false;
                snakeRight = false;
            }
        }
        if (ctrlData.Buttons & PSP_CTRL_DOWN) {
            if (snakeUp != true) {
                snakeLeft = false;
                snakeUp = false;
                snakeDown = true;
                snakeRight = false;
            }
        }

        // Snake draw
        if (snakeDown == true) {
            rectY += MOVE_SPEED;
        }
        if (snakeLeft == true) {
            rectX -= MOVE_SPEED;
        }
        if (snakeRight == true) {
            rectX += MOVE_SPEED;
        }
        if (snakeUp == true) {
            rectY -= MOVE_SPEED;
        }

        startFrame();
        // Draw the rectangle at the updated position
        drawSnakeHead(rectX, rectY, rectWidth, rectHeight);

        if (rectX < foodX + foodSize && rectX + rectWidth > foodX &&
            rectY < foodY + foodSize && rectY + rectHeight > foodY) {
            // Snake ate the food, update the position
            foodAvailable = false;

            // Reposition the food to a new random location
            do {
                foodX = getRandomFloat(0.0f, SCREEN_WIDTH - foodSize);
                foodY = getRandomFloat(0.0f, SCREEN_HEIGHT - foodSize);
            } while (
                // Check if the new food position is inside the snake
                (foodX >= rectX && foodX <= rectX + rectWidth &&
                 foodY >= rectY && foodY <= rectY + rectHeight)
            );

            // Increase the size of the snake or update the score as needed
            // (you can add your scoring logic here)
        }
        if (!foodAvailable) {
            foodAvailable = true;
        }

        if (foodAvailable) {
            
            drawFood(foodX, foodY, foodSize);
        }

        endFrame();
    }

    return 0;
}


