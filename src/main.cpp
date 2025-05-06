#include "../raylib/raylib.h"
#include "../raylib/raymath.h"
#include "../raylib/rlgl.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <vector>

// minimal translation vector struct
struct MTV {
    Vector3 axis;
    float depth;
};

class Collider {
  public:
    Vector3 pos;
    Vector3 size;
    Vector3 half_extents; // distance from center to border (size /2)
    Matrix rotation;
    bool is_colliding = false; // this is optional
    Mesh mesh;
    Model model; // this is also optional if you want to draw it

    Collider() {}

    ~Collider() {
        // UnloadModel(model);
        // ^ this causes a double free crash in this case, as its all stack objects, you dont need to do this
        // in this example
    }

    void populate() {
        half_extents = {size.x / 2, size.y / 2, size.z / 2};
        mesh = GenMeshCube(size.x, size.y, size.z);
        model = LoadModelFromMesh(mesh);
        rotation = MatrixIdentity();
    }

    float get_projection_radius(Vector3 axis) const {
        Vector3 local_x = {rotation.m0, rotation.m4, rotation.m8};
        Vector3 local_y = {rotation.m1, rotation.m5, rotation.m9};
        Vector3 local_z = {rotation.m2, rotation.m6, rotation.m10};
        return fabs(Vector3DotProduct(local_x, axis)) * half_extents.x +
               fabs(Vector3DotProduct(local_y, axis)) * half_extents.y +
               fabs(Vector3DotProduct(local_z, axis)) * half_extents.z;
    }

    float get_max_radius() const {
        return Vector3Length(half_extents);
    }

    void draw(Color color) const {

        DrawModel(model, pos, 1, color);
        DrawModelWires(model, pos, 1.0f, BLACK);
    }
};

class Player {
  public:
    Collider collider;
    Vector3 velocity = {0, 0, 0};
    bool is_grounded = false;

    // constants
    const float gravity = 20.0f;
    const float jump_speed = 8.0f;
    const float move_speed = 5.0f;

    void update_gravity(float delta_time) {
        velocity.y -= gravity * delta_time;
        // update position based on velocity
        collider.pos = Vector3Add(collider.pos, Vector3Scale(velocity, delta_time));
    }

    void jump() {
        if (is_grounded) {
            velocity.y = jump_speed;
            is_grounded = false;
        }
    }
};

// collision detection function
bool check_collision_collider(const Collider &a, const Collider &b, MTV *mtv) {

    Vector3 axes[] = {
        {1, 0, 0}, // x axis
        {0, 1, 0}, // y axis
        {0, 0, 1}, // z axis
    };

    float min_overlap = INFINITY;
    Vector3 min_axis = {0, 0, 0};

    Vector3 delta = Vector3Subtract(b.pos, a.pos);

    for (int i = 0; i < 3; i++) {
        Vector3 axis = axes[i];
        float a_proj = a.get_projection_radius(axis);
        float b_proj = b.get_projection_radius(axis);
        float delta_proj = fabs(Vector3DotProduct(delta, axis));
        float overlap = a_proj + b_proj - delta_proj;

        if (overlap <= 0)
            return false;

        if (overlap < min_overlap) {
            min_overlap = overlap;
            min_axis = axis;
        }
    }

    mtv->axis = min_axis;
    mtv->depth = min_overlap;
    return true;
}

int main() {
    InitWindow(800, 600, "Raylib SAT 3D collision demo");
    SetTargetFPS(60);

    // Camera setup
    Camera camera = {};
    camera.position = {10.0f, 10.0f, 10.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // seed random number
    srand(static_cast<unsigned>(time(0)));

    // create player
    Player player;
    player.collider.size = {1.0f, 2.0f, 1.0f};
    player.collider.pos = {0.0f, 1.0f, 0.0f};
    player.collider.populate();

    // create static colliders (including floor)
    std::vector<Collider> static_colliders;

    // Floor
    Collider floor;
    floor.size = {100.0f, 0.1f, 100.0f};
    floor.pos = {0.0f, -0.05f, 0.0f};
    floor.populate();
    static_colliders.push_back(floor);

    // create 6 randomly genetared colliders
    const int num_static_obbs = 6;

    for (int i = 0; i < num_static_obbs; i++) {

        Collider static_collider;

        static_collider.size = {
            static_cast<float>(rand() % 30 + 10) / 10.0f,
            static_cast<float>(rand() % 30 + 10) / 10.0f,
            static_cast<float>(rand() % 30 + 10) / 10.0f};

        static_collider.pos = {
            static_cast<float>(rand() % 100) / 10.0f - 5.0f,
            static_collider.size.y / 2,
            static_cast<float>(rand() % 100) / 10.0f - 5.0f};
        static_collider.populate();
        static_colliders.push_back(static_collider);
    }

    // Game loop
    while (!WindowShouldClose()) {
        float delta_time = GetFrameTime();

        // Handle jumping input
        if (IsKeyDown(KEY_SPACE)) {
            player.jump();
        }

        // Handle horizontal movement
        Vector3 movement = {0, 0, 0};
        if (IsKeyDown(KEY_D))
            movement.x += player.move_speed * delta_time;
        if (IsKeyDown(KEY_A))
            movement.x -= player.move_speed * delta_time;
        if (IsKeyDown(KEY_W))
            movement.z -= player.move_speed * delta_time;
        if (IsKeyDown(KEY_S))
            movement.z += player.move_speed * delta_time;
        player.collider.pos = Vector3Add(player.collider.pos, movement);

        player.update_gravity(delta_time);

        // this is optional
        player.collider.is_colliding = false;
        for (auto &static_collider : static_colliders) {
            static_collider.is_colliding = false;
        }

        player.is_grounded = false;

        // collision detection
        for (auto &static_collider : static_colliders) {

            MTV mtv;

            if (check_collision_collider(player.collider, static_collider, &mtv)) {
                player.collider.is_colliding = true;
                static_collider.is_colliding = true;

                // calculate translation to resolve the collision
                Vector3 mtv_direction = Vector3Normalize(mtv.axis);
                Vector3 translation = Vector3Scale(mtv_direction, mtv.depth);
                Vector3 to_player = Vector3Subtract(player.collider.pos, static_collider.pos);

                if (Vector3DotProduct(to_player, mtv_direction) < 0) {
                    translation = Vector3Negate(translation);
                }

                // apply the calculated translation
                player.collider.pos = Vector3Add(player.collider.pos, translation);

                // check for vertical collision from above (landed on top), so allow jumping
                if (mtv.axis.y == 1 && translation.y > 0) {
                    player.is_grounded = true;
                    player.velocity.y = 0;
                }
            }
        }

        /* optional: make the camera spin arond the sample area */
        // UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);

        // draw static colliders
        for (const auto &static_collider : static_colliders) {
            Color color = BLUE;
            if (static_collider.is_colliding)
                color = RED;
            static_collider.draw(color);
        }

        // draw the floor plate

        floor.draw(GRAY);

        // draw player
        Color player_color = player.collider.is_colliding ? RED : GREEN;
        player.collider.draw(player_color);

        EndMode3D();
        DrawText(player.collider.is_colliding ? "Collision Detected!" : "No Collision", 10, 10, 20, BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
