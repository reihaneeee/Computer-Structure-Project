#include <raylib.h>
#include <iostream>
#include <chrono>
#include <cmath>
///

//Global score conters
int player_score = 0;
int cpu_score = 0;

// Global timing accumulators
std::chrono::duration<double> clamp_hl_time{0};
std::chrono::duration<double> clamp_asm_time{0};
std::chrono::duration<double> update_hl_time{0};
std::chrono::duration<double> update_asm_time{0};

enum UpdateMode {HL_MODE, ASM_MODE};
UpdateMode currentMode = HL_MODE;

// User-selectable path options (0 = straight, 1 = curve upward, 2 = curve downward)
int pathOption = 0;

//A fallback C++ clamping function (used by HL_MODE)
void ClampBallY(float &y, int radius, int &speed_y) {
    float fRadius = static_cast<float>(radius);
    float fScreenHeight = static_cast<float>(GetScreenHeight());
    if (y - radius < 0) {
        y = radius;
        speed_y = abs(speed_y); // move downward
    }
    if (y + radius > GetScreenHeight()) {
        y = GetScreenHeight() - radius;
        speed_y = -abs(speed_y); // move upward
    }
}

class Ball {
 public:
    float x, y;
    int speed_x, speed_y;
    int radius;
    float rotation_angle = 0.0f;    //Track rotation angle

    void Draw() {
        const int num_stripes = 16; //Number of Stripes
        const float stripe_angle = 360.0f / num_stripes; // Angle for each stripe

        for(int i=0; i < num_stripes; i++) {
            //Alternate between Green and Orange
            Color stripe_color = (i % 2 == 0) ? BROWN : ORANGE;

            //Draw each stripe as a circle sector
            DrawCircleSector(
                Vector2{x, y}, //center of the ball
                radius,
                i * stripe_angle, 
                (i+1) * stripe_angle,
                36,             //number of segments
                stripe_color
            );
        }
        /*DrawCircle(x, y, radius, Yellow);*/
    }

    // High-level C++ update:add speed to position and update rotation
    void UpdateHL() {

        auto start_update = std::chrono::high_resolution_clock::now();

        x += speed_x;

        // Modify y based on pathOption:
        if (pathOption == 1) {  // curve upward
            y += speed_y + 5.0f;  // add a positive offset
        }
        else if (pathOption == 2) {  // curve downward
            y += speed_y - 5.0f;  // subtract offset
        }
        else {  // straight (pathOption == 0)
            y += speed_y;
        }

        //y += speed_y;
        rotation_angle += speed_x * 30.0f;

        // Time the ClampBallY call
        auto start_clamp = std::chrono::high_resolution_clock::now();
        // Bounce off top and bottom by clamping the y position:
        ClampBallY(y, radius, speed_y);
        auto end_clamp = std::chrono::high_resolution_clock::now();
        clamp_hl_time += (end_clamp - start_clamp);

        //Update rotation based on movment
        rotation_angle += speed_x * 30.0f;

        /*if (y + radius >= GetScreenHeight() || y - radius <= 0) {
            speed_y *= -1;
        } */
        // Cpu wins
        if (x + radius >= GetScreenWidth()) {
            cpu_score++;
            ResetBall();
        }

        if (x - radius <= 0) {
            player_score++;
            ResetBall();
        }
        auto end_update = std::chrono::high_resolution_clock::now();
        update_hl_time += (end_update - start_update);
    }

    /*Calling an external assembly routine to update the ball`s
    position and rotation based on selected pathOption*/ 
    void UpdateASM();

    //TODO
    void ResetBall() {
        x = GetScreenWidth() / 2.0f;
        y = GetScreenHeight() / 2.0f;

        int speed_choices[2] = {-1, 1};
        speed_x *= speed_choices[GetRandomValue(0, 1)];
        speed_y *= speed_choices[GetRandomValue(0, 1)];

        rotation_angle = 0.0f;  //Reset the rotationa angle
    }
    void ReverseRotation() {
        rotation_angle *= -1;   //Reverse rotation direction
    }
};

class Paddle {
 protected:
    void LimitMovement() {
        if (y <= 0) y = 0;
        if (y + height >= GetScreenHeight()) y = GetScreenHeight() - height;
    }

 public:
    float x, y;
    float width, height;
    int speed;

    void Draw() {
        DrawRectangleRounded(Rectangle{x, y, width, height}, 0.8, 0, BLACK);
    }

    void Update() {
        if (IsKeyDown(KEY_UP)) {
            y = y - speed;
        }
        if (IsKeyDown(KEY_DOWN)) {
            y = y + speed;
        }
        LimitMovement();
    }
};

class CpuPaddle : public Paddle {
 public:
    void Update(int ball_y){
        if (y + height / 2 > ball_y) {
            y = y - speed;
        }
        if (y + height / 2 <= ball_y) {
            y = y + speed;
        }
        LimitMovement();
    }
};

Ball ball;
Paddle player;
CpuPaddle cpu;

/* Assembly routine to update the ball position and rotation using SSE
 instructions
The algorithm is as follows:
   1. Convert the ball’s integer speeds to float and add them to x and y.
   2. Depending on pathOption (0,1,2), add a fixed offset to y (simulate curve).
   3. Update the rotation angle: rotation += speed_x * 30.0f 
 */
extern "C" void asmUpdateBall(float* x, float* y, int* speed_x, int* speed_y, float* rotation, int pathOption);

extern "C" void asmClampBallY(float** y, int radius, int* speed_y, int screen_height);

void Ball::UpdateASM() {

    auto start_update = std::chrono::high_resolution_clock::now();

    // Time the asmClampBallY call
    auto start_clamp = std::chrono::high_resolution_clock::now();
    // Call the assembly routin passing address of the ball fields
    asmUpdateBall(&x, &y, &speed_x, &speed_y, &rotation_angle, pathOption);
    auto end_clamp = std::chrono::high_resolution_clock::now();
    clamp_asm_time += (end_clamp - start_clamp);

    // Clamp y position after assembly update to bounce off top/bottom walls.
    float* y_ptr = &y;
    asmClampBallY(&y_ptr, radius, &speed_y, GetScreenHeight());

/*     if (y + radius >= GetScreenHeight() || y - radius <= 0) {
        speed_y *= -1;
    } */
    // Check for scoring conditions:
    if (x + radius >= GetScreenWidth()) {
        cpu_score++;
        ResetBall();
    }
    if (x - radius <= 0) {
        player_score++;
        ResetBall();
    }

    auto end_update = std::chrono::high_resolution_clock::now();
    update_asm_time += (end_update - start_update);
}
void UpdateASM();

