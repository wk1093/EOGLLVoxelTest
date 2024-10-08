#include <eogll.h>
#include <stdbool.h>

// EOGLL voxel engine

// TODO: lights stored elsewhere
// TODO: Movement and collision (no noclipping)
// TODO: More chunks
// TODO: Textures
// TODO: worldgen
// TODO: cull hidden faces (use a uniform to tell a block about it's neighbors)

typedef enum {
    VOXEL_AIR,
    VOXEL_DIRT,
    VOXEL_GRASS,
    VOXEL_STONE,
    VOXEL_LIGHT,
    VOXEL_SELECTED,
} VoxelType;

typedef struct {
    VoxelType type;
} Voxel;

typedef struct {
    Voxel *voxels; // stored in a 1D array
} Chunk;

typedef struct {
    Chunk *chunks; // stored in a 1D array
    int width; // this is the number of chunks in the x direction
    int height;
    int depth;
    int chunkWidth; // this is the number of voxels in each chunk in the x direction
    int chunkHeight;
    int chunkDepth;
} World;

vec3 lightPos = {5.0f, 5.0f, 5.0f};

Voxel *getVoxelFromChunk(Chunk *chunk, int x, int y, int z) {
    return &chunk->voxels[x + y * 16 + z * 16 * 16];
}

Voxel *getVoxelFromWorld(World *world, int x, int y, int z) {
    int chunkX = x / 16;
    int chunkY = y / 16;
    int chunkZ = z / 16;
    int voxelX = x % 16;
    int voxelY = y % 16;
    int voxelZ = z % 16;
    return getVoxelFromChunk(&world->chunks[chunkX + chunkY * world->chunkWidth + chunkZ * world->chunkWidth * world->chunkHeight], voxelX, voxelY, voxelZ);
}

void setVoxelFromWorld(World *world, int x, int y, int z, Voxel voxel) {
    int chunkX = x / 16;
    int chunkY = y / 16;
    int chunkZ = z / 16;
    int voxelX = x % 16;
    int voxelY = y % 16;
    int voxelZ = z % 16;
//    *getVoxelFromChunk(&world->chunks[chunkX + chunkY * world->chunkWidth + chunkZ * world->chunkWidth * world->chunkHeight], voxelX, voxelY, voxelZ) = voxel;
    // segfault
    world->chunks[chunkX + chunkY * world->chunkWidth + chunkZ * world->chunkWidth * world->chunkHeight].voxels[voxelX + voxelY * 16 + voxelZ * 16 * 16] = voxel;
}

typedef struct {
    EogllBufferObject* buffer;
    EogllShaderProgram* program;
    EogllTexture** textures;
    uint32_t textureCount;
    EogllProjection projection;
    EogllCamera camera;
    EogllWindow* window;
    // model is calculated per voxel
    vec3 selectedVoxel;
    bool selectedVoxelValid;
} GameData;


