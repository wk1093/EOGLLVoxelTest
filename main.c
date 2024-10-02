#include <eogll.h>
#include <stdbool.h>

// EOGLL voxel engine

// TODO: lights stored elsewhere
// TODO: Light color

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



    float vertices[] = {
            // positions            // texture coords  // normals
            -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 0.0f,  0.0f, -1.0f,
            0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f,  0.0f, -1.0f,
            0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, -1.0f,
            0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f,  0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,  0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, 0.0f,  0.0f, -1.0f,

            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 0.0f,  0.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 0.0f,  0.0f,  1.0f,

            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, -1.0f,  0.0f,  0.0f,

            0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,  0.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 1.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,  0.0f,  0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, -1.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f, -1.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, -1.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, 0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, -1.0f,  0.0f,

            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
    };

    unsigned int indices[] = {
            0, 1,  2,
            3, 4,  5,
            6, 7,  8,
            9, 10, 11,
            12, 13, 14,
            15, 16, 17,
            18, 19, 20,
            21, 22, 23,
            24, 25, 26,
            27, 28, 29,
            30, 31, 32,
            33, 34, 35
    };


    const char *vert =
            "#version 330 core\n"
            "layout (location = 0) in vec3 aPos; // the position variable has attribute position 0\n"
            "layout (location = 1) in vec2 aTexCoord; // the texture coordinate has attribute position 1\n"
            "layout (location = 2) in vec3 aNormal; // the normal has attribute position 2\n"
            "\n"
            "out vec2 TexCoord; // output a texture coordinate to the fragment shader\n"
            "out vec3 Normal; // output a normal to the fragment shader\n"
            "out vec3 FragPos; // output a fragment position to the fragment shader\n"
            "\n"
            "uniform mat4 model;\n"
            "uniform mat4 view;\n"
            "uniform mat4 projection;\n"
            "\n"
            "void main() {\n"
            "FragPos = vec3(model * vec4(aPos, 1.0));\n"
            "gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
            "TexCoord = aTexCoord; // pass the texture coordinate to the fragment shader\n"
            "Normal = mat3(transpose(inverse(model))) * aNormal; // pass the normal vector to the fragment shader\n"
            "}";

    const char *frag =
            "#version 330 core\n"
            "out vec4 FragColor; // the output color of the fragment shader\n"
            "in vec2 TexCoord; // the input texture coordinate from the vertex shader\n"
            "in vec3 Normal; // the input normal from the vertex shader\n"
            "in vec3 FragPos; // the input fragment position from the vertex shader\n"
            "\n"
            "// texture sampler\n"
            "uniform sampler2D texture1;\n"
            "uniform vec3 lightPos;\n"
            "uniform vec3 viewPos;\n"
            "\n"
            "void main() {\n"
            "vec3 lightColor = vec3(0.2, 0.2, 1.0);\n"
            "// ambient\n"
            "float ambientStrength = 0.3;\n"
            "vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);\n"
            "\n"
            "// diffuse\n"
            "vec3 norm = normalize(Normal);\n"
            "vec3 lightDir = normalize(lightPos - FragPos);\n"
            "float diff = max(dot(norm, lightDir), 0.0);\n"
            "vec3 diffuse = diff * lightColor;\n"
            "\n"
            "// specular\n"
            "float specularStrength = 0.5;\n"
            "vec3 viewDir = normalize(viewPos - FragPos);\n"
            "vec3 reflectDir = reflect(-lightDir, norm);\n"
            "float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
            "vec3 specular = specularStrength * spec * lightColor;\n"
            "\n"
            "vec4 result = vec4((ambient + diffuse + specular),1.0) * texture(texture1, TexCoord).rgba;\n"
            "FragColor = result;\n"
            "}";

    EogllShaderProgram *program = eogllLinkProgram(vert, frag);

    EogllAttribBuilder builder = eogllCreateAttribBuilder();
    eogllAddAttribute(&builder, GL_FLOAT, 3); // position
    eogllAddAttribute(&builder, GL_FLOAT, 2); // texture coords
    eogllAddAttribute(&builder, GL_FLOAT, 3); // normals
    unsigned int vao = eogllGenVertexArray();
    unsigned int vbo = eogllGenBuffer(vao, GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    unsigned int ebo = eogllGenBuffer(vao, GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    eogllBuildAttributes(&builder, vao);
    EogllBufferObject object = eogllCreateBufferObject(vao, vbo, ebo, sizeof(indices), GL_UNSIGNED_INT);

    // crosshair stuff
    float crosshair_vert[] = {
        // positions tex coords
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
            0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
            -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    };
    unsigned int crosshair_indices[] = {
            0, 1, 2,
            2, 3, 0
    };
    const char *crosshair_verts =
            "#version 330 core\n"
            "layout (location = 0) in vec3 aPos;\n"
            "layout (location = 1) in vec2 aTexCoord;\n"
            "out vec2 TexCoord;\n"
            "uniform mat4 model;\n"
            "void main() {\n"
            "TexCoord = aTexCoord;\n"
            "gl_Position = model * vec4(aPos.x, aPos.y, 0.0, 1.0);\n"
            "}";
    const char *crosshair_frags =
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "in vec2 TexCoord;\n"
            "uniform sampler2D texture1;\n"
            "void main() {\n"
            "FragColor = texture(texture1, TexCoord);\n"
            "}";
    
    EogllAttribBuilder crosshair_builder = eogllCreateAttribBuilder();
    eogllAddAttribute(&crosshair_builder, GL_FLOAT, 3);
    eogllAddAttribute(&crosshair_builder, GL_FLOAT, 2);
    unsigned int crosshair_vao = eogllGenVertexArray();
    unsigned int crosshair_vbo = eogllGenBuffer(crosshair_vao, GL_ARRAY_BUFFER, sizeof(crosshair_vert), crosshair_vert, GL_STATIC_DRAW);
    unsigned int crosshair_ebo = eogllGenBuffer(crosshair_vao, GL_ELEMENT_ARRAY_BUFFER, sizeof(crosshair_indices), crosshair_indices, GL_STATIC_DRAW);
    eogllBuildAttributes(&crosshair_builder, crosshair_vao);
    EogllBufferObject crosshair_object = eogllCreateBufferObject(crosshair_vao, crosshair_vbo, crosshair_ebo, sizeof(crosshair_indices), GL_UNSIGNED_INT);
    
    // set gl to not smooth the texture

    EogllTexture *crosshair_texture = eogllStartTexture();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    eogllFinishTexture(crosshair_texture, "crosshair.png");
    EogllModel crosshair_model = eogllCreateModel();
    glm_vec3_copy((vec3) {0.0f, 0.0f, 0.0f}, crosshair_model.pos);
    glm_vec3_copy((vec3) {0.0f, 0.0f, 0.0f}, crosshair_model.rot);
    
    // adjust crosshair size to match the window size, so it doesn't appear stretched
    // normally the crosshair looks stretched to match the aspect ratio of the window
    // we want to counteract this
    float crosshair_width = 0.06f;
    float crosshair_height = 0.06f;
    if (window->width > window->height) {
        crosshair_width *= (float)window->height / (float)window->width;
    } else {
        crosshair_height *= (float)window->width / (float)window->height;
    }
    glm_vec3_copy((vec3) {crosshair_width, crosshair_height, 1.0f}, crosshair_model.scale);



    EogllShaderProgram *crosshair_program = eogllLinkProgram(crosshair_verts, crosshair_frags);

    EogllTexture *texture1 = eogllCreateTexture("wall.jpg");
    EogllTexture *texture2 = eogllCreateTexture("light.png");
    EogllTexture *texture3 = eogllCreateTexture("dirt.png");
    EogllTexture *texture4 = eogllStartTexture();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    eogllFinishTexture(texture4, "outline.png");

    EogllTexture* textures[] = { // walls for now
            // air, dirt, grass, stone, light
            texture1, texture3, texture1, texture1, texture2, texture4
    };

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
                // place a block at the cursor
                // we need to raycast to find the block we are pointing at, we also need to stop at a certain distance to prevent infinite recursion
                // Eogll does not have any ray utilities (yet) so we have to do it ourselves
                if (result.hitBlock) {
                    Voxel *voxel = getVoxelFromWorld(&world, result.block[0], result.block[1], result.block[2]);
                    *voxel = (Voxel) {VOXEL_AIR};
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