extern "C" void asmUpdateBall(float* x, float* y, int* speed_x, int* speed_y, float* rotation, int pathOption) {

    const float curveOffset = 5.0f; // constant offset for curved paths

#if defined(_GNUC_) 
    //using extended inline assembly with AT&T syntax
    __asm__ volatile(
        // --- Update x: newX = *x + float(*speed_x) ---
        "movss    (%0), %%xmm0            \n\t"  // xmm0 = *x
        "movl     (%2), %%eax             \n\t"  // eax = *speed_x (int)
        "cvtsi2ss %%eax, %%xmm1            \n\t"  // xmm1 = float(*speed_x)
        "addss    %%xmm1, %%xmm0          \n\t"  // xmm0 = xmm0 + xmm1
        "movss    %%xmm0, (%0)            \n\t"  // *x = new x

        // --- Update y: newY = *y + float(*speed_y) ---
        "movss    (%1), %%xmm2            \n\t"  // xmm2 = *y
        "movl     (%3), %%eax             \n\t"  // eax = *speed_y (int)
        "cvtsi2ss %%eax, %%xmm3            \n\t"  // xmm3 = float(*speed_y)
        "addss    %%xmm3, %%xmm2          \n\t"  // xmm2 = xmm2 + xmm3

        // --- Apply curve offset based on pathOption ---
        "cmpl     $1, (%4)                \n\t"  // compare pathOption with 1
        "je       curve_up                \n\t"
        "cmpl     $2, (%4)                \n\t"  // compare pathOption with 2
        "je       curve_down              \n\t"
        "jmp      update_rotation         \n\t"  // if option 0, no curve

        "curve_up:                        \n\t"
        "movss    %5, %%xmm4              \n\t"  // xmm4 = curveOffset (5.0)
        "addss    %%xmm4, %%xmm2          \n\t"  // add positive offset (curve upward)
        "jmp      update_rotation         \n\t"

        "curve_down:                      \n\t"
        "movss    %5, %%xmm4              \n\t"  // xmm4 = curveOffset (5.0)
        "subss    %%xmm4, %%xmm2          \n\t"  // subtract offset (curve downward)

        "update_rotation:                 \n\t"
        "movss    %%xmm2, (%1)            \n\t"  // *y = new y

        // --- Update rotation: *rotation += float(*speed_x) * 30.0 ---
        "movss    (%6), %%xmm5            \n\t"  // xmm5 = *rotation
        "movl     (%2), %%eax             \n\t"  // eax = *speed_x
        "cvtsi2ss %%eax, %%xmm6            \n\t"  // xmm6 = float(*speed_x)
        "mulss    %7, %%xmm6              \n\t"  // xmm6 = xmm6 * 30.0
        "addss    %%xmm6, %%xmm5          \n\t"  // xmm5 = xmm5 + (speed_x * 30)
        "movss    %%xmm5, (%6)            \n\t"  // *rotation = updated rotation

        :
        : "r"(x),         // %0
          "r"(y),         // %1
          "r"(speed_x),   // %2
          "r"(speed_y),   // %3
          "r"(&pathOption), // %4 : pointer to pathOption variable
          "m"(curveOffset), // %5 : memory for constant curve offset (5.0f)
          "r"(rotation),  // %6
          "m"(30.0f)      // %7 : multiplier for rotation update constant (30.0f)
        : "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "memory"
    );
#else
    // If not using gcc/Clang, fall back to high level code.
    *x += static_cast<float>(*speed_x);
    *y += static_cast<float>(*speed_y);
    if(pathOption == 1) {
        *y += curveOffset;
    } else if(pathOption == 2) {
        *y -= curveOffset;
    }
    *rotation += static_cast<float>(*speed_x) * 30.0f;
#endif
}

extern "C" void asmClampBallY(float** y, int radius, int* speed_y, int screen_height);

extern "C" void asmClampBallY(float** y, int radius, int* speed_y, int screen_height) {
#if defined(_GNUC_)
    __asm__ volatile (
        // Load *y (a float) into xmm0
        "movss    (%0), %%xmm0            \n\t"  // xmm0 = *y
        // Convert radius (int) to float in xmm1:
        "movl     %1, %%eax               \n\t"  // eax = radius
        "cvtsi2ss %%eax, %%xmm1            \n\t"  // xmm1 = (float)radius
        // Compute temp = *y - (float)radius, store in xmm0
        "subss    %%xmm1, %%xmm0          \n\t"  // xmm0 = *y - radius
        // Clear xmm2 and compare with 0.0:
        "xorps    %%xmm2, %%xmm2          \n\t"  // xmm2 = 0.0
        "ucomiss  %%xmm2, %%xmm0          \n\t"  // compare (*y - radius) with 0.0
        "jb       clamp_bottom            \n\t"  // if (*y - radius) < 0, jump

        // Otherwise, check upper boundary:
        // Reload *y into xmm0
        "movss    (%0), %%xmm0            \n\t"  // xmm0 = *y
        "addss    %%xmm1, %%xmm0          \n\t"  // xmm0 = *y + radius
        // Convert screen_height (int) to float in xmm3:
        "movl     %3, %%eax               \n\t"  // eax = screen_height
        "cvtsi2ss %%eax, %%xmm3            \n\t"  // xmm3 = (float)screen_height
        "ucomiss  %%xmm0, %%xmm3          \n\t"  // compare ( *y + radius ) with screen_height
        "jb       end_clamp               \n\t"  // if screen_height > (*y + radius), no clamp needed
        "jmp      clamp_top               \n\t"

        "clamp_bottom:                   \n\t"
        // Set *y = (float)radius
        "movss    %%xmm1, (%0)            \n\t"
        // Set *speed_y = abs(*speed_y)
        "movl     (%2), %%eax             \n\t"
        "andl     $0x7FFFFFFF, %%eax      \n\t"
        "movl     %%eax, (%2)             \n\t"
        "jmp      end_clamp               \n\t"

        "clamp_top:                      \n\t"
        // Compute (float)screen_height - (float)radius
        "movl     %3, %%eax               \n\t"  // eax = screen_height
        "cvtsi2ss %%eax, %%xmm4            \n\t"  // xmm4 = (float)screen_height
        "subss    %%xmm1, %%xmm4          \n\t"  // xmm4 = screen_height - radius
        "movss    %%xmm4, (%0)            \n\t"
        // Set *speed_y = -abs(*speed_y)
        "movl     (%2), %%eax             \n\t"
        "andl     $0x7FFFFFFF, %%eax      \n\t"
        "negl     %%eax                   \n\t"
        "movl     %%eax, (%2)             \n\t"

        "end_clamp:                      \n\t"
        :
        : "r"(y), "r"(radius), "r"(speed_y), "r"(screen_height)
        : "eax", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "memory"
    );
#else
    // Fallback C++ version:
    float fRadius = static_cast<float>(radius);
    float fScreenHeight = static_cast<float>(screen_height);
    if ((*(*y)) - fRadius < 0) {
        *(*y) = fRadius;
        *speed_y = abs(*speed_y);
    }
    if ((*(*y)) + fRadius > fScreenHeight) {
        *(*y) = fScreenHeight - fRadius;
        *speed_y = -abs(*speed_y);
    }
#endif
}