void renderVoxel(GameData* dat, Voxel *voxel, int x, int y, int z) {
    EogllModel model = eogllCreateModel();
    glm_vec3_copy((vec3) {(float)x, (float)y, (float)z}, model.pos);
    glm_vec3_copy((vec3) {1.0f, 1.0f, 1.0f}, model.scale);
    glm_vec3_copy((vec3) {0.0f, 0.0f, 0.0f}, model.rot);

    if (voxel->type == VOXEL_AIR) {
        return;
    }

    if (voxel->type >= dat->textureCount) {
        EOGLL_LOG_ERROR(stderr, "Voxel type %d is out of bounds", voxel->type);
        return;
    }

    eogllUseProgram(dat->program);
    eogllBindTextureUniform(dat->textures[voxel->type], dat->program, "texture1", 0);
    eogllUpdateModelMatrix(&model, dat->program, "model");
    eogllUpdateProjectionMatrix(&dat->projection, dat->program, "projection", dat->window->width, dat->window->height);
    eogllUpdateCameraMatrix(&dat->camera, dat->program, "view");
    eogllSetUniform3f(dat->program, "lightPos", lightPos[0], lightPos[1], lightPos[2]);
    eogllSetUniform3f(dat->program, "viewPos", dat->camera.pos[0], dat->camera.pos[1], dat->camera.pos[2]);
    eogllDrawBufferObject(dat->buffer, GL_TRIANGLES);
}
void renderSelection(GameData* dat) {
    if (!dat->selectedVoxelValid) {
        return;
    }
    EogllModel model = eogllCreateModel();
    glm_vec3_copy(dat->selectedVoxel, model.pos);
    glm_vec3_copy((vec3) {1.05f, 1.05f, 1.05f}, model.scale);
    glm_vec3_copy((vec3) {0.0f, 0.0f, 0.0f}, model.rot);


    eogllUseProgram(dat->program);
    eogllBindTextureUniform(dat->textures[VOXEL_SELECTED], dat->program, "texture1", 0);
    eogllUpdateModelMatrix(&model, dat->program, "model");
    eogllUpdateProjectionMatrix(&dat->projection, dat->program, "projection", dat->window->width, dat->window->height);
    eogllUpdateCameraMatrix(&dat->camera, dat->program, "view");
    eogllSetUniform3f(dat->program, "lightPos", lightPos[0], lightPos[1], lightPos[2]);
    eogllSetUniform3f(dat->program, "viewPos", dat->camera.pos[0], dat->camera.pos[1], dat->camera.pos[2]);
    eogllDrawBufferObject(dat->buffer, GL_TRIANGLES);
}

void renderWorld(GameData* dat, World *world) {
    for (int x = 0; x < world->width * world->chunkWidth; x++) {
        for (int y = 0; y < world->height * world->chunkHeight; y++) {
            for (int z = 0; z < world->depth * world->chunkDepth; z++) {
                Voxel *voxel = getVoxelFromWorld(world, x, y, z);
                if (voxel->type == VOXEL_AIR) {
                    continue;
                }
                // render voxel
                renderVoxel(dat, voxel, x, y, z);
            }
        }
    }
}
typedef struct {
    vec3 hit;
    float distance;
    bool hitBlock;
    vec3 block;
} RaycastResult;

RaycastResult raycastBlock(EogllCamera camera, World *world) {
    RaycastResult result = {0};
    result.hitBlock = false;
    const static int raycastSteps = 3000;
    const static float raycastDistance = 10.0f;
    vec3 curpos;
    glm_vec3_copy(camera.pos, curpos);
    vec3 dir;
    glm_vec3_copy(camera.front, dir);
    vec3 movedelta;
    glm_vec3_scale(dir, raycastDistance/raycastSteps, movedelta);

    for (int i = 0; i < raycastSteps; i++) {

        float distance = raycastDistance * (float)(i / raycastSteps);

        glm_vec3_add(curpos, movedelta, curpos);

        vec3 voxelPos;
        voxelPos[0] = curpos[0];
        voxelPos[1] = curpos[1];
        voxelPos[2] = curpos[2];
        voxelPos[0] = roundf(voxelPos[0]);
        voxelPos[1] = roundf(voxelPos[1]);
        voxelPos[2] = roundf(voxelPos[2]);

        // check bounds
        if (voxelPos[0] < 0 || voxelPos[0] >= world->width * world->chunkWidth) {
            continue;
        }
        if (voxelPos[1] < 0 || voxelPos[1] >= world->height * world->chunkHeight) {
            continue;
        }
        if (voxelPos[2] < 0 || voxelPos[2] >= world->depth * world->chunkDepth) {
            continue;
        }

        Voxel *voxel = getVoxelFromWorld(world, voxelPos[0], voxelPos[1], voxelPos[2]);

        if (voxel->type == VOXEL_AIR) {
            continue;
        }
        glm_vec3_copy(curpos, result.hit);
        glm_vec3_copy(voxelPos, result.block);
        result.distance = distance;
        result.hitBlock = true;


        return result;
    }
    result.hitBlock = false;
    return result;
}

typedef enum {
    DIR_UP, // y+
    DIR_DOWN, // y-
    DIR_WEST, // x-
    DIR_EAST, // x+
    DIR_NORTH, // z+
    DIR_SOUTH, // z-
    DIR_NONE // 0
} Direction;

