#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1
#define NUMBER_OF_ENEMIES 3
#define FIXED_TIMESTEP 0.0166666f
#define ACC_OF_GRAVITY -2.0f
#define PLATFORM_COUNT 10

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"

// ————— STRUCTS AND ENUMS —————//
struct GameState
{
    Entity* player;
    Entity* platforms;
    Entity* winmessage;
    Entity* losemessage;
};

// ————— CONSTANTS ————— //
const int WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

const float BG_RED = 0.1922f,
BG_BLUE = 0.549f,
BG_GREEN = 0.9059f,
BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;
const char  SPRITESHEET_FILEPATH[] = "doctor.png",
PLATFORM_FILEPATH_WIN[] = "platformwin.png",
PLATFORM_FILEPATH_LOSE[] = "platformlose.png",
MESSAGE_WIN[] = "message_win.png",
MESSAGE_LOSE[] = "message_lose.png";


const int NUMBER_OF_TEXTURES = 1;  // to be generated, that is
const GLint LEVEL_OF_DETAIL = 0;  // base image level; Level n is the nth mipmap reduction image
const GLint TEXTURE_BORDER = 0;  // this value MUST be zero

enum Game {
    Start,
    Win,
    Lose
};

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;


void draw_win_message();
void draw_lose_message();

// ———— GENERAL FUNCTIONS ———— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}
void draw_object(glm::mat4& object_model_matrix, GLuint& object_texture_id)
{
    g_shader_program.set_model_matrix(object_model_matrix);
    glBindTexture(GL_TEXTURE_2D, object_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, 6); // we are now drawing 2 triangles, so we use 6 instead of 3
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Hello,Assignment 3!",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ————— PLAYER ————— //
    // Existing
    g_game_state.player = new Entity();
    g_game_state.player->set_position(glm::vec3(-4.5f, 4.5f, 0.0f));
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1, 0.0f));
    g_game_state.player->set_speed(1.0f);
    g_game_state.player->m_texture_id = load_texture(SPRITESHEET_FILEPATH);

    // Walking
    g_game_state.player->m_walking[g_game_state.player->LEFT] = new int[4]{ 1, 5, 9,  13 };
    g_game_state.player->m_walking[g_game_state.player->RIGHT] = new int[4]{ 3, 7, 11, 15 };
    g_game_state.player->m_walking[g_game_state.player->UP] = new int[4]{ 2, 6, 10, 14 };
    g_game_state.player->m_walking[g_game_state.player->DOWN] = new int[4]{ 0, 4, 8,  12 };

    g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];  // start George looking right
    g_game_state.player->m_animation_frames = 4;
    g_game_state.player->m_animation_index = 0;
    g_game_state.player->m_animation_time = 0.0f;
    g_game_state.player->m_animation_cols = 4;
    g_game_state.player->m_animation_rows = 4;
    g_game_state.player->set_height(0.9f);
    g_game_state.player->set_width(0.9f);


    // ————— PLATFORM ————— //
    g_game_state.platforms = new Entity[PLATFORM_COUNT];

    for (int i = 0; i < PLATFORM_COUNT; i++)
    {
        if (i % 3 != 0) {
            g_game_state.platforms[i].m_texture_id = load_texture(PLATFORM_FILEPATH_LOSE);
            g_game_state.platforms[i].set_platformtype_lose();
            g_game_state.platforms[i].set_position(glm::vec3(i - 4.5f, -3.0f, 0.0f));
            g_game_state.platforms[i].update(0.0f, NULL, 0);
        }else{
            g_game_state.platforms[i].m_texture_id = load_texture(PLATFORM_FILEPATH_WIN);
            g_game_state.platforms[i].set_platformtype_win();
            g_game_state.platforms[i].set_position(glm::vec3(i - 4.5f, -3.0f, 0.0f));
            g_game_state.platforms[i].update(0.0f, NULL, 0);
        }
        
    }
    // ————— Winning/Losing Messages ————— //
    g_game_state.winmessage = new Entity();
    g_game_state.winmessage->set_position(glm::vec3(0.0f));
    g_game_state.winmessage->m_texture_id = load_texture(MESSAGE_WIN);
    g_game_state.winmessage->update(0.0f, NULL, 0);
    g_game_state.losemessage = new Entity();
    g_game_state.losemessage->set_position(glm::vec3(0.0f));
    g_game_state.losemessage->m_texture_id = load_texture(MESSAGE_LOSE);
    g_game_state.losemessage->update(0.0f, NULL, 0);

    // ————— GENERAL ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;

            case SDLK_LEFT:
                g_game_state.player->move_left();
                g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->LEFT];
                break;

            case SDLK_RIGHT:
                g_game_state.player->move_right();
                g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];
                break;

            case SDLK_UP:
                g_game_state.player->move_up();
                g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->UP];
                break;

            case SDLK_DOWN:

                g_game_state.player->move_down();
                g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->DOWN];
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);



    // This makes sure that the player can't move faster diagonally
    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
    {
        g_game_state.player->set_movement(glm::normalize(g_game_state.player->get_movement()));
    }
}
Game game_state = Start;
void update()

{
    if (game_state!=Start) {
        return; // stops updating game mechanics
    }

    // ————— DELTA TIME ————— //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND; // get the current number of ticks
    float delta_time = ticks - g_previous_ticks; // the delta time is the difference from the last frame
    g_previous_ticks = ticks;

    // ————— FIXED TIMESTEP ————— //
    // STEP 1: Keep track of how much time has passed since last step
    delta_time += g_time_accumulator;

    // STEP 2: Accumulate the ammount of time passed while we're under our fixed timestep
    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    // STEP 3: Once we exceed our fixed timestep, apply that elapsed time into the objects' update function invocation
    while (delta_time >= FIXED_TIMESTEP)
    {
        // Notice that we're using FIXED_TIMESTEP as our delta time
        g_game_state.player->update(FIXED_TIMESTEP, g_game_state.platforms, PLATFORM_COUNT);
        delta_time -= FIXED_TIMESTEP;
    }

    g_time_accumulator = delta_time;



    int state = g_game_state.player->get_collisiontype();
    if (state != 2) {
        if (state == 1) {
            game_state = Win;
        }
        else {
            game_state = Lose;
        }
    }


}

void render()
{
    // ————— GENERAL ————— //
    glClear(GL_COLOR_BUFFER_BIT);

    // ————— PLAYER ————— //
    g_game_state.player->render(&g_shader_program);

    // ————— PLATFORM ————— //
    for (int i = 0; i < PLATFORM_COUNT; i++) g_game_state.platforms[i].render(&g_shader_program);


    if (game_state == Win) {
        g_game_state.winmessage->render(&g_shader_program);
    }
    else if (game_state == Lose) {
        g_game_state.losemessage->render(&g_shader_program);
    }

    // ————— GENERAL ————— //
    SDL_GL_SwapWindow(g_display_window);

    
}

void shutdown() { SDL_Quit(); }

// ————— DRIVER GAME LOOP ————— /
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}




/*
void draw_win_message() {
    g_game_state.winmessage = new Entity();    
    g_game_state.winmessage->m_texture_id = load_texture(MESSAGE_WIN);
    g_game_state.winmessage->set_position(glm::vec3(1.0f));
    g_game_state.winmessage->update(0.0f, NULL, 0);
}
void draw_lose_message() {
    g_game_state.losemessage = new Entity();
    g_game_state.losemessage->m_texture_id = load_texture(MESSAGE_LOSE);
    g_game_state.losemessage->set_position(glm::vec3(1.0f));
    g_game_state.losemessage->update(0.0f, NULL, 0);

}
*/