int main() {
    std::cout << "Starting the game" << std::endl;
    const int screen_width = 1280;
    const int screen_height = 800;
    InitWindow(screen_width, screen_height, "Reihane`s First Game!");
    SetTargetFPS(90);
    ball.radius = 18;
    ball.x = screen_width / 2.0f;
    ball.y = screen_height / 2.0f;
    ball.speed_x = 5;
    ball.speed_y =5;

    player.width = 25;
    player.height = 120;
    player.x = screen_width - player.width - 10;
    player.y = screen_height / 2 - player.height / 2;
    player.speed = 15;

    cpu.height = 120;
    cpu.width = 25;
    cpu.x = 10;
    cpu.y = screen_height / 2 - cpu.height / 2;
    cpu.speed = 6;

    // Timing for performance measurment (resets per minute)
    auto startTime = std::chrono::steady_clock::now();
    int resetHL = 0, resetASM = 0;

    while (WindowShouldClose() == false) {

        // Allow sitwiching between update modes:
        if(IsKeyDown(KEY_ONE)) {
            currentMode = HL_MODE;
            std::cout << "Switched to High-Level C++ update mode.\n";
        }
        if(IsKeyPressed(KEY_TWO)) {
            currentMode = ASM_MODE;
            std::cout << "Switched to Assembly-accelerated update mode.\n";
        }
        if (IsKeyPressed(KEY_SPACE)) {
            pathOption = (pathOption + 1) % 3;
            std::cout << "Path option set to " << pathOption << "\n";
        }


        BeginDrawing();
        // Updating
/*         ball.Update();
        player.Update();
        cpu.Update(ball.y); */

        // Checking for collisions
         if (CheckCollisionCircleRec({ball.x, ball.y}, ball.radius, {player.x, player.y, player.width, player.height})) {
            ball.speed_x *= -1;
            ball.ReverseRotation();
            // Move the ball to the left of the paddle so it doesn't count as a score.
            ball.x = player.x - ball.radius - 1;
        }

        if (CheckCollisionCircleRec({ball.x, ball.y}, ball.radius, {cpu.x, cpu.y, cpu.width, cpu.height})) {
            ball.speed_x *= -1;
            ball.ReverseRotation();
            // Move the ball to the right of the CPU paddle.
            ball.x = cpu.x + cpu.width + ball.radius + 1;
        } 

        // Update paddles:
        player.Update();
        cpu.Update(static_cast<int>(ball.y));

        // Drawing
        ClearBackground(DARKBLUE);
        DrawRectangle(screen_width / 2, 0, screen_width / 2, screen_height, BLUE);
        DrawCircle(screen_width / 2, screen_height / 2, 150, GRAY);
        DrawLine(screen_width / 2, 0, screen_width / 2, screen_height, WHITE);

        // Update game objects:
        if(currentMode == HL_MODE) {
            ball.UpdateHL();
            resetHL++;  // count resets in HL mode (when ball resets, in reality you’d check inside Reset())
        }
        else {
            ball.UpdateASM();
            resetASM++;
        }
        
        ball.Draw();
        cpu.Draw();
        player.Draw();
/*      DrawText(TextFormat("%i", cpu_score), screen_width / 4 - 20, 20, 80, WHITE);
        DrawText(TextFormat("%i", player_score), 3 * screen_width / 4 - 20, 20, 80, WHITE); */
        DrawText(TextFormat("CPU: %i", cpu_score), screen_width / 4 - 30, 20, 80, WHITE);
        DrawText(TextFormat("Player: %i", player_score), 2.4 * screen_width / 4 - 20, 20, 80, WHITE);
        DrawText(TextFormat("Mode: %s", (currentMode==HL_MODE ? "High-Level" : "Assembly")), 20, 20, 20, YELLOW);
        DrawText(TextFormat("Path Option: %i", pathOption), 20, 50, 20, YELLOW);
        EndDrawing();

        // Inside the main loop's timing check (every 60 seconds)
        // After one minute, print the performance statistics (resets per minute).
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = currentTime - startTime;
        if (elapsed.count() >= 60.0) {
            std::cout << "\nPerformance Report (Last 60 seconds):\n";

            // High-Level Mode
            std::cout << "High-Level Mode:\n";
            std::cout << "  UpdateHL Calls: " << resetHL << "\n";
            std::cout << "  Total UpdateHL Time: " << update_hl_time.count() << "s\n";
            if (resetHL > 0) {
                std::cout << "  Average UpdateHL Time/Call: " 
                    << (update_hl_time.count() / resetHL) * 1e6 << " µs\n";
                std::cout << "  Total ClampBallY Time: " << clamp_hl_time.count() << "s\n";
                std::cout << "  Average ClampBallY Time/Call: " 
                    << (clamp_hl_time.count() / resetHL) * 1e6 << " µs\n";
            }

            // Assembly Mode
            std::cout << "Assembly Mode:\n";
            std::cout << "  UpdateASM Calls: " << resetASM << "\n";
            std::cout << "  Total UpdateASM Time: " << update_asm_time.count() << "s\n";
            if (resetASM > 0) {
                std::cout << "  Average UpdateASM Time/Call: " 
                    << (update_asm_time.count() / resetASM) * 1e6 << " µs\n";
                std::cout << "  Total asmClampBallY Time: " << clamp_asm_time.count() << "s\n";
                std::cout << "  Average asmClampBallY Time/Call: " 
                    << (clamp_asm_time.count() / resetASM) * 1e6 << " µs\n";
            }

            std::cout << "\nPerformance Report (approximate):\n";
            std::cout << "High-Level Mode ball updates (per minute): " << resetHL << "\n";
            std::cout << "Assembly Mode ball updates (per minute): " << resetASM << "\n";
            // Reset counters and timers
            resetHL = 0;
            resetASM = 0;
            update_hl_time = std::chrono::duration<double>::zero();
            clamp_hl_time = std::chrono::duration<double>::zero();
            update_asm_time = std::chrono::duration<double>::zero();
            clamp_asm_time = std::chrono::duration<double>::zero();
            startTime = currentTime;

            
            // Reset counters and timer for the next measurement period
/*             resetHL = 0;
            resetASM = 0;
            startTime = currentTime; */
        }
    }

    CloseWindow();
    return 0;
}