Direction getDirectionOfRaycast(RaycastResult result) {
    // compare result.block with result.hit
    float diffx = result.block[0] - result.hit[0];
    float diffy = result.block[1] - result.hit[1];
    float diffz = result.block[2] - result.hit[2];
    // greatest difference

    if (fabs(diffx) > fabs(diffy) && fabs(diffx) > fabs(diffz)) {
        return diffx < 0 ? DIR_EAST : DIR_WEST;
    }
    if (fabs(diffy) > fabs(diffx) && fabs(diffy) > fabs(diffz)) {
        return diffy < 0 ? DIR_UP : DIR_DOWN;
    }
    if (fabs(diffz) > fabs(diffx) && fabs(diffz) > fabs(diffy)) {
        return diffz > 0 ? DIR_SOUTH : DIR_NORTH;
    }
    return DIR_NONE;
    
    
}

typedef struct {
    vec3 pos;
} BlockPos;

BlockPos getBlockOnFace(RaycastResult res) {
    Direction dir = getDirectionOfRaycast(res);
    // just project the block pos by the direction
    BlockPos pos;
    pos.pos[0] = res.block[0];
    pos.pos[1] = res.block[1];
    pos.pos[2] = res.block[2];
    switch (dir) {
        case DIR_UP: pos.pos[1] += 1; break;
        case DIR_DOWN: pos.pos[1] -= 1; break;
        case DIR_WEST: pos.pos[0] -= 1; break;
        case DIR_EAST: pos.pos[0] += 1; break;
        case DIR_NORTH: pos.pos[2] += 1; break;
        case DIR_SOUTH: pos.pos[2] -= 1; break;
        default: break;
    }
    return pos;
}



int main() {
    // EOGLL requires us to always check the success value
    if (eogllInit() != EOGLL_SUCCESS) {
        return -1;
    }

    // this is a basic struct that contains all the extra window settings
    EogllWindowHints hints = eogllDefaultWindowHints(); // default

    // create the window
    EogllWindow *window = eogllCreateWindow(800, 600, "LearnEOGLL", hints);
    if (window == NULL) {
        EOGLL_LOG_ERROR(stderr, "Failed to create window");
        eogllTerminate();
        return -1;
    }

    eogllEnableTransparency();
    eogllEnableDepth();
    eogllEnableFaceCulling();

    EogllShaderProgram *program = eogllLinkProgramFromFile("assets/shaders/cube.vert", "assets/shaders/cube.frag");
    EogllShaderProgram *crosshair_program = eogllLinkProgramFromFile("assets/shaders/crosshair.vert", "assets/shaders/crosshair.frag");

    EogllObjectAttrs attrs = eogllCreateObjectAttrs();
    eogllAddObjectAttr(&attrs, GL_FLOAT, 3, EOGLL_ATTR_POSITION);
    eogllAddObjectAttr(&attrs, GL_FLOAT, 2, EOGLL_ATTR_TEXTURE);
    eogllAddObjectAttr(&attrs, GL_FLOAT, 3, EOGLL_ATTR_NORMAL);
    EogllBufferObject object = eogllLoadBufferObject("assets/models/cube.obj", attrs, GL_STATIC_DRAW);

    EogllObjectAttrs crosshair_attrs = eogllCreateObjectAttrs();
    eogllAddObjectAttr(&crosshair_attrs, GL_FLOAT, 3, EOGLL_ATTR_POSITION);
    eogllAddObjectAttr(&crosshair_attrs, GL_FLOAT, 2, EOGLL_ATTR_TEXTURE);
    EogllBufferObject crosshair_object = eogllLoadBufferObject("assets/models/crosshair.obj", crosshair_attrs, GL_STATIC_DRAW);

    // set gl to not smooth the texture

    EogllTexture *crosshair_texture = eogllStartTexture();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    eogllFinishTexture(crosshair_texture, "assets/textures/crosshair.png");

    EogllTexture *temp = eogllCreateTexture("assets/textures/wall.jpg");
    EogllTexture *light = eogllCreateTexture("assets/textures/light.png");
    EogllTexture *dirt = eogllCreateTexture("assets/textures/dirt.png");

    EogllTexture *outline = eogllStartTexture();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    eogllFinishTexture(outline, "assets/textures/outline.png");

    EogllTexture* textures[] = { // walls for now
            // air, dirt, grass, stone, light, outline
            NULL, dirt, temp, temp, light, outline
    };
    

    EogllModel crosshair_model = eogllCreateModel();
    glm_vec3_copy((vec3) {0.0f, 0.0f, 0.0f}, crosshair_model.pos);
    glm_vec3_copy((vec3) {0.0f, 0.0f, 0.0f}, crosshair_model.rot);

    float crosshair_width = 0.06f;
    float crosshair_height = 0.06f;
    if (window->width > window->height) {
        crosshair_width *= (float)window->height / (float)window->width;
    } else {
        crosshair_height *= (float)window->width / (float)window->height;
    }
    glm_vec3_copy((vec3) {crosshair_width, crosshair_height, 1.0f}, crosshair_model.scale);

    GameData dat = {
            .buffer = &object,
            .program = program,
            .textures = textures,
            .textureCount = 6,
            .projection = eogllPerspectiveProjection(45.0f, 0.1f, 100.0f),
            .camera = eogllCreateCamera(),
            .window = window,
            .selectedVoxelValid = false
    };

    World world = {
            .width = 1,
            .height = 1,
            .depth = 1,
            .chunkWidth = 16,
            .chunkHeight = 16,
            .chunkDepth = 16,
            .chunks = calloc(1, sizeof(Chunk))
    };

    world.chunks[0].voxels = calloc(16 * 16 * 16, sizeof(Voxel));

    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            for (int z = 0; z < 16; z++) {
                setVoxelFromWorld(&world, x, y, z, (Voxel) {VOXEL_AIR});
            }
        }
    }

    // place some dirt
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            setVoxelFromWorld(&world, x, 0, z, (Voxel) {VOXEL_DIRT});
        }
    }


    eogllTranslateCamera3f(&dat.camera, 0.0f, 0.0f, -3.0f);

//    EogllModel datmodel = eogllCreateModel();

    float speed = 5.0f;
    float rotSpeed = 100.0f;
    float sensitivity = 0.1f;
    double lastTime = eogllGetTime();

    bool cursorDisabled = true;

    Voxel vox = {1};

    // render a small cube at the position of the light
    EogllModel cube = eogllCreateModel();
    glm_vec3_copy(lightPos, cube.pos);
    glm_vec3_copy((vec3) {0.2f, 0.2f, 0.2f}, cube.scale);
    glm_vec3_copy((vec3) {0.0f, 0.0f, 0.0f}, cube.rot);


    // auto rotate camera 180 to look at the world
    eogllYawCamera(&dat.camera, 180.0f);

    eogllSetCursorMode(window, EOGLL_CURSOR_DISABLED);

    while (!eogllWindowShouldClose(window)) {
        double currentTime = eogllGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;

        lightPos[0] = 5 + sin(currentTime * 2) * 8;
        glm_vec3_copy(lightPos, cube.pos);


        if (window->press[EOGLL_KEY_ESCAPE]) {
            cursorDisabled = false;
            eogllSetCursorMode(window, EOGLL_CURSOR_NORMAL);
        }

        if (window->mousePress[EOGLL_MOUSE_BUTTON_LEFT]) {
            cursorDisabled = true;
            eogllSetCursorMode(window, EOGLL_CURSOR_DISABLED);
        }

        // movement
        if (window->isDown[EOGLL_KEY_W]) {
            eogllMoveCamera(&dat.camera, EOGLL_FORWARD, speed*deltaTime);
        }
        if (window->isDown[EOGLL_KEY_S]) {
            eogllMoveCamera(&dat.camera, EOGLL_BACKWARD, speed*deltaTime);
        }
        if (window->isDown[EOGLL_KEY_A]) {
            eogllMoveCamera(&dat.camera, EOGLL_LEFT, speed*deltaTime);
        }
        if (window->isDown[EOGLL_KEY_D]) {
            eogllMoveCamera(&dat.camera, EOGLL_RIGHT, speed*deltaTime);
        }
        if (window->isDown[EOGLL_KEY_SPACE]) {
            eogllMoveCamera(&dat.camera, EOGLL_UP, speed*deltaTime);
        }
        if (window->isDown[EOGLL_KEY_LEFT_SHIFT]) {
            eogllMoveCamera(&dat.camera, EOGLL_DOWN, speed*deltaTime);
        }

        // rotation

        if (window->isDown[EOGLL_KEY_LEFT]) {
            eogllYawCamera(&dat.camera, -rotSpeed*deltaTime);
        }
        if (window->isDown[EOGLL_KEY_RIGHT]) {
            eogllYawCamera(&dat.camera, rotSpeed*deltaTime);
        }
        if (window->isDown[EOGLL_KEY_UP]) {
            eogllPitchCamera(&dat.camera, rotSpeed*deltaTime);
        }
        if (window->isDown[EOGLL_KEY_DOWN]) {
            eogllPitchCamera(&dat.camera, -rotSpeed*deltaTime);
        }

        // mouse rotation
        if (cursorDisabled) {
            if (window->mousedx != 0) {
                eogllYawCamera(&dat.camera, window->mousedx * sensitivity);
                window->mousedx = 0;
            }
            if (window->mousedy != 0) {
                eogllPitchCamera(&dat.camera, -window->mousedy * sensitivity);
                window->mousedy = 0;
            }
            RaycastResult result = raycastBlock(dat.camera, &world);
            if (result.hitBlock) {
                dat.selectedVoxel[0] = result.block[0];
                dat.selectedVoxel[1] = result.block[1];
                dat.selectedVoxel[2] = result.block[2];
                dat.selectedVoxelValid = true;
            } else {
                dat.selectedVoxelValid = false;
            }
            if (window->mousePress[EOGLL_MOUSE_BUTTON_LEFT]) {
                if (result.hitBlock) {
                    Voxel *voxel = getVoxelFromWorld(&world, result.block[0], result.block[1], result.block[2]);
                    *voxel = (Voxel) {VOXEL_AIR};
                }
            }
            if (window->mousePress[EOGLL_MOUSE_BUTTON_RIGHT]) {
                BlockPos pos = getBlockOnFace(result);
                Voxel *voxel = getVoxelFromWorld(&world, pos.pos[0], pos.pos[1], pos.pos[2]);
                if (voxel->type == VOXEL_AIR) {
                    *voxel = (Voxel) {VOXEL_DIRT};
                }
            }

        }

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderWorld(&dat, &world);
        renderSelection(&dat);

        eogllUseProgram(dat.program);
        eogllBindTextureUniform(dat.textures[4], dat.program, "texture1", 0);
        eogllUpdateModelMatrix(&cube, dat.program, "model");
        eogllUpdateProjectionMatrix(&dat.projection, dat.program, "projection", window->width, window->height);
        eogllUpdateCameraMatrix(&dat.camera, dat.program, "view");
        eogllSetUniform3f(dat.program, "lightPos", dat.camera.pos[0], dat.camera.pos[1], dat.camera.pos[2]);
        eogllSetUniform3f(dat.program, "viewPos", dat.camera.pos[0], dat.camera.pos[1], dat.camera.pos[2]);
        eogllDrawBufferObject(dat.buffer, GL_TRIANGLES);

        eogllUseProgram(crosshair_program);
        eogllUpdateModelMatrix(&crosshair_model, crosshair_program, "model");
        eogllBindTextureUniform(crosshair_texture, crosshair_program, "texture1", 0);
        eogllDrawBufferObject(&crosshair_object, GL_TRIANGLES);


        // set all grass back to dirt
        for (int x = 0; x < world.width * world.chunkWidth; x++) {
            for (int y = 0; y < world.height * world.chunkHeight; y++) {
                for (int z = 0; z < world.depth * world.chunkDepth; z++) {
                    Voxel *voxel = getVoxelFromWorld(&world, x, y, z);
                    if (voxel->type == VOXEL_SELECTED) {
                        *voxel = (Voxel) {VOXEL_DIRT};
                    }
                }
            }
        }

        eogllSwapBuffers(window);
        eogllPollEvents(window);
    }


    eogllTerminate();
    return 0;
}