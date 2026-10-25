#include "settings.h"
#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// In-repo regression tests for nga.cfg parsing, sanitising and round-trips.

static int failures;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);               \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static const char* kTestPath = "tests/.settings_test.cfg";

static void WriteText(const char* path, const char* text) {
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "FAIL: cannot write %s\n", path);
        failures++;
        return;
    }
    fputs(text, f);
    fclose(f);
}

static void TestDefaults(void) {
    Settings s;
    Settings_Default(&s);
    CHECK(s.resIndex == 0, "default resIndex %d", s.resIndex);
    CHECK(!s.fullscreen && !s.vsync, "default window flags");
    CHECK(s.shadowQuality == 3, "default shadows %d", s.shadowQuality);
    CHECK(s.fov == 70, "default fov %d", s.fov);
    CHECK(fabsf(s.sens - MOUSE_SENS) < 1e-7f, "default sens %.6f", s.sens);
    CHECK(fabsf(s.volume - 1.0f) < 1e-7f, "default volume %.3f", s.volume);
    CHECK(s.pointLights && s.fog, "default lights/fog");
    CHECK(s.aiEnabled && !s.infighting, "default ai/infighting");
    CHECK(s.worldMode == WORLD_CLASSIC, "default world %d", s.worldMode);
    CHECK(s.worldSeed == 1u, "default seed %u", s.worldSeed);
}

static void TestRoundTrip(void) {
    Settings in, out;
    Settings_Default(&in);
    in.resIndex = 2;
    in.fullscreen = true;
    in.vsync = true;
    in.shadowQuality = 1;
    in.fov = 95;
    in.sens = 0.00731f;
    in.volume = 0.375f;
    in.pointLights = false;
    in.fog = false;
    in.aiEnabled = false;
    in.infighting = true;
    in.worldMode = WORLD_RANDOM_256;
    in.worldSeed = 4000000000u;

    Settings_Save(&in, kTestPath);
    Settings_Load(&out, kTestPath);
    remove(kTestPath);

    CHECK(out.resIndex == in.resIndex, "resIndex round-trip");
    CHECK(out.fullscreen == in.fullscreen, "fullscreen round-trip");
    CHECK(out.vsync == in.vsync, "vsync round-trip");
    CHECK(out.shadowQuality == in.shadowQuality, "shadows round-trip");
    CHECK(out.fov == in.fov, "fov round-trip");
    CHECK(fabsf(out.sens - in.sens) < 1e-5f, "sens round-trip %.7f vs %.7f",
          out.sens, in.sens);
    CHECK(fabsf(out.volume - in.volume) < 1e-3f,
          "volume round-trip %.4f vs %.4f", out.volume, in.volume);
    CHECK(out.pointLights == in.pointLights, "lights round-trip");
    CHECK(out.fog == in.fog, "fog round-trip");
    CHECK(out.aiEnabled == in.aiEnabled, "ai round-trip");
    CHECK(out.infighting == in.infighting, "infighting round-trip");
    CHECK(out.worldMode == in.worldMode, "world round-trip");
    CHECK(out.worldSeed == in.worldSeed, "seed round-trip %u vs %u",
          out.worldSeed, in.worldSeed);
}

static void TestMissingAndEmpty(void) {
    Settings s;
    remove(kTestPath);
    Settings_Load(&s, kTestPath);
    CHECK(s.resIndex == 0 && s.fov == 70 && s.worldMode == WORLD_CLASSIC &&
          s.worldSeed == 1u, "missing cfg must load defaults");
    CHECK(s.aiEnabled && s.shadowQuality == 3, "missing cfg defaults");

    WriteText(kTestPath, "");
    Settings_Load(&s, kTestPath);
    remove(kTestPath);
    CHECK(s.resIndex == 0 && s.fov == 70 && s.worldSeed == 1u,
          "empty cfg must load defaults");
}

static void TestSanitize(void) {
    WriteText(kTestPath,
              "res -4\n"
              "fullscreen 2\n"
              "vsync 0\n"
              "shadows 99\n"
              "fov 1\n"
              "sens 99\n"
              "volume -2\n"
              "lights 0\n"
              "fog 1\n"
              "ai 0\n"
              "infighting 1\n"
              "world 7\n"
              "worldseed 0\n"
              "unknown 123\n"
              "this line is intentionally too long to parse as a key value pair\n");
    Settings s;
    Settings_Load(&s, kTestPath);
    remove(kTestPath);

    CHECK(s.resIndex == 0, "res must sanitise to 0, got %d", s.resIndex);
    CHECK(s.fullscreen, "nonzero fullscreen must stay true");
    CHECK(!s.vsync, "vsync");
    CHECK(s.shadowQuality == 3, "shadows must clamp to 3, got %d",
          s.shadowQuality);
    CHECK(s.fov == 50, "fov must clamp to 50, got %d", s.fov);
    CHECK(fabsf(s.sens - 0.02f) < 1e-7f, "sens must clamp to 0.02, got %.5f",
          s.sens);
    CHECK(fabsf(s.volume - 0.0f) < 1e-7f, "volume must clamp to 0, got %.3f",
          s.volume);
    CHECK(!s.pointLights && s.fog, "lights/fog flags");
    CHECK(!s.aiEnabled && s.infighting, "ai/infighting flags");
    CHECK(s.worldMode == WORLD_CLASSIC, "world must sanitise to 0, got %d",
          s.worldMode);
    CHECK(s.worldSeed == 1u, "seed 0 must sanitise to 1, got %u",
          s.worldSeed);
}

static void TestSparseConfig(void) {
    WriteText(kTestPath, "sens 0.00420\nvolume 0.125\nworld 1\nworldseed 77\n");
    Settings s;
    Settings_Load(&s, kTestPath);
    remove(kTestPath);
    CHECK(fabsf(s.sens - 0.00420f) < 1e-7f, "sens %.6f", s.sens);
    CHECK(fabsf(s.volume - 0.125f) < 1e-7f, "volume %.3f", s.volume);
    CHECK(s.worldMode == WORLD_RANDOM, "world mode %d", s.worldMode);
    CHECK(s.worldSeed == 77u, "seed %u", s.worldSeed);
    CHECK(s.fov == 70 && s.shadowQuality == 3 && s.aiEnabled,
          "missing keys must keep defaults");
}

int main(void) {
    TestDefaults();
    TestRoundTrip();
    TestMissingAndEmpty();
    TestSanitize();
    TestSparseConfig();
    if (failures) {
        fprintf(stderr, "test_settings: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_settings: defaults, round-trip and sanitising OK\n");
    return 0;
}
