#include "wl_assets.h"
#include "wl_game_model.h"
#include "wl_gameplay.h"
#include "wl_map_semantics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int path(char *out, size_t out_size, const char *dir, const char *name) {
    int ok = wl_join_path(out, out_size, dir, name);
    CHECK(ok == 0);
    return 0;
}

static int expect_file_size(const char *dir, const char *name, size_t expected) {
    char p[1024];
    size_t actual = 0;
    CHECK(path(p, sizeof(p), dir, name) == 0);
    CHECK(wl_file_size(p, &actual) == 0);
    CHECK(actual == expected);
    return 0;
}

static uint32_t fnv1a_bytes(const unsigned char *bytes, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static size_t count_byte_not_value(const unsigned char *bytes, size_t count,
                                   unsigned char value) {
    size_t hits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (bytes[i] != value) {
            ++hits;
        }
    }
    return hits;
}

static uint32_t fnv1a_scene_sprite_refs(const wl_scene_sprite_ref *refs, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t values[] = {
            (uint32_t)refs[i].kind,
            refs[i].model_index,
            refs[i].source_index,
            refs[i].vswap_chunk_index,
            refs[i].world_x,
            refs[i].world_y,
        };
        for (size_t j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
            for (size_t b = 0; b < 4; ++b) {
                hash ^= (uint8_t)(values[j] >> (8u * b));
                hash *= 16777619u;
            }
        }
    }
    return hash;
}

static uint32_t fnv1a_words(const uint16_t *words, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        hash ^= (uint8_t)(words[i] & 0xffu);
        hash *= 16777619u;
        hash ^= (uint8_t)(words[i] >> 8);
        hash *= 16777619u;
    }
    return hash;
}

static size_t count_value(const uint16_t *words, size_t count, uint16_t value) {
    size_t hits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (words[i] == value) {
            ++hits;
        }
    }
    return hits;
}

static size_t count_nonzero(const uint16_t *words, size_t count) {
    size_t hits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (words[i] != 0) {
            ++hits;
        }
    }
    return hits;
}

static int check_gameplay_events(void) {
    wl_player_gameplay_state state;
    wl_player_damage_result damage;
    wl_palette_shift_result shift;
    int32_t extra_lives = -1;
    int32_t thresholds = -1;

    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 39000, WL_EXTRA_POINTS) == 0);
    CHECK(state.health == 100);
    CHECK(state.lives == 3);
    CHECK(state.score == 39000);
    CHECK(state.next_extra == 40000);
    CHECK(state.play_state == WL_PLAYER_PLAY_RUNNING);

    CHECK(wl_apply_player_damage(&state, WL_DIFFICULTY_BABY, 20, 0, 0, &damage) == 0);
    CHECK(damage.requested_points == 20);
    CHECK(damage.effective_points == 5);
    CHECK(damage.health == 95);
    CHECK(damage.damage_count == 5);
    CHECK(damage.died == 0);
    CHECK(state.health == 95);
    CHECK(state.palette_shift.damage_count == 5);
    CHECK(wl_update_palette_shift_state(&state.palette_shift, 1, &shift) == 0);
    CHECK(shift.kind == WL_PALETTE_SHIFT_RED);
    CHECK(shift.shift_index == 0);
    CHECK(shift.damage_count == 4);

    CHECK(wl_start_player_bonus_flash(&state) == 0);
    CHECK(state.palette_shift.bonus_count == WL_NUM_WHITE_SHIFTS * WL_WHITE_SHIFT_TICS);
    CHECK(wl_update_palette_shift_state(&state.palette_shift, 0, &shift) == 0);
    CHECK(shift.kind == WL_PALETTE_SHIFT_RED);
    CHECK(shift.damage_count == 4);
    CHECK(shift.bonus_count == 18);

    CHECK(wl_apply_player_damage(&state, WL_DIFFICULTY_HARD, 200, 1, 0, &damage) == 0);
    CHECK(damage.effective_points == 200);
    CHECK(damage.health == 95);
    CHECK(damage.died == 0);
    CHECK(state.health == 95);
    CHECK(state.palette_shift.damage_count == 204);
    CHECK(wl_apply_player_damage(&state, WL_DIFFICULTY_HARD, 200, 0, 1, &damage) == 0);
    CHECK(damage.ignored == 1);
    CHECK(damage.effective_points == 0);
    CHECK(state.health == 95);
    CHECK(state.palette_shift.damage_count == 204);

    CHECK(wl_apply_player_damage(&state, WL_DIFFICULTY_HARD, 120, 0, 0, &damage) == 0);
    CHECK(damage.health == 0);
    CHECK(damage.died == 1);
    CHECK(state.play_state == WL_PLAYER_PLAY_DIED);

    CHECK(wl_heal_player(&state, 150) == 0);
    CHECK(state.health == 100);
    CHECK(state.got_gat_gun == 0);

    CHECK(wl_init_player_gameplay_state(&state, 100, 8, 39000, WL_EXTRA_POINTS) == 0);
    CHECK(wl_award_player_points(&state, 101000, &extra_lives, &thresholds) == 0);
    CHECK(state.score == 140000);
    CHECK(state.next_extra == 160000);
    CHECK(state.lives == 9);
    CHECK(extra_lives == 1);
    CHECK(thresholds == 3);
    CHECK(wl_award_player_points(&state, 0, NULL, NULL) == 0);
    CHECK(wl_award_player_points(&state, -1, NULL, NULL) == -1);
    CHECK(wl_apply_player_damage(&state, WL_DIFFICULTY_HARD, -1, 0, 0, &damage) == -1);
    CHECK(wl_heal_player(&state, -1) == -1);

    wl_actor_desc dog;
    memset(&dog, 0, sizeof(dog));
    dog.kind = WL_ACTOR_DOG;
    dog.tile_x = 5;
    dog.tile_y = 4;
    wl_player_motion_state bite_motion = { (3u << 16) + 0x8000u, (4u << 16) + 0x8000u, 3, 4 };
    wl_actor_contact_damage_result bite;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_try_actor_bite_player(&state, &dog, &bite_motion, WL_DIFFICULTY_HARD,
                                   179, 80, 0, 0, &bite) == 0);
    CHECK(bite.in_range == 1);
    CHECK(bite.chance_hit == 1);
    CHECK(bite.damaged == 1);
    CHECK(bite.damage.effective_points == 5);
    CHECK(state.health == 95);
    CHECK(state.palette_shift.damage_count == 5);
    CHECK(wl_update_palette_shift_state(&state.palette_shift, 1, &shift) == 0);
    CHECK(shift.kind == WL_PALETTE_SHIFT_RED);
    bite_motion.x = (2u << 16) + 0x7000u;
    CHECK(wl_try_actor_bite_player(&state, &dog, &bite_motion, WL_DIFFICULTY_HARD,
                                   0, 240, 0, 0, &bite) == 0);
    CHECK(bite.in_range == 0);
    CHECK(state.health == 95);
    bite_motion.x = (4u << 16) + 0x8000u;
    CHECK(wl_try_actor_bite_player(&state, &dog, &bite_motion, WL_DIFFICULTY_BABY,
                                   180, 240, 0, 0, &bite) == 0);
    CHECK(bite.in_range == 1);
    CHECK(bite.chance_hit == 0);
    CHECK(state.health == 95);
    CHECK(wl_try_actor_bite_player(&state, &dog, &bite_motion, WL_DIFFICULTY_BABY,
                                   0, 240, 0, 0, &bite) == 0);
    CHECK(bite.chance_hit == 1);
    CHECK(bite.damage.effective_points == 3);
    CHECK(state.health == 92);
    dog.kind = WL_ACTOR_GUARD;
    CHECK(wl_try_actor_bite_player(&state, &dog, &bite_motion, WL_DIFFICULTY_HARD,
                                   0, 80, 0, 0, &bite) == -1);

    wl_actor_desc shooter;
    memset(&shooter, 0, sizeof(shooter));
    shooter.kind = WL_ACTOR_GUARD;
    shooter.shootable = 1;
    shooter.tile_x = 8;
    shooter.tile_y = 4;
    wl_player_motion_state shot_motion = { (4u << 16) + 0x8000u, (4u << 16) + 0x8000u, 4, 4 };
    wl_actor_shot_damage_result shot;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_try_actor_shoot_player(&state, &shooter, &shot_motion,
                                    WL_DIFFICULTY_HARD, 0, 1, 0, 1,
                                    0, 240, 0, 0, &shot) == 0);
    CHECK(shot.area_active == 0);
    CHECK(shot.line_of_sight == 1);
    CHECK(shot.distance_tiles == 4);
    CHECK(shot.hit_chance == 192);
    CHECK(shot.damaged == 0);
    CHECK(state.health == 100);
    CHECK(wl_try_actor_shoot_player(&state, &shooter, &shot_motion,
                                    WL_DIFFICULTY_HARD, 1, 0, 0, 1,
                                    0, 240, 0, 0, &shot) == 0);
    CHECK(shot.line_of_sight == 0);
    CHECK(shot.damaged == 0);
    CHECK(wl_try_actor_shoot_player(&state, &shooter, &shot_motion,
                                    WL_DIFFICULTY_HARD, 1, 1, 0, 1,
                                    192, 240, 0, 0, &shot) == 0);
    CHECK(shot.chance_hit == 0);
    CHECK(state.health == 100);
    CHECK(wl_try_actor_shoot_player(&state, &shooter, &shot_motion,
                                    WL_DIFFICULTY_HARD, 1, 1, 0, 1,
                                    191, 240, 0, 0, &shot) == 0);
    CHECK(shot.chance_hit == 1);
    CHECK(shot.damage.effective_points == 15);
    CHECK(state.health == 85);
    CHECK(state.palette_shift.damage_count == 15);
    shooter.kind = WL_ACTOR_SS;
    shooter.tile_x = 7;
    CHECK(wl_try_actor_shoot_player(&state, &shooter, &shot_motion,
                                    WL_DIFFICULTY_BABY, 1, 1, 1, 1,
                                    127, 240, 0, 0, &shot) == 0);
    CHECK(shot.distance_tiles == 2);
    CHECK(shot.hit_chance == 128);
    CHECK(shot.damage.requested_points == 30);
    CHECK(shot.damage.effective_points == 7);
    CHECK(state.health == 78);
    shooter.kind = WL_ACTOR_GUARD;
    shooter.tile_x = 5;
    CHECK(wl_try_actor_shoot_player(&state, &shooter, &shot_motion,
                                    WL_DIFFICULTY_HARD, 1, 1, 1, 1,
                                    143, 80, 0, 0, &shot) == 0);
    CHECK(shot.distance_tiles == 1);
    CHECK(shot.hit_chance == 144);
    CHECK(shot.damage.effective_points == 20);
    CHECK(state.health == 58);
    shooter.shootable = 0;
    CHECK(wl_try_actor_shoot_player(&state, &shooter, &shot_motion,
                                    WL_DIFFICULTY_HARD, 1, 1, 1, 1,
                                    0, 80, 0, 0, &shot) == -1);

    wl_actor_combat_state actor_state;
    wl_actor_damage_result actor_damage;
    shooter.kind = WL_ACTOR_GUARD;
    shooter.shootable = 1;
    shooter.tile_x = 6;
    shooter.tile_y = 4;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_init_actor_combat_state(&shooter, WL_DIFFICULTY_HARD,
                                     &actor_state) == 0);
    CHECK(actor_state.hitpoints == 25);
    CHECK(actor_state.attack_mode == 0);
    CHECK(wl_apply_actor_damage(&state, &actor_state, 5, &actor_damage) == 0);
    CHECK(actor_damage.effective_points == 10);
    CHECK(actor_damage.hitpoints == 15);
    CHECK(actor_damage.attack_mode_started == 1);
    CHECK(actor_damage.pain_state_variant == 1);
    CHECK(actor_damage.killed == 0);
    CHECK(state.score == 0);
    CHECK(actor_state.attack_mode == 1);
    CHECK(wl_apply_actor_damage(&state, &actor_state, 15, &actor_damage) == 0);
    CHECK(actor_damage.effective_points == 15);
    CHECK(actor_damage.killed == 1);
    CHECK(actor_damage.hitpoints == 0);
    CHECK(actor_damage.score_awarded == 100);
    CHECK(actor_damage.dropped_item == 1);
    CHECK(actor_damage.drop_item == WL_BONUS_CLIP2);
    CHECK(actor_state.alive == 0);
    CHECK(actor_state.shootable == 0);
    CHECK(state.score == 100);
    wl_actor_death_state death_state;
    wl_actor_death_step_result death_step;
    CHECK(wl_start_actor_death_state(&actor_state, &actor_damage,
                                     &death_state) == 0);
    CHECK(death_state.kind == WL_ACTOR_GUARD);
    CHECK(death_state.stage == 0);
    CHECK(death_state.stage_count == 4);
    CHECK(death_state.sprite_source_index == 91);
    CHECK(death_state.tics_remaining == 15);
    CHECK(death_state.death_scream == 1);
    CHECK(wl_step_actor_death_state(&death_state, 14, &death_step) == 0);
    CHECK(death_step.advanced == 0);
    CHECK(death_step.sprite_source_index == 91);
    CHECK(death_step.tics_remaining == 1);
    CHECK(wl_step_actor_death_state(&death_state, 1, &death_step) == 0);
    CHECK(death_step.advanced == 1);
    CHECK(death_step.stage == 1);
    CHECK(death_step.sprite_source_index == 92);
    CHECK(death_step.tics_remaining == 15);
    CHECK(wl_step_actor_death_state(&death_state, 30, &death_step) == 0);
    CHECK(death_step.advanced == 1);
    CHECK(death_step.finished == 1);
    CHECK(death_step.stage == 3);
    CHECK(death_step.sprite_source_index == 95);
    CHECK(death_step.tics_remaining == 0);
    CHECK(wl_step_actor_death_state(&death_state, 20, &death_step) == 0);
    CHECK(death_step.finished == 1);
    CHECK(death_step.sprite_source_index == 95);
    wl_scene_sprite_ref death_ref;
    CHECK(wl_build_actor_death_scene_ref(&actor_state, &death_state, 106, 7,
                                         &death_ref) == 0);
    CHECK(death_ref.kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(death_ref.model_index == 7);
    CHECK(death_ref.source_index == 95);
    CHECK(death_ref.vswap_chunk_index == 201);
    CHECK(death_ref.world_x == ((uint32_t)actor_state.tile_x << 16) + 0x8000u);
    CHECK(death_ref.world_y == ((uint32_t)actor_state.tile_y << 16) + 0x8000u);
    wl_game_model final_frame_model;
    memset(&final_frame_model, 0, sizeof(final_frame_model));
    final_frame_model.actor_count = 1;
    final_frame_model.actors[0].kind = WL_ACTOR_GUARD;
    final_frame_model.actors[0].mode = WL_ACTOR_STAND;
    final_frame_model.actors[0].shootable = 1;
    final_frame_model.actors[0].tile_x = actor_state.tile_x;
    final_frame_model.actors[0].tile_y = actor_state.tile_y;
    wl_scene_sprite_ref final_frame_refs[1];
    size_t final_frame_ref_count = 0;
    CHECK(wl_collect_scene_sprite_refs(&final_frame_model, 106,
                                       final_frame_refs, 1,
                                       &final_frame_ref_count) == 0);
    CHECK(final_frame_ref_count == 1);
    CHECK(final_frame_refs[0].source_index == 50);
    CHECK(wl_apply_actor_death_final_frame(&final_frame_model, 0,
                                           &death_state) == 0);
    CHECK(final_frame_model.actors[0].mode == WL_ACTOR_INERT);
    CHECK(final_frame_model.actors[0].shootable == 0);
    CHECK(final_frame_model.actors[0].scene_source_override == 1);
    CHECK(wl_collect_scene_sprite_refs(&final_frame_model, 106,
                                       final_frame_refs, 1,
                                       &final_frame_ref_count) == 0);
    CHECK(final_frame_ref_count == 1);
    CHECK(final_frame_refs[0].kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(final_frame_refs[0].model_index == 0);
    CHECK(final_frame_refs[0].source_index == death_state.sprite_source_index);
    CHECK(final_frame_refs[0].source_index == 95);
    CHECK(final_frame_refs[0].vswap_chunk_index == 201);
    CHECK(final_frame_refs[0].world_x == death_ref.world_x);
    CHECK(final_frame_refs[0].world_y == death_ref.world_y);
    death_state.finished = 0;
    CHECK(wl_apply_actor_death_final_frame(&final_frame_model, 0,
                                           &death_state) == -1);
    death_state.finished = 1;
    CHECK(wl_apply_actor_death_final_frame(&final_frame_model, 1,
                                           &death_state) == -1);

    struct death_final_expectation {
        wl_actor_kind kind;
        int32_t damage_points;
        uint16_t final_source;
        uint16_t final_chunk;
    } death_finals[] = {
        { WL_ACTOR_OFFICER, 25, 284, 390 },
        { WL_ACTOR_SS, 50, 183, 289 },
        { WL_ACTOR_DOG, 1, 134, 240 },
        { WL_ACTOR_MUTANT, 33, 233, 339 },
        { WL_ACTOR_BOSS, 600, 303, 409 },
    };
    for (size_t i = 0; i < sizeof(death_finals) / sizeof(death_finals[0]); ++i) {
        wl_actor_desc final_actor_desc;
        memset(&final_actor_desc, 0, sizeof(final_actor_desc));
        final_actor_desc.kind = death_finals[i].kind;
        final_actor_desc.shootable = 1;
        final_actor_desc.tile_x = (uint16_t)(10 + i);
        final_actor_desc.tile_y = 12;
        wl_actor_combat_state final_actor_state;
        CHECK(wl_init_actor_combat_state(&final_actor_desc,
                                         WL_DIFFICULTY_HARD,
                                         &final_actor_state) == 0);
        wl_actor_damage_result final_damage;
        CHECK(wl_apply_actor_damage(&state, &final_actor_state,
                                    death_finals[i].damage_points,
                                    &final_damage) == 0);
        CHECK(final_damage.killed == 1);
        wl_actor_death_state final_death_state;
        CHECK(wl_start_actor_death_state(&final_actor_state, &final_damage,
                                         &final_death_state) == 0);
        wl_actor_death_step_result final_death_step;
        CHECK(wl_step_actor_death_state(&final_death_state, 120,
                                        &final_death_step) == 0);
        CHECK(final_death_step.finished == 1);
        CHECK(final_death_step.sprite_source_index == death_finals[i].final_source);
        wl_game_model final_actor_model;
        memset(&final_actor_model, 0, sizeof(final_actor_model));
        final_actor_model.actor_count = 1;
        final_actor_model.actors[0] = final_actor_desc;
        CHECK(wl_apply_actor_death_final_frame(&final_actor_model, 0,
                                               &final_death_state) == 0);
        CHECK(final_actor_model.actors[0].shootable == 0);
        CHECK(final_actor_model.actors[0].scene_source_override == 1);
        CHECK(final_actor_model.actors[0].scene_source_index == death_finals[i].final_source);
        CHECK(wl_collect_scene_sprite_refs(&final_actor_model, 106,
                                           final_frame_refs, 1,
                                           &final_frame_ref_count) == 0);
        CHECK(final_frame_ref_count == 1);
        CHECK(final_frame_refs[0].source_index == death_finals[i].final_source);
        CHECK(final_frame_refs[0].vswap_chunk_index == death_finals[i].final_chunk);
    }

    wl_game_model death_tick_model;
    memset(&death_tick_model, 0, sizeof(death_tick_model));
    death_tick_model.actor_count = 1;
    death_tick_model.actors[0].kind = WL_ACTOR_GUARD;
    death_tick_model.actors[0].mode = WL_ACTOR_STAND;
    death_tick_model.actors[0].shootable = 1;
    death_tick_model.actors[0].tile_x = actor_state.tile_x;
    death_tick_model.actors[0].tile_y = actor_state.tile_y;
    CHECK(wl_start_actor_death_state(&actor_state, &actor_damage,
                                     &death_state) == 0);
    wl_live_actor_death_tick_result live_death_tick;
    CHECK(wl_step_live_actor_death_tick(&death_tick_model, 0,
                                        &death_state, 15, 106,
                                        &live_death_tick) == 0);
    CHECK(live_death_tick.step.advanced == 1);
    CHECK(live_death_tick.step.sprite_source_index == 92);
    CHECK(live_death_tick.death_ref_built == 1);
    CHECK(live_death_tick.final_frame_applied == 0);
    CHECK(live_death_tick.death_ref.source_index == 92);
    CHECK(live_death_tick.death_ref.vswap_chunk_index == 198);
    CHECK(death_tick_model.actors[0].scene_source_override == 0);
    CHECK(wl_step_live_actor_death_tick(&death_tick_model, 0,
                                        &death_state, 30, 106,
                                        &live_death_tick) == 0);
    CHECK(live_death_tick.step.finished == 1);
    CHECK(live_death_tick.step.sprite_source_index == 95);
    CHECK(live_death_tick.death_ref.source_index == 95);
    CHECK(live_death_tick.death_ref.vswap_chunk_index == 201);
    CHECK(live_death_tick.death_ref.world_x == death_ref.world_x);
    CHECK(live_death_tick.death_ref.world_y == death_ref.world_y);
    CHECK(live_death_tick.final_frame_applied == 1);
    CHECK(death_tick_model.actors[0].mode == WL_ACTOR_INERT);
    CHECK(death_tick_model.actors[0].shootable == 0);
    CHECK(death_tick_model.actors[0].scene_source_override == 1);
    CHECK(wl_collect_scene_sprite_refs(&death_tick_model, 106,
                                       final_frame_refs, 1,
                                       &final_frame_ref_count) == 0);
    CHECK(final_frame_ref_count == 1);
    CHECK(final_frame_refs[0].source_index == 95);
    CHECK(final_frame_refs[0].vswap_chunk_index == 201);
    CHECK(wl_step_live_actor_death_tick(&death_tick_model, 1,
                                        &death_state, 1, 106,
                                        &live_death_tick) == -1);
    wl_game_model drop_model;
    memset(&drop_model, 0, sizeof(drop_model));
    size_t drop_static_index = 999;
    CHECK(wl_spawn_actor_drop_static(&drop_model, &actor_state, &actor_damage,
                                     &drop_static_index) == 0);
    CHECK(drop_static_index == 0);
    CHECK(drop_model.static_count == 1);
    CHECK(drop_model.bonus_static_count == 1);
    CHECK(drop_model.statics[0].x == actor_state.tile_x);
    CHECK(drop_model.statics[0].y == actor_state.tile_y);
    CHECK(drop_model.statics[0].source_tile == 71);
    CHECK(drop_model.statics[0].type == 48);
    CHECK(drop_model.statics[0].bonus == 1);
    CHECK(drop_model.statics[0].blocking == 0);
    CHECK(drop_model.statics[0].active == 1);
    state.ammo = 0;
    uint8_t drop_picked_up = 0;
    CHECK(wl_try_pickup_static_bonus(&state, &drop_model.statics[0],
                                     &drop_picked_up) == 0);
    CHECK(drop_picked_up == 1);
    CHECK(state.ammo == 4);
    CHECK(drop_model.statics[0].active == 0);
    CHECK(wl_apply_actor_damage(&state, &actor_state, 1, &actor_damage) == -1);

    shooter.kind = WL_ACTOR_SS;
    shooter.shootable = 1;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 39500, WL_EXTRA_POINTS) == 0);
    state.best_weapon = WL_WEAPON_PISTOL;
    CHECK(wl_init_actor_combat_state(&shooter, WL_DIFFICULTY_HARD,
                                     &actor_state) == 0);
    actor_state.attack_mode = 1;
    CHECK(wl_apply_actor_damage(&state, &actor_state, 100, &actor_damage) == 0);
    CHECK(actor_damage.killed == 1);
    CHECK(actor_damage.score_awarded == 500);
    CHECK(actor_damage.dropped_item == 1);
    CHECK(actor_damage.drop_item == WL_BONUS_MACHINEGUN);
    CHECK(actor_damage.extra_lives_awarded == 1);
    CHECK(actor_damage.score_thresholds_crossed == 1);
    CHECK(state.score == 40000);
    CHECK(state.lives == 4);
    memset(&drop_model, 0, sizeof(drop_model));
    CHECK(wl_spawn_actor_drop_static(&drop_model, &actor_state, &actor_damage,
                                     &drop_static_index) == 0);
    CHECK(drop_model.statics[0].source_tile == 50);
    CHECK(drop_model.statics[0].type == 27);

    dog.kind = WL_ACTOR_DOG;
    dog.shootable = 1;
    CHECK(wl_init_actor_combat_state(&dog, WL_DIFFICULTY_HARD,
                                     &actor_state) == 0);
    CHECK(wl_apply_actor_damage(&state, &actor_state, 1, &actor_damage) == 0);
    CHECK(actor_damage.killed == 1);
    CHECK(actor_damage.dropped_item == 0);
    CHECK(wl_start_actor_death_state(&actor_state, &actor_damage,
                                     &death_state) == 0);
    CHECK(death_state.sprite_source_index == 131);
    CHECK(wl_step_actor_death_state(&death_state, 45, &death_step) == 0);
    CHECK(death_step.stage == 3);
    CHECK(death_step.sprite_source_index == 134);
    CHECK(death_step.finished == 0);
    CHECK(death_step.tics_remaining == 15);
    CHECK(wl_spawn_actor_drop_static(&drop_model, &actor_state, &actor_damage,
                                     &drop_static_index) == 0);
    CHECK(drop_model.static_count == 1);

    uint16_t live_empty_wall[WL_MAP_PLANE_WORDS];
    uint16_t live_empty_info[WL_MAP_PLANE_WORDS];
    memset(live_empty_wall, 0, sizeof(live_empty_wall));
    memset(live_empty_info, 0, sizeof(live_empty_info));
    wl_game_model live_damage_model;
    memset(&live_damage_model, 0, sizeof(live_damage_model));
    wl_player_motion_state live_damage_motion = {
        (4u << 16) + 0x8000u,
        (4u << 16) + 0x8000u,
        4,
        4,
    };
    wl_live_actor_damage_tick_result live_damage;
    shooter.kind = WL_ACTOR_GUARD;
    shooter.shootable = 1;
    shooter.tile_x = 7;
    shooter.tile_y = 4;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_init_actor_combat_state(&shooter, WL_DIFFICULTY_HARD,
                                     &actor_state) == 0);
    CHECK(wl_step_live_actor_damage_tick(&state, &live_damage_model,
                                         live_empty_wall, live_empty_info,
                                         WL_MAP_PLANE_WORDS, &live_damage_motion,
                                         0, 0, 0x10000, 0, WL_DIR_EAST,
                                         0, 0, &actor_state, 25, 1,
                                         &live_damage) == 0);
    CHECK(live_damage.actor_damaged == 1);
    CHECK(live_damage.actor_damage.killed == 1);
    CHECK(live_damage.actor_damage.score_awarded == 100);
    CHECK(live_damage.actor_damage.dropped_item == 1);
    CHECK(live_damage.drop_spawned == 1);
    CHECK(live_damage.drop_static_index == 0);
    CHECK(live_damage_model.static_count == 1);
    CHECK(live_damage_model.bonus_static_count == 1);
    CHECK(live_damage_model.statics[0].x == 7);
    CHECK(live_damage_model.statics[0].y == 4);
    CHECK(live_damage_model.statics[0].type == 48);
    CHECK(live_damage_model.statics[0].active == 1);
    CHECK(state.score == 100);
    CHECK(actor_state.alive == 0);
    CHECK(live_damage.live.palette.kind == WL_PALETTE_SHIFT_NONE);
    CHECK(wl_step_live_actor_damage_tick(&state, &live_damage_model,
                                         live_empty_wall, live_empty_info,
                                         WL_MAP_PLANE_WORDS, &live_damage_motion,
                                         0, 0, 0x10000, 0, WL_DIR_EAST,
                                         0, 0, NULL, 0, 1,
                                         &live_damage) == 0);
    CHECK(live_damage.actor_damaged == 0);
    CHECK(live_damage.drop_spawned == 0);
    CHECK(live_damage.drop_static_index == 1);
    CHECK(live_damage_model.static_count == 1);

    shooter.kind = WL_ACTOR_BOSS;
    shooter.shootable = 1;
    CHECK(wl_init_actor_combat_state(&shooter, WL_DIFFICULTY_HARD,
                                     &actor_state) == 0);
    CHECK(actor_state.hitpoints == 1200);
    CHECK(wl_apply_actor_damage(&state, &actor_state, 600, &actor_damage) == 0);
    CHECK(actor_damage.killed == 1);
    CHECK(actor_damage.score_awarded == 5000);
    CHECK(actor_damage.drop_item == WL_BONUS_KEY1);
    CHECK(wl_start_actor_death_state(&actor_state, &actor_damage,
                                     &death_state) == 0);
    CHECK(death_state.sprite_source_index == 304);
    CHECK(wl_step_actor_death_state(&death_state, 45, &death_step) == 0);
    CHECK(death_step.finished == 1);
    CHECK(death_step.sprite_source_index == 303);
    shooter.kind = WL_ACTOR_DEAD_GUARD;
    CHECK(wl_init_actor_combat_state(&shooter, WL_DIFFICULTY_HARD,
                                     &actor_state) == -1);

    wl_game_model projectile_model;
    memset(&projectile_model, 0, sizeof(projectile_model));
    projectile_model.tilemap[5 + 4 * WL_MAP_SIDE] = 1;
    wl_projectile_state projectile = {
        (4u << 16) + 0xf000u, (4u << 16) + 0x8000u, 4, 4,
        WL_PROJECTILE_ROCKET, 1,
    };
    wl_projectile_step_result projectile_step;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_projectile(&state, &projectile_model, &shot_motion,
                             &projectile, WL_DIFFICULTY_HARD,
                             0x1000, 0, 80, 0, 0,
                             &projectile_step) == 0);
    CHECK(projectile_step.moved == 1);
    CHECK(projectile_step.hit_wall == 1);
    CHECK(projectile_step.removed == 1);
    CHECK(projectile.active == 0);
    CHECK(state.health == 100);
    memset(&projectile_model, 0, sizeof(projectile_model));
    projectile.x = (3u << 16) + 0x8000u;
    projectile.y = (4u << 16) + 0x8000u;
    projectile.tile_x = 3;
    projectile.tile_y = 4;
    projectile.kind = WL_PROJECTILE_NEEDLE;
    projectile.active = 1;
    CHECK(wl_step_projectile(&state, &projectile_model, &shot_motion,
                             &projectile, WL_DIFFICULTY_HARD,
                             0x10000, 0, 80, 0, 0,
                             &projectile_step) == 0);
    CHECK(projectile_step.hit_player == 1);
    CHECK(projectile_step.damage.requested_points == 30);
    CHECK(projectile_step.damage.effective_points == 30);
    CHECK(projectile_step.tile_x == 4);
    CHECK(projectile.active == 0);
    CHECK(state.health == 70);
    CHECK(state.palette_shift.damage_count == 30);
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    projectile.x = (4u << 16) + 0x8000u;
    projectile.y = (5u << 16) + 0x3000u;
    projectile.tile_x = 4;
    projectile.tile_y = 5;
    projectile.kind = WL_PROJECTILE_ROCKET;
    projectile.active = 1;
    CHECK(wl_step_projectile(&state, &projectile_model, &shot_motion,
                             &projectile, WL_DIFFICULTY_BABY,
                             0, -0x10000, 80, 0, 0,
                             &projectile_step) == 0);
    CHECK(projectile_step.hit_player == 1);
    CHECK(projectile_step.damage.requested_points == 40);
    CHECK(projectile_step.damage.effective_points == 10);
    CHECK(state.health == 90);
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    projectile.x = (2u << 16) + 0x8000u;
    projectile.y = (4u << 16) + 0x8000u;
    projectile.tile_x = 2;
    projectile.tile_y = 4;
    projectile.kind = WL_PROJECTILE_FIRE;
    projectile.active = 1;
    CHECK(wl_step_projectile(&state, &projectile_model, &shot_motion,
                             &projectile, WL_DIFFICULTY_HARD,
                             0x10000, 0, 80, 0, 0,
                             &projectile_step) == 0);
    CHECK(projectile_step.hit_player == 0);
    CHECK(projectile_step.removed == 0);
    CHECK(projectile.active == 1);
    CHECK(projectile.tile_x == 3);
    CHECK(wl_step_projectile(&state, &projectile_model, &shot_motion,
                             &projectile, WL_DIFFICULTY_HARD,
                             0x10000, 0, 80, 0, 0,
                             &projectile_step) == 0);
    CHECK(projectile_step.hit_player == 1);
    CHECK(projectile_step.damage.effective_points == 10);
    CHECK(state.health == 90);
    CHECK(wl_step_projectile(&state, &projectile_model, &shot_motion,
                             &projectile, WL_DIFFICULTY_HARD,
                             0, 0, 80, 0, 0,
                             &projectile_step) == -1);

    uint16_t live_wall[WL_MAP_PLANE_WORDS];
    uint16_t live_info[WL_MAP_PLANE_WORDS];
    memset(live_wall, 0, sizeof(live_wall));
    memset(live_info, 0, sizeof(live_info));
    wl_live_projectile_tick_result live_projectile;
    wl_player_motion_state live_projectile_motion = {
        (4u << 16) + 0x8000u, (4u << 16) + 0x8000u, 4, 4,
    };
    projectile.x = (3u << 16) + 0x8000u;
    projectile.y = (4u << 16) + 0x8000u;
    projectile.tile_x = 3;
    projectile.tile_y = 4;
    projectile.kind = WL_PROJECTILE_NEEDLE;
    projectile.active = 1;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_projectile_tick(&state, &projectile_model,
                                       live_wall, live_info, WL_MAP_PLANE_WORDS,
                                       &live_projectile_motion, 0, 0,
                                       0x10000, 0, WL_DIR_EAST, 0, 0,
                                       &projectile, WL_DIFFICULTY_HARD,
                                       0x10000, 0, 80, 0, 0, 1,
                                       &live_projectile) == 0);
    CHECK(live_projectile.projectile_stepped == 1);
    CHECK(live_projectile.projectile.hit_player == 1);
    CHECK(live_projectile.projectile.damage.effective_points == 30);
    CHECK(live_projectile.live.palette.kind == WL_PALETTE_SHIFT_RED);
    CHECK(live_projectile.live.palette.shift_index == 3);
    CHECK(live_projectile.live.palette.damage_count == 29);
    CHECK(state.health == 70);
    CHECK(projectile.active == 0);
    CHECK(wl_step_live_projectile_tick(&state, &projectile_model,
                                       live_wall, live_info, WL_MAP_PLANE_WORDS,
                                       &live_projectile_motion, 0, 0,
                                       0x10000, 0, WL_DIR_EAST, 0, 0,
                                       NULL, WL_DIFFICULTY_HARD,
                                       0, 0, 0, 0, 0, 1,
                                       &live_projectile) == 0);
    CHECK(live_projectile.projectile_stepped == 0);
    CHECK(live_projectile.live.palette.kind == WL_PALETTE_SHIFT_RED);

    wl_live_actor_tick_result live_actor;
    wl_player_motion_state live_actor_motion = {
        (4u << 16) + 0x8000u, (4u << 16) + 0x8000u, 4, 4,
    };
    dog.kind = WL_ACTOR_DOG;
    dog.tile_x = 5;
    dog.tile_y = 4;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_actor_tick(&state, &projectile_model,
                                  live_wall, live_info, WL_MAP_PLANE_WORDS,
                                  &live_actor_motion, 0, 0,
                                  0x10000, 0, WL_DIR_EAST, 0, 0,
                                  &dog, WL_DIFFICULTY_HARD,
                                  1, 1, 0, 1, 179, 80, 0, 0, 1,
                                  &live_actor) == 0);
    CHECK(live_actor.actor_attacked == 1);
    CHECK(live_actor.attack_kind == WL_LIVE_ACTOR_ATTACK_BITE);
    CHECK(live_actor.bite.damaged == 1);
    CHECK(live_actor.bite.damage.effective_points == 5);
    CHECK(live_actor.live.palette.kind == WL_PALETTE_SHIFT_RED);
    CHECK(live_actor.live.palette.shift_index == 0);
    CHECK(live_actor.live.palette.damage_count == 4);
    CHECK(state.health == 95);

    shooter.kind = WL_ACTOR_GUARD;
    shooter.shootable = 1;
    shooter.tile_x = 5;
    shooter.tile_y = 4;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_actor_tick(&state, &projectile_model,
                                  live_wall, live_info, WL_MAP_PLANE_WORDS,
                                  &live_actor_motion, 0, 0,
                                  0x10000, 0, WL_DIR_EAST, 0, 0,
                                  &shooter, WL_DIFFICULTY_HARD,
                                  1, 1, 1, 1, 143, 80, 0, 0, 1,
                                  &live_actor) == 0);
    CHECK(live_actor.actor_attacked == 1);
    CHECK(live_actor.attack_kind == WL_LIVE_ACTOR_ATTACK_SHOOT);
    CHECK(live_actor.shot.damaged == 1);
    CHECK(live_actor.shot.distance_tiles == 1);
    CHECK(live_actor.shot.damage.effective_points == 20);
    CHECK(live_actor.live.palette.kind == WL_PALETTE_SHIFT_RED);
    CHECK(live_actor.live.palette.shift_index == 2);
    CHECK(live_actor.live.palette.damage_count == 19);
    CHECK(state.health == 80);

    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_actor_tick(&state, &projectile_model,
                                  live_wall, live_info, WL_MAP_PLANE_WORDS,
                                  &live_actor_motion, 0, 0,
                                  0x10000, 0, WL_DIR_EAST, 0, 0,
                                  NULL, WL_DIFFICULTY_HARD,
                                  0, 0, 0, 0, 0, 0, 0, 0, 1,
                                  &live_actor) == 0);
    CHECK(live_actor.actor_attacked == 0);
    CHECK(live_actor.attack_kind == WL_LIVE_ACTOR_ATTACK_NONE);
    CHECK(live_actor.live.palette.kind == WL_PALETTE_SHIFT_NONE);

    wl_live_combat_tick_result live_combat;
    dog.kind = WL_ACTOR_DOG;
    dog.tile_x = 5;
    dog.tile_y = 4;
    projectile.x = (3u << 16) + 0x8000u;
    projectile.y = (4u << 16) + 0x8000u;
    projectile.tile_x = 3;
    projectile.tile_y = 4;
    projectile.kind = WL_PROJECTILE_NEEDLE;
    projectile.active = 1;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_combat_tick(&state, &projectile_model,
                                   live_wall, live_info, WL_MAP_PLANE_WORDS,
                                   &live_actor_motion, 0, 0,
                                   0x10000, 0, WL_DIR_EAST, 0, 0,
                                   &dog, &projectile, WL_DIFFICULTY_HARD,
                                   1, 1, 0, 1, 179, 80,
                                   0x10000, 0, 80, 0, 0, 1,
                                   &live_combat) == 0);
    CHECK(live_combat.actor_attacked == 1);
    CHECK(live_combat.actor_attack_kind == WL_LIVE_ACTOR_ATTACK_BITE);
    CHECK(live_combat.bite.damaged == 1);
    CHECK(live_combat.bite.damage.effective_points == 5);
    CHECK(live_combat.projectile_stepped == 1);
    CHECK(live_combat.projectile.hit_player == 1);
    CHECK(live_combat.projectile.damage.effective_points == 30);
    CHECK(live_combat.live.palette.kind == WL_PALETTE_SHIFT_RED);
    CHECK(live_combat.live.palette.shift_index == 3);
    CHECK(live_combat.live.palette.damage_count == 34);
    CHECK(state.health == 65);
    CHECK(projectile.active == 0);

    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_combat_tick(&state, &projectile_model,
                                   live_wall, live_info, WL_MAP_PLANE_WORDS,
                                   &live_actor_motion, 0, 0,
                                   0x10000, 0, WL_DIR_EAST, 0, 0,
                                   NULL, NULL, WL_DIFFICULTY_HARD,
                                   0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 1,
                                   &live_combat) == 0);
    CHECK(live_combat.actor_attacked == 0);
    CHECK(live_combat.projectile_stepped == 0);
    CHECK(live_combat.live.palette.kind == WL_PALETTE_SHIFT_NONE);

    wl_live_full_combat_tick_result full_combat;
    wl_game_model full_combat_model;
    memset(&full_combat_model, 0, sizeof(full_combat_model));
    full_combat_model.actor_count = 10;
    full_combat_model.actors[9].kind = WL_ACTOR_GUARD;
    full_combat_model.actors[9].mode = WL_ACTOR_STAND;
    full_combat_model.actors[9].shootable = 1;
    full_combat_model.actors[9].tile_x = 7;
    full_combat_model.actors[9].tile_y = 4;
    wl_actor_desc full_target_desc;
    memset(&full_target_desc, 0, sizeof(full_target_desc));
    full_target_desc.kind = WL_ACTOR_GUARD;
    full_target_desc.shootable = 1;
    full_target_desc.tile_x = 7;
    full_target_desc.tile_y = 4;
    wl_actor_combat_state full_target;
    CHECK(wl_init_actor_combat_state(&full_target_desc, WL_DIFFICULTY_HARD,
                                     &full_target) == 0);
    dog.kind = WL_ACTOR_DOG;
    dog.tile_x = 5;
    dog.tile_y = 4;
    projectile.x = (3u << 16) + 0x8000u;
    projectile.y = (4u << 16) + 0x8000u;
    projectile.tile_x = 3;
    projectile.tile_y = 4;
    projectile.kind = WL_PROJECTILE_NEEDLE;
    projectile.active = 1;
    live_actor_motion.x = (4u << 16) + 0x8000u;
    live_actor_motion.y = (4u << 16) + 0x8000u;
    live_actor_motion.tile_x = 4;
    live_actor_motion.tile_y = 4;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_full_combat_tick(&state, &full_combat_model,
                                        live_wall, live_info, WL_MAP_PLANE_WORDS,
                                        &live_actor_motion, 0, 0,
                                        0x10000, 0, WL_DIR_EAST, 0, 0,
                                        &dog, &projectile, &full_target, 25,
                                        9, 106, WL_DIFFICULTY_HARD,
                                        1, 1, 0, 1, 179, 80,
                                        0x10000, 0, 80, 0, 0, 1,
                                        &full_combat) == 0);
    CHECK(full_combat.actor_damaged == 1);
    CHECK(full_combat.actor_damage.killed == 1);
    CHECK(full_combat.actor_damage.score_awarded == 100);
    CHECK(full_combat.drop_spawned == 1);
    CHECK(full_combat.drop_static_index == 0);
    CHECK(full_combat.death_started == 1);
    CHECK(full_combat.death_ref_built == 1);
    CHECK(full_combat.actor_death.sprite_source_index == 91);
    CHECK(full_combat.actor_death_ref.model_index == 9);
    CHECK(full_combat.actor_death_ref.source_index == 91);
    CHECK(full_combat.actor_death_ref.vswap_chunk_index == 197);
    CHECK(full_combat_model.static_count == 1);
    CHECK(full_combat_model.statics[0].type == 48);
    CHECK(full_combat.actor_attacked == 1);
    CHECK(full_combat.actor_attack_kind == WL_LIVE_ACTOR_ATTACK_BITE);
    CHECK(full_combat.bite.damage.effective_points == 5);
    CHECK(full_combat.projectile_stepped == 1);
    CHECK(full_combat.projectile.damage.effective_points == 30);
    CHECK(full_combat.live.palette.kind == WL_PALETTE_SHIFT_RED);
    CHECK(full_combat.live.palette.shift_index == 3);
    CHECK(full_combat.live.palette.damage_count == 34);
    CHECK(state.health == 65);
    CHECK(state.score == 100);
    CHECK(projectile.active == 0);
    CHECK(full_target.alive == 0);

    wl_actor_death_state active_full_death = full_combat.actor_death;
    wl_live_full_combat_death_tick_result full_death_tick;
    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_full_combat_death_tick(&state, &full_combat_model,
                                              live_wall, live_info,
                                              WL_MAP_PLANE_WORDS,
                                              &live_actor_motion, 0, 0,
                                              0x10000, 0, WL_DIR_EAST, 0, 0,
                                              NULL, NULL, NULL, 0,
                                              0, 106, WL_DIFFICULTY_HARD,
                                              0, 0, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0, 45,
                                              9, &active_full_death,
                                              &full_death_tick) == 0);
    CHECK(full_death_tick.combat.actor_damaged == 0);
    CHECK(full_death_tick.combat.drop_spawned == 0);
    CHECK(full_death_tick.combat.drop_static_index == 1);
    CHECK(full_death_tick.combat.live.palette.kind == WL_PALETTE_SHIFT_NONE);
    CHECK(full_death_tick.death_stepped == 1);
    CHECK(full_death_tick.death.step.finished == 1);
    CHECK(full_death_tick.death.death_ref_built == 1);
    CHECK(full_death_tick.death.death_ref.model_index == 9);
    CHECK(full_death_tick.death.death_ref.source_index == 95);
    CHECK(full_death_tick.death.death_ref.vswap_chunk_index == 201);
    CHECK(full_death_tick.death.final_frame_applied == 1);
    CHECK(full_combat_model.actors[9].mode == WL_ACTOR_INERT);
    CHECK(full_combat_model.actors[9].shootable == 0);
    CHECK(full_combat_model.actors[9].scene_source_override == 1);
    CHECK(full_combat_model.actors[9].scene_source_index == 95);

    CHECK(wl_init_player_gameplay_state(&state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_full_combat_tick(&state, &full_combat_model,
                                        live_wall, live_info, WL_MAP_PLANE_WORDS,
                                        &live_actor_motion, 0, 0,
                                        0x10000, 0, WL_DIR_EAST, 0, 0,
                                        NULL, NULL, NULL, 0,
                                        0, 106, WL_DIFFICULTY_HARD,
                                        0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 1,
                                        &full_combat) == 0);
    CHECK(full_combat.actor_damaged == 0);
    CHECK(full_combat.drop_spawned == 0);
    CHECK(full_combat.actor_attacked == 0);
    CHECK(full_combat.projectile_stepped == 0);
    CHECK(full_combat.death_started == 0);
    CHECK(full_combat.death_ref_built == 0);
    CHECK(full_combat.drop_static_index == 1);
    CHECK(full_combat_model.static_count == 1);
    CHECK(full_combat.live.palette.kind == WL_PALETTE_SHIFT_NONE);

    uint8_t picked_up = 255;
    CHECK(wl_init_player_gameplay_state(&state, 50, 2, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_FOOD, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK(state.health == 60);
    CHECK(state.palette_shift.bonus_count == WL_NUM_WHITE_SHIFTS * WL_WHITE_SHIFT_TICS);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_FIRSTAID, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK(state.health == 85);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_GIBS, &picked_up) == 0);
    CHECK(picked_up == 0);
    CHECK(state.health == 85);
    CHECK(wl_heal_player(&state, 50) == 0);
    CHECK(state.health == 100);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_FIRSTAID, &picked_up) == 0);
    CHECK(picked_up == 0);

    state.ammo = 0;
    state.weapon = WL_WEAPON_KNIFE;
    state.chosen_weapon = WL_WEAPON_PISTOL;
    state.attack_frame = 0;
    CHECK(wl_give_player_ammo(&state, 8) == 0);
    CHECK(state.ammo == 8);
    CHECK(state.weapon == WL_WEAPON_PISTOL);
    state.ammo = 98;
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_CLIP, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK(state.ammo == 99);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_CLIP2, &picked_up) == 0);
    CHECK(picked_up == 0);
    CHECK(state.ammo == 99);

    CHECK(wl_apply_player_bonus(&state, WL_BONUS_KEY3, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK((state.keys & (1u << 2)) != 0);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_CROWN, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK(state.score == 5000);
    CHECK(state.treasure_count == 1);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_MACHINEGUN, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK(state.best_weapon == WL_WEAPON_MACHINEGUN);
    CHECK(state.weapon == WL_WEAPON_MACHINEGUN);
    CHECK(state.chosen_weapon == WL_WEAPON_MACHINEGUN);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_CHAINGUN, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK(state.best_weapon == WL_WEAPON_CHAINGUN);
    CHECK(state.got_gat_gun == 1);

    wl_player_fire_result fire;
    state.ammo = 2;
    CHECK(wl_try_player_fire_weapon(&state, WL_WEAPON_CHAINGUN, &fire) == 0);
    CHECK(fire.fired == 1);
    CHECK(fire.consumed_ammo == 1);
    CHECK(fire.ammo_before == 2);
    CHECK(fire.ammo_after == 1);
    CHECK(state.ammo == 1);
    CHECK(state.weapon == WL_WEAPON_CHAINGUN);
    CHECK(state.chosen_weapon == WL_WEAPON_CHAINGUN);
    CHECK(wl_try_player_fire_weapon(&state, WL_WEAPON_KNIFE, &fire) == 0);
    CHECK(fire.fired == 1);
    CHECK(fire.consumed_ammo == 0);
    CHECK(fire.ammo_after == 1);
    CHECK(state.weapon == WL_WEAPON_KNIFE);
    state.ammo = 0;
    CHECK(wl_try_player_fire_weapon(&state, WL_WEAPON_PISTOL, &fire) == 0);
    CHECK(fire.fired == 0);
    CHECK(fire.no_ammo == 1);
    CHECK(fire.fired_weapon == WL_WEAPON_KNIFE);
    CHECK(state.weapon == WL_WEAPON_KNIFE);
    state.best_weapon = WL_WEAPON_PISTOL;
    CHECK(wl_try_player_fire_weapon(&state, WL_WEAPON_CHAINGUN, &fire) == 0);
    CHECK(fire.unavailable == 1);
    CHECK(wl_try_player_fire_weapon(&state, (wl_weapon_type)4, &fire) == -1);

    wl_player_fire_attack_result fire_attack;
    state.best_weapon = WL_WEAPON_CHAINGUN;
    state.weapon = WL_WEAPON_CHAINGUN;
    state.chosen_weapon = WL_WEAPON_CHAINGUN;
    state.ammo = 3;
    state.attack_frame = 0;
    CHECK(wl_start_player_fire_attack(&state, WL_WEAPON_CHAINGUN, &fire_attack) == 0);
    CHECK(fire_attack.fire.fired == 1);
    CHECK(fire_attack.fire.consumed_ammo == 1);
    CHECK(fire_attack.attack_started == 1);
    CHECK(fire_attack.attack_frame_before == 0);
    CHECK(fire_attack.attack_frame_after == 2);
    CHECK(state.attack_frame == 2);
    CHECK(state.ammo == 2);
    state.ammo = 0;
    state.weapon = WL_WEAPON_PISTOL;
    state.chosen_weapon = WL_WEAPON_PISTOL;
    state.attack_frame = 7;
    CHECK(wl_start_player_fire_attack(&state, WL_WEAPON_PISTOL, &fire_attack) == 0);
    CHECK(fire_attack.fire.fired == 0);
    CHECK(fire_attack.fire.no_ammo == 1);
    CHECK(fire_attack.attack_started == 0);
    CHECK(fire_attack.attack_frame_after == 7);
    CHECK(state.attack_frame == 7);
    CHECK(state.weapon == WL_WEAPON_KNIFE);
    state.best_weapon = WL_WEAPON_PISTOL;
    CHECK(wl_start_player_fire_attack(&state, WL_WEAPON_CHAINGUN, &fire_attack) == 0);
    CHECK(fire_attack.fire.unavailable == 1);
    CHECK(fire_attack.attack_started == 0);
    CHECK(wl_start_player_fire_attack(&state, (wl_weapon_type)4, &fire_attack) == -1);

    wl_player_attack_step_result attack_step;
    state.best_weapon = WL_WEAPON_CHAINGUN;
    state.chosen_weapon = WL_WEAPON_CHAINGUN;
    state.weapon = WL_WEAPON_KNIFE;
    state.ammo = 4;
    state.attack_frame = 6;
    CHECK(wl_step_player_attack_state(&state, 2, &attack_step) == 0);
    CHECK(attack_step.advanced == 1);
    CHECK(attack_step.finished == 0);
    CHECK(attack_step.frame_before == 6);
    CHECK(attack_step.frame_after == 4);
    CHECK(state.weapon == WL_WEAPON_KNIFE);
    CHECK(wl_step_player_attack_state(&state, 6, &attack_step) == 0);
    CHECK(attack_step.finished == 1);
    CHECK(attack_step.restored_chosen_weapon == 1);
    CHECK(attack_step.frame_after == 0);
    CHECK(state.attack_frame == 0);
    CHECK(state.weapon == WL_WEAPON_CHAINGUN);
    state.attack_frame = 3;
    state.weapon = WL_WEAPON_KNIFE;
    state.ammo = 0;
    CHECK(wl_step_player_attack_state(&state, 3, &attack_step) == 0);
    CHECK(attack_step.finished == 1);
    CHECK(attack_step.restored_chosen_weapon == 0);
    CHECK(state.weapon == WL_WEAPON_KNIFE);
    CHECK(wl_step_player_attack_state(&state, -1, &attack_step) == -1);

    state.best_weapon = WL_WEAPON_MACHINEGUN;
    state.chosen_weapon = WL_WEAPON_PISTOL;
    state.weapon = WL_WEAPON_PISTOL;
    state.ammo = 2;
    state.attack_frame = 0;
    CHECK(wl_try_player_fire_weapon_attack(&state, WL_WEAPON_MACHINEGUN, 7,
                                           &fire_attack) == 0);
    CHECK(fire_attack.fire.fired == 1);
    CHECK(fire_attack.fire.consumed_ammo == 1);
    CHECK(fire_attack.attack_started == 1);
    CHECK(fire_attack.frame_before == 0);
    CHECK(fire_attack.frame_after == 7);
    CHECK(state.attack_frame == 7);
    CHECK(state.ammo == 1);
    state.ammo = 0;
    CHECK(wl_try_player_fire_weapon_attack(&state, WL_WEAPON_PISTOL, 7,
                                           &fire_attack) == 0);
    CHECK(fire_attack.fire.no_ammo == 1);
    CHECK(fire_attack.attack_started == 0);
    CHECK(fire_attack.frame_after == 7);
    CHECK(state.attack_frame == 7);
    CHECK(wl_try_player_fire_weapon_attack(&state, WL_WEAPON_KNIFE, 0,
                                           &fire_attack) == -1);

    CHECK(wl_apply_player_bonus(&state, WL_BONUS_FULLHEAL, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK(state.health == 100);
    CHECK(state.lives == 3);
    CHECK(state.treasure_count == 2);
    CHECK(wl_apply_player_bonus(&state, WL_BONUS_SPEAR, &picked_up) == 0);
    CHECK(picked_up == 1);
    CHECK(state.play_state == WL_PLAYER_PLAY_COMPLETED);
    CHECK(wl_give_player_key(&state, 32) == -1);
    CHECK(wl_give_player_weapon(&state, (wl_weapon_type)4) == -1);
    CHECK(wl_apply_player_bonus(&state, (wl_bonus_item)21, &picked_up) == -1);
    return 0;
}

static int check_decode_helpers(void) {
    const unsigned char carmack_src[] = {
        0x11, 0x11, 0x22, 0x22, 0x02, 0xa7, 0x02,
        0x00, 0xa7, 0x44, 0x00, 0xa8, 0x55,
    };
    uint16_t carmack_out[6];
    size_t written = 0;
    CHECK(wl_carmack_expand(carmack_src, sizeof(carmack_src), sizeof(carmack_out),
                            carmack_out, 6, &written) == 0);
    CHECK(written == 6);
    CHECK(carmack_out[0] == 0x1111);
    CHECK(carmack_out[1] == 0x2222);
    CHECK(carmack_out[2] == 0x1111);
    CHECK(carmack_out[3] == 0x2222);
    CHECK(carmack_out[4] == 0xa744);
    CHECK(carmack_out[5] == 0xa855);

    const uint16_t rlew_src[] = { 7, 0xabcd, 3, 9, 11 };
    uint16_t rlew_out[5];
    CHECK(wl_rlew_expand(rlew_src, 5, 0xabcd, sizeof(rlew_out),
                         rlew_out, 5, &written) == 0);
    CHECK(written == 5);
    CHECK(rlew_out[0] == 7);
    CHECK(rlew_out[1] == 9);
    CHECK(rlew_out[2] == 9);
    CHECK(rlew_out[3] == 9);
    CHECK(rlew_out[4] == 11);
    return 0;
}

static int check_wl6(const char *dir) {
    CHECK(expect_file_size(dir, "MAPHEAD.WL6", 402) == 0);
    CHECK(expect_file_size(dir, "GAMEMAPS.WL6", 150652) == 0);
    CHECK(expect_file_size(dir, "VSWAP.WL6", 1544376) == 0);
    CHECK(expect_file_size(dir, "VGAHEAD.WL6", 450) == 0);
    CHECK(expect_file_size(dir, "VGADICT.WL6", 1024) == 0);
    CHECK(expect_file_size(dir, "VGAGRAPH.WL6", 275774) == 0);
    CHECK(expect_file_size(dir, "AUDIOHED.WL6", 1156) == 0);
    CHECK(expect_file_size(dir, "AUDIOT.WL6", 320209) == 0);

    char maphead_path[1024];
    char gamemaps_path[1024];
    char vswap_path[1024];
    char vgahead_path[1024];
    char vgadict_path[1024];
    char vgagraph_path[1024];
    CHECK(path(maphead_path, sizeof(maphead_path), dir, "MAPHEAD.WL6") == 0);
    CHECK(path(gamemaps_path, sizeof(gamemaps_path), dir, "GAMEMAPS.WL6") == 0);
    CHECK(path(vswap_path, sizeof(vswap_path), dir, "VSWAP.WL6") == 0);
    CHECK(path(vgahead_path, sizeof(vgahead_path), dir, "VGAHEAD.WL6") == 0);
    CHECK(path(vgadict_path, sizeof(vgadict_path), dir, "VGADICT.WL6") == 0);
    CHECK(path(vgagraph_path, sizeof(vgagraph_path), dir, "VGAGRAPH.WL6") == 0);

    wl_maphead mh;
    CHECK(wl_read_maphead(maphead_path, &mh) == 0);
    CHECK(mh.file_size == 402);
    CHECK(mh.rlew_tag == 0xabcd);
    CHECK(mh.offsets[0] == 2250);

    wl_map_header map0;
    CHECK(wl_read_map_header(gamemaps_path, mh.offsets[0], &map0) == 0);
    CHECK(strcmp(map0.name, "Wolf1 Map1") == 0);
    CHECK(map0.width == 64);
    CHECK(map0.height == 64);
    CHECK(map0.plane_starts[0] == 11);
    CHECK(map0.plane_starts[1] == 1445);
    CHECK(map0.plane_starts[2] == 2240);
    CHECK(map0.plane_lengths[0] == 1434);
    CHECK(map0.plane_lengths[1] == 795);
    CHECK(map0.plane_lengths[2] == 10);

    uint16_t wall_plane[WL_MAP_PLANE_WORDS];
    uint16_t info_plane[WL_MAP_PLANE_WORDS];
    uint16_t extra_plane[WL_MAP_PLANE_WORDS];
    CHECK(wl_read_map_plane(gamemaps_path, &map0, 0, mh.rlew_tag,
                            wall_plane, WL_MAP_PLANE_WORDS) == 0);
    CHECK(fnv1a_words(wall_plane, WL_MAP_PLANE_WORDS) == 0x5940a18e);
    CHECK(count_nonzero(wall_plane, WL_MAP_PLANE_WORDS) == 4096);
    CHECK(count_value(wall_plane, WL_MAP_PLANE_WORDS, 1) == 2331);
    CHECK(count_value(wall_plane, WL_MAP_PLANE_WORDS, 8) == 230);
    CHECK(wall_plane[0] == 1);
    CHECK(wall_plane[WL_MAP_SIDE - 1] == 1);

    CHECK(wl_read_map_plane(gamemaps_path, &map0, 1, mh.rlew_tag,
                            info_plane, WL_MAP_PLANE_WORDS) == 0);
    CHECK(fnv1a_words(info_plane, WL_MAP_PLANE_WORDS) == 0xacf24351);
    CHECK(count_nonzero(info_plane, WL_MAP_PLANE_WORDS) == 183);
    CHECK(count_value(info_plane, WL_MAP_PLANE_WORDS, 0) == 3913);
    CHECK(count_value(info_plane, WL_MAP_PLANE_WORDS, 37) == 30);
    CHECK(info_plane[0] == 0);

    CHECK(wl_read_map_plane(gamemaps_path, &map0, 2, mh.rlew_tag,
                            extra_plane, WL_MAP_PLANE_WORDS) == 0);
    CHECK(fnv1a_words(extra_plane, WL_MAP_PLANE_WORDS) == 0xbcc31dc5);
    CHECK(count_nonzero(extra_plane, WL_MAP_PLANE_WORDS) == 0);

    wl_map_semantics sem;
    CHECK(wl_classify_map_semantics(wall_plane, info_plane, WL_MAP_PLANE_WORDS, &sem) == 0);
    CHECK(sem.solid_tiles == 3082);
    CHECK(sem.area_tiles == 1014);
    CHECK(sem.door_tiles == 22);
    CHECK(sem.vertical_doors == 14);
    CHECK(sem.horizontal_doors == 8);
    CHECK(sem.locked_doors[0] == 20);
    CHECK(sem.locked_doors[5] == 2);
    CHECK(sem.ambush_tiles == 3);
    CHECK(sem.elevator_tiles == 6);
    CHECK(sem.alt_elevator_tiles == 1);

    CHECK(sem.player_starts == 1);
    CHECK(sem.first_player_x == 29);
    CHECK(sem.first_player_y == 57);
    CHECK(sem.first_player_dir == WL_DIR_EAST);
    CHECK(sem.static_objects == 121);
    CHECK(sem.static_blocking == 34);
    CHECK(sem.static_bonus == 48);
    CHECK(sem.static_treasure == 23);
    CHECK(sem.pushwall_markers == 5);
    CHECK(sem.path_markers == 18);
    CHECK(sem.guard_easy_starts == 10);
    CHECK(sem.guard_medium_starts == 7);
    CHECK(sem.guard_hard_starts == 15);
    CHECK(sem.dog_easy_starts == 1);
    CHECK(sem.dog_medium_starts == 2);
    CHECK(sem.dog_hard_starts == 2);
    CHECK(sem.dead_guards == 1);
    CHECK(sem.officer_starts == 0);
    CHECK(sem.ss_starts == 0);
    CHECK(sem.mutant_starts == 0);
    CHECK(sem.boss_starts == 0);
    CHECK(sem.ghost_starts == 0);
    CHECK(sem.unknown_info_tiles == 0);

    wl_game_model model;
    CHECK(wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                              WL_DIFFICULTY_EASY, &model) == 0);
    CHECK(model.player.present == 1);
    CHECK(model.player.x == 29);
    CHECK(model.player.y == 57);
    CHECK(model.player.dir == WL_DIR_EAST);
    CHECK(model.door_count == 22);
    CHECK(model.door_area_connection_count == 22);
    CHECK(model.unique_door_area_connection_count == 17);
    CHECK(model.doors[0].x == 28);
    CHECK(model.doors[0].y == 11);
    CHECK(model.doors[0].vertical == 1);
    CHECK(model.doors[0].lock == 0);
    CHECK(model.doors[0].area1 == 36);
    CHECK(model.doors[0].area2 == 35);
    CHECK(model.doors[2].area1 == 36);
    CHECK(model.doors[2].area2 == 3);
    CHECK(model.doors[17].source_tile == 100);
    CHECK(model.doors[17].lock == 5);
    CHECK(model.doors[17].area1 == 33);
    CHECK(model.doors[17].area2 == 0);
    CHECK(model.door_area_connections[35][36] == 2);
    CHECK(model.door_area_connections[2][2] == 8);
    CHECK(model.door_area_connections[0][33] == 1);
    CHECK(model.static_count == 121);
    CHECK(model.blocking_static_count == 34);
    CHECK(model.bonus_static_count == 48);
    CHECK(model.treasure_total == 23);
    CHECK(model.statics[0].x == 29);
    CHECK(model.statics[0].y == 8);
    CHECK(model.statics[0].source_tile == 35);
    CHECK(model.statics[0].blocking == 1);
    CHECK(model.path_marker_count == 18);
    CHECK(model.path_markers[0].x == 2);
    CHECK(model.path_markers[0].y == 33);
    CHECK(model.path_markers[0].source_tile == 96);
    CHECK(model.path_markers[0].dir == (wl_direction)(96 - WL_ICONARROWS));
    wl_direction selected_path_dir = WL_DIR_NONE;
    CHECK(wl_select_path_direction(&model, model.path_markers[0].x,
                                   model.path_markers[0].y, WL_DIR_EAST,
                                   &selected_path_dir) == 0);
    CHECK(selected_path_dir == WL_DIR_NONE);
    wl_game_model path_model;
    memset(&path_model, 0, sizeof(path_model));
    path_model.path_marker_count = 1;
    path_model.path_markers[0].x = 5;
    path_model.path_markers[0].y = 5;
    path_model.path_markers[0].source_tile = WL_ICONARROWS + WL_DIR_WEST;
    path_model.path_markers[0].dir = WL_DIR_WEST;
    CHECK(wl_select_path_direction(&path_model, 5, 5, WL_DIR_EAST,
                                   &selected_path_dir) == 0);
    CHECK(selected_path_dir == WL_DIR_WEST);
    path_model.tilemap[4 + 5 * WL_MAP_SIDE] = 1;
    CHECK(wl_select_path_direction(&path_model, 5, 5, WL_DIR_EAST,
                                   &selected_path_dir) == 0);
    CHECK(selected_path_dir == WL_DIR_NONE);
    path_model.tilemap[4 + 5 * WL_MAP_SIDE] = 0;
    CHECK(wl_select_path_direction(&path_model, 6, 5, WL_DIR_WEST,
                                   &selected_path_dir) == 0);
    CHECK(selected_path_dir == WL_DIR_WEST);
    CHECK(wl_select_path_direction(&path_model, WL_MAP_SIDE, 5, WL_DIR_WEST,
                                   &selected_path_dir) == -1);
    wl_actor_chase_dir_result chase_dir;
    memset(&path_model, 0, sizeof(path_model));
    CHECK(wl_select_chase_direction(&path_model, 5, 5, 8, 4, WL_DIR_WEST, 1,
                                    &chase_dir) == 0);
    CHECK(chase_dir.selected == 1);
    CHECK(chase_dir.blocked == 0);
    CHECK(chase_dir.dir == WL_DIR_NORTH);
    CHECK(chase_dir.next_x == 5);
    CHECK(chase_dir.next_y == 4);
    CHECK(wl_select_chase_direction(&path_model, 5, 5, 8, 4, WL_DIR_NORTH, 1,
                                    &chase_dir) == 0);
    CHECK(chase_dir.dir == WL_DIR_EAST);
    CHECK(chase_dir.next_x == 6);
    CHECK(chase_dir.next_y == 5);
    path_model.tilemap[6 + 5 * WL_MAP_SIDE] = 1;
    CHECK(wl_select_chase_direction(&path_model, 5, 5, 8, 5, WL_DIR_NORTH, 1,
                                    &chase_dir) == 0);
    CHECK(chase_dir.selected == 1);
    CHECK(chase_dir.dir == WL_DIR_NORTH);
    CHECK(chase_dir.next_x == 5);
    CHECK(chase_dir.next_y == 4);
    path_model.tilemap[5 + 4 * WL_MAP_SIDE] = 1;
    path_model.tilemap[5 + 6 * WL_MAP_SIDE] = 1;
    path_model.tilemap[4 + 5 * WL_MAP_SIDE] = 1;
    CHECK(wl_select_chase_direction(&path_model, 5, 5, 8, 5, WL_DIR_NORTH, 1,
                                    &chase_dir) == 0);
    CHECK(chase_dir.selected == 0);
    CHECK(chase_dir.blocked == 1);
    CHECK(chase_dir.dir == WL_DIR_NONE);
    CHECK(wl_select_chase_direction(&path_model, WL_MAP_SIDE, 5, 8, 5,
                                    WL_DIR_NORTH, 1, &chase_dir) == -1);
    wl_scene_sprite_ref patrol_refs[1];
    size_t patrol_ref_count = 0;
    memset(&path_model, 0, sizeof(path_model));
    path_model.actor_count = 1;
    path_model.actors[0].kind = WL_ACTOR_GUARD;
    path_model.actors[0].mode = WL_ACTOR_CHASE;
    path_model.actors[0].dir = WL_DIR_WEST;
    path_model.actors[0].tile_x = 5;
    path_model.actors[0].tile_y = 5;
    wl_actor_chase_step_result chase_step;
    CHECK(wl_step_chase_actor(&path_model, 0, 8, 4, 1, &chase_step) == 0);
    CHECK(chase_step.stepped == 1);
    CHECK(chase_step.blocked == 0);
    CHECK(chase_step.dir == WL_DIR_NORTH);
    CHECK(chase_step.tile_x == 5);
    CHECK(chase_step.tile_y == 4);
    CHECK(path_model.actors[0].dir == WL_DIR_NORTH);
    CHECK(path_model.actors[0].fine_x == 0x58000u);
    CHECK(path_model.actors[0].fine_y == 0x48000u);
    CHECK(wl_collect_scene_sprite_refs(&path_model, 106, patrol_refs, 1,
                                       &patrol_ref_count) == 0);
    CHECK(patrol_ref_count == 1);
    CHECK(patrol_refs[0].source_index == 58);
    CHECK(patrol_refs[0].world_x == 0x58000u);
    CHECK(patrol_refs[0].world_y == 0x48000u);
    path_model.tilemap[5 + 3 * WL_MAP_SIDE] = 1;
    path_model.tilemap[6 + 4 * WL_MAP_SIDE] = 1;
    path_model.tilemap[5 + 5 * WL_MAP_SIDE] = 1;
    path_model.tilemap[4 + 4 * WL_MAP_SIDE] = 1;
    CHECK(wl_step_chase_actor(&path_model, 0, 8, 4, 1, &chase_step) == 0);
    CHECK(chase_step.stepped == 0);
    CHECK(chase_step.blocked == 1);
    path_model.actors[0].mode = WL_ACTOR_PATROL;
    CHECK(wl_step_chase_actor(&path_model, 0, 8, 4, 1, &chase_step) == -1);

    memset(&path_model, 0, sizeof(path_model));
    path_model.actor_count = 1;
    path_model.actors[0].kind = WL_ACTOR_GUARD;
    path_model.actors[0].mode = WL_ACTOR_CHASE;
    path_model.actors[0].dir = WL_DIR_WEST;
    path_model.actors[0].tile_x = 5;
    path_model.actors[0].tile_y = 5;
    wl_actor_chase_tic_result chase_tic;
    CHECK(wl_step_chase_actor_tics(&path_model, 0, 8, 4, 1, 0x8000u, 1,
                                   &chase_tic) == 0);
    CHECK(chase_tic.tiles_stepped == 0);
    CHECK(chase_tic.blocked == 0);
    CHECK(chase_tic.dir == WL_DIR_NORTH);
    CHECK(chase_tic.leftover_move == 0x8000u);
    CHECK(path_model.actors[0].tile_x == 5);
    CHECK(path_model.actors[0].tile_y == 5);
    CHECK(path_model.actors[0].fine_x == 0x58000u);
    CHECK(path_model.actors[0].fine_y == 0x50000u);
    CHECK(wl_step_chase_actor_tics(&path_model, 0, 8, 4, 1, 0x8000u, 1,
                                   &chase_tic) == 0);
    CHECK(chase_tic.tiles_stepped == 1);
    CHECK(chase_tic.leftover_move == 0);
    CHECK(path_model.actors[0].tile_x == 5);
    CHECK(path_model.actors[0].tile_y == 4);
    CHECK(path_model.actors[0].fine_x == 0x58000u);
    CHECK(path_model.actors[0].fine_y == 0x48000u);
    CHECK(wl_step_chase_actor_tics(&path_model, 0, 8, 4, 1, 0x8000u, -1,
                                   &chase_tic) == -1);
    memset(&path_model, 0, sizeof(path_model));
    path_model.path_marker_count = 1;
    path_model.path_markers[0].x = 5;
    path_model.path_markers[0].y = 5;
    path_model.path_markers[0].source_tile = WL_ICONARROWS + WL_DIR_WEST;
    path_model.path_markers[0].dir = WL_DIR_WEST;
    wl_actor_patrol_step_result patrol_step;
    path_model.actor_count = 1;
    path_model.actors[0].kind = WL_ACTOR_GUARD;
    path_model.actors[0].mode = WL_ACTOR_PATROL;
    path_model.actors[0].dir = WL_DIR_EAST;
    path_model.actors[0].tile_x = 5;
    path_model.actors[0].tile_y = 5;
    CHECK(wl_step_patrol_actor(&path_model, 0, &patrol_step) == 0);
    CHECK(patrol_step.stepped == 1);
    CHECK(patrol_step.blocked == 0);
    CHECK(patrol_step.dir == WL_DIR_WEST);
    CHECK(patrol_step.tile_x == 4);
    CHECK(patrol_step.tile_y == 5);
    CHECK(path_model.actors[0].dir == WL_DIR_WEST);
    CHECK(path_model.actors[0].tile_x == 4);
    CHECK(path_model.actors[0].tile_y == 5);
    patrol_ref_count = 0;
    CHECK(wl_collect_scene_sprite_refs(&path_model, 106, patrol_refs, 1,
                                       &patrol_ref_count) == 0);
    CHECK(patrol_ref_count == 1);
    CHECK(patrol_refs[0].kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(patrol_refs[0].model_index == 0);
    CHECK(patrol_refs[0].source_index == 58);
    CHECK(patrol_refs[0].vswap_chunk_index == 164);
    CHECK(patrol_refs[0].world_x == 0x48000u);
    CHECK(patrol_refs[0].world_y == 0x58000u);
    CHECK(wl_step_patrol_actor(&path_model, 0, &patrol_step) == 0);
    CHECK(patrol_step.stepped == 1);
    CHECK(patrol_step.dir == WL_DIR_WEST);
    CHECK(patrol_step.tile_x == 3);
    CHECK(wl_collect_scene_sprite_refs(&path_model, 106, patrol_refs, 1,
                                       &patrol_ref_count) == 0);
    CHECK(patrol_ref_count == 1);
    CHECK(patrol_refs[0].world_x == 0x38000u);
    CHECK(patrol_refs[0].world_y == 0x58000u);
    path_model.tilemap[2 + 5 * WL_MAP_SIDE] = 1;
    CHECK(wl_step_patrol_actor(&path_model, 0, &patrol_step) == 0);
    CHECK(patrol_step.stepped == 0);
    CHECK(patrol_step.blocked == 1);
    CHECK(patrol_step.dir == WL_DIR_NONE);
    CHECK(path_model.actors[0].tile_x == 3);
    CHECK(wl_collect_scene_sprite_refs(&path_model, 106, patrol_refs, 1,
                                       &patrol_ref_count) == 0);
    CHECK(patrol_ref_count == 1);
    CHECK(patrol_refs[0].world_x == 0x38000u);
    CHECK(patrol_refs[0].world_y == 0x58000u);
    path_model.actors[0].mode = WL_ACTOR_STAND;
    CHECK(wl_step_patrol_actor(&path_model, 0, &patrol_step) == -1);
    CHECK(wl_step_patrol_actor(&path_model, 1, &patrol_step) == -1);

    wl_actor_patrol_tic_result patrol_tics;
    memset(&path_model, 0, sizeof(path_model));
    path_model.actor_count = 1;
    path_model.actors[0].kind = WL_ACTOR_GUARD;
    path_model.actors[0].mode = WL_ACTOR_PATROL;
    path_model.actors[0].dir = WL_DIR_EAST;
    path_model.actors[0].tile_x = 5;
    path_model.actors[0].tile_y = 5;
    CHECK(wl_step_patrol_actor_tics(&path_model, 0, 0x8000u, 1,
                                    &patrol_tics) == 0);
    CHECK(patrol_tics.tiles_stepped == 0);
    CHECK(patrol_tics.leftover_move == 0x8000u);
    CHECK(patrol_tics.fine_x == 0x60000u);
    CHECK(patrol_tics.fine_y == 0x58000u);
    CHECK(path_model.actors[0].tile_x == 5);
    CHECK(path_model.actors[0].fine_x == 0x60000u);
    CHECK(wl_collect_scene_sprite_refs(&path_model, 106, patrol_refs, 1,
                                       &patrol_ref_count) == 0);
    CHECK(patrol_refs[0].world_x == 0x60000u);
    CHECK(patrol_refs[0].world_y == 0x58000u);
    CHECK(path_model.actors[0].patrol_remainder == 0x8000u);
    CHECK(wl_step_patrol_actor_tics(&path_model, 0, 0x8000u, 1,
                                    &patrol_tics) == 0);
    CHECK(patrol_tics.requested_move == 0x10000u);
    CHECK(patrol_tics.tiles_stepped == 1);
    CHECK(patrol_tics.leftover_move == 0);
    CHECK(patrol_tics.tile_x == 6);
    CHECK(path_model.actors[0].patrol_remainder == 0);
    CHECK(wl_step_patrol_actor_tics(&path_model, 0, 0x10000u, 1,
                                    &patrol_tics) == 0);
    CHECK(patrol_tics.requested_move == 0x10000u);
    CHECK(patrol_tics.tiles_stepped == 1);
    CHECK(patrol_tics.leftover_move == 0);
    CHECK(patrol_tics.tile_x == 7);
    CHECK(patrol_tics.tile_y == 5);
    CHECK(patrol_tics.fine_x == 0x78000u);
    CHECK(patrol_tics.fine_y == 0x58000u);
    CHECK(path_model.actors[0].tile_x == 7);
    path_model.tilemap[8 + 5 * WL_MAP_SIDE] = 1;
    CHECK(wl_step_patrol_actor_tics(&path_model, 0, 0x10000u, 2,
                                    &patrol_tics) == 0);
    CHECK(patrol_tics.tiles_stepped == 0);
    CHECK(patrol_tics.blocked == 1);
    CHECK(patrol_tics.leftover_move == 0x20000u);
    CHECK(patrol_tics.fine_x == 0x78000u);
    CHECK(path_model.actors[0].tile_x == 7);
    CHECK(wl_step_patrol_actor_tics(&path_model, 0, 0x10000u, -1,
                                    &patrol_tics) == -1);

    wl_actor_patrols_tic_result patrols_tics;
    memset(&path_model, 0, sizeof(path_model));
    path_model.actor_count = 3;
    path_model.actors[0].kind = WL_ACTOR_GUARD;
    path_model.actors[0].mode = WL_ACTOR_PATROL;
    path_model.actors[0].dir = WL_DIR_EAST;
    path_model.actors[0].tile_x = 5;
    path_model.actors[0].tile_y = 5;
    path_model.actors[1].kind = WL_ACTOR_DOG;
    path_model.actors[1].mode = WL_ACTOR_PATROL;
    path_model.actors[1].dir = WL_DIR_SOUTH;
    path_model.actors[1].tile_x = 10;
    path_model.actors[1].tile_y = 10;
    path_model.actors[2].kind = WL_ACTOR_GUARD;
    path_model.actors[2].mode = WL_ACTOR_STAND;
    path_model.actors[2].dir = WL_DIR_WEST;
    path_model.actors[2].tile_x = 20;
    path_model.actors[2].tile_y = 20;
    path_model.tilemap[10 + 11 * WL_MAP_SIDE] = 1;
    CHECK(wl_step_patrol_actors_tics(&path_model, 0x10000u, 1,
                                     &patrols_tics) == 0);
    CHECK(patrols_tics.actors_considered == 2);
    CHECK(patrols_tics.actors_stepped == 1);
    CHECK(patrols_tics.actors_blocked == 1);
    CHECK(patrols_tics.tiles_stepped == 1);
    CHECK(path_model.actors[0].tile_x == 6);
    CHECK(path_model.actors[0].tile_y == 5);
    CHECK(path_model.actors[1].tile_x == 10);
    CHECK(path_model.actors[1].tile_y == 10);
    CHECK(path_model.actors[2].tile_x == 20);
    CHECK(wl_step_patrol_actors_tics(&path_model, 0x10000u, -1,
                                     &patrols_tics) == -1);

    wl_game_model live_ai_model;
    memset(&live_ai_model, 0, sizeof(live_ai_model));
    live_ai_model.actor_count = 2;
    live_ai_model.actors[0].kind = WL_ACTOR_GUARD;
    live_ai_model.actors[0].mode = WL_ACTOR_PATROL;
    live_ai_model.actors[0].dir = WL_DIR_EAST;
    live_ai_model.actors[0].tile_x = 5;
    live_ai_model.actors[0].tile_y = 5;
    live_ai_model.actors[1].kind = WL_ACTOR_GUARD;
    live_ai_model.actors[1].mode = WL_ACTOR_STAND;
    live_ai_model.actors[1].dir = WL_DIR_WEST;
    live_ai_model.actors[1].tile_x = 8;
    live_ai_model.actors[1].tile_y = 8;
    wl_player_gameplay_state live_ai_player;
    wl_player_motion_state live_ai_motion = {0x38000u, 0x38000u, 3, 3};
    wl_live_actor_ai_tick_result live_ai;
    uint16_t empty_plane[WL_MAP_PLANE_WORDS] = { 0 };
    CHECK(wl_init_player_gameplay_state(&live_ai_player, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_actor_ai_tick(&live_ai_player, &live_ai_model,
                                     empty_plane, empty_plane,
                                     WL_MAP_PLANE_WORDS, &live_ai_motion,
                                     0, 0, 0x10000, 0, WL_DIR_EAST,
                                     0, 0, 0x10000u, 1, &live_ai) == 0);
    CHECK(live_ai.live.palette.kind == WL_PALETTE_SHIFT_NONE);
    CHECK(live_ai.patrols_stepped == 1);
    CHECK(live_ai.patrols.actors_considered == 1);
    CHECK(live_ai.patrols.actors_stepped == 1);
    CHECK(live_ai.patrols.tiles_stepped == 1);
    CHECK(live_ai_model.actors[0].tile_x == 6);
    CHECK(live_ai_model.actors[1].tile_x == 8);
    wl_scene_sprite_ref live_ai_refs[WL_MAX_ACTORS];
    size_t live_ai_ref_count = 0;
    CHECK(wl_collect_scene_sprite_refs(&live_ai_model, 106, live_ai_refs,
                                       sizeof(live_ai_refs) / sizeof(live_ai_refs[0]),
                                       &live_ai_ref_count) == 0);
    CHECK(live_ai_ref_count == 2);
    CHECK(live_ai_refs[0].kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(live_ai_refs[0].model_index == 0);
    CHECK(live_ai_refs[0].source_index == 58);
    CHECK(live_ai_refs[0].vswap_chunk_index == 164);
    CHECK(live_ai_refs[0].world_x == 0x68000u);
    CHECK(live_ai_refs[0].world_y == 0x58000u);
    CHECK(live_ai_refs[1].source_index == 50);
    CHECK(live_ai_refs[1].world_x == 0x88000u);
    CHECK(live_ai_refs[1].world_y == 0x88000u);
    live_ai_model.tilemap[7 + 5 * WL_MAP_SIDE] = 1;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_player, &live_ai_model,
                                     empty_plane, empty_plane,
                                     WL_MAP_PLANE_WORDS, &live_ai_motion,
                                     0, 0, 0x10000, 0, WL_DIR_EAST,
                                     0, 0, 0x10000u, 1, &live_ai) == 0);
    CHECK(live_ai.patrols_stepped == 0);
    CHECK(live_ai.patrols.actors_blocked == 1);
    CHECK(live_ai_model.actors[0].tile_x == 6);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_model, 106, live_ai_refs,
                                       sizeof(live_ai_refs) / sizeof(live_ai_refs[0]),
                                       &live_ai_ref_count) == 0);
    CHECK(live_ai_ref_count == 2);
    CHECK(live_ai_refs[0].source_index == 58);
    CHECK(live_ai_refs[0].world_x == 0x68000u);
    CHECK(live_ai_refs[0].world_y == 0x58000u);
    CHECK(wl_step_live_actor_ai_tick(&live_ai_player, &live_ai_model,
                                     empty_plane, empty_plane,
                                     WL_MAP_PLANE_WORDS, &live_ai_motion,
                                     0, 0, 0x10000, 0, WL_DIR_EAST,
                                     0, 0, 0x10000u, -1, &live_ai) == -1);
    CHECK(model.pushwall_count == 5);
    CHECK(model.secret_total == 5);
    CHECK(model.pushwalls[0].x == 10);
    CHECK(model.pushwalls[0].y == 13);
    CHECK(model.actor_count == 12);
    CHECK(model.kill_total == 11);
    CHECK(model.actors[0].source_tile == 111);
    CHECK(model.actors[0].spawn_x == 33);
    CHECK(model.actors[0].spawn_y == 9);
    CHECK(model.actors[0].kind == WL_ACTOR_GUARD);
    CHECK(model.actors[0].mode == WL_ACTOR_STAND);
    CHECK(model.actors[9].kind == WL_ACTOR_DOG);
    CHECK(model.actors[9].mode == WL_ACTOR_PATROL);
    CHECK(model.actors[9].spawn_x == 54);
    CHECK(model.actors[9].spawn_y == 45);
    CHECK(model.actors[9].tile_x == 55);
    CHECK(model.actors[11].kind == WL_ACTOR_DEAD_GUARD);
    CHECK(model.actors[11].counts_for_kill_total == 0);
    CHECK(model.unknown_info_tiles == 0);
    wl_scene_sprite_ref scene_refs[WL_MAX_STATS + WL_MAX_ACTORS];
    size_t scene_ref_count = 0;
    CHECK(wl_collect_scene_sprite_refs(&model, 106, scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 133);
    CHECK(scene_refs[0].kind == WL_SCENE_SPRITE_STATIC);
    CHECK(scene_refs[0].model_index == 0);
    CHECK(scene_refs[0].source_index == 14);
    CHECK(scene_refs[0].vswap_chunk_index == 120);
    CHECK(scene_refs[0].world_x == 0x1d8000);
    CHECK(scene_refs[0].world_y == 0x088000);
    CHECK(scene_refs[2].source_index == 28);
    CHECK(scene_refs[2].vswap_chunk_index == 134);
    CHECK(scene_refs[128].kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(scene_refs[128].model_index == 7);
    CHECK(scene_refs[128].source_index == 58);
    CHECK(scene_refs[128].vswap_chunk_index == 164);
    CHECK(scene_refs[130].source_index == 99);
    CHECK(scene_refs[130].vswap_chunk_index == 205);
    CHECK(scene_refs[132].source_index == 95);
    CHECK(scene_refs[132].vswap_chunk_index == 201);
    CHECK(fnv1a_scene_sprite_refs(scene_refs, scene_ref_count) == 0x2ab36473);

    wl_game_model pickup_model = model;
    wl_player_gameplay_state pickup_state;
    uint8_t picked_up = 255;
    CHECK(wl_init_player_gameplay_state(&pickup_state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_try_pickup_static_bonus(&pickup_state, &pickup_model.statics[0],
                                     &picked_up) == 0);
    CHECK(picked_up == 0);
    CHECK(pickup_model.statics[0].active == 1);
    size_t food_static_index = pickup_model.static_count;
    for (size_t i = 0; i < pickup_model.static_count; ++i) {
        if (pickup_model.statics[i].type == 24) {
            food_static_index = i;
            break;
        }
    }
    CHECK(food_static_index < pickup_model.static_count);
    const wl_static_desc *food_static = &pickup_model.statics[food_static_index];
    uint32_t food_center_x = ((uint32_t)food_static->x << 16) + 0x8000u;
    uint32_t food_center_y = ((uint32_t)food_static->y << 16) + 0x8000u;
    uint32_t pickup_origin_x = food_center_x - 0x8000u;
    size_t touched_static_index = pickup_model.static_count;
    CHECK(wl_try_pickup_visible_static_bonus(&pickup_state, &pickup_model,
                                             pickup_origin_x, food_center_y + 0x8000u,
                                             0x10000, 0,
                                             &picked_up, &touched_static_index) == 0);
    CHECK(picked_up == 0);
    CHECK(touched_static_index == pickup_model.static_count);
    CHECK(wl_try_pickup_visible_static_bonus(&pickup_state, &pickup_model,
                                             pickup_origin_x, food_center_y,
                                             0x10000, 0,
                                             &picked_up, &touched_static_index) == 0);
    CHECK(picked_up == 0);
    CHECK(touched_static_index == food_static_index);
    CHECK(pickup_model.statics[food_static_index].active == 1);
    pickup_state.health = 90;
    CHECK(wl_try_pickup_visible_static_bonus(&pickup_state, &pickup_model,
                                             pickup_origin_x, food_center_y,
                                             0x10000, 0,
                                             &picked_up, &touched_static_index) == 0);
    CHECK(picked_up == 1);
    CHECK(touched_static_index == food_static_index);
    CHECK(pickup_state.health == 100);
    CHECK(pickup_state.palette_shift.bonus_count == WL_NUM_WHITE_SHIFTS * WL_WHITE_SHIFT_TICS);
    CHECK(pickup_model.statics[food_static_index].active == 0);
    CHECK(wl_collect_scene_sprite_refs(&pickup_model, 106, scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 132);
    CHECK(wl_try_pickup_static_bonus(&pickup_state, &pickup_model.statics[food_static_index],
                                     &picked_up) == 0);
    CHECK(picked_up == 0);
    CHECK(wl_collect_scene_sprite_refs(&model, 106, scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 133);

    wl_player_motion_state spawn_motion;
    CHECK(wl_init_player_motion_from_spawn(&model, &spawn_motion) == 0);
    CHECK(spawn_motion.tile_x == 29);
    CHECK(spawn_motion.tile_y == 57);
    CHECK(spawn_motion.x == 0x1d8000u);
    CHECK(spawn_motion.y == 0x398000u);

    wl_game_model motion_model;
    memset(&motion_model, 0, sizeof(motion_model));
    motion_model.static_count = 1;
    motion_model.statics[0].x = 4;
    motion_model.statics[0].y = 4;
    motion_model.statics[0].type = 24;
    motion_model.statics[0].bonus = 1;
    motion_model.statics[0].active = 1;
    wl_player_motion_state motion = {0x3f000u, 0x48000u, 3, 4};
    wl_player_step_result step_result;
    CHECK(wl_init_player_gameplay_state(&pickup_state, 90, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_player_motion(&pickup_state, &motion_model, &motion,
                                0x1000, 0, 0x10000, 0,
                                &step_result) == 0);
    CHECK(step_result.moved == 1);
    CHECK(step_result.blocked == 0);
    CHECK(step_result.picked_up == 1);
    CHECK(step_result.static_index == 0);
    CHECK(step_result.x == 0x40000u);
    CHECK(step_result.y == 0x48000u);
    CHECK(step_result.tile_x == 4);
    CHECK(step_result.tile_y == 4);
    CHECK(pickup_state.health == 100);
    CHECK(motion_model.statics[0].active == 0);

    wl_game_model blocked_model;
    memset(&blocked_model, 0, sizeof(blocked_model));
    blocked_model.static_count = 1;
    blocked_model.statics[0].x = 4;
    blocked_model.statics[0].y = 4;
    blocked_model.statics[0].blocking = 1;
    blocked_model.statics[0].active = 1;
    wl_player_motion_state blocked_motion = {0x38000u, 0x48000u, 3, 4};
    CHECK(wl_step_player_motion(&pickup_state, &blocked_model, &blocked_motion,
                                0x10000, 0, 0x10000, 0,
                                &step_result) == 0);
    CHECK(step_result.moved == 0);
    CHECK(step_result.blocked == 1);
    CHECK(step_result.picked_up == 0);
    CHECK(blocked_motion.x == 0x38000u);
    CHECK(blocked_motion.y == 0x48000u);

    uint16_t use_wall[WL_MAP_PLANE_WORDS];
    uint16_t use_info[WL_MAP_PLANE_WORDS];
    memset(use_wall, 0, sizeof(use_wall));
    memset(use_info, 0, sizeof(use_info));
    wl_player_use_result use_result;
    wl_game_model use_model;
    memset(&use_model, 0, sizeof(use_model));
    wl_player_motion_state use_motion = {0x38000u, 0x48000u, 3, 4};
    CHECK(wl_init_player_gameplay_state(&pickup_state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    use_info[4 + 4 * WL_MAP_SIDE] = WL_PUSHABLETILE;
    use_model.tilemap[4 + 4 * WL_MAP_SIDE] = 37;
    use_model.pushwall_count = 1;
    use_model.pushwalls[0].x = 4;
    use_model.pushwalls[0].y = 4;
    CHECK(wl_use_player_facing(&pickup_state, &use_model, use_wall, use_info,
                               WL_MAP_PLANE_WORDS, &use_motion, WL_DIR_EAST, 1,
                               &use_result) == 0);
    CHECK(use_result.kind == WL_USE_PUSHWALL);
    CHECK(use_result.check_x == 4);
    CHECK(use_result.check_y == 4);
    CHECK(use_result.dir == WL_DIR_EAST);
    CHECK(use_result.opened == 1);
    CHECK(use_result.pushwall_index == 0);
    CHECK(pickup_state.secret_count == 1);
    CHECK(use_model.pushwall_motion.active == 1);
    CHECK(use_model.pushwall_motion.state == 1);
    CHECK(use_model.pushwall_motion.pos == 0);
    CHECK(use_model.pushwall_motion.x == 4);
    CHECK(use_model.pushwall_motion.y == 4);
    CHECK(use_model.tilemap[4 + 4 * WL_MAP_SIDE] == (37 | 0xc0));
    CHECK(use_model.tilemap[5 + 4 * WL_MAP_SIDE] == 37);

    wl_pushwall_step_result push_step;
    CHECK(wl_step_pushwall(&use_model, 126, &push_step) == 0);
    CHECK(push_step.active == 1);
    CHECK(push_step.crossed_tile == 0);
    CHECK(push_step.state == 127);
    CHECK(push_step.pos == 63);
    CHECK(use_model.tilemap[4 + 4 * WL_MAP_SIDE] == (37 | 0xc0));
    CHECK(wl_step_pushwall(&use_model, 1, &push_step) == 0);
    CHECK(push_step.active == 1);
    CHECK(push_step.crossed_tile == 1);
    CHECK(push_step.x == 5);
    CHECK(push_step.y == 4);
    CHECK(push_step.state == 128);
    CHECK(push_step.pos == 0);
    CHECK(use_model.tilemap[4 + 4 * WL_MAP_SIDE] == 0);
    CHECK(use_model.tilemap[5 + 4 * WL_MAP_SIDE] == (37 | 0xc0));
    CHECK(use_model.tilemap[6 + 4 * WL_MAP_SIDE] == 37);
    CHECK(wl_step_pushwall(&use_model, 129, &push_step) == 0);
    CHECK(push_step.active == 0);
    CHECK(push_step.stopped == 1);
    CHECK(use_model.tilemap[5 + 4 * WL_MAP_SIDE] == 0);
    CHECK(use_model.tilemap[6 + 4 * WL_MAP_SIDE] == 37);

    wl_live_tick_result live_tick;
    wl_game_model live_tick_pickup_model;
    memset(&live_tick_pickup_model, 0, sizeof(live_tick_pickup_model));
    live_tick_pickup_model.static_count = 1;
    live_tick_pickup_model.statics[0].x = 4;
    live_tick_pickup_model.statics[0].y = 4;
    live_tick_pickup_model.statics[0].type = 24;
    live_tick_pickup_model.statics[0].bonus = 1;
    live_tick_pickup_model.statics[0].active = 1;
    wl_player_motion_state live_tick_motion = {0x3f000u, 0x48000u, 3, 4};
    CHECK(wl_init_player_gameplay_state(&pickup_state, 90, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_tick(&pickup_state, &live_tick_pickup_model,
                            use_wall, use_info, WL_MAP_PLANE_WORDS,
                            &live_tick_motion, 0x1000, 0, 0x10000, 0,
                            WL_DIR_EAST, 0, 0, 1, &live_tick) == 0);
    CHECK(live_tick.motion.moved == 1);
    CHECK(live_tick.motion.picked_up == 1);
    CHECK(live_tick.used == 0);
    CHECK(live_tick.palette.kind == WL_PALETTE_SHIFT_WHITE);
    CHECK(pickup_state.health == 100);
    CHECK(live_tick_pickup_model.statics[0].active == 0);

    wl_live_player_fire_tick_result live_fire_tick;
    CHECK(wl_init_player_gameplay_state(&pickup_state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    pickup_state.ammo = 3;
    pickup_state.best_weapon = WL_WEAPON_MACHINEGUN;
    pickup_state.weapon = WL_WEAPON_PISTOL;
    pickup_state.chosen_weapon = WL_WEAPON_PISTOL;
    CHECK(wl_step_live_player_fire_tick(&pickup_state, &live_tick_pickup_model,
                                        use_wall, use_info, WL_MAP_PLANE_WORDS,
                                        &live_tick_motion, 0, 0, 0x10000, 0,
                                        WL_DIR_EAST, 0, 0, 1,
                                        WL_WEAPON_MACHINEGUN, 1,
                                        &live_fire_tick) == 0);
    CHECK(live_fire_tick.fire_attempted == 1);
    CHECK(live_fire_tick.fire_blocked_by_active_attack == 0);
    CHECK(live_fire_tick.fire.fired == 1);
    CHECK(live_fire_tick.fire.consumed_ammo == 1);
    CHECK(live_fire_tick.fire.ammo_before == 3);
    CHECK(live_fire_tick.fire.ammo_after == 2);
    CHECK(live_fire_tick.fire_attack.attack_started == 1);
    CHECK(live_fire_tick.fire_attack.attack_frame_before == 0);
    CHECK(live_fire_tick.fire_attack.attack_frame_after == 3);
    CHECK(live_fire_tick.live.palette.kind == WL_PALETTE_SHIFT_NONE);
    CHECK(pickup_state.weapon == WL_WEAPON_MACHINEGUN);
    CHECK(pickup_state.attack_frame == 3);
    CHECK(pickup_state.ammo == 2);
    pickup_state.attack_frame = 7;
    pickup_state.ammo = 0;
    CHECK(wl_step_live_player_fire_tick(&pickup_state, &live_tick_pickup_model,
                                        use_wall, use_info, WL_MAP_PLANE_WORDS,
                                        &live_tick_motion, 0, 0, 0x10000, 0,
                                        WL_DIR_EAST, 0, 0, 1,
                                        WL_WEAPON_PISTOL, 1,
                                        &live_fire_tick) == 0);
    CHECK(live_fire_tick.fire_attempted == 1);
    CHECK(live_fire_tick.fire_blocked_by_active_attack == 1);
    CHECK(live_fire_tick.fire.no_ammo == 1);
    CHECK(live_fire_tick.fire.fired_weapon == WL_WEAPON_MACHINEGUN);
    CHECK(live_fire_tick.fire_attack.attack_started == 0);
    CHECK(live_fire_tick.fire_attack.attack_frame_before == 7);
    CHECK(live_fire_tick.fire_attack.attack_frame_after == 6);
    CHECK(pickup_state.weapon == WL_WEAPON_MACHINEGUN);
    CHECK(pickup_state.attack_frame == 6);
    pickup_state.attack_frame = 3;
    pickup_state.ammo = 2;
    pickup_state.weapon = WL_WEAPON_PISTOL;
    pickup_state.chosen_weapon = WL_WEAPON_PISTOL;
    CHECK(wl_step_live_player_fire_tick(&pickup_state, &live_tick_pickup_model,
                                        use_wall, use_info, WL_MAP_PLANE_WORDS,
                                        &live_tick_motion, 0, 0, 0x10000, 0,
                                        WL_DIR_EAST, 0, 0, 1,
                                        WL_WEAPON_PISTOL, 1,
                                        &live_fire_tick) == 0);
    CHECK(live_fire_tick.fire_attempted == 1);
    CHECK(live_fire_tick.fire_blocked_by_active_attack == 1);
    CHECK(live_fire_tick.live.attack.advanced == 1);
    CHECK(live_fire_tick.live.attack.frame_before == 3);
    CHECK(live_fire_tick.live.attack.frame_after == 2);
    CHECK(live_fire_tick.fire.fired == 0);
    CHECK(live_fire_tick.fire.consumed_ammo == 0);
    CHECK(live_fire_tick.fire.ammo_before == 2);
    CHECK(live_fire_tick.fire.ammo_after == 2);
    CHECK(live_fire_tick.fire_attack.attack_started == 0);
    CHECK(live_fire_tick.fire_attack.attack_frame_before == 3);
    CHECK(live_fire_tick.fire_attack.attack_frame_after == 2);
    CHECK(pickup_state.attack_frame == 2);
    CHECK(pickup_state.ammo == 2);
    pickup_state.attack_frame = 1;
    pickup_state.ammo = 2;
    CHECK(wl_step_live_player_fire_tick(&pickup_state, &live_tick_pickup_model,
                                        use_wall, use_info, WL_MAP_PLANE_WORDS,
                                        &live_tick_motion, 0, 0, 0x10000, 0,
                                        WL_DIR_EAST, 0, 0, 1,
                                        WL_WEAPON_PISTOL, 1,
                                        &live_fire_tick) == 0);
    CHECK(live_fire_tick.fire_attempted == 1);
    CHECK(live_fire_tick.fire_blocked_by_active_attack == 1);
    CHECK(live_fire_tick.live.attack.finished == 1);
    CHECK(live_fire_tick.fire.fired == 0);
    CHECK(live_fire_tick.fire.consumed_ammo == 0);
    CHECK(live_fire_tick.fire.ammo_before == 2);
    CHECK(live_fire_tick.fire.ammo_after == 2);
    CHECK(live_fire_tick.fire_attack.attack_started == 0);
    CHECK(live_fire_tick.fire_attack.attack_frame_before == 1);
    CHECK(live_fire_tick.fire_attack.attack_frame_after == 0);
    CHECK(pickup_state.attack_frame == 0);
    CHECK(pickup_state.ammo == 2);
    CHECK(wl_step_live_player_fire_tick(&pickup_state, &live_tick_pickup_model,
                                        use_wall, use_info, WL_MAP_PLANE_WORDS,
                                        &live_tick_motion, 0, 0, 0x10000, 0,
                                        WL_DIR_EAST, 0, 0, 0,
                                        WL_WEAPON_KNIFE, 1,
                                        &live_fire_tick) == 0);
    CHECK(live_fire_tick.fire_attempted == 0);
    CHECK(live_fire_tick.fire_blocked_by_active_attack == 0);
    CHECK(live_fire_tick.fire.fired == 0);
    CHECK(live_fire_tick.fire_attack.attack_started == 0);

    memset(use_info, 0, sizeof(use_info));
    memset(&use_model, 0, sizeof(use_model));
    CHECK(wl_init_player_gameplay_state(&pickup_state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    use_motion.x = 0x38000u;
    use_motion.y = 0x48000u;
    use_motion.tile_x = 3;
    use_motion.tile_y = 4;
    use_info[4 + 4 * WL_MAP_SIDE] = WL_PUSHABLETILE;
    use_model.tilemap[4 + 4 * WL_MAP_SIDE] = 37;
    use_model.pushwall_count = 1;
    use_model.pushwalls[0].x = 4;
    use_model.pushwalls[0].y = 4;
    CHECK(wl_step_live_tick(&pickup_state, &use_model,
                            use_wall, use_info, WL_MAP_PLANE_WORDS,
                            &use_motion, 0, 0, 0x10000, 0,
                            WL_DIR_EAST, 1, 0, 127, &live_tick) == 0);
    CHECK(live_tick.used == 1);
    CHECK(live_tick.use.kind == WL_USE_PUSHWALL);
    CHECK(live_tick.use.opened == 1);
    CHECK(live_tick.pushwall.crossed_tile == 1);
    CHECK(live_tick.pushwall.x == 5);
    CHECK(live_tick.pushwall.y == 4);
    CHECK(live_tick.pushwall.state == 128);
    CHECK(live_tick.palette.kind == WL_PALETTE_SHIFT_NONE);
    CHECK(pickup_state.secret_count == 1);
    CHECK(use_model.tilemap[4 + 4 * WL_MAP_SIDE] == 0);
    CHECK(use_model.tilemap[5 + 4 * WL_MAP_SIDE] == (37 | 0xc0));
    CHECK(use_model.tilemap[6 + 4 * WL_MAP_SIDE] == 37);

    wl_game_model runtime_model;
    uint16_t runtime_plane[WL_MAP_PLANE_WORDS];
    wl_map_wall_hit live_hit;
    memset(&runtime_model, 0, sizeof(runtime_model));
    runtime_model.door_count = 1;
    runtime_model.doors[0].x = 4;
    runtime_model.doors[0].y = 4;
    runtime_model.doors[0].action = WL_DOOR_CLOSED;
    runtime_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0x80u;
    runtime_model.tilemap[6 + 4 * WL_MAP_SIDE] = 22;
    CHECK(wl_build_runtime_solid_plane(&runtime_model, 1, runtime_plane) == 0);
    CHECK(runtime_plane[4 + 4 * WL_MAP_SIDE] == 1);
    CHECK(wl_cast_cardinal_wall_ray(runtime_plane, WL_MAP_PLANE_WORDS, 3, 4,
                                    WL_RAY_EAST, 0, 7, 64, &live_hit) == 0);
    CHECK(live_hit.tile_x == 4);
    CHECK(live_hit.tile_y == 4);
    CHECK(live_hit.source_tile == 1);
    runtime_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0;
    CHECK(wl_build_runtime_solid_plane(&runtime_model, 1, runtime_plane) == 0);
    CHECK(wl_cast_cardinal_wall_ray(runtime_plane, WL_MAP_PLANE_WORDS, 3, 4,
                                    WL_RAY_EAST, 0, 7, 64, &live_hit) == 0);
    CHECK(live_hit.tile_x == 6);
    CHECK(live_hit.source_tile == 22);
    runtime_model.tilemap[5 + 4 * WL_MAP_SIDE] = (uint16_t)(37 | 0xc0u);
    runtime_model.tilemap[6 + 4 * WL_MAP_SIDE] = 37;
    CHECK(wl_build_runtime_solid_plane(&runtime_model, 1, runtime_plane) == 0);
    CHECK(runtime_plane[5 + 4 * WL_MAP_SIDE] == 37);
    CHECK(runtime_plane[6 + 4 * WL_MAP_SIDE] == 37);
    CHECK(wl_cast_cardinal_wall_ray(runtime_plane, WL_MAP_PLANE_WORDS, 4, 4,
                                    WL_RAY_EAST, 0, 7, 64, &live_hit) == 0);
    CHECK(live_hit.tile_x == 5);
    CHECK(live_hit.source_tile == 37);

    memset(use_info, 0, sizeof(use_info));
    memset(&use_model, 0, sizeof(use_model));
    CHECK(wl_init_player_gameplay_state(&pickup_state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    use_model.tilemap[4 + 4 * WL_MAP_SIDE] = WL_ELEVATORTILE;
    use_wall[3 + 4 * WL_MAP_SIDE] = WL_ALTELEVATORTILE;
    CHECK(wl_use_player_facing(&pickup_state, &use_model, use_wall, use_info,
                               WL_MAP_PLANE_WORDS, &use_motion, WL_DIR_EAST, 0,
                               &use_result) == 0);
    CHECK(use_result.kind == WL_USE_ELEVATOR);
    CHECK(use_result.elevator_ok == 1);
    CHECK(use_result.completed == 1);
    CHECK(use_result.secret_level == 1);
    CHECK(pickup_state.play_state == WL_PLAYER_PLAY_COMPLETED);
    CHECK(use_model.tilemap[4 + 4 * WL_MAP_SIDE] == WL_ELEVATORTILE + 1);

    memset(use_wall, 0, sizeof(use_wall));
    memset(&use_model, 0, sizeof(use_model));
    CHECK(wl_init_player_gameplay_state(&pickup_state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    use_model.door_count = 1;
    use_model.doors[0].x = 4;
    use_model.doors[0].y = 4;
    use_model.doors[0].lock = 1;
    use_model.doors[0].action = WL_DOOR_CLOSED;
    use_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0x80u;
    CHECK(wl_use_player_facing(&pickup_state, &use_model, use_wall, use_info,
                               WL_MAP_PLANE_WORDS, &use_motion, WL_DIR_EAST, 0,
                               &use_result) == 0);
    CHECK(use_result.kind == WL_USE_DOOR);
    CHECK(use_result.door_index == 0);
    CHECK(use_result.locked == 1);
    CHECK(use_model.doors[0].action == WL_DOOR_CLOSED);
    CHECK(wl_give_player_key(&pickup_state, 0) == 0);
    CHECK(wl_use_player_facing(&pickup_state, &use_model, use_wall, use_info,
                               WL_MAP_PLANE_WORDS, &use_motion, WL_DIR_EAST, 0,
                               &use_result) == 0);
    CHECK(use_result.opened == 1);
    CHECK(use_model.doors[0].action == WL_DOOR_OPENING);
    CHECK(wl_use_player_facing(&pickup_state, &use_model, use_wall, use_info,
                               WL_MAP_PLANE_WORDS, &use_motion, WL_DIR_EAST, 0,
                               &use_result) == 0);
    CHECK(use_result.closed == 1);
    CHECK(use_model.doors[0].action == WL_DOOR_CLOSING);

    wl_door_step_result door_step;
    use_model.doors[0].action = WL_DOOR_OPENING;
    use_model.doors[0].position = 0;
    use_model.doors[0].ticcount = 123;
    use_model.doors[0].vertical = 1;
    CHECK(wl_step_doors(&use_model, &use_motion, 10, &door_step) == 0);
    CHECK(use_model.doors[0].action == WL_DOOR_OPENING);
    CHECK(use_model.doors[0].position == (10u << 10));
    CHECK(door_step.opening_count == 1);
    CHECK(wl_step_doors(&use_model, &use_motion, 100, &door_step) == 0);
    CHECK(use_model.doors[0].action == WL_DOOR_OPEN);
    CHECK(use_model.doors[0].position == 0xffffu);
    CHECK(use_model.doors[0].ticcount == 0);
    CHECK(use_model.tilemap[4 + 4 * WL_MAP_SIDE] == 0);
    CHECK(door_step.open_count == 1);
    CHECK(door_step.released_collision_count == 1);
    CHECK(wl_step_doors(&use_model, &use_motion, 299, &door_step) == 0);
    CHECK(use_model.doors[0].action == WL_DOOR_OPEN);
    CHECK(use_model.doors[0].ticcount == 299);
    CHECK(door_step.open_count == 1);
    CHECK(wl_step_doors(&use_model, &use_motion, 1, &door_step) == 0);
    CHECK(use_model.doors[0].action == WL_DOOR_CLOSING);
    CHECK(use_model.tilemap[4 + 4 * WL_MAP_SIDE] == 0x80u);
    CHECK(door_step.closing_count == 1);
    CHECK(door_step.restored_collision_count == 1);
    wl_player_motion_state door_block_motion = {0x48000u, 0x48000u, 4, 4};
    CHECK(wl_step_doors(&use_model, &door_block_motion, 1, &door_step) == 0);
    CHECK(use_model.doors[0].action == WL_DOOR_OPENING);
    CHECK(door_step.reopened_blocked_count == 1);
    use_model.doors[0].action = WL_DOOR_CLOSING;
    use_model.doors[0].position = 2048;
    use_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0x80u;
    CHECK(wl_step_doors(&use_model, NULL, 2, &door_step) == 0);
    CHECK(use_model.doors[0].action == WL_DOOR_CLOSED);
    CHECK(use_model.doors[0].position == 0);
    CHECK(use_model.tilemap[4 + 4 * WL_MAP_SIDE] == 0x80u);
    CHECK(door_step.closed_count == 1);

    wl_vswap_directory sprite_dirinfo;
    CHECK(wl_read_vswap_directory(vswap_path, &sprite_dirinfo) == 0);
    const uint16_t ref_chunks[] = {
        scene_refs[0].vswap_chunk_index,
        scene_refs[2].vswap_chunk_index,
        scene_refs[128].vswap_chunk_index,
        scene_refs[130].vswap_chunk_index,
        scene_refs[132].vswap_chunk_index,
    };
    unsigned char ref_sprite_pixels[5 * WL_MAP_PLANE_WORDS];
    wl_indexed_surface ref_surfaces[5];
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               ref_chunks, 5, 0, ref_sprite_pixels,
                                               sizeof(ref_sprite_pixels),
                                               ref_surfaces) == 0);
    CHECK(fnv1a_bytes(ref_surfaces[0].pixels, ref_surfaces[0].pixel_count) == 0x38769770);
    CHECK(fnv1a_bytes(ref_surfaces[1].pixels, ref_surfaces[1].pixel_count) == 0xbd6176ba);
    CHECK(fnv1a_bytes(ref_surfaces[2].pixels, ref_surfaces[2].pixel_count) == 0x0fe580fa);
    CHECK(fnv1a_bytes(ref_surfaces[3].pixels, ref_surfaces[3].pixel_count) == 0xa875d685);
    CHECK(fnv1a_bytes(ref_surfaces[4].pixels, ref_surfaces[4].pixel_count) == 0x63f7eba2);
    CHECK(fnv1a_bytes(ref_sprite_pixels, sizeof(ref_sprite_pixels)) == 0x4a8eb8db);
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               ref_chunks, 5, 0, ref_sprite_pixels,
                                               sizeof(ref_sprite_pixels) - 1,
                                               ref_surfaces) == -1);
    const uint16_t wall_chunk_ref[] = { 0 };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               wall_chunk_ref, 1, 0,
                                               ref_sprite_pixels,
                                               sizeof(ref_sprite_pixels),
                                               ref_surfaces) == -1);
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               ref_chunks, 0, 0, NULL, 0,
                                               NULL) == 0);
    CHECK(wl_collect_scene_sprite_refs(&model, 106, scene_refs, 132,
                                       &scene_ref_count) == -1);

    CHECK(wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                              WL_DIFFICULTY_MEDIUM, &model) == 0);
    CHECK(model.actor_count == 21);
    CHECK(model.kill_total == 20);
    CHECK(model.unknown_info_tiles == 0);

    CHECK(wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                              WL_DIFFICULTY_HARD, &model) == 0);
    CHECK(model.actor_count == 38);
    CHECK(model.kill_total == 37);
    CHECK(model.unknown_info_tiles == 0);

    struct model_map_expectation {
        uint16_t map_index;
        const char *name;
        uint16_t player_x;
        uint16_t player_y;
        wl_direction player_dir;
        size_t door_count;
        size_t static_count;
        size_t actor_count;
        size_t kill_total;
        size_t treasure_total;
        size_t secret_total;
        size_t path_marker_count;
        size_t scene_ref_count;
        uint32_t scene_ref_hash;
        size_t guard_count;
        size_t officer_count;
        size_t ss_count;
        size_t dog_count;
        size_t mutant_count;
        size_t boss_count;
        size_t ghost_count;
    } model_maps[] = {
        { 1, "Wolf1 Map2", 16, 61, WL_DIR_NORTH, 47, 344, 40, 40, 62, 4, 52,
          384, 0xab87ed41, 29, 0, 5, 6, 0, 0, 0 },
        { 8, "Wolf1 Boss", 34, 58, WL_DIR_NORTH, 3, 57, 1, 1, 1, 2, 0,
          58, 0x950314e6, 0, 0, 0, 0, 0, 1, 0 },
        { 10, "Wolf2 Map1", 7, 45, WL_DIR_EAST, 16, 173, 8, 8, 17, 4, 0,
          181, 0x89b8f3c0, 0, 0, 0, 0, 8, 0, 0 },
        { 18, "Wolf2 Boss", 31, 62, WL_DIR_NORTH, 2, 165, 3, 3, 1, 6, 0,
          168, 0xb2dab28b, 0, 0, 0, 0, 2, 1, 0 },
        { 20, "Wolf3 Map1", 9, 17, WL_DIR_EAST, 8, 132, 9, 9, 41, 5, 22,
          141, 0xc090c2df, 8, 1, 0, 0, 0, 0, 0 },
        { 28, "Wolf3 Boss", 13, 57, WL_DIR_EAST, 6, 85, 12, 12, 0, 1, 0,
          97, 0x713b74e4, 2, 4, 0, 0, 0, 6, 0 },
        { 29, "Wolf3 Secret", 33, 53, WL_DIR_WEST, 2, 110, 24, 24, 97, 0, 17,
          134, 0xe03fdb45, 17, 2, 1, 0, 0, 0, 4 },
        { 38, "Wolf4 Boss", 10, 1, WL_DIR_SOUTH, 9, 130, 9, 9, 1, 1, 0,
          139, 0x7a3bd88c, 1, 7, 0, 0, 0, 1, 0 },
        { 48, "Wolf5 Boss", 32, 21, WL_DIR_SOUTH, 15, 193, 9, 9, 50, 2, 0,
          202, 0x05f9df63, 0, 4, 4, 0, 0, 1, 0 },
        { 58, "Wolf6 Boss", 32, 61, WL_DIR_NORTH, 9, 200, 41, 41, 48, 5, 24,
          241, 0x2b8bf08d, 14, 12, 14, 0, 0, 1, 0 },
        { 59, "Wolf6 Secret", 34, 32, WL_DIR_EAST, 29, 386, 37, 37, 35, 17, 8,
          423, 0x59f0ca20, 11, 8, 3, 2, 9, 3, 1 },
    };
    for (size_t i = 0; i < sizeof(model_maps) / sizeof(model_maps[0]); ++i) {
        wl_map_header extra_map;
        CHECK(wl_read_map_header(gamemaps_path, mh.offsets[model_maps[i].map_index],
                                 &extra_map) == 0);
        CHECK(strcmp(extra_map.name, model_maps[i].name) == 0);
        CHECK(wl_read_map_plane(gamemaps_path, &extra_map, 0, mh.rlew_tag,
                                wall_plane, WL_MAP_PLANE_WORDS) == 0);
        CHECK(wl_read_map_plane(gamemaps_path, &extra_map, 1, mh.rlew_tag,
                                info_plane, WL_MAP_PLANE_WORDS) == 0);
        CHECK(wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                                  WL_DIFFICULTY_EASY, &model) == 0);
        CHECK(model.player.present == 1);
        CHECK(model.player.x == model_maps[i].player_x);
        CHECK(model.player.y == model_maps[i].player_y);
        CHECK(model.player.dir == model_maps[i].player_dir);
        CHECK(model.door_count == model_maps[i].door_count);
        CHECK(model.static_count == model_maps[i].static_count);
        CHECK(model.actor_count == model_maps[i].actor_count);
        CHECK(model.kill_total == model_maps[i].kill_total);
        CHECK(model.treasure_total == model_maps[i].treasure_total);
        CHECK(model.secret_total == model_maps[i].secret_total);
        CHECK(model.path_marker_count == model_maps[i].path_marker_count);
        CHECK(model.unknown_info_tiles == 0);
        size_t actor_kind_counts[WL_ACTOR_DEAD_GUARD + 1] = { 0 };
        for (size_t actor_index = 0; actor_index < model.actor_count; ++actor_index) {
            CHECK(model.actors[actor_index].kind <= WL_ACTOR_DEAD_GUARD);
            ++actor_kind_counts[model.actors[actor_index].kind];
        }
        CHECK(actor_kind_counts[WL_ACTOR_GUARD] == model_maps[i].guard_count);
        CHECK(actor_kind_counts[WL_ACTOR_OFFICER] == model_maps[i].officer_count);
        CHECK(actor_kind_counts[WL_ACTOR_SS] == model_maps[i].ss_count);
        CHECK(actor_kind_counts[WL_ACTOR_DOG] == model_maps[i].dog_count);
        CHECK(actor_kind_counts[WL_ACTOR_MUTANT] == model_maps[i].mutant_count);
        CHECK(actor_kind_counts[WL_ACTOR_BOSS] == model_maps[i].boss_count);
        CHECK(actor_kind_counts[WL_ACTOR_GHOST] == model_maps[i].ghost_count);
        CHECK(wl_collect_scene_sprite_refs(&model, 106, scene_refs,
                                           sizeof(scene_refs) / sizeof(scene_refs[0]),
                                           &scene_ref_count) == 0);
        CHECK(scene_ref_count == model_maps[i].scene_ref_count);
        CHECK(fnv1a_scene_sprite_refs(scene_refs, scene_ref_count) ==
              model_maps[i].scene_ref_hash);
    }
    CHECK(wl_read_map_plane(gamemaps_path, &map0, 0, mh.rlew_tag,
                            wall_plane, WL_MAP_PLANE_WORDS) == 0);
    CHECK(wl_read_map_plane(gamemaps_path, &map0, 1, mh.rlew_tag,
                            info_plane, WL_MAP_PLANE_WORDS) == 0);
    CHECK(wl_build_game_model(wall_plane, info_plane, WL_MAP_PLANE_WORDS,
                              WL_DIFFICULTY_EASY, &model) == 0);
    CHECK(wl_collect_scene_sprite_refs(&model, 106, scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);

    wl_graphics_header gh;
    wl_huffman_node huff[WL_HUFFMAN_NODE_COUNT];
    unsigned char graphics_buf[65536];
    unsigned char indexed_buf[65536];
    unsigned char rgba_buf[65536 * 4];
    unsigned char upload_palette[256 * 3];
    unsigned char red_shift_palettes[WL_NUM_RED_SHIFTS * 256u * 3u];
    unsigned char white_shift_palettes[WL_NUM_WHITE_SHIFTS * 256u * 3u];
    unsigned char fade_target_palette[256 * 3];
    unsigned char fade_palette[256 * 3];
    unsigned char fade_sample_pixels[16];
    wl_indexed_surface fade_sample_surface;
    unsigned char fade_sample_rgba[16 * 4];
    unsigned char canvas_pixels[160 * 120];
    wl_indexed_surface surface;
    wl_indexed_surface canvas;
    wl_texture_upload_descriptor upload;
    wl_texture_upload_descriptor rgba_upload;
    size_t graphics_bytes = 0;
    size_t compressed_bytes = 0;
    for (size_t i = 0; i < 256; ++i) {
        upload_palette[i * 3 + 0] = (unsigned char)(i & 63u);
        upload_palette[i * 3 + 1] = (unsigned char)((i * 2u) & 63u);
        upload_palette[i * 3 + 2] = (unsigned char)((63u - i) & 63u);
        fade_target_palette[i * 3 + 0] = 0;
        fade_target_palette[i * 3 + 1] = 17;
        fade_target_palette[i * 3 + 2] = 17;
    }
    for (uint16_t i = 1; i <= WL_NUM_RED_SHIFTS; ++i) {
        CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                     64, 0, 0, i, WL_RED_SHIFT_STEPS,
                                     red_shift_palettes +
                                         (size_t)(i - 1u) * sizeof(upload_palette),
                                     sizeof(upload_palette)) == 0);
    }
    for (uint16_t i = 1; i <= WL_NUM_WHITE_SHIFTS; ++i) {
        CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                     64, 62, 0, i, WL_WHITE_SHIFT_STEPS,
                                     white_shift_palettes +
                                         (size_t)(i - 1u) * sizeof(upload_palette),
                                     sizeof(upload_palette)) == 0);
    }
    for (size_t i = 0; i < sizeof(fade_sample_pixels); ++i) {
        fade_sample_pixels[i] = (unsigned char)(i * 17u);
    }
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 6, 0, 255, 5,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == 0);
    CHECK(fnv1a_bytes(fade_palette, sizeof(fade_palette)) == 0xa93a5ba5);
    CHECK(fade_palette[0] == 0);
    CHECK(fade_palette[1] == 8);
    CHECK(fade_palette[2] == 40);
    CHECK(fade_palette[255u * 3u + 0u] == 32);
    CHECK(fade_palette[255u * 3u + 1u] == 40);
    CHECK(fade_palette[255u * 3u + 2u] == 8);
    CHECK(wl_wrap_indexed_surface(4, 4, fade_sample_pixels,
                                  sizeof(fade_sample_pixels),
                                  &fade_sample_surface) == 0);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, fade_palette,
                                            sizeof(fade_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0x50918d48);
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 6, 32, 63, 10,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == 0);
    CHECK(fnv1a_bytes(fade_palette, sizeof(fade_palette)) == 0x91f102c5);
    CHECK(fade_palette[31u * 3u + 0u] == 31);
    CHECK(fade_palette[32u * 3u + 0u] == 0);
    CHECK(fade_palette[32u * 3u + 1u] == 17);
    CHECK(fade_palette[32u * 3u + 2u] == 17);
    CHECK(fade_palette[63u * 3u + 0u] == 0);
    CHECK(fade_palette[63u * 3u + 1u] == 17);
    CHECK(fade_palette[63u * 3u + 2u] == 17);
    CHECK(fade_palette[64u * 3u + 2u] == 63);
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 6, 64, 32, 0,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == -1);
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 5, 0, 255, 0,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == -1);
    CHECK(wl_interpolate_palette_range(upload_palette, fade_target_palette,
                                       sizeof(upload_palette), 6, 0, 255, 11,
                                       10, fade_palette,
                                       sizeof(fade_palette)) == -1);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                 64, 0, 0, 6, 8, fade_palette,
                                 sizeof(fade_palette)) == 0);
    CHECK(fnv1a_bytes(fade_palette, sizeof(fade_palette)) == 0xb8462fc5);
    CHECK(fade_palette[0] == 48);
    CHECK(fade_palette[1] == 0);
    CHECK(fade_palette[2] == 16);
    CHECK(fade_palette[255u * 3u + 0u] == 63);
    CHECK(fade_palette[255u * 3u + 1u] == 16);
    CHECK(fade_palette[255u * 3u + 2u] == 0);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, fade_palette,
                                            sizeof(fade_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0xfa0a0cd7);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                 64, 62, 0, 3, 20, fade_palette,
                                 sizeof(fade_palette)) == 0);
    CHECK(fnv1a_bytes(fade_palette, sizeof(fade_palette)) == 0x3c8da1ed);
    CHECK(fade_palette[0] == 9);
    CHECK(fade_palette[1] == 9);
    CHECK(fade_palette[2] == 54);
    CHECK(fade_palette[255u * 3u + 0u] == 63);
    CHECK(fade_palette[255u * 3u + 1u] == 62);
    CHECK(fade_palette[255u * 3u + 2u] == 0);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, fade_palette,
                                            sizeof(fade_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0x93adda7f);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                 65, 0, 0, 1, 8, fade_palette,
                                 sizeof(fade_palette)) == -1);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 5,
                                 64, 0, 0, 1, 8, fade_palette,
                                 sizeof(fade_palette)) == -1);
    CHECK(wl_build_palette_shift(upload_palette, sizeof(upload_palette), 6,
                                 64, 0, 0, 9, 8, fade_palette,
                                 sizeof(fade_palette)) == -1);
    wl_palette_shift_state shift_state;
    wl_palette_shift_result shift_result;
    const unsigned char *selected_palette = NULL;
    CHECK(wl_reset_palette_shift_state(&shift_state) == 0);
    CHECK(wl_update_palette_shift_state(&shift_state, 1, &shift_result) == 0);
    CHECK(shift_result.kind == WL_PALETTE_SHIFT_NONE);
    CHECK(shift_result.shift_index == 0);
    CHECK(shift_result.damage_count == 0);
    CHECK(shift_result.bonus_count == 0);
    CHECK(shift_result.palette_shifted == 0);
    CHECK(wl_start_bonus_palette_shift(&shift_state) == 0);
    CHECK(wl_update_palette_shift_state(&shift_state, 5, &shift_result) == 0);
    CHECK(shift_result.kind == WL_PALETTE_SHIFT_WHITE);
    CHECK(shift_result.shift_index == 2);
    CHECK(shift_result.damage_count == 0);
    CHECK(shift_result.bonus_count == 13);
    CHECK(shift_result.palette_shifted == 1);
    CHECK(wl_select_palette_for_shift(&shift_result, upload_palette,
                                      red_shift_palettes, WL_NUM_RED_SHIFTS,
                                      white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                      sizeof(upload_palette),
                                      &selected_palette) == 0);
    CHECK(selected_palette == white_shift_palettes + 2u * sizeof(upload_palette));
    CHECK(fnv1a_bytes(selected_palette, sizeof(upload_palette)) == 0x3c8da1ed);
    CHECK(wl_describe_palette_shifted_texture_upload(
              &fade_sample_surface, &shift_result, upload_palette,
              red_shift_palettes, WL_NUM_RED_SHIFTS, white_shift_palettes,
              WL_NUM_WHITE_SHIFTS, sizeof(upload_palette), 6, &upload) == 0);
    CHECK(upload.palette == selected_palette);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, upload.palette,
                                            sizeof(upload_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0x93adda7f);
    CHECK(wl_start_damage_palette_shift(&shift_state, 25) == 0);
    CHECK(wl_update_palette_shift_state(&shift_state, 4, &shift_result) == 0);
    CHECK(shift_result.kind == WL_PALETTE_SHIFT_RED);
    CHECK(shift_result.shift_index == 2);
    CHECK(shift_result.damage_count == 21);
    CHECK(shift_result.bonus_count == 9);
    CHECK(shift_result.palette_shifted == 1);
    CHECK(wl_select_palette_for_shift(&shift_result, upload_palette,
                                      red_shift_palettes, WL_NUM_RED_SHIFTS,
                                      white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                      sizeof(upload_palette),
                                      &selected_palette) == 0);
    CHECK(selected_palette == red_shift_palettes + 2u * sizeof(upload_palette));
    CHECK(fnv1a_bytes(selected_palette, sizeof(upload_palette)) == 0x90a6cdc5);
    CHECK(wl_describe_palette_shifted_texture_upload(
              &fade_sample_surface, &shift_result, upload_palette,
              red_shift_palettes, WL_NUM_RED_SHIFTS, white_shift_palettes,
              WL_NUM_WHITE_SHIFTS, sizeof(upload_palette), 6, &upload) == 0);
    CHECK(upload.palette == selected_palette);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, upload.palette,
                                            sizeof(upload_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0xd742b64a);
    CHECK(wl_update_palette_shift_state(&shift_state, 30, &shift_result) == 0);
    CHECK(shift_result.kind == WL_PALETTE_SHIFT_RED);
    CHECK(shift_result.shift_index == 2);
    CHECK(shift_result.damage_count == 0);
    CHECK(shift_result.bonus_count == 0);
    CHECK(shift_result.palette_shifted == 1);
    CHECK(wl_update_palette_shift_state(&shift_state, 1, &shift_result) == 0);
    CHECK(shift_result.kind == WL_PALETTE_SHIFT_BASE);
    CHECK(shift_result.shift_index == 0);
    CHECK(shift_result.palette_shifted == 0);
    CHECK(wl_select_palette_for_shift(&shift_result, upload_palette,
                                      red_shift_palettes, WL_NUM_RED_SHIFTS,
                                      white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                      sizeof(upload_palette),
                                      &selected_palette) == 0);
    CHECK(selected_palette == upload_palette);
    CHECK(wl_describe_palette_shifted_texture_upload(
              &fade_sample_surface, &shift_result, upload_palette,
              red_shift_palettes, WL_NUM_RED_SHIFTS, white_shift_palettes,
              WL_NUM_WHITE_SHIFTS, sizeof(upload_palette), 6, &upload) == 0);
    CHECK(upload.palette == upload_palette);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, upload.palette,
                                            sizeof(upload_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0xccd1a665);

    memset(&live_tick_pickup_model, 0, sizeof(live_tick_pickup_model));
    live_tick_pickup_model.static_count = 1;
    live_tick_pickup_model.statics[0].x = 4;
    live_tick_pickup_model.statics[0].y = 4;
    live_tick_pickup_model.statics[0].type = 24;
    live_tick_pickup_model.statics[0].bonus = 1;
    live_tick_pickup_model.statics[0].active = 1;
    live_tick_motion.x = 0x3f000u;
    live_tick_motion.y = 0x48000u;
    live_tick_motion.tile_x = 3;
    live_tick_motion.tile_y = 4;
    CHECK(wl_init_player_gameplay_state(&pickup_state, 90, 3, 0, WL_EXTRA_POINTS) == 0);
    pickup_state.best_weapon = WL_WEAPON_MACHINEGUN;
    pickup_state.chosen_weapon = WL_WEAPON_MACHINEGUN;
    pickup_state.weapon = WL_WEAPON_KNIFE;
    pickup_state.ammo = 3;
    pickup_state.attack_frame = 2;
    CHECK(wl_step_live_tick(&pickup_state, &live_tick_pickup_model,
                            use_wall, use_info, WL_MAP_PLANE_WORDS,
                            &live_tick_motion, 0x1000, 0, 0x10000, 0,
                            WL_DIR_EAST, 0, 0, 1, &live_tick) == 0);
    CHECK(live_tick.attack.advanced == 1);
    CHECK(live_tick.attack.frame_before == 2);
    CHECK(live_tick.attack.frame_after == 1);
    CHECK(live_tick.attack.finished == 0);
    CHECK(pickup_state.attack_frame == 1);
    CHECK(live_tick.palette.kind == WL_PALETTE_SHIFT_WHITE);
    CHECK(wl_describe_palette_shifted_texture_upload(
              &fade_sample_surface, &live_tick.palette, upload_palette,
              red_shift_palettes, WL_NUM_RED_SHIFTS, white_shift_palettes,
              WL_NUM_WHITE_SHIFTS, sizeof(upload_palette), 6, &upload) == 0);
    CHECK(upload.palette == white_shift_palettes + 2u * sizeof(upload_palette));
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, upload.palette,
                                            sizeof(upload_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0x93adda7f);

    wl_game_model live_combat_model;
    memset(&live_combat_model, 0, sizeof(live_combat_model));
    wl_actor_desc live_combat_dog;
    memset(&live_combat_dog, 0, sizeof(live_combat_dog));
    live_combat_dog.kind = WL_ACTOR_DOG;
    live_combat_dog.tile_x = 5;
    live_combat_dog.tile_y = 4;
    wl_projectile_state live_combat_projectile = {
        (3u << 16) + 0x8000u, (4u << 16) + 0x8000u, 3, 4,
        WL_PROJECTILE_NEEDLE, 1,
    };
    wl_player_motion_state live_combat_motion = {
        (4u << 16) + 0x8000u, (4u << 16) + 0x8000u, 4, 4,
    };
    wl_player_gameplay_state live_combat_state;
    wl_live_combat_tick_result live_combat;
    CHECK(wl_init_player_gameplay_state(&live_combat_state, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_combat_tick(&live_combat_state, &live_combat_model,
                                   use_wall, use_info, WL_MAP_PLANE_WORDS,
                                   &live_combat_motion, 0, 0, 0x10000, 0,
                                   WL_DIR_EAST, 0, 0,
                                   &live_combat_dog, &live_combat_projectile,
                                   WL_DIFFICULTY_HARD, 1, 1, 0, 1, 179, 80,
                                   0x10000, 0, 80, 0, 0, 1,
                                   &live_combat) == 0);
    CHECK(live_combat.live.palette.kind == WL_PALETTE_SHIFT_RED);
    CHECK(live_combat.live.palette.shift_index == 3);
    CHECK(live_combat.live.palette.damage_count == 34);
    CHECK(wl_describe_palette_shifted_texture_upload(
              &fade_sample_surface, &live_combat.live.palette, upload_palette,
              red_shift_palettes, WL_NUM_RED_SHIFTS, white_shift_palettes,
              WL_NUM_WHITE_SHIFTS, sizeof(upload_palette), 6, &upload) == 0);
    CHECK(upload.palette == red_shift_palettes + 3u * sizeof(upload_palette));
    CHECK(fnv1a_bytes(upload.palette, sizeof(upload_palette)) == 0x35132dc5);
    wl_present_frame_descriptor live_combat_present;
    CHECK(wl_describe_present_frame(&fade_sample_surface,
                                    &live_combat.live.palette,
                                    upload_palette,
                                    red_shift_palettes, WL_NUM_RED_SHIFTS,
                                    white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                    sizeof(upload_palette), 6,
                                    &live_combat_present) == 0);
    CHECK(live_combat_present.viewport_width == 4);
    CHECK(live_combat_present.viewport_height == 4);
    CHECK(live_combat_present.pixel_hash ==
          fnv1a_bytes(fade_sample_pixels, sizeof(fade_sample_pixels)));
    CHECK(live_combat_present.palette_hash == 0x35132dc5);
    CHECK(live_combat_present.palette_shift_kind == WL_PALETTE_SHIFT_RED);
    CHECK(live_combat_present.palette_shift_index == 3);
    CHECK(live_combat_present.texture.palette == upload.palette);
    CHECK(wl_expand_indexed_surface_to_rgba(&fade_sample_surface, upload.palette,
                                            sizeof(upload_palette), 6,
                                            fade_sample_rgba,
                                            sizeof(fade_sample_rgba),
                                            NULL) == 0);
    CHECK(fnv1a_bytes(fade_sample_rgba, sizeof(fade_sample_rgba)) == 0x8e2b026e);
    CHECK(wl_update_palette_shift_state(&shift_state, 1, &shift_result) == 0);
    CHECK(shift_result.kind == WL_PALETTE_SHIFT_NONE);
    CHECK(wl_start_damage_palette_shift(&shift_state, -1) == -1);
    CHECK(wl_update_palette_shift_state(&shift_state, -1, &shift_result) == -1);
    CHECK(wl_start_bonus_palette_shift(NULL) == -1);
    shift_result.kind = WL_PALETTE_SHIFT_RED;
    shift_result.shift_index = WL_NUM_RED_SHIFTS;
    CHECK(wl_select_palette_for_shift(&shift_result, upload_palette,
                                      red_shift_palettes, WL_NUM_RED_SHIFTS,
                                      white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                      sizeof(upload_palette),
                                      &selected_palette) == -1);
    CHECK(wl_describe_palette_shifted_texture_upload(
              &fade_sample_surface, &shift_result, upload_palette,
              red_shift_palettes, WL_NUM_RED_SHIFTS, white_shift_palettes,
              WL_NUM_WHITE_SHIFTS, sizeof(upload_palette), 6, &upload) == -1);
    CHECK(wl_read_graphics_header(vgahead_path, &gh) == 0);
    CHECK(gh.chunk_count == 149);
    CHECK(gh.file_size == 450);
    CHECK(gh.offsets[0] == 0);
    CHECK(gh.offsets[1] == 354);
    CHECK(gh.offsets[3] == 9570);
    CHECK(gh.offsets[149] == 275774);
    CHECK(wl_read_huffman_dictionary(vgadict_path, huff) == 0);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 0, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 354);
    CHECK(graphics_bytes == 528);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x0a6f459a);
    wl_picture_table_metadata pictures;
    CHECK(wl_decode_picture_table(graphics_buf, graphics_bytes, &pictures) == 0);
    CHECK(pictures.picture_count == 132);
    CHECK(pictures.min_width == 8);
    CHECK(pictures.max_width == 320);
    CHECK(pictures.min_height == 8);
    CHECK(pictures.max_height == 200);
    CHECK(pictures.total_pixels == 342464);
    CHECK(pictures.pictures[0].width == 96);
    CHECK(pictures.pictures[0].height == 88);
    CHECK(pictures.pictures[3].width == 320);
    CHECK(pictures.pictures[3].height == 8);
    CHECK(pictures.pictures[84].width == 320);
    CHECK(pictures.pictures[84].height == 200);
    CHECK(pictures.pictures[86].width == 320);
    CHECK(pictures.pictures[86].height == 200);
    CHECK(pictures.pictures[87].width == 224);
    CHECK(pictures.pictures[87].height == 56);
    CHECK(pictures.pictures[131].width == 224);
    CHECK(pictures.pictures[131].height == 48);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 1, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 3467);
    CHECK(graphics_bytes == 8300);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xdb48ce2b);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 3, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 8057);
    CHECK(graphics_bytes == 8448);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x5c152b5c);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[0].width,
                                           pictures.pictures[0].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.format == WL_SURFACE_INDEXED8);
    CHECK(surface.width == 96);
    CHECK(surface.height == 88);
    CHECK(surface.stride == 96);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(surface.pixels == indexed_buf);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0xa9c1ea92);
    CHECK(wl_describe_indexed_texture_upload(&surface, upload_palette,
                                             sizeof(upload_palette), 6,
                                             &upload) == 0);
    CHECK(upload.format == WL_TEXTURE_UPLOAD_INDEXED8_RGB_PALETTE);
    CHECK(upload.width == 96);
    CHECK(upload.height == 88);
    CHECK(upload.pitch == 96);
    CHECK(upload.pixel_bytes == 8448);
    CHECK(upload.pixels == surface.pixels);
    CHECK(upload.palette == upload_palette);
    CHECK(upload.palette_entries == 256);
    CHECK(upload.palette_component_bits == 6);
    CHECK(wl_expand_indexed_surface_to_rgba(&surface, upload_palette,
                                            sizeof(upload_palette), 6, rgba_buf,
                                            sizeof(rgba_buf), &rgba_upload) == 0);
    CHECK(rgba_upload.format == WL_TEXTURE_UPLOAD_RGBA8888);
    CHECK(rgba_upload.width == 96);
    CHECK(rgba_upload.height == 88);
    CHECK(rgba_upload.pitch == 384);
    CHECK(rgba_upload.pixel_bytes == 33792);
    CHECK(rgba_upload.pixels == rgba_buf);
    CHECK(rgba_upload.palette == NULL);
    CHECK(fnv1a_bytes(rgba_buf, rgba_upload.pixel_bytes) == 0xb75bdee9);
    CHECK(wl_describe_indexed_texture_upload(&surface, upload_palette,
                                             sizeof(upload_palette), 5,
                                             &upload) == -1);
    CHECK(wl_expand_indexed_surface_to_rgba(&surface, upload_palette,
                                            sizeof(upload_palette), 6, rgba_buf,
                                            rgba_upload.pixel_bytes - 1,
                                            &rgba_upload) == -1);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(160, 120, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_blit_indexed_surface(&surface, &canvas, 13, 17) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xf7c5e35c);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 87, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 45948);
    CHECK(graphics_bytes == 64000);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x01643ebc);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[84].width,
                                           pictures.pictures[84].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.width == 320);
    CHECK(surface.height == 200);
    CHECK(surface.stride == 320);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x4b172b02);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 134, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 5127);
    CHECK(graphics_bytes == 10752);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xeb393cc0);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[131].width,
                                           pictures.pictures[131].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.width == 224);
    CHECK(surface.height == 48);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x46e4bd08);
    CHECK(wl_blit_indexed_surface(&surface, &canvas, -12, 70) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x338ae9cd);

    wl_vswap_header vs;
    CHECK(wl_read_vswap_header(vswap_path, &vs) == 0);
    CHECK(vs.file_size == 1544376);
    CHECK(vs.chunks_in_file == 663);
    CHECK(vs.sprite_start == 106);
    CHECK(vs.sound_start == 542);
    CHECK(vs.first_offsets[0] == 4096);
    CHECK(vs.first_offsets[1] == 8192);
    CHECK(vs.first_offsets[2] == 12288);
    CHECK(vs.first_offsets[3] == 16384);
    CHECK(vs.first_offsets[4] == 20480);

    wl_vswap_directory dirinfo;
    CHECK(wl_read_vswap_directory(vswap_path, &dirinfo) == 0);
    CHECK(dirinfo.header.chunks_in_file == 663);
    CHECK(dirinfo.data_start == 3984);
    CHECK(dirinfo.max_chunk_end == 1544376);
    CHECK(dirinfo.wall_count == 106);
    CHECK(dirinfo.sprite_count == 436);
    CHECK(dirinfo.sound_count == 121);
    CHECK(dirinfo.sparse_count == 0);
    CHECK(dirinfo.chunks[0].offset == 4096);
    CHECK(dirinfo.chunks[0].length == 4096);
    CHECK(dirinfo.chunks[0].kind == WL_VSWAP_CHUNK_WALL);
    CHECK(dirinfo.chunks[105].offset == 434176);
    CHECK(dirinfo.chunks[105].length == 4096);
    CHECK(dirinfo.chunks[106].offset == 438272);
    CHECK(dirinfo.chunks[106].length == 1306);
    CHECK(dirinfo.chunks[106].kind == WL_VSWAP_CHUNK_SPRITE);
    CHECK(dirinfo.chunks[541].offset == 1139200);
    CHECK(dirinfo.chunks[541].length == 650);
    CHECK(dirinfo.chunks[542].offset == 1140224);
    CHECK(dirinfo.chunks[542].length == 4096);
    CHECK(dirinfo.chunks[542].kind == WL_VSWAP_CHUNK_SOUND);
    CHECK(dirinfo.chunks[662].offset == 1544192);
    CHECK(dirinfo.chunks[662].length == 184);

    unsigned char chunk_buf[4096];
    unsigned char wall0_buf[4096];
    unsigned char wall1_buf[4096];
    unsigned char wall3_buf[4096];
    unsigned char wall9_buf[4096];
    unsigned char wall14_buf[4096];
    unsigned char wall15_buf[4096];
    unsigned char wall16_buf[4096];
    unsigned char wall17_buf[4096];
    unsigned char wall73_buf[4096];
    unsigned char door99_buf[4096];
    unsigned char door105_buf[4096];
    unsigned char wall63_buf[4096];
    unsigned char column_buf[64];
    unsigned char surface_column_buf[64];
    size_t chunk_bytes = 0;
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 0, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x98d020a5);
    memcpy(wall0_buf, chunk_buf, sizeof(wall0_buf));
    wl_vswap_shape_metadata shape;
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[0].kind,
                                         &shape) == 0);
    CHECK(shape.kind == WL_VSWAP_CHUNK_WALL);
    CHECK(shape.width == 64);
    CHECK(shape.height == 64);
    CHECK(shape.visible_columns == 64);
    wl_wall_page_metadata wallmeta;
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.width == 64);
    CHECK(wallmeta.height == 64);
    CHECK(wallmeta.column_count == 64);
    CHECK(wallmeta.bytes_per_column == 64);
    CHECK(wallmeta.min_color == 7);
    CHECK(wallmeta.max_color == 31);
    CHECK(wallmeta.unique_color_count == 18);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(surface.format == WL_SURFACE_INDEXED8);
    CHECK(surface.width == 64);
    CHECK(surface.height == 64);
    CHECK(surface.stride == 64);
    CHECK(surface.pixel_count == 4096);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x8fe4d8ff);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0000, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0xc77d483d);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0040, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x272b5483);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0800, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x2fbb79bb);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0fc0, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x19c55a4e);
    CHECK(wl_sample_indexed_surface_column(&surface, 63, surface_column_buf,
                                           sizeof(surface_column_buf)) == 0);
    CHECK(memcmp(column_buf, surface_column_buf, sizeof(column_buf)) == 0);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0000, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &canvas,
                                          7, 64) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xceb8a051);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0fc0, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &canvas,
                                          29, 160) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xf25f51d9);
    CHECK(wl_scale_wall_column_to_surface(column_buf, 63, &canvas, 7, 64) == -1);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &canvas,
                                          80, 64) == -1);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0001, column_buf,
                                     sizeof(column_buf)) == -1);
    CHECK(wl_sample_indexed_surface_column(&surface, 64, surface_column_buf,
                                           sizeof(surface_column_buf)) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 1, wall1_buf, sizeof(wall1_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 3, wall3_buf, sizeof(wall3_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 63, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xa9a1ca8c);
    memcpy(wall63_buf, chunk_buf, sizeof(wall63_buf));
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 26);
    CHECK(wallmeta.max_color == 223);
    CHECK(wallmeta.unique_color_count == 31);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x5b4d4c38);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0800, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x8a859220);
    CHECK(wl_sample_indexed_surface_column(&surface, 32, surface_column_buf,
                                           sizeof(surface_column_buf)) == 0);
    CHECK(memcmp(column_buf, surface_column_buf, sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &canvas,
                                          13, 96) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xb200118);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    const wl_wall_strip wl6_strips[] = {
        { wall0_buf, sizeof(wall0_buf), 0x0000, 7, 64 },
        { wall0_buf, sizeof(wall0_buf), 0x0fc0, 29, 160 },
        { wall63_buf, sizeof(wall63_buf), 0x0800, 13, 96 },
    };
    CHECK(wl_render_wall_strip_viewport(wl6_strips, 3, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xb200118);
    CHECK(wl_render_wall_strip_viewport(wl6_strips, 0, &canvas) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 9, wall9_buf, sizeof(wall9_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 14, wall14_buf, sizeof(wall14_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 15, wall15_buf, sizeof(wall15_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 16, wall16_buf, sizeof(wall16_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 17, wall17_buf, sizeof(wall17_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 73, wall73_buf, sizeof(wall73_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 99, door99_buf, sizeof(door99_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 105, door105_buf, sizeof(door105_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    wl_map_wall_hit hits[4];
    CHECK(wl_build_map_wall_hit(wall_plane, WL_MAP_PLANE_WORDS, 0, 0,
                                WL_WALL_SIDE_HORIZONTAL, 0, 7, 64,
                                &hits[0]) == 0);
    CHECK(hits[0].source_tile == 1);
    CHECK(hits[0].wall_page_index == 0);
    CHECK(hits[0].texture_offset == 0x0000);
    CHECK(wl_build_map_wall_hit(wall_plane, WL_MAP_PLANE_WORDS, 62, 33,
                                WL_WALL_SIDE_VERTICAL, 32, 13, 96,
                                &hits[1]) == 0);
    CHECK(hits[1].source_tile == 5);
    CHECK(hits[1].wall_page_index == 9);
    CHECK(hits[1].texture_offset == 0x0800);
    CHECK(wl_build_map_wall_hit(wall_plane, WL_MAP_PLANE_WORDS, 53, 28,
                                WL_WALL_SIDE_HORIZONTAL, 63, 29, 160,
                                &hits[2]) == 0);
    CHECK(hits[2].source_tile == 8);
    CHECK(hits[2].wall_page_index == 14);
    CHECK(hits[2].texture_offset == 0x0fc0);
    const unsigned char *hit_pages[] = { wall0_buf, wall9_buf, wall14_buf };
    wl_wall_strip hit_strips[3];
    for (size_t i = 0; i < 3; ++i) {
        CHECK(wl_wall_hit_to_strip(&hits[i], hit_pages[i], 4096, &hit_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(hit_strips, 3, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x7ffb21c0);
    CHECK(wl_build_map_wall_hit(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                WL_WALL_SIDE_HORIZONTAL, 0, 0, 64,
                                &hits[0]) == -1);

    wl_map_wall_hit door_hit;
    wl_door_desc locked_vertical_door = model.doors[0];
    locked_vertical_door.lock = 1;
    locked_vertical_door.position = 0x4000u;
    CHECK(wl_build_door_wall_hit(&model.doors[0], dirinfo.header.sprite_start,
                                 (29u << 16) + 0x8000u, 21, 128,
                                 &door_hit) == 0);
    CHECK(door_hit.tile_x == model.doors[0].x);
    CHECK(door_hit.tile_y == model.doors[0].y);
    CHECK(door_hit.side == WL_WALL_SIDE_VERTICAL);
    CHECK(door_hit.wall_page_index == 99);
    CHECK(door_hit.texture_offset == 0x0800);
    CHECK(wl_build_door_wall_hit(&locked_vertical_door, dirinfo.header.sprite_start,
                                 (29u << 16) + 0x8000u, 22, 128,
                                 &door_hit) == 0);
    CHECK(door_hit.wall_page_index == 105);
    CHECK(door_hit.texture_offset == 0x0400);
    wl_wall_strip door_strip;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_wall_hit_to_strip(&door_hit, door105_buf, sizeof(door105_buf),
                               &door_strip) == 0);
    CHECK(wl_render_wall_strip_viewport(&door_strip, 1, &canvas) == 0);
    uint32_t locked_door_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(locked_door_hash == 0x40d8b9a5);
    CHECK(wl_build_door_wall_hit(&model.doors[0], 7, (29u << 16) + 0x8000u,
                                 21, 128, &door_hit) == -1);

    wl_game_model ray_model;
    memset(&ray_model, 0, sizeof(ray_model));
    ray_model.door_count = 1;
    ray_model.doors[0].x = 4;
    ray_model.doors[0].y = 4;
    ray_model.doors[0].source_tile = 90;
    ray_model.doors[0].vertical = 1;
    ray_model.doors[0].lock = 1;
    ray_model.doors[0].position = 0x4000u;
    ray_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0x80u;
    ray_model.tilemap[6 + 4 * WL_MAP_SIDE] = 2;
    CHECK(wl_cast_runtime_fixed_wall_ray(&ray_model, dirinfo.header.sprite_start,
                                         (3u << 16) + 0x8000u,
                                         (4u << 16) + 0x8000u,
                                         0x10000, 0, 31, 128,
                                         &door_hit) == 0);
    CHECK(door_hit.tile_x == 4);
    CHECK(door_hit.tile_y == 4);
    CHECK(door_hit.wall_page_index == 105);
    CHECK(door_hit.texture_offset == 0x0400);
    CHECK(door_hit.distance == 0x8000u);
    ray_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0;
    CHECK(wl_cast_runtime_fixed_wall_ray(&ray_model, dirinfo.header.sprite_start,
                                         (3u << 16) + 0x8000u,
                                         (4u << 16) + 0x8000u,
                                         0x10000, 0, 32, 128,
                                         &door_hit) == 0);
    CHECK(door_hit.tile_x == 6);
    CHECK(door_hit.source_tile == 2);
    CHECK(door_hit.wall_page_index == 3);
    CHECK(door_hit.texture_offset == 0x0800);
    CHECK(door_hit.distance == 0x28000u);
    ray_model.tilemap[5 + 4 * WL_MAP_SIDE] = (uint16_t)(37u | 0xc0u);
    CHECK(wl_cast_runtime_fixed_wall_ray(&ray_model, dirinfo.header.sprite_start,
                                         (4u << 16) + 0x8000u,
                                         (4u << 16) + 0x8000u,
                                         0x10000, 0, 33, 128,
                                         &door_hit) == 0);
    CHECK(door_hit.tile_x == 5);
    CHECK(door_hit.source_tile == 37);
    CHECK(door_hit.wall_page_index == 73);
    CHECK(door_hit.texture_offset == 0x0800);
    CHECK(door_hit.distance == 0x8000u);
    ray_model.pushwall_motion.active = 1;
    ray_model.pushwall_motion.pos = 16;
    CHECK(wl_cast_runtime_fixed_wall_ray(&ray_model, dirinfo.header.sprite_start,
                                         (4u << 16) + 0x8000u,
                                         (4u << 16) + 0x8000u,
                                         0x10000, 0, 33, 128,
                                         &door_hit) == 0);
    CHECK(door_hit.wall_page_index == 73);
    CHECK(door_hit.texture_offset == 0x0800);
    CHECK(door_hit.distance == 0x0c000u);
    ray_model.pushwall_motion.active = 0;
    ray_model.pushwall_motion.pos = 0;

    wl_map_wall_hit pwall_hit;
    CHECK(wl_build_pushwall_wall_hit(37, WL_WALL_SIDE_HORIZONTAL,
                                     (4u << 16) + 0x8000u, 16, -1, 34, 128,
                                     &pwall_hit) == 0);
    CHECK(pwall_hit.wall_page_index == 72);
    CHECK(pwall_hit.texture_offset == 0x0800);
    CHECK(wl_build_pushwall_wall_hit(37, WL_WALL_SIDE_HORIZONTAL,
                                     (4u << 16) + 0x8000u, 16, 1, 35, 128,
                                     &pwall_hit) == 0);
    CHECK(pwall_hit.wall_page_index == 72);
    CHECK(pwall_hit.texture_offset == 0x07c0);
    CHECK(wl_build_pushwall_wall_hit(37, WL_WALL_SIDE_VERTICAL,
                                     (4u << 16) + 0x8000u, 16, -1, 36, 128,
                                     &pwall_hit) == 0);
    CHECK(pwall_hit.wall_page_index == 73);
    CHECK(pwall_hit.texture_offset == 0x07c0);
    CHECK(wl_build_pushwall_wall_hit(37, WL_WALL_SIDE_VERTICAL,
                                     (4u << 16) + 0x8000u, 16, 1, 37, 128,
                                     &pwall_hit) == 0);
    CHECK(pwall_hit.wall_page_index == 73);
    CHECK(pwall_hit.texture_offset == 0x0800);
    CHECK(wl_build_pushwall_wall_hit(0, WL_WALL_SIDE_VERTICAL,
                                     (4u << 16) + 0x8000u, 16, 1, 37, 128,
                                     &pwall_hit) == -1);
    CHECK(wl_build_pushwall_wall_hit(37, WL_WALL_SIDE_VERTICAL,
                                     (4u << 16) + 0x8000u, 64, 1, 37, 128,
                                     &pwall_hit) == -1);

    const unsigned char *runtime_door_pages[106] = { 0 };
    size_t runtime_door_page_sizes[106] = { 0 };
    int32_t runtime_dirs_x[3];
    int32_t runtime_dirs_y[3];
    wl_map_wall_hit runtime_view_hits[3];
    wl_wall_strip runtime_view_strips[3];
    runtime_door_pages[3] = wall3_buf;
    runtime_door_page_sizes[3] = sizeof(wall3_buf);
    runtime_door_pages[14] = wall14_buf;
    runtime_door_page_sizes[14] = sizeof(wall14_buf);
    runtime_door_pages[15] = wall15_buf;
    runtime_door_page_sizes[15] = sizeof(wall15_buf);
    runtime_door_pages[16] = wall16_buf;
    runtime_door_page_sizes[16] = sizeof(wall16_buf);
    runtime_door_pages[17] = wall17_buf;
    runtime_door_page_sizes[17] = sizeof(wall17_buf);
    runtime_door_pages[73] = wall73_buf;
    runtime_door_page_sizes[73] = sizeof(wall73_buf);
    runtime_door_pages[99] = door99_buf;
    runtime_door_page_sizes[99] = sizeof(door99_buf);
    runtime_door_pages[105] = door105_buf;
    runtime_door_page_sizes[105] = sizeof(door105_buf);
    ray_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0x80u;
    ray_model.tilemap[5 + 4 * WL_MAP_SIDE] = 0;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_wall_view(&ray_model,
                                                  dirinfo.header.sprite_start,
                                                  (3u << 16) + 0x8000u,
                                                  (4u << 16) + 0x8000u,
                                                  0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                  runtime_door_pages,
                                                  runtime_door_page_sizes, 106,
                                                  &canvas, runtime_dirs_x, runtime_dirs_y,
                                                  runtime_view_hits,
                                                  runtime_view_strips) == 0);
    CHECK(runtime_view_hits[0].tile_x == 4);
    CHECK(runtime_view_hits[0].wall_page_index == 105);
    uint32_t closed_door_view_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    ray_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_wall_view(&ray_model,
                                                  dirinfo.header.sprite_start,
                                                  (3u << 16) + 0x8000u,
                                                  (4u << 16) + 0x8000u,
                                                  0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                  runtime_door_pages,
                                                  runtime_door_page_sizes, 106,
                                                  &canvas, runtime_dirs_x, runtime_dirs_y,
                                                  runtime_view_hits,
                                                  runtime_view_strips) == 0);
    CHECK(runtime_view_hits[0].tile_x == 6);
    CHECK(runtime_view_hits[0].wall_page_index == 3);
    uint32_t open_door_view_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(closed_door_view_hash == 0x9102177d);
    CHECK(open_door_view_hash == 0x32a9148e);

    CHECK(wl_cast_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                    WL_RAY_EAST, 32, 5, 96, &hits[0]) == 0);
    CHECK(hits[0].tile_x == 41);
    CHECK(hits[0].tile_y == 57);
    CHECK(hits[0].source_tile == 9);
    CHECK(hits[0].wall_page_index == 17);
    CHECK(hits[0].texture_offset == 0x0800);
    CHECK(wl_cast_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                    WL_RAY_WEST, 32, 15, 96, &hits[1]) == 0);
    CHECK(hits[1].tile_x == 27);
    CHECK(hits[1].tile_y == 57);
    CHECK(hits[1].wall_page_index == 17);
    CHECK(wl_cast_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                    WL_RAY_NORTH, 32, 25, 96, &hits[2]) == 0);
    CHECK(hits[2].tile_x == 29);
    CHECK(hits[2].tile_y == 55);
    CHECK(hits[2].wall_page_index == 16);
    CHECK(wl_cast_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, 29, 57,
                                    WL_RAY_SOUTH, 32, 35, 96, &hits[3]) == 0);
    CHECK(hits[3].tile_x == 29);
    CHECK(hits[3].tile_y == 59);
    CHECK(hits[3].wall_page_index == 16);
    const unsigned char *ray_pages[] = { wall17_buf, wall17_buf, wall16_buf, wall16_buf };
    wl_wall_strip ray_strips[4];
    for (size_t i = 0; i < 4; ++i) {
        CHECK(wl_wall_hit_to_strip(&hits[i], ray_pages[i], 4096, &ray_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(ray_strips, 4, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xa4c9e6e1);
    const uint32_t player_x = (29u << 16) + 0x8000u;
    const uint32_t player_y = (57u << 16) + 0x8000u;
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          player_x, player_y, WL_RAY_EAST,
                                          5, 96, &hits[0]) == 0);
    CHECK(hits[0].tile_x == 41);
    CHECK(hits[0].tile_y == 57);
    CHECK(hits[0].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          player_x, player_y, WL_RAY_WEST,
                                          15, 96, &hits[1]) == 0);
    CHECK(hits[1].tile_x == 27);
    CHECK(hits[1].tile_y == 57);
    CHECK(hits[1].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          player_x, player_y, WL_RAY_NORTH,
                                          25, 96, &hits[2]) == 0);
    CHECK(hits[2].tile_x == 29);
    CHECK(hits[2].tile_y == 55);
    CHECK(hits[2].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          player_x, player_y, WL_RAY_SOUTH,
                                          35, 96, &hits[3]) == 0);
    CHECK(hits[3].tile_x == 29);
    CHECK(hits[3].tile_y == 59);
    CHECK(hits[3].texture_offset == 0x0800);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(wl_wall_hit_to_strip(&hits[i], ray_pages[i], 4096, &ray_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(ray_strips, 4, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xa4c9e6e1);
    CHECK(wl_cast_fixed_cardinal_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                          (64u << 16), player_y, WL_RAY_EAST,
                                          0, 64, &hits[0]) == -1);

    wl_map_wall_hit dda_hits[5];
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, 0x10000, 0,
                                 5, 96, &dda_hits[0]) == 0);
    CHECK(dda_hits[0].tile_x == 41);
    CHECK(dda_hits[0].tile_y == 57);
    CHECK(dda_hits[0].wall_page_index == 17);
    CHECK(dda_hits[0].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, -0x10000, 0,
                                 15, 96, &dda_hits[1]) == 0);
    CHECK(dda_hits[1].tile_x == 27);
    CHECK(dda_hits[1].tile_y == 57);
    CHECK(dda_hits[1].wall_page_index == 17);
    CHECK(dda_hits[1].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, 0, -0x10000,
                                 25, 96, &dda_hits[2]) == 0);
    CHECK(dda_hits[2].tile_x == 29);
    CHECK(dda_hits[2].tile_y == 55);
    CHECK(dda_hits[2].wall_page_index == 16);
    CHECK(dda_hits[2].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, 0, 0x10000,
                                 35, 96, &dda_hits[3]) == 0);
    CHECK(dda_hits[3].tile_x == 29);
    CHECK(dda_hits[3].tile_y == 59);
    CHECK(dda_hits[3].wall_page_index == 16);
    CHECK(dda_hits[3].texture_offset == 0x0800);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 player_x, player_y, 0x10000, -0x8000,
                                 45, 96, &dda_hits[4]) == 0);
    CHECK(dda_hits[4].tile_x == 32);
    CHECK(dda_hits[4].tile_y == 56);
    CHECK(dda_hits[4].source_tile == 8);
    CHECK(dda_hits[4].wall_page_index == 15);
    CHECK(dda_hits[4].texture_offset == 0x0400);
    const unsigned char *dda_pages[] = { wall17_buf, wall17_buf, wall16_buf,
                                         wall16_buf, wall15_buf };
    wl_wall_strip dda_strips[5];
    for (size_t i = 0; i < 5; ++i) {
        CHECK(wl_wall_hit_to_strip(&dda_hits[i], dda_pages[i], 4096,
                                   &dda_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(dda_strips, 5, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xae40b70c);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                 player_y, 0, 0, 0, 64, &dda_hits[0]) == -1);
    CHECK(wl_cast_fixed_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                 (64u << 16), player_y, 0x10000, 0,
                                 0, 64, &dda_hits[0]) == -1);

    CHECK(wl_project_wall_height(0x5800, 80, 128) == 128);
    CHECK(wl_project_wall_height(0x28000, 80, 128) == 86);
    CHECK(wl_project_wall_height(0x0b8000, 80, 128) == 18);
    wl_map_wall_hit projected_hits[5];
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0x10000, 0,
                                     5, 80, 128, &projected_hits[0]) == 0);
    CHECK(projected_hits[0].tile_x == 41);
    CHECK(projected_hits[0].tile_y == 57);
    CHECK(projected_hits[0].distance == 0x0b8000);
    CHECK(projected_hits[0].scaled_height == 18);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, -0x10000, 0,
                                     15, 80, 128, &projected_hits[1]) == 0);
    CHECK(projected_hits[1].tile_x == 27);
    CHECK(projected_hits[1].tile_y == 57);
    CHECK(projected_hits[1].distance == 0x018000);
    CHECK(projected_hits[1].scaled_height == 128);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0, -0x10000,
                                     25, 80, 128, &projected_hits[2]) == 0);
    CHECK(projected_hits[2].tile_x == 29);
    CHECK(projected_hits[2].tile_y == 55);
    CHECK(projected_hits[2].distance == 0x018000);
    CHECK(projected_hits[2].scaled_height == 128);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0, 0x10000,
                                     35, 80, 128, &projected_hits[3]) == 0);
    CHECK(projected_hits[3].tile_x == 29);
    CHECK(projected_hits[3].tile_y == 59);
    CHECK(projected_hits[3].distance == 0x018000);
    CHECK(projected_hits[3].scaled_height == 128);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0x10000, -0x8000,
                                     45, 80, 128, &projected_hits[4]) == 0);
    CHECK(projected_hits[4].tile_x == 32);
    CHECK(projected_hits[4].tile_y == 56);
    CHECK(projected_hits[4].distance == 0x028000);
    CHECK(projected_hits[4].scaled_height == 86);
    const unsigned char *projected_pages[] = { wall17_buf, wall17_buf, wall16_buf,
                                               wall16_buf, wall15_buf };
    wl_wall_strip projected_strips[5];
    for (size_t i = 0; i < 5; ++i) {
        CHECK(wl_wall_hit_to_strip(&projected_hits[i], projected_pages[i], 4096,
                                   &projected_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(projected_strips, 5, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xd48f2f6d);
    CHECK(wl_cast_projected_wall_ray(wall_plane, WL_MAP_PLANE_WORDS,
                                     player_x, player_y, 0x10000, 0,
                                     80, 80, 128, &projected_hits[0]) == -1);

    const int32_t batch_dirs_x[] = { 0x10000, -0x10000, 0, 0, 0x10000 };
    const int32_t batch_dirs_y[] = { 0, 0, -0x10000, 0x10000, -0x8000 };
    wl_map_wall_hit batch_hits[5];
    CHECK(wl_cast_projected_wall_ray_batch(wall_plane, WL_MAP_PLANE_WORDS,
                                           player_x, player_y, batch_dirs_x,
                                           batch_dirs_y, 5, 20, 1, 80, 128,
                                           batch_hits) == 0);
    CHECK(batch_hits[0].x == 20);
    CHECK(batch_hits[0].tile_x == 41);
    CHECK(batch_hits[0].scaled_height == 18);
    CHECK(batch_hits[1].x == 21);
    CHECK(batch_hits[1].tile_x == 27);
    CHECK(batch_hits[1].scaled_height == 128);
    CHECK(batch_hits[2].x == 22);
    CHECK(batch_hits[2].tile_y == 55);
    CHECK(batch_hits[2].scaled_height == 128);
    CHECK(batch_hits[3].x == 23);
    CHECK(batch_hits[3].tile_y == 59);
    CHECK(batch_hits[3].scaled_height == 128);
    CHECK(batch_hits[4].x == 24);
    CHECK(batch_hits[4].tile_x == 32);
    CHECK(batch_hits[4].tile_y == 56);
    CHECK(batch_hits[4].scaled_height == 86);
    wl_wall_strip batch_strips[5];
    for (size_t i = 0; i < 5; ++i) {
        CHECK(wl_wall_hit_to_strip(&batch_hits[i], projected_pages[i], 4096,
                                   &batch_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(batch_strips, 5, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x7209a9ed);
    CHECK(wl_cast_projected_wall_ray_batch(wall_plane, WL_MAP_PLANE_WORDS,
                                           player_x, player_y, batch_dirs_x,
                                           batch_dirs_y, 5, 78, 1, 80, 128,
                                           batch_hits) == -1);
    CHECK(wl_cast_projected_wall_ray_batch(wall_plane, WL_MAP_PLANE_WORDS,
                                           player_x, player_y, batch_dirs_x,
                                           batch_dirs_y, 0, 20, 1, 80, 128,
                                           batch_hits) == -1);

    int32_t camera_dirs_x[5];
    int32_t camera_dirs_y[5];
    CHECK(wl_build_camera_ray_directions(0x10000, 0, 0, -0x8000, 5, 0, 1, 5,
                                         camera_dirs_x, camera_dirs_y) == 0);
    CHECK(camera_dirs_x[0] == 0x10000);
    CHECK(camera_dirs_y[0] == 26214);
    CHECK(camera_dirs_x[2] == 0x10000);
    CHECK(camera_dirs_y[2] == 0);
    CHECK(camera_dirs_x[4] == 0x10000);
    CHECK(camera_dirs_y[4] == -26214);
    wl_map_wall_hit camera_hits[5];
    CHECK(wl_cast_projected_wall_ray_batch(wall_plane, WL_MAP_PLANE_WORDS,
                                           player_x, player_y, camera_dirs_x,
                                           camera_dirs_y, 5, 30, 1, 80, 128,
                                           camera_hits) == 0);
    CHECK(camera_hits[0].tile_x == 32);
    CHECK(camera_hits[0].tile_y == 58);
    CHECK(camera_hits[0].wall_page_index == 15);
    CHECK(camera_hits[0].texture_offset == 0x07c0);
    CHECK(camera_hits[0].scaled_height == 86);
    CHECK(camera_hits[1].tile_x == 32);
    CHECK(camera_hits[1].tile_y == 58);
    CHECK(camera_hits[1].wall_page_index == 14);
    CHECK(camera_hits[1].texture_offset == 0x0000);
    CHECK(camera_hits[2].tile_x == 41);
    CHECK(camera_hits[2].tile_y == 57);
    CHECK(camera_hits[2].wall_page_index == 17);
    CHECK(camera_hits[2].scaled_height == 18);
    CHECK(camera_hits[3].tile_x == 32);
    CHECK(camera_hits[3].tile_y == 56);
    CHECK(camera_hits[3].wall_page_index == 14);
    CHECK(camera_hits[4].tile_x == 32);
    CHECK(camera_hits[4].tile_y == 56);
    CHECK(camera_hits[4].wall_page_index == 15);
    CHECK(camera_hits[4].texture_offset == 0x0800);
    const unsigned char *camera_pages[] = { wall15_buf, wall14_buf, wall17_buf,
                                            wall14_buf, wall15_buf };
    wl_wall_strip camera_strips[5];
    for (size_t i = 0; i < 5; ++i) {
        CHECK(wl_wall_hit_to_strip(&camera_hits[i], camera_pages[i], 4096,
                                   &camera_strips[i]) == 0);
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_wall_strip_viewport(camera_strips, 5, &canvas) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x7320f695);
    CHECK(wl_build_camera_ray_directions(0, 0, 0, -0x8000, 5, 0, 1, 5,
                                         camera_dirs_x, camera_dirs_y) == -1);
    CHECK(wl_build_camera_ray_directions(0x10000, 0, 0, -0x8000, 5, 4, 1, 2,
                                         camera_dirs_x, camera_dirs_y) == -1);

    const unsigned char *view_pages[18] = { 0 };
    size_t view_page_sizes[18] = { 0 };
    view_pages[14] = wall14_buf;
    view_page_sizes[14] = sizeof(wall14_buf);
    view_pages[15] = wall15_buf;
    view_page_sizes[15] = sizeof(wall15_buf);
    view_pages[16] = wall16_buf;
    view_page_sizes[16] = sizeof(wall16_buf);
    view_pages[17] = wall17_buf;
    view_page_sizes[17] = sizeof(wall17_buf);
    wl_map_wall_hit view_hits[5];
    wl_wall_strip view_strips[5];
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_camera_wall_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                     player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                     view_pages, view_page_sizes, 18, &canvas,
                                     camera_dirs_x, camera_dirs_y, view_hits,
                                     view_strips) == 0);
    CHECK(view_hits[0].tile_x == 36);
    CHECK(view_hits[0].tile_y == 58);
    CHECK(view_hits[0].wall_page_index == 15);
    CHECK(view_hits[0].scaled_height == 33);
    CHECK(view_hits[2].tile_x == 36);
    CHECK(view_hits[2].tile_y == 58);
    CHECK(view_hits[2].wall_page_index == 15);
    CHECK(view_hits[2].texture_offset == 0x01c0);
    CHECK(view_hits[4].tile_x == 36);
    CHECK(view_hits[4].tile_y == 58);
    CHECK(view_hits[4].wall_page_index == 14);
    CHECK(view_hits[4].scaled_height == 29);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xfad71929);

    wl_game_model render_model;
    memset(&render_model, 0, sizeof(render_model));
    render_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0x80u;
    render_model.tilemap[6 + 4 * WL_MAP_SIDE] = 2;
    const unsigned char *runtime_pages[4] = { 0 };
    size_t runtime_page_sizes[4] = { 0 };
    runtime_pages[1] = wall1_buf;
    runtime_page_sizes[1] = sizeof(wall1_buf);
    runtime_pages[3] = wall3_buf;
    runtime_page_sizes[3] = sizeof(wall3_buf);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_camera_wall_view(&render_model, 1,
                                             (3u << 16) + 0x8000u,
                                             (4u << 16) + 0x8000u,
                                             0x10000, 0, 0, -0x8000, 39, 1, 3,
                                             runtime_pages, runtime_page_sizes, 4,
                                             &canvas, camera_dirs_x, camera_dirs_y,
                                             view_hits, view_strips) == 0);
    CHECK(view_hits[0].tile_x == 4);
    CHECK(view_hits[0].source_tile == 1);
    CHECK(view_hits[1].wall_page_index == 1);
    uint32_t closed_runtime_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    render_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_camera_wall_view(&render_model, 1,
                                             (3u << 16) + 0x8000u,
                                             (4u << 16) + 0x8000u,
                                             0x10000, 0, 0, -0x8000, 39, 1, 3,
                                             runtime_pages, runtime_page_sizes, 4,
                                             &canvas, camera_dirs_x, camera_dirs_y,
                                             view_hits, view_strips) == 0);
    CHECK(view_hits[0].tile_x == 6);
    CHECK(view_hits[0].source_tile == 2);
    CHECK(view_hits[1].wall_page_index == 3);
    uint32_t open_runtime_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(closed_runtime_hash == 0x62d02b0d);
    CHECK(open_runtime_hash == 0x32a9148e);

    view_pages[15] = NULL;
    CHECK(wl_render_camera_wall_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                     player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                     view_pages, view_page_sizes, 18, &canvas,
                                     camera_dirs_x, camera_dirs_y, view_hits,
                                     view_strips) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 105, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x33b7f33f);
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 0);
    CHECK(wallmeta.max_color == 31);
    CHECK(wallmeta.unique_color_count == 11);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x66874cf5);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 106, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 1306);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xbf4fcd99);
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[106].kind,
                                         &shape) == 0);
    CHECK(shape.kind == WL_VSWAP_CHUNK_SPRITE);
    CHECK(shape.width == 64);
    CHECK(shape.height == 64);
    CHECK(shape.leftpix == 4);
    CHECK(shape.rightpix == 58);
    CHECK(shape.visible_columns == 55);
    CHECK(shape.first_column_offset == 800);
    CHECK(shape.last_column_offset == 1298);
    CHECK(shape.min_column_offset == 800);
    CHECK(shape.max_column_offset == 1298);
    CHECK(shape.post_count == 66);
    CHECK(shape.terminal_count == 55);
    CHECK(shape.min_posts_per_column == 1);
    CHECK(shape.max_posts_per_column == 2);
    CHECK(shape.min_post_span == 2);
    CHECK(shape.max_post_span == 40);
    CHECK(shape.max_post_start == 36);
    CHECK(shape.max_post_end == 46);
    CHECK(shape.min_source_offset == 108);
    CHECK(shape.max_source_offset == 782);
    CHECK(shape.total_post_span == 1372);
    CHECK(wl_decode_sprite_shape_surface(chunk_buf, chunk_bytes, 0, indexed_buf,
                                         sizeof(indexed_buf), &surface) == 0);
    CHECK(surface.format == WL_SURFACE_INDEXED8);
    CHECK(surface.width == 64);
    CHECK(surface.height == 64);
    CHECK(surface.stride == 64);
    CHECK(surface.pixel_count == 4096);
    CHECK(surface.pixels == indexed_buf);
    CHECK(count_byte_not_value(surface.pixels, surface.pixel_count, 0) == 614);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x918ed728);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_scaled_sprite(&surface, &canvas, 40, 64, 0, NULL, 0) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x3f753ac8);
    uint16_t wall_heights[80];
    for (size_t i = 0; i < 80; ++i) {
        wall_heights[i] = 0;
    }
    for (size_t i = 36; i < 44; ++i) {
        wall_heights[i] = 80;
    }
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_render_scaled_sprite(&surface, &canvas, 40, 64, 0,
                                  wall_heights, 80) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xaa7c2838);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_render_scaled_sprite(&surface, &canvas, -4, 96, 0, NULL, 0) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x6ff0f5c8);
    CHECK(wl_render_scaled_sprite(&surface, &canvas, 40, 0, 0, NULL, 0) == -1);
    CHECK(wl_render_scaled_sprite(&surface, &canvas, 40, 64, 0, wall_heights, 79) == -1);
    wl_projected_sprite sprites[5];
    CHECK(wl_project_world_sprite(106, player_x, player_y,
                                  (34u << 16) + 0x8000u,
                                  (57u << 16) + 0x8000u,
                                  0x10000, 0, 80, 128, &sprites[0]) == 0);
    CHECK(sprites[0].visible == 1);
    CHECK(sprites[0].view_x == 39);
    CHECK(sprites[0].scaled_height == 42);
    CHECK(sprites[0].distance == 0x51700);
    CHECK(sprites[0].trans_y == 0);
    CHECK(wl_project_world_sprite(107, player_x, player_y,
                                  (36u << 16) + 0x8000u,
                                  (58u << 16) + 0x8000u,
                                  0x10000, 0, 80, 128, &sprites[1]) == 0);
    CHECK(sprites[1].visible == 1);
    CHECK(sprites[1].view_x == 46);
    CHECK(sprites[1].scaled_height == 30);
    CHECK(sprites[1].distance == 0x71700);
    CHECK(sprites[1].trans_y == 0x10000);
    CHECK(wl_project_world_sprite(108, player_x, player_y,
                                  (28u << 16) + 0x8000u,
                                  (57u << 16) + 0x8000u,
                                  0x10000, 0, 80, 128, &sprites[2]) == 0);
    CHECK(sprites[2].visible == 0);
    CHECK(wl_sort_projected_sprites_far_to_near(sprites, 3) == 0);
    CHECK(sprites[0].source_index == 107);
    CHECK(sprites[1].source_index == 106);
    CHECK(sprites[2].source_index == 108);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    for (size_t i = 0; i < 80; ++i) {
        wall_heights[i] = 0;
    }
    for (size_t i = 30; i < 36; ++i) {
        wall_heights[i] = 45;
    }
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    for (size_t i = 0; i < 3; ++i) {
        if (sprites[i].visible) {
            CHECK(wl_render_scaled_sprite(&surface, &canvas, sprites[i].view_x,
                                          sprites[i].scaled_height, 0,
                                          wall_heights, 80) == 0);
        }
    }
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x819b1035);
    view_pages[15] = wall15_buf;
    const wl_indexed_surface *scene_sprites[] = { &surface, &surface };
    const uint32_t scene_sprite_x[] = {
        (34u << 16) + 0x8000u,
        (36u << 16) + 0x8000u,
    };
    const uint32_t scene_sprite_y[] = {
        (57u << 16) + 0x8000u,
        (58u << 16) + 0x8000u,
    };
    const uint16_t scene_sprite_ids[] = { 106, 107 };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_camera_scene_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                      player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                      view_pages, view_page_sizes, 18,
                                      scene_sprites, scene_sprite_x, scene_sprite_y,
                                      scene_sprite_ids, 2, 0, &canvas,
                                      camera_dirs_x, camera_dirs_y, view_hits,
                                      view_strips, sprites, wall_heights) == 0);
    CHECK(sprites[0].source_index == 107);
    CHECK(sprites[0].surface_index == 1);
    CHECK(sprites[0].view_x == 46);
    CHECK(sprites[0].scaled_height == 30);
    CHECK(sprites[1].source_index == 106);
    CHECK(sprites[1].surface_index == 0);
    CHECK(sprites[1].view_x == 39);
    CHECK(sprites[1].scaled_height == 42);
    CHECK(wall_heights[30] == 33);
    CHECK(wall_heights[32] == 33);
    CHECK(wall_heights[34] == 29);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x1e4a8264);

    const wl_indexed_surface *runtime_scene_sprites[] = { &surface };
    const uint32_t runtime_scene_x[] = { (5u << 16) + 0x8000u };
    const uint32_t runtime_scene_y[] = { (4u << 16) + 0x8000u };
    const uint16_t runtime_scene_ids[] = { 106 };
    ray_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0x80u;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   runtime_scene_sprites,
                                                   runtime_scene_x, runtime_scene_y,
                                                   runtime_scene_ids, 1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 106);
    CHECK(sprites[0].surface_index == 0);
    uint32_t closed_door_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    ray_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   runtime_scene_sprites,
                                                   runtime_scene_x, runtime_scene_y,
                                                   runtime_scene_ids, 1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    uint32_t open_door_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(closed_door_scene_hash == 0x01053e89);
    CHECK(open_door_scene_hash == 0xa06c2183);

    memset(use_wall, 0, sizeof(use_wall));
    memset(use_info, 0, sizeof(use_info));
    CHECK(wl_init_player_gameplay_state(&pickup_state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_give_player_key(&pickup_state, 0) == 0);
    ray_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0x80u;
    ray_model.doors[0].action = WL_DOOR_CLOSED;
    ray_model.doors[0].position = 0;
    live_tick_motion.x = (3u << 16) + 0x8000u;
    live_tick_motion.y = (4u << 16) + 0x8000u;
    live_tick_motion.tile_x = 3;
    live_tick_motion.tile_y = 4;
    CHECK(wl_step_live_tick(&pickup_state, &ray_model,
                            use_wall, use_info, WL_MAP_PLANE_WORDS,
                            &live_tick_motion, 0, 0, 0x10000, 0,
                            WL_DIR_EAST, 1, 0, 100, &live_tick) == 0);
    CHECK(live_tick.used == 1);
    CHECK(live_tick.use.kind == WL_USE_DOOR);
    CHECK(live_tick.use.opened == 1);
    CHECK(live_tick.doors.open_count == 1);
    CHECK(live_tick.doors.released_collision_count == 1);
    CHECK(ray_model.doors[0].action == WL_DOOR_OPEN);
    CHECK(ray_model.tilemap[4 + 4 * WL_MAP_SIDE] == 0);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   runtime_scene_sprites,
                                                   runtime_scene_x, runtime_scene_y,
                                                   runtime_scene_ids, 1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == open_door_scene_hash);

    wl_game_model patrol_render_model;
    memset(&patrol_render_model, 0, sizeof(patrol_render_model));
    for (size_t i = 0; i < WL_MAP_SIDE; ++i) {
        patrol_render_model.tilemap[i] = 15;
        patrol_render_model.tilemap[i + (WL_MAP_SIDE - 1u) * WL_MAP_SIDE] = 15;
        patrol_render_model.tilemap[i * WL_MAP_SIDE] = 15;
        patrol_render_model.tilemap[(WL_MAP_SIDE - 1u) + i * WL_MAP_SIDE] = 15;
    }
    const wl_indexed_surface *patrol_scene_surfaces[] = { &surface };
    const uint32_t patrol_scene_x[] = { (5u << 16) + 0x8000u };
    const uint32_t patrol_scene_y[] = { (4u << 16) + 0x8000u };
    const uint16_t patrol_scene_ids[] = { patrol_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   patrol_scene_surfaces,
                                                   patrol_scene_x, patrol_scene_y,
                                                   patrol_scene_ids, 1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 58);
    CHECK(sprites[0].visible == 1);
    uint32_t patrol_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(patrol_scene_hash == open_door_scene_hash);

    const uint32_t pushwall_scene_x[] = { (6u << 16) + 0x8000u };
    const uint32_t pushwall_scene_y[] = { (4u << 16) + 0x8000u };
    ray_model.tilemap[4 + 4 * WL_MAP_SIDE] = 0;
    ray_model.tilemap[5 + 4 * WL_MAP_SIDE] = (uint16_t)(37u | 0xc0u);
    ray_model.tilemap[6 + 4 * WL_MAP_SIDE] = 0;
    ray_model.tilemap[7 + 4 * WL_MAP_SIDE] = 2;
    ray_model.pushwall_motion.active = 1;
    ray_model.pushwall_motion.pos = 0;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   runtime_scene_sprites,
                                                   pushwall_scene_x, pushwall_scene_y,
                                                   runtime_scene_ids, 1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(runtime_view_hits[0].tile_x == 5);
    CHECK(runtime_view_hits[0].wall_page_index == 73);
    uint32_t pushwall_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    ray_model.pushwall_motion.pos = 16;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   runtime_scene_sprites,
                                                   pushwall_scene_x, pushwall_scene_y,
                                                   runtime_scene_ids, 1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(runtime_view_hits[0].tile_x == 5);
    CHECK(runtime_view_hits[0].distance == 0x1c000u);
    CHECK(runtime_view_hits[0].scaled_height == 123);
    uint32_t shifted_pushwall_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    ray_model.tilemap[5 + 4 * WL_MAP_SIDE] = 0;
    ray_model.pushwall_motion.active = 0;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   runtime_scene_sprites,
                                                   pushwall_scene_x, pushwall_scene_y,
                                                   runtime_scene_ids, 1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(runtime_view_hits[0].tile_x == 7);
    uint32_t open_pushwall_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(pushwall_scene_hash == 0x81e9da6b);
    CHECK(shifted_pushwall_scene_hash == 0x83a0d93b);
    CHECK(open_pushwall_scene_hash == 0xf80cfa3f);

    wl_game_model tick_push_model;
    memset(&tick_push_model, 0, sizeof(tick_push_model));
    memset(use_info, 0, sizeof(use_info));
    tick_push_model.tilemap[4 + 4 * WL_MAP_SIDE] = 37;
    tick_push_model.pushwall_count = 1;
    tick_push_model.pushwalls[0].x = 4;
    tick_push_model.pushwalls[0].y = 4;
    use_info[4 + 4 * WL_MAP_SIDE] = WL_PUSHABLETILE;
    live_tick_motion.x = (3u << 16) + 0x8000u;
    live_tick_motion.y = (4u << 16) + 0x8000u;
    live_tick_motion.tile_x = 3;
    live_tick_motion.tile_y = 4;
    CHECK(wl_init_player_gameplay_state(&pickup_state, 100, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_tick(&pickup_state, &tick_push_model,
                            use_wall, use_info, WL_MAP_PLANE_WORDS,
                            &live_tick_motion, 0, 0, 0x10000, 0,
                            WL_DIR_EAST, 1, 0, 127, &live_tick) == 0);
    CHECK(live_tick.used == 1);
    CHECK(live_tick.use.kind == WL_USE_PUSHWALL);
    CHECK(live_tick.pushwall.crossed_tile == 1);
    CHECK(tick_push_model.tilemap[5 + 4 * WL_MAP_SIDE] == (37 | 0xc0));
    CHECK(tick_push_model.tilemap[6 + 4 * WL_MAP_SIDE] == 37);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&tick_push_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   runtime_scene_sprites,
                                                   pushwall_scene_x, pushwall_scene_y,
                                                   runtime_scene_ids, 1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(runtime_view_hits[0].tile_x == 5);
    CHECK(runtime_view_hits[0].wall_page_index == 73);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == pushwall_scene_hash);

    wl_game_model tick_static_model;
    memset(&tick_static_model, 0, sizeof(tick_static_model));
    tick_static_model.tilemap[7 + 4 * WL_MAP_SIDE] = 2;
    tick_static_model.static_count = 1;
    tick_static_model.statics[0].x = 5;
    tick_static_model.statics[0].y = 4;
    tick_static_model.statics[0].type = 24;
    tick_static_model.statics[0].bonus = 1;
    tick_static_model.statics[0].active = 1;
    CHECK(wl_collect_scene_sprite_refs(&tick_static_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 26);
    CHECK(scene_refs[0].vswap_chunk_index == 132);
    unsigned char tick_static_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface tick_static_surface;
    const uint16_t tick_static_chunks[] = { scene_refs[0].vswap_chunk_index };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               tick_static_chunks, 1, 0,
                                               tick_static_pixels,
                                               sizeof(tick_static_pixels),
                                               &tick_static_surface) == 0);
    const wl_indexed_surface *tick_static_surfaces[] = { &tick_static_surface };
    const uint32_t tick_static_scene_x[] = { scene_refs[0].world_x };
    const uint32_t tick_static_scene_y[] = { scene_refs[0].world_y };
    const uint16_t tick_static_scene_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&tick_static_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   tick_static_surfaces,
                                                   tick_static_scene_x,
                                                   tick_static_scene_y,
                                                   tick_static_scene_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    uint32_t active_static_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    memset(use_wall, 0, sizeof(use_wall));
    memset(use_info, 0, sizeof(use_info));
    live_tick_motion.x = 0x4f000u;
    live_tick_motion.y = 0x48000u;
    live_tick_motion.tile_x = 4;
    live_tick_motion.tile_y = 4;
    CHECK(wl_init_player_gameplay_state(&pickup_state, 90, 3, 0, WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_tick(&pickup_state, &tick_static_model,
                            use_wall, use_info, WL_MAP_PLANE_WORDS,
                            &live_tick_motion, 0x1000, 0, 0x10000, 0,
                            WL_DIR_EAST, 0, 0, 1, &live_tick) == 0);
    CHECK(live_tick.motion.picked_up == 1);
    CHECK(live_tick.motion.static_index == 0);
    CHECK(live_tick.palette.kind == WL_PALETTE_SHIFT_WHITE);
    CHECK(tick_static_model.statics[0].active == 0);
    CHECK(wl_collect_scene_sprite_refs(&tick_static_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 0);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&tick_static_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   NULL, NULL, NULL, NULL, 0, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    uint32_t picked_static_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(active_static_scene_hash == 0x7e68266c);
    CHECK(picked_static_scene_hash == 0xc928b202);

    wl_game_model live_ai_render_model;
    memset(&live_ai_render_model, 0, sizeof(live_ai_render_model));
    live_ai_render_model.actor_count = 1;
    live_ai_render_model.actors[0].kind = WL_ACTOR_GUARD;
    live_ai_render_model.actors[0].mode = WL_ACTOR_PATROL;
    live_ai_render_model.actors[0].dir = WL_DIR_EAST;
    live_ai_render_model.actors[0].tile_x = 5;
    live_ai_render_model.actors[0].tile_y = 5;
    for (size_t wall_y = 0; wall_y < WL_MAP_SIDE; ++wall_y) {
        live_ai_render_model.tilemap[10 + wall_y * WL_MAP_SIDE] = 15;
    }
    wl_player_gameplay_state live_ai_render_player;
    wl_player_motion_state live_ai_render_motion = { 0x38000u, 0x48000u, 3, 4 };
    wl_live_actor_ai_tick_result live_ai_render_tick;
    memset(use_wall, 0, sizeof(use_wall));
    memset(use_info, 0, sizeof(use_info));
    CHECK(wl_init_player_gameplay_state(&live_ai_render_player, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(live_ai_render_tick.patrols_stepped == 0);
    CHECK(live_ai_render_tick.patrols.tiles_stepped == 0);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 58);
    CHECK(scene_refs[0].vswap_chunk_index == 164);
    CHECK(scene_refs[0].world_x == 0x60000u);
    CHECK(scene_refs[0].world_y == 0x58000u);
    unsigned char live_ai_guard_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface live_ai_guard_surface;
    const uint16_t live_ai_guard_chunks[] = { scene_refs[0].vswap_chunk_index };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               live_ai_guard_chunks, 1, 0,
                                               live_ai_guard_pixels,
                                               sizeof(live_ai_guard_pixels),
                                               &live_ai_guard_surface) == 0);
    const wl_indexed_surface *live_ai_guard_surfaces[] = { &live_ai_guard_surface };
    const uint32_t live_ai_guard_x[] = { scene_refs[0].world_x };
    const uint32_t live_ai_guard_y[] = { scene_refs[0].world_y };
    const uint16_t live_ai_guard_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_ai_guard_surfaces,
                                                   live_ai_guard_x,
                                                   live_ai_guard_y,
                                                   live_ai_guard_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 58);
    CHECK(sprites[0].surface_index == 0);
    CHECK(sprites[0].visible == 1);
    uint32_t live_ai_guard_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(live_ai_guard_scene_hash == 0xcf61b07b);
    CHECK(live_ai_render_model.actors[0].patrol_remainder == 0x8000u);

    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(live_ai_render_tick.patrols_stepped == 1);
    CHECK(live_ai_render_tick.patrols.tiles_stepped == 1);
    CHECK(live_ai_render_model.actors[0].tile_x == 6);
    CHECK(live_ai_render_model.actors[0].patrol_remainder == 0);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].world_x == 0x68000u);
    CHECK(scene_refs[0].world_y == 0x58000u);
    const uint32_t live_ai_guard_step_x[] = { scene_refs[0].world_x };
    const uint32_t live_ai_guard_step_y[] = { scene_refs[0].world_y };
    const uint16_t live_ai_guard_step_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_ai_guard_surfaces,
                                                   live_ai_guard_step_x,
                                                   live_ai_guard_step_y,
                                                   live_ai_guard_step_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 58);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x6ee1f8bf);

    memset(&live_ai_render_model, 0, sizeof(live_ai_render_model));
    live_ai_render_model.actor_count = 1;
    live_ai_render_model.actors[0].kind = WL_ACTOR_DOG;
    live_ai_render_model.actors[0].mode = WL_ACTOR_PATROL;
    live_ai_render_model.actors[0].dir = WL_DIR_EAST;
    live_ai_render_model.actors[0].tile_x = 5;
    live_ai_render_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 99);
    CHECK(scene_refs[0].vswap_chunk_index == 205);
    CHECK(scene_refs[0].world_x == 0x60000u);
    unsigned char live_ai_dog_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface live_ai_dog_surface;
    const uint16_t live_ai_dog_chunks[] = { scene_refs[0].vswap_chunk_index };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               live_ai_dog_chunks, 1, 0,
                                               live_ai_dog_pixels,
                                               sizeof(live_ai_dog_pixels),
                                               &live_ai_dog_surface) == 0);
    const wl_indexed_surface *live_ai_dog_surfaces[] = { &live_ai_dog_surface };
    const uint32_t live_ai_dog_x[] = { scene_refs[0].world_x };
    const uint32_t live_ai_dog_y[] = { scene_refs[0].world_y };
    const uint16_t live_ai_dog_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_ai_dog_surfaces,
                                                   live_ai_dog_x,
                                                   live_ai_dog_y,
                                                   live_ai_dog_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 99);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x08ab64f0);

    memset(&live_ai_render_model, 0, sizeof(live_ai_render_model));
    live_ai_render_model.actor_count = 1;
    live_ai_render_model.actors[0].kind = WL_ACTOR_OFFICER;
    live_ai_render_model.actors[0].mode = WL_ACTOR_PATROL;
    live_ai_render_model.actors[0].dir = WL_DIR_EAST;
    live_ai_render_model.actors[0].tile_x = 5;
    live_ai_render_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 246);
    CHECK(scene_refs[0].vswap_chunk_index == 352);
    CHECK(scene_refs[0].world_x == 0x60000u);
    unsigned char live_ai_officer_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface live_ai_officer_surface;
    const uint16_t live_ai_officer_chunks[] = { scene_refs[0].vswap_chunk_index };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               live_ai_officer_chunks, 1, 0,
                                               live_ai_officer_pixels,
                                               sizeof(live_ai_officer_pixels),
                                               &live_ai_officer_surface) == 0);
    const wl_indexed_surface *live_ai_officer_surfaces[] = { &live_ai_officer_surface };
    const uint32_t live_ai_officer_x[] = { scene_refs[0].world_x };
    const uint32_t live_ai_officer_y[] = { scene_refs[0].world_y };
    const uint16_t live_ai_officer_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_ai_officer_surfaces,
                                                   live_ai_officer_x,
                                                   live_ai_officer_y,
                                                   live_ai_officer_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 246);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xa6544334);

    memset(&live_ai_render_model, 0, sizeof(live_ai_render_model));
    live_ai_render_model.actor_count = 1;
    live_ai_render_model.actors[0].kind = WL_ACTOR_SS;
    live_ai_render_model.actors[0].mode = WL_ACTOR_PATROL;
    live_ai_render_model.actors[0].dir = WL_DIR_EAST;
    live_ai_render_model.actors[0].tile_x = 5;
    live_ai_render_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 146);
    CHECK(scene_refs[0].vswap_chunk_index == 252);
    CHECK(scene_refs[0].world_x == 0x60000u);
    unsigned char live_ai_ss_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface live_ai_ss_surface;
    const uint16_t live_ai_ss_chunks[] = { scene_refs[0].vswap_chunk_index };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               live_ai_ss_chunks, 1, 0,
                                               live_ai_ss_pixels,
                                               sizeof(live_ai_ss_pixels),
                                               &live_ai_ss_surface) == 0);
    const wl_indexed_surface *live_ai_ss_surfaces[] = { &live_ai_ss_surface };
    const uint32_t live_ai_ss_x[] = { scene_refs[0].world_x };
    const uint32_t live_ai_ss_y[] = { scene_refs[0].world_y };
    const uint16_t live_ai_ss_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_ai_ss_surfaces,
                                                   live_ai_ss_x,
                                                   live_ai_ss_y,
                                                   live_ai_ss_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 146);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x0b6fe30b);

    memset(&live_ai_render_model, 0, sizeof(live_ai_render_model));
    live_ai_render_model.actor_count = 1;
    live_ai_render_model.actors[0].kind = WL_ACTOR_MUTANT;
    live_ai_render_model.actors[0].mode = WL_ACTOR_PATROL;
    live_ai_render_model.actors[0].dir = WL_DIR_EAST;
    live_ai_render_model.actors[0].tile_x = 5;
    live_ai_render_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 195);
    CHECK(scene_refs[0].vswap_chunk_index == 301);
    CHECK(scene_refs[0].world_x == 0x60000u);
    unsigned char live_ai_mutant_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface live_ai_mutant_surface;
    const uint16_t live_ai_mutant_chunks[] = { scene_refs[0].vswap_chunk_index };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               live_ai_mutant_chunks, 1, 0,
                                               live_ai_mutant_pixels,
                                               sizeof(live_ai_mutant_pixels),
                                               &live_ai_mutant_surface) == 0);
    const wl_indexed_surface *live_ai_mutant_surfaces[] = { &live_ai_mutant_surface };
    const uint32_t live_ai_mutant_x[] = { scene_refs[0].world_x };
    const uint32_t live_ai_mutant_y[] = { scene_refs[0].world_y };
    const uint16_t live_ai_mutant_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_ai_mutant_surfaces,
                                                   live_ai_mutant_x,
                                                   live_ai_mutant_y,
                                                   live_ai_mutant_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 195);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x96655cea);

    memset(&live_ai_render_model, 0, sizeof(live_ai_render_model));
    live_ai_render_model.actor_count = 1;
    live_ai_render_model.actors[0].kind = WL_ACTOR_BOSS;
    live_ai_render_model.actors[0].source_tile = 214;
    live_ai_render_model.actors[0].mode = WL_ACTOR_PATROL;
    live_ai_render_model.actors[0].dir = WL_DIR_EAST;
    live_ai_render_model.actors[0].tile_x = 5;
    live_ai_render_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 296);
    CHECK(scene_refs[0].vswap_chunk_index == 402);
    CHECK(scene_refs[0].world_x == 0x60000u);
    unsigned char live_ai_boss_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface live_ai_boss_surface;
    const uint16_t live_ai_boss_chunks[] = { scene_refs[0].vswap_chunk_index };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               live_ai_boss_chunks, 1, 0,
                                               live_ai_boss_pixels,
                                               sizeof(live_ai_boss_pixels),
                                               &live_ai_boss_surface) == 0);
    const wl_indexed_surface *live_ai_boss_surfaces[] = { &live_ai_boss_surface };
    const uint32_t live_ai_boss_x[] = { scene_refs[0].world_x };
    const uint32_t live_ai_boss_y[] = { scene_refs[0].world_y };
    const uint16_t live_ai_boss_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_ai_boss_surfaces,
                                                   live_ai_boss_x,
                                                   live_ai_boss_y,
                                                   live_ai_boss_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 296);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x731d6cb3);

    memset(&live_ai_render_model, 0, sizeof(live_ai_render_model));
    live_ai_render_model.actor_count = 1;
    live_ai_render_model.actors[0].kind = WL_ACTOR_GUARD;
    live_ai_render_model.actors[0].shootable = 1;
    live_ai_render_model.actors[0].mode = WL_ACTOR_CHASE;
    live_ai_render_model.actors[0].dir = WL_DIR_EAST;
    live_ai_render_model.actors[0].tile_x = 5;
    live_ai_render_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(live_ai_render_tick.patrols.actors_considered == 0);
    CHECK(live_ai_render_tick.chases.actors_considered == 1);
    CHECK(live_ai_render_tick.chases.actors_stepped == 1);
    CHECK(live_ai_render_tick.chases.tiles_stepped == 0);
    CHECK(live_ai_render_model.actors[0].patrol_remainder == 0x8000u);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 58);
    CHECK(scene_refs[0].vswap_chunk_index == 164);
    CHECK(scene_refs[0].world_x == 0x58000u);
    CHECK(scene_refs[0].world_y == 0x50000u);
    unsigned char chase_render_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface chase_render_surface;
    const uint16_t chase_render_chunks[] = { scene_refs[0].vswap_chunk_index };
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               chase_render_chunks, 1, 0,
                                               chase_render_pixels,
                                               sizeof(chase_render_pixels),
                                               &chase_render_surface) == 0);
    const wl_indexed_surface *chase_render_surfaces[] = { &chase_render_surface };
    const uint32_t chase_render_x[] = { scene_refs[0].world_x };
    const uint32_t chase_render_y[] = { scene_refs[0].world_y };
    const uint16_t chase_render_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   chase_render_surfaces,
                                                   chase_render_x,
                                                   chase_render_y,
                                                   chase_render_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 58);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xa71311c2);

    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &live_ai_render_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(live_ai_render_tick.chases.tiles_stepped == 1);
    CHECK(live_ai_render_model.actors[0].patrol_remainder == 0);
    CHECK(wl_collect_scene_sprite_refs(&live_ai_render_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 58);
    CHECK(scene_refs[0].vswap_chunk_index == 164);
    CHECK(scene_refs[0].world_x == 0x58000u);
    CHECK(scene_refs[0].world_y == 0x48000u);
    const uint32_t chase_render_second_x[] = { scene_refs[0].world_x };
    const uint32_t chase_render_second_y[] = { scene_refs[0].world_y };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&ray_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   chase_render_surfaces,
                                                   chase_render_second_x,
                                                   chase_render_second_y,
                                                   chase_render_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 58);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x4a4c3e4f);

    wl_player_gameplay_state chase_attack_player;
    wl_live_actor_tick_result chase_attack_tick;
    CHECK(wl_init_player_gameplay_state(&chase_attack_player, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_actor_tick(&chase_attack_player, &live_ai_render_model,
                                  use_wall, use_info, WL_MAP_PLANE_WORDS,
                                  &live_ai_render_motion, 0, 0,
                                  0x10000, 0, WL_DIR_EAST, 0, 0,
                                  &live_ai_render_model.actors[0],
                                  WL_DIFFICULTY_HARD,
                                  1, 1, 1, 1, 0, 80, 0, 0, 1,
                                  &chase_attack_tick) == 0);
    CHECK(chase_attack_tick.actor_attacked == 1);
    CHECK(chase_attack_tick.attack_kind == WL_LIVE_ACTOR_ATTACK_SHOOT);
    CHECK(chase_attack_tick.shot.damaged == 1);
    CHECK(chase_attack_tick.shot.distance_tiles == 2);
    CHECK(chase_attack_tick.shot.damage.effective_points == 10);
    CHECK(chase_attack_tick.live.palette.kind == WL_PALETTE_SHIFT_RED);
    CHECK(chase_attack_player.health == 90);
    wl_present_frame_descriptor chase_attack_present;
    CHECK(wl_describe_present_frame(&canvas, &chase_attack_tick.live.palette,
                                    upload_palette,
                                    red_shift_palettes, WL_NUM_RED_SHIFTS,
                                    white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                    sizeof(upload_palette), 6,
                                    &chase_attack_present) == 0);
    CHECK(chase_attack_present.viewport_width == 80);
    CHECK(chase_attack_present.viewport_height == 128);
    CHECK(chase_attack_present.pixel_hash == 0x4a4c3e4f);
    CHECK(chase_attack_present.palette_shift_kind == WL_PALETTE_SHIFT_RED);
    CHECK(chase_attack_present.palette_shift_index ==
          chase_attack_tick.live.palette.shift_index);
    CHECK(chase_attack_present.palette_hash ==
          fnv1a_bytes(red_shift_palettes +
                          (size_t)chase_attack_present.palette_shift_index *
                              sizeof(upload_palette),
                      sizeof(upload_palette)));
    CHECK(chase_attack_present.texture.palette ==
          red_shift_palettes +
              (size_t)chase_attack_present.palette_shift_index *
                  sizeof(upload_palette));
    unsigned char chase_attack_rgba[80u * 128u * 4u];
    wl_texture_upload_descriptor chase_attack_rgba_upload;
    size_t chase_attack_rgba_pitch = 0;
    size_t chase_attack_rgba_size = 0;
    CHECK(wl_present_frame_rgba_layout(&chase_attack_present,
                                       &chase_attack_rgba_pitch,
                                       &chase_attack_rgba_size) == 0);
    CHECK(chase_attack_rgba_pitch == 80u * 4u);
    CHECK(chase_attack_rgba_size == sizeof(chase_attack_rgba));
    CHECK(wl_present_frame_rgba_size(&chase_attack_present,
                                     &chase_attack_rgba_size) == 0);
    CHECK(chase_attack_rgba_size == sizeof(chase_attack_rgba));
    CHECK(wl_expand_present_frame_to_rgba(&chase_attack_present,
                                          chase_attack_rgba,
                                          sizeof(chase_attack_rgba) - 1u,
                                          &chase_attack_rgba_upload) == -1);
    wl_present_frame_descriptor invalid_chase_attack_present = chase_attack_present;
    invalid_chase_attack_present.texture.palette_component_bits = 7;
    CHECK(wl_present_frame_rgba_layout(&invalid_chase_attack_present,
                                       &chase_attack_rgba_pitch,
                                       &chase_attack_rgba_size) == -1);
    CHECK(wl_present_frame_rgba_size(&invalid_chase_attack_present,
                                     &chase_attack_rgba_size) == -1);
    CHECK(wl_expand_present_frame_to_rgba(&invalid_chase_attack_present,
                                          chase_attack_rgba,
                                          sizeof(chase_attack_rgba),
                                          &chase_attack_rgba_upload) == -1);
    invalid_chase_attack_present = chase_attack_present;
    invalid_chase_attack_present.viewport_width =
        (uint16_t)(chase_attack_present.viewport_width - 1u);
    CHECK(wl_present_frame_rgba_layout(&invalid_chase_attack_present,
                                       &chase_attack_rgba_pitch,
                                       &chase_attack_rgba_size) == -1);
    CHECK(wl_expand_present_frame_to_rgba(&invalid_chase_attack_present,
                                          chase_attack_rgba,
                                          sizeof(chase_attack_rgba),
                                          &chase_attack_rgba_upload) == -1);
    CHECK(wl_expand_present_frame_to_rgba(&chase_attack_present,
                                          chase_attack_rgba,
                                          sizeof(chase_attack_rgba),
                                          &chase_attack_rgba_upload) == 0);
    CHECK(chase_attack_rgba_upload.format == WL_TEXTURE_UPLOAD_RGBA8888);
    CHECK(chase_attack_rgba_upload.width == chase_attack_present.viewport_width);
    CHECK(chase_attack_rgba_upload.height == chase_attack_present.viewport_height);
    CHECK(chase_attack_rgba_upload.pixel_bytes == sizeof(chase_attack_rgba));
    CHECK(chase_attack_rgba_upload.pixels == chase_attack_rgba);
    CHECK(chase_attack_rgba_upload.palette == NULL);
    CHECK(fnv1a_bytes(chase_attack_rgba, sizeof(chase_attack_rgba)) == 0x5483a51du);

    wl_game_model dog_chase_combat_model;
    memset(&dog_chase_combat_model, 0, sizeof(dog_chase_combat_model));
    dog_chase_combat_model.actor_count = 1;
    dog_chase_combat_model.actors[0].kind = WL_ACTOR_DOG;
    dog_chase_combat_model.actors[0].mode = WL_ACTOR_CHASE;
    dog_chase_combat_model.actors[0].dir = WL_DIR_EAST;
    dog_chase_combat_model.actors[0].tile_x = 5;
    dog_chase_combat_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &dog_chase_combat_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &dog_chase_combat_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(live_ai_render_tick.chases.tiles_stepped == 1);
    CHECK(dog_chase_combat_model.actors[0].tile_x == 5);
    CHECK(dog_chase_combat_model.actors[0].tile_y == 4);
    CHECK(wl_init_player_gameplay_state(&chase_attack_player, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    CHECK(wl_step_live_actor_tick(&chase_attack_player, &dog_chase_combat_model,
                                  use_wall, use_info, WL_MAP_PLANE_WORDS,
                                  &live_ai_render_motion, 0, 0,
                                  0x10000, 0, WL_DIR_EAST, 0, 0,
                                  &dog_chase_combat_model.actors[0],
                                  WL_DIFFICULTY_HARD,
                                  1, 1, 0, 1, 0, 80, 0, 0, 1,
                                  &chase_attack_tick) == 0);
    CHECK(chase_attack_tick.actor_attacked == 1);
    CHECK(chase_attack_tick.attack_kind == WL_LIVE_ACTOR_ATTACK_BITE);
    CHECK(chase_attack_tick.bite.in_range == 1);
    CHECK(chase_attack_tick.bite.damaged == 1);
    CHECK(chase_attack_tick.bite.damage.effective_points == 5);
    CHECK(chase_attack_player.health == 95);

    const struct {
        wl_actor_kind kind;
        uint16_t expected_distance;
        int32_t expected_damage;
        int32_t expected_health;
    } shooter_chase_cases[] = {
        { WL_ACTOR_OFFICER, 2, 10, 90 },
        { WL_ACTOR_SS, 1, 20, 80 },
        { WL_ACTOR_MUTANT, 2, 10, 90 },
        { WL_ACTOR_BOSS, 1, 20, 80 },
    };
    for (size_t shooter_case = 0;
         shooter_case < sizeof(shooter_chase_cases) / sizeof(shooter_chase_cases[0]);
         ++shooter_case) {
        wl_game_model shooter_chase_model;
        memset(&shooter_chase_model, 0, sizeof(shooter_chase_model));
        shooter_chase_model.actor_count = 1;
        shooter_chase_model.actors[0].kind = shooter_chase_cases[shooter_case].kind;
        shooter_chase_model.actors[0].shootable = 1;
        shooter_chase_model.actors[0].mode = WL_ACTOR_CHASE;
        shooter_chase_model.actors[0].dir = WL_DIR_EAST;
        shooter_chase_model.actors[0].tile_x = 5;
        shooter_chase_model.actors[0].tile_y = 5;
        CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                         &shooter_chase_model,
                                         use_wall, use_info, WL_MAP_PLANE_WORDS,
                                         &live_ai_render_motion, 0, 0,
                                         0x10000, 0, WL_DIR_EAST, 0, 0,
                                         0x8000u, 1,
                                         &live_ai_render_tick) == 0);
        CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                         &shooter_chase_model,
                                         use_wall, use_info, WL_MAP_PLANE_WORDS,
                                         &live_ai_render_motion, 0, 0,
                                         0x10000, 0, WL_DIR_EAST, 0, 0,
                                         0x8000u, 1,
                                         &live_ai_render_tick) == 0);
        CHECK(live_ai_render_tick.chases.tiles_stepped == 1);
        CHECK(wl_init_player_gameplay_state(&chase_attack_player, 100, 3, 0,
                                            WL_EXTRA_POINTS) == 0);
        CHECK(wl_step_live_actor_tick(&chase_attack_player, &shooter_chase_model,
                                      use_wall, use_info, WL_MAP_PLANE_WORDS,
                                      &live_ai_render_motion, 0, 0,
                                      0x10000, 0, WL_DIR_EAST, 0, 0,
                                      &shooter_chase_model.actors[0],
                                      WL_DIFFICULTY_HARD,
                                      1, 1, 1, 1, 0, 80, 0, 0, 1,
                                      &chase_attack_tick) == 0);
        CHECK(chase_attack_tick.actor_attacked == 1);
        CHECK(chase_attack_tick.attack_kind == WL_LIVE_ACTOR_ATTACK_SHOOT);
        CHECK(chase_attack_tick.shot.damaged == 1);
        CHECK(chase_attack_tick.shot.distance_tiles ==
              shooter_chase_cases[shooter_case].expected_distance);
        CHECK(chase_attack_tick.shot.damage.effective_points ==
              shooter_chase_cases[shooter_case].expected_damage);
        CHECK(chase_attack_player.health ==
              shooter_chase_cases[shooter_case].expected_health);
    }

    wl_game_model chase_full_combat_model;
    memset(&chase_full_combat_model, 0, sizeof(chase_full_combat_model));
    chase_full_combat_model.tilemap[7 + 4 * WL_MAP_SIDE] = 2;
    chase_full_combat_model.actor_count = 1;
    chase_full_combat_model.actors[0].kind = WL_ACTOR_GUARD;
    chase_full_combat_model.actors[0].shootable = 1;
    chase_full_combat_model.actors[0].mode = WL_ACTOR_CHASE;
    chase_full_combat_model.actors[0].dir = WL_DIR_EAST;
    chase_full_combat_model.actors[0].tile_x = 5;
    chase_full_combat_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &chase_full_combat_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &chase_full_combat_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(chase_full_combat_model.actors[0].tile_x == 5);
    CHECK(chase_full_combat_model.actors[0].tile_y == 4);
    wl_actor_combat_state chase_damage_actor;
    CHECK(wl_init_actor_combat_state(&chase_full_combat_model.actors[0],
                                     WL_DIFFICULTY_HARD,
                                     &chase_damage_actor) == 0);
    CHECK(wl_init_player_gameplay_state(&chase_attack_player, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    wl_live_full_combat_tick_result chase_full_combat;
    CHECK(wl_step_live_full_combat_tick(&chase_attack_player,
                                        &chase_full_combat_model,
                                        use_wall, use_info, WL_MAP_PLANE_WORDS,
                                        &live_ai_render_motion, 0, 0,
                                        0x10000, 0, WL_DIR_EAST, 0, 0,
                                        &chase_full_combat_model.actors[0],
                                        NULL, &chase_damage_actor, 25,
                                        0, dirinfo.header.sprite_start,
                                        WL_DIFFICULTY_HARD,
                                        1, 1, 1, 1, 0, 80,
                                        0, 0, 0, 0, 0, 1,
                                        &chase_full_combat) == 0);
    CHECK(chase_full_combat.actor_attacked == 1);
    CHECK(chase_full_combat.actor_attack_kind == WL_LIVE_ACTOR_ATTACK_SHOOT);
    CHECK(chase_full_combat.shot.damaged == 1);
    CHECK(chase_full_combat.shot.distance_tiles == 2);
    CHECK(chase_full_combat.actor_damaged == 1);
    CHECK(chase_full_combat.actor_damage.killed == 1);
    CHECK(chase_full_combat.drop_spawned == 1);
    CHECK(chase_full_combat.death_started == 1);
    CHECK(chase_full_combat.death_ref_built == 1);
    CHECK(chase_full_combat.actor_death_ref.source_index == 91);
    CHECK(chase_full_combat.actor_death_ref.vswap_chunk_index == 197);
    CHECK(chase_full_combat.actor_death_ref.world_x == 0x58000u);
    CHECK(chase_full_combat.actor_death_ref.world_y == 0x48000u);
    CHECK(chase_attack_player.health == 90);
    CHECK(wl_collect_scene_sprite_refs(&chase_full_combat_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 2);
    CHECK(scene_refs[0].kind == WL_SCENE_SPRITE_STATIC);
    CHECK(scene_refs[0].source_index == 28);
    CHECK(scene_refs[0].vswap_chunk_index == 134);
    CHECK(scene_refs[1].kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(scene_refs[1].source_index == 58);
    CHECK(scene_refs[1].vswap_chunk_index == 164);
    const uint16_t chase_full_scene_chunks[] = {
        chase_full_combat.actor_death_ref.vswap_chunk_index,
        scene_refs[0].vswap_chunk_index,
    };
    unsigned char chase_full_scene_pixels[WL_MAP_PLANE_WORDS * 2u];
    wl_indexed_surface chase_full_scene_surfaces_storage[2];
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               chase_full_scene_chunks, 2, 0,
                                               chase_full_scene_pixels,
                                               sizeof(chase_full_scene_pixels),
                                               chase_full_scene_surfaces_storage) == 0);
    const wl_indexed_surface *chase_full_scene_surfaces[] = {
        &chase_full_scene_surfaces_storage[0],
        &chase_full_scene_surfaces_storage[1],
    };
    const uint32_t chase_full_scene_x[] = {
        chase_full_combat.actor_death_ref.world_x,
        scene_refs[0].world_x,
    };
    const uint32_t chase_full_scene_y[] = {
        chase_full_combat.actor_death_ref.world_y,
        scene_refs[0].world_y,
    };
    const uint16_t chase_full_scene_ids[] = {
        chase_full_combat.actor_death_ref.source_index,
        scene_refs[0].source_index,
    };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&chase_full_combat_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   chase_full_scene_surfaces,
                                                   chase_full_scene_x,
                                                   chase_full_scene_y,
                                                   chase_full_scene_ids,
                                                   2, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 91);
    CHECK(sprites[0].visible == 1);
    CHECK(sprites[1].source_index == 28);
    uint32_t chase_initial_death_drop_hash =
        fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(chase_initial_death_drop_hash == 0x4a76f09a);

    wl_actor_death_state chase_active_death = chase_full_combat.actor_death;
    wl_live_full_combat_death_tick_result chase_final_death;
    CHECK(wl_step_live_full_combat_death_tick(&chase_attack_player,
                                              &chase_full_combat_model,
                                              use_wall, use_info,
                                              WL_MAP_PLANE_WORDS,
                                              &live_ai_render_motion, 0, 0,
                                              0x10000, 0, WL_DIR_EAST, 0, 0,
                                              NULL, NULL, NULL, 0,
                                              0, dirinfo.header.sprite_start,
                                              WL_DIFFICULTY_HARD,
                                              0, 0, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0, 45,
                                              0, &chase_active_death,
                                              &chase_final_death) == 0);
    CHECK(chase_final_death.death_stepped == 1);
    CHECK(chase_final_death.death.step.finished == 1);
    CHECK(chase_final_death.death.final_frame_applied == 1);
    CHECK(chase_final_death.death.death_ref.source_index == 95);
    CHECK(chase_final_death.death.death_ref.vswap_chunk_index == 201);
    CHECK(chase_full_combat_model.actors[0].scene_source_override == 1);
    CHECK(chase_full_combat_model.actors[0].scene_source_index == 95);
    CHECK(wl_collect_scene_sprite_refs(&chase_full_combat_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 2);
    CHECK(scene_refs[0].kind == WL_SCENE_SPRITE_STATIC);
    CHECK(scene_refs[1].kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(scene_refs[1].source_index == 95);
    const uint16_t chase_final_scene_chunks[] = {
        chase_final_death.death.death_ref.vswap_chunk_index,
        scene_refs[0].vswap_chunk_index,
    };
    unsigned char chase_final_scene_pixels[WL_MAP_PLANE_WORDS * 2u];
    wl_indexed_surface chase_final_scene_surfaces_storage[2];
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               chase_final_scene_chunks, 2, 0,
                                               chase_final_scene_pixels,
                                               sizeof(chase_final_scene_pixels),
                                               chase_final_scene_surfaces_storage) == 0);
    const wl_indexed_surface *chase_final_scene_surfaces[] = {
        &chase_final_scene_surfaces_storage[0],
        &chase_final_scene_surfaces_storage[1],
    };
    const uint32_t chase_final_scene_x[] = {
        chase_final_death.death.death_ref.world_x,
        scene_refs[0].world_x,
    };
    const uint32_t chase_final_scene_y[] = {
        chase_final_death.death.death_ref.world_y,
        scene_refs[0].world_y,
    };
    const uint16_t chase_final_scene_ids[] = {
        chase_final_death.death.death_ref.source_index,
        scene_refs[0].source_index,
    };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&chase_full_combat_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   chase_final_scene_surfaces,
                                                   chase_final_scene_x,
                                                   chase_final_scene_y,
                                                   chase_final_scene_ids,
                                                   2, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(sprites[0].source_index == 95);
    CHECK(sprites[0].visible == 1);
    CHECK(sprites[1].source_index == 28);
    uint32_t chase_final_death_drop_hash =
        fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(chase_final_death_drop_hash == 0x8a2741bf);
    CHECK(chase_final_death_drop_hash != chase_initial_death_drop_hash);
    CHECK(chase_final_death_drop_hash != 0x81c10dcf);

    const struct {
        wl_actor_kind kind;
        uint16_t final_source;
        uint16_t final_chunk;
        uint32_t final_scene_hash;
    } chase_death_classes[] = {
        { WL_ACTOR_OFFICER, 284, 390, 0x9b24b352u },
        { WL_ACTOR_SS, 183, 289, 0x5b093720u },
        { WL_ACTOR_MUTANT, 233, 339, 0xbfccde1bu },
        { WL_ACTOR_BOSS, 303, 409, 0xc6d3eb4du },
    };
    wl_game_model chase_dog_model;
    memset(&chase_dog_model, 0, sizeof(chase_dog_model));
    chase_dog_model.tilemap[7 + 4 * WL_MAP_SIDE] = 2;
    chase_dog_model.actor_count = 1;
    chase_dog_model.actors[0].kind = WL_ACTOR_DOG;
    chase_dog_model.actors[0].shootable = 1;
    chase_dog_model.actors[0].mode = WL_ACTOR_CHASE;
    chase_dog_model.actors[0].dir = WL_DIR_EAST;
    chase_dog_model.actors[0].tile_x = 5;
    chase_dog_model.actors[0].tile_y = 5;
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &chase_dog_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                     &chase_dog_model,
                                     use_wall, use_info, WL_MAP_PLANE_WORDS,
                                     &live_ai_render_motion, 0, 0,
                                     0x10000, 0, WL_DIR_EAST, 0, 0,
                                     0x8000u, 1,
                                     &live_ai_render_tick) == 0);
    wl_actor_combat_state chase_dog_actor;
    CHECK(wl_init_actor_combat_state(&chase_dog_model.actors[0],
                                     WL_DIFFICULTY_HARD,
                                     &chase_dog_actor) == 0);
    CHECK(wl_init_player_gameplay_state(&chase_attack_player, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    wl_live_full_combat_tick_result chase_dog_combat;
    CHECK(wl_step_live_full_combat_tick(&chase_attack_player,
                                        &chase_dog_model,
                                        use_wall, use_info, WL_MAP_PLANE_WORDS,
                                        &live_ai_render_motion, 0, 0,
                                        0x10000, 0, WL_DIR_EAST, 0, 0,
                                        NULL, NULL, &chase_dog_actor, 5,
                                        0, dirinfo.header.sprite_start,
                                        WL_DIFFICULTY_HARD,
                                        0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 1,
                                        &chase_dog_combat) == 0);
    CHECK(chase_dog_combat.death_started == 1);
    CHECK(chase_dog_combat.drop_spawned == 0);
    wl_actor_death_state chase_dog_death = chase_dog_combat.actor_death;
    wl_live_full_combat_death_tick_result chase_dog_final;
    CHECK(wl_step_live_full_combat_death_tick(&chase_attack_player,
                                              &chase_dog_model,
                                              use_wall, use_info,
                                              WL_MAP_PLANE_WORDS,
                                              &live_ai_render_motion, 0, 0,
                                              0x10000, 0, WL_DIR_EAST, 0, 0,
                                              NULL, NULL, NULL, 0,
                                              0, dirinfo.header.sprite_start,
                                              WL_DIFFICULTY_HARD,
                                              0, 0, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0, 80,
                                              0, &chase_dog_death,
                                              &chase_dog_final) == 0);
    CHECK(chase_dog_final.death.step.finished == 1);
    CHECK(chase_dog_final.death.death_ref.source_index == 134);
    CHECK(chase_dog_final.death.death_ref.vswap_chunk_index == 240);
    CHECK(wl_collect_scene_sprite_refs(&chase_dog_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].source_index == 134);
    const uint16_t chase_dog_scene_chunks[] = {
        chase_dog_final.death.death_ref.vswap_chunk_index,
    };
    unsigned char chase_dog_scene_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface chase_dog_scene_surface;
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               chase_dog_scene_chunks, 1, 0,
                                               chase_dog_scene_pixels,
                                               sizeof(chase_dog_scene_pixels),
                                               &chase_dog_scene_surface) == 0);
    const wl_indexed_surface *chase_dog_scene_surfaces[] = {
        &chase_dog_scene_surface,
    };
    const uint32_t chase_dog_scene_x[] = {
        chase_dog_final.death.death_ref.world_x,
    };
    const uint32_t chase_dog_scene_y[] = {
        chase_dog_final.death.death_ref.world_y,
    };
    const uint16_t chase_dog_scene_ids[] = {
        chase_dog_final.death.death_ref.source_index,
    };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&chase_dog_model,
                                                   dirinfo.header.sprite_start,
                                                   live_ai_render_motion.x,
                                                   live_ai_render_motion.y,
                                                   0x10000, 0, 0, -0x8000,
                                                   39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   chase_dog_scene_surfaces,
                                                   chase_dog_scene_x,
                                                   chase_dog_scene_y,
                                                   chase_dog_scene_ids,
                                                   1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips,
                                                   sprites, wall_heights) == 0);
    CHECK(sprites[0].source_index == 134);
    CHECK(sprites[0].visible == 1);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x92ff40dd);
    wl_palette_shift_result present_shift;
    memset(&present_shift, 0, sizeof(present_shift));
    present_shift.kind = WL_PALETTE_SHIFT_NONE;
    wl_present_frame_descriptor present_frame;
    CHECK(wl_describe_present_frame(&canvas, &present_shift,
                                    upload_palette,
                                    red_shift_palettes, WL_NUM_RED_SHIFTS,
                                    white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                    sizeof(upload_palette), 6,
                                    &present_frame) == 0);
    CHECK(present_frame.texture.format == WL_TEXTURE_UPLOAD_INDEXED8_RGB_PALETTE);
    CHECK(present_frame.viewport_width == 80);
    CHECK(present_frame.viewport_height == 128);
    CHECK(present_frame.pixel_hash == 0x92ff40dd);
    CHECK(present_frame.palette_hash ==
          fnv1a_bytes(upload_palette, sizeof(upload_palette)));
    CHECK(present_frame.palette_shift_kind == WL_PALETTE_SHIFT_NONE);
    CHECK(present_frame.texture.pixels == canvas.pixels);
    CHECK(present_frame.texture.palette == upload_palette);
    size_t present_rgba_size = 0;
    CHECK(wl_present_frame_rgba_size(&present_frame, &present_rgba_size) == 0);
    CHECK(present_rgba_size == 80u * 128u * 4u);
    unsigned char present_rgba[80u * 128u * 4u];
    wl_texture_upload_descriptor present_rgba_upload;
    CHECK(wl_expand_present_frame_to_rgba(&present_frame, present_rgba,
                                          present_rgba_size,
                                          &present_rgba_upload) == 0);
    CHECK(present_rgba_upload.format == WL_TEXTURE_UPLOAD_RGBA8888);
    CHECK(present_rgba_upload.width == present_frame.viewport_width);
    CHECK(present_rgba_upload.height == present_frame.viewport_height);
    CHECK(present_rgba_upload.pitch == 80u * 4u);
    CHECK(present_rgba_upload.pixel_bytes == sizeof(present_rgba));
    CHECK(present_rgba_upload.pixels == present_rgba);
    CHECK(present_rgba_upload.palette == NULL);
    enum { PRESENT_PADDED_PITCH = 80u * 4u + 16u };
    unsigned char present_rgba_padded[PRESENT_PADDED_PITCH * 128u];
    memset(present_rgba_padded, 0xa5, sizeof(present_rgba_padded));
    wl_texture_upload_descriptor present_rgba_padded_upload;
    CHECK(wl_expand_present_frame_to_rgba_pitched(&present_frame,
                                                  present_rgba_padded,
                                                  sizeof(present_rgba_padded),
                                                  PRESENT_PADDED_PITCH,
                                                  &present_rgba_padded_upload) == 0);
    CHECK(present_rgba_padded_upload.format == WL_TEXTURE_UPLOAD_RGBA8888);
    CHECK(present_rgba_padded_upload.pitch == PRESENT_PADDED_PITCH);
    CHECK(present_rgba_padded_upload.pixel_bytes == sizeof(present_rgba_padded));
    wl_texture_upload_descriptor described_padded_rgba_upload;
    CHECK(wl_describe_present_frame_rgba_upload_pitched(
              &present_frame, present_rgba_padded, sizeof(present_rgba_padded),
              PRESENT_PADDED_PITCH, &described_padded_rgba_upload) == 0);
    CHECK(described_padded_rgba_upload.format == WL_TEXTURE_UPLOAD_RGBA8888);
    CHECK(described_padded_rgba_upload.width == present_frame.viewport_width);
    CHECK(described_padded_rgba_upload.height == present_frame.viewport_height);
    CHECK(described_padded_rgba_upload.pitch == PRESENT_PADDED_PITCH);
    CHECK(described_padded_rgba_upload.pixel_bytes == sizeof(present_rgba_padded));
    CHECK(described_padded_rgba_upload.pixels == present_rgba_padded);
    CHECK(described_padded_rgba_upload.palette == NULL);
    for (size_t row = 0; row < 128u; ++row) {
        CHECK(memcmp(present_rgba_padded + row * PRESENT_PADDED_PITCH,
                     present_rgba + row * 80u * 4u, 80u * 4u) == 0);
        for (size_t pad = 80u * 4u; pad < PRESENT_PADDED_PITCH; ++pad) {
            CHECK(present_rgba_padded[row * PRESENT_PADDED_PITCH + pad] == 0xa5);
        }
    }
    CHECK(wl_expand_present_frame_to_rgba_pitched(&present_frame,
                                                  present_rgba_padded,
                                                  sizeof(present_rgba_padded),
                                                  80u * 4u - 1u,
                                                  &present_rgba_padded_upload) == -1);
    CHECK(wl_expand_present_frame_to_rgba_pitched(&present_frame,
                                                  present_rgba_padded,
                                                  sizeof(present_rgba_padded) - 17u,
                                                  PRESENT_PADDED_PITCH,
                                                  &present_rgba_padded_upload) == -1);
    CHECK(wl_expand_present_frame_to_rgba_pitched(&present_frame,
                                                  present_rgba_padded,
                                                  sizeof(present_rgba_padded) - 16u,
                                                  PRESENT_PADDED_PITCH,
                                                  NULL) == 0);
    CHECK(wl_expand_present_frame_to_rgba_pitched(&present_frame,
                                                  present_rgba_padded,
                                                  sizeof(present_rgba_padded) - 16u,
                                                  PRESENT_PADDED_PITCH,
                                                  &present_rgba_padded_upload) == -1);
    wl_texture_upload_descriptor described_rgba_upload;
    CHECK(wl_describe_present_frame_rgba_upload(&present_frame, present_rgba,
                                                present_rgba_size,
                                                &described_rgba_upload) == 0);
    CHECK(described_rgba_upload.format == present_rgba_upload.format);
    CHECK(described_rgba_upload.width == present_rgba_upload.width);
    CHECK(described_rgba_upload.height == present_rgba_upload.height);
    CHECK(described_rgba_upload.pitch == present_rgba_upload.pitch);
    CHECK(described_rgba_upload.pixel_bytes == present_rgba_upload.pixel_bytes);
    CHECK(described_rgba_upload.pixels == present_rgba_upload.pixels);
    wl_present_frame_descriptor invalid_present_frame = present_frame;
    invalid_present_frame.viewport_width = (uint16_t)(invalid_present_frame.viewport_width + 1u);
    CHECK(wl_expand_present_frame_to_rgba(&invalid_present_frame, present_rgba,
                                          sizeof(present_rgba),
                                          &present_rgba_upload) == -1);
    invalid_present_frame = present_frame;
    invalid_present_frame.viewport_height = (uint16_t)(invalid_present_frame.viewport_height - 1u);
    CHECK(wl_expand_present_frame_to_rgba(&invalid_present_frame, present_rgba,
                                          sizeof(present_rgba),
                                          &present_rgba_upload) == -1);
    CHECK(wl_expand_present_frame_to_rgba(NULL, present_rgba,
                                          sizeof(present_rgba),
                                          &present_rgba_upload) == -1);
    CHECK(wl_present_frame_rgba_size(NULL, &present_rgba_size) == -1);
    CHECK(wl_present_frame_rgba_size(&present_frame, NULL) == -1);
    invalid_present_frame = present_frame;
    invalid_present_frame.texture.palette_entries = 255;
    CHECK(wl_present_frame_rgba_size(&invalid_present_frame,
                                     &present_rgba_size) == -1);
    CHECK(wl_describe_present_frame_rgba_upload(&invalid_present_frame,
                                                present_rgba,
                                                sizeof(present_rgba),
                                                &described_rgba_upload) == -1);
    CHECK(wl_describe_present_frame_rgba_upload(&present_frame, NULL,
                                                sizeof(present_rgba),
                                                &described_rgba_upload) == -1);
    CHECK(wl_describe_present_frame_rgba_upload(&present_frame, present_rgba,
                                                sizeof(present_rgba),
                                                NULL) == -1);
    CHECK(wl_describe_present_frame_rgba_upload(&present_frame, present_rgba,
                                                sizeof(present_rgba) - 1u,
                                                &described_rgba_upload) == -1);
    CHECK(wl_describe_present_frame_rgba_upload_pitched(
              &present_frame, present_rgba_padded, sizeof(present_rgba_padded),
              80u * 4u - 1u, &described_padded_rgba_upload) == -1);
    CHECK(wl_describe_present_frame_rgba_upload_pitched(
              &present_frame, present_rgba_padded, sizeof(present_rgba_padded) - 17u,
              PRESENT_PADDED_PITCH, &described_padded_rgba_upload) == -1);
    CHECK(wl_describe_present_frame_rgba_upload_pitched(
              &present_frame, present_rgba_padded, sizeof(present_rgba_padded) - 16u,
              PRESENT_PADDED_PITCH, &described_padded_rgba_upload) == -1);
    CHECK(wl_describe_present_frame_rgba_upload_pitched(
              &present_frame, present_rgba_padded, sizeof(present_rgba_padded),
              PRESENT_PADDED_PITCH, NULL) == -1);
    CHECK(wl_expand_present_frame_to_rgba(&present_frame, present_rgba,
                                          sizeof(present_rgba) - 1u,
                                          &present_rgba_upload) == -1);
    memset(&present_shift, 0, sizeof(present_shift));
    present_shift.kind = WL_PALETTE_SHIFT_RED;
    present_shift.shift_index = 2;
    CHECK(wl_describe_present_frame(&canvas, &present_shift,
                                    upload_palette,
                                    red_shift_palettes, WL_NUM_RED_SHIFTS,
                                    white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                    sizeof(upload_palette), 6,
                                    &present_frame) == 0);
    CHECK(present_frame.pixel_hash == 0x92ff40dd);
    CHECK(present_frame.palette_hash == 0x90a6cdc5);
    CHECK(present_frame.palette_shift_kind == WL_PALETTE_SHIFT_RED);
    CHECK(present_frame.palette_shift_index == 2);
    CHECK(present_frame.texture.palette ==
          red_shift_palettes + 2u * sizeof(upload_palette));
    memset(&present_shift, 0, sizeof(present_shift));
    present_shift.kind = WL_PALETTE_SHIFT_WHITE;
    present_shift.shift_index = 2;
    CHECK(wl_describe_present_frame(&canvas, &present_shift,
                                    upload_palette,
                                    red_shift_palettes, WL_NUM_RED_SHIFTS,
                                    white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                    sizeof(upload_palette), 6,
                                    &present_frame) == 0);
    CHECK(present_frame.pixel_hash == 0x92ff40dd);
    CHECK(present_frame.palette_hash == 0x3c8da1ed);
    CHECK(present_frame.palette_shift_kind == WL_PALETTE_SHIFT_WHITE);
    CHECK(present_frame.palette_shift_index == 2);
    CHECK(present_frame.texture.palette ==
          white_shift_palettes + 2u * sizeof(upload_palette));
    CHECK(wl_describe_present_frame(NULL, &present_shift,
                                    upload_palette,
                                    red_shift_palettes, WL_NUM_RED_SHIFTS,
                                    white_shift_palettes, WL_NUM_WHITE_SHIFTS,
                                    sizeof(upload_palette), 6,
                                    &present_frame) == -1);

    for (size_t death_case = 0;
         death_case < sizeof(chase_death_classes) / sizeof(chase_death_classes[0]);
         ++death_case) {
        wl_game_model chase_class_model;
        memset(&chase_class_model, 0, sizeof(chase_class_model));
        chase_class_model.tilemap[7 + 4 * WL_MAP_SIDE] = 2;
        chase_class_model.actor_count = 1;
        chase_class_model.actors[0].kind = chase_death_classes[death_case].kind;
        chase_class_model.actors[0].shootable =
            chase_death_classes[death_case].kind == WL_ACTOR_DOG ? 0u : 1u;
        chase_class_model.actors[0].mode = WL_ACTOR_CHASE;
        chase_class_model.actors[0].dir = WL_DIR_EAST;
        chase_class_model.actors[0].tile_x = 5;
        chase_class_model.actors[0].tile_y = 5;
        CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                         &chase_class_model,
                                         use_wall, use_info, WL_MAP_PLANE_WORDS,
                                         &live_ai_render_motion, 0, 0,
                                         0x10000, 0, WL_DIR_EAST, 0, 0,
                                         0x8000u, 1,
                                         &live_ai_render_tick) == 0);
        CHECK(wl_step_live_actor_ai_tick(&live_ai_render_player,
                                         &chase_class_model,
                                         use_wall, use_info, WL_MAP_PLANE_WORDS,
                                         &live_ai_render_motion, 0, 0,
                                         0x10000, 0, WL_DIR_EAST, 0, 0,
                                         0x8000u, 1,
                                         &live_ai_render_tick) == 0);
        wl_actor_combat_state chase_class_actor;
        CHECK(wl_init_actor_combat_state(&chase_class_model.actors[0],
                                         WL_DIFFICULTY_HARD,
                                         &chase_class_actor) == 0);
        CHECK(wl_init_player_gameplay_state(&chase_attack_player, 100, 3, 0,
                                            WL_EXTRA_POINTS) == 0);
        wl_live_full_combat_tick_result chase_class_combat;
        CHECK(wl_step_live_full_combat_tick(&chase_attack_player,
                                            &chase_class_model,
                                            use_wall, use_info,
                                            WL_MAP_PLANE_WORDS,
                                            &live_ai_render_motion, 0, 0,
                                            0x10000, 0, WL_DIR_EAST, 0, 0,
                                            &chase_class_model.actors[0],
                                            NULL, &chase_class_actor, 2000,
                                            0, dirinfo.header.sprite_start,
                                            WL_DIFFICULTY_HARD,
                                            1, 1, 1, 1, 0, 80,
                                            0, 0, 0, 0, 0, 1,
                                            &chase_class_combat) == 0);
        CHECK(chase_class_combat.death_started == 1);
        wl_actor_death_state chase_class_death = chase_class_combat.actor_death;
        wl_live_full_combat_death_tick_result chase_class_final;
        CHECK(wl_step_live_full_combat_death_tick(&chase_attack_player,
                                                  &chase_class_model,
                                                  use_wall, use_info,
                                                  WL_MAP_PLANE_WORDS,
                                                  &live_ai_render_motion, 0, 0,
                                                  0x10000, 0, WL_DIR_EAST, 0, 0,
                                                  NULL, NULL, NULL, 0,
                                                  0, dirinfo.header.sprite_start,
                                                  WL_DIFFICULTY_HARD,
                                                  0, 0, 0, 0, 0, 0,
                                                  0, 0, 0, 0, 0, 80,
                                                  0, &chase_class_death,
                                                  &chase_class_final) == 0);
        CHECK(chase_class_final.death.step.finished == 1);
        CHECK(chase_class_final.death.final_frame_applied == 1);
        CHECK(chase_class_final.death.death_ref.source_index ==
              chase_death_classes[death_case].final_source);
        CHECK(chase_class_final.death.death_ref.vswap_chunk_index ==
              chase_death_classes[death_case].final_chunk);
        CHECK(chase_class_model.actors[0].scene_source_override == 1);
        CHECK(chase_class_model.actors[0].scene_source_index ==
              chase_death_classes[death_case].final_source);
        CHECK(wl_collect_scene_sprite_refs(&chase_class_model,
                                           dirinfo.header.sprite_start,
                                           scene_refs,
                                           sizeof(scene_refs) / sizeof(scene_refs[0]),
                                           &scene_ref_count) == 0);
        CHECK(scene_ref_count == 2);
        CHECK(scene_refs[0].kind == WL_SCENE_SPRITE_STATIC);
        CHECK(scene_refs[1].kind == WL_SCENE_SPRITE_ACTOR);
        CHECK(scene_refs[1].source_index ==
              chase_death_classes[death_case].final_source);
        const uint16_t chase_class_scene_chunks[] = {
            chase_class_final.death.death_ref.vswap_chunk_index,
            scene_refs[0].vswap_chunk_index,
        };
        unsigned char chase_class_scene_pixels[WL_MAP_PLANE_WORDS * 2u];
        wl_indexed_surface chase_class_scene_surfaces_storage[2];
        CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                                   chase_class_scene_chunks, 2, 0,
                                                   chase_class_scene_pixels,
                                                   sizeof(chase_class_scene_pixels),
                                                   chase_class_scene_surfaces_storage) == 0);
        const wl_indexed_surface *chase_class_scene_surfaces[] = {
            &chase_class_scene_surfaces_storage[0],
            &chase_class_scene_surfaces_storage[1],
        };
        const uint32_t chase_class_scene_x[] = {
            chase_class_final.death.death_ref.world_x,
            scene_refs[0].world_x,
        };
        const uint32_t chase_class_scene_y[] = {
            chase_class_final.death.death_ref.world_y,
            scene_refs[0].world_y,
        };
        const uint16_t chase_class_scene_ids[] = {
            chase_class_final.death.death_ref.source_index,
            scene_refs[0].source_index,
        };
        memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
        CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels,
                                      sizeof(canvas_pixels), &canvas) == 0);
        CHECK(wl_render_runtime_door_camera_scene_view(&chase_class_model,
                                                       dirinfo.header.sprite_start,
                                                       live_ai_render_motion.x,
                                                       live_ai_render_motion.y,
                                                       0x10000, 0, 0, -0x8000,
                                                       39, 1, 3,
                                                       runtime_door_pages,
                                                       runtime_door_page_sizes, 106,
                                                       chase_class_scene_surfaces,
                                                       chase_class_scene_x,
                                                       chase_class_scene_y,
                                                       chase_class_scene_ids,
                                                       2, 0,
                                                       &canvas, runtime_dirs_x,
                                                       runtime_dirs_y,
                                                       runtime_view_hits,
                                                       runtime_view_strips,
                                                       sprites, wall_heights) == 0);
        CHECK(sprites[0].source_index ==
              chase_death_classes[death_case].final_source);
        CHECK(sprites[0].visible == 1);
        CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) ==
              chase_death_classes[death_case].final_scene_hash);
    }

    wl_game_model live_drop_scene_model;
    memset(&live_drop_scene_model, 0, sizeof(live_drop_scene_model));
    live_drop_scene_model.tilemap[7 + 4 * WL_MAP_SIDE] = 2;
    memset(use_wall, 0, sizeof(use_wall));
    memset(use_info, 0, sizeof(use_info));
    wl_actor_desc live_drop_actor;
    memset(&live_drop_actor, 0, sizeof(live_drop_actor));
    live_drop_actor.kind = WL_ACTOR_GUARD;
    live_drop_actor.shootable = 1;
    live_drop_actor.tile_x = 5;
    live_drop_actor.tile_y = 4;
    wl_actor_combat_state live_drop_actor_state;
    CHECK(wl_init_actor_combat_state(&live_drop_actor, WL_DIFFICULTY_HARD,
                                     &live_drop_actor_state) == 0);
    wl_player_gameplay_state live_drop_player;
    CHECK(wl_init_player_gameplay_state(&live_drop_player, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    wl_player_motion_state live_drop_motion = {
        (3u << 16) + 0x8000u,
        (4u << 16) + 0x8000u,
        3,
        4,
    };
    wl_live_actor_damage_tick_result live_drop_tick;
    CHECK(wl_step_live_actor_damage_tick(&live_drop_player, &live_drop_scene_model,
                                         use_wall, use_info, WL_MAP_PLANE_WORDS,
                                         &live_drop_motion, 0, 0,
                                         0x10000, 0, WL_DIR_EAST, 0, 0,
                                         &live_drop_actor_state, 25, 1,
                                         &live_drop_tick) == 0);
    CHECK(live_drop_tick.actor_damage.killed == 1);
    CHECK(live_drop_tick.drop_spawned == 1);
    CHECK(live_drop_tick.drop_static_index == 0);
    CHECK(live_drop_scene_model.static_count == 1);
    CHECK(live_drop_scene_model.statics[0].active == 1);
    CHECK(wl_collect_scene_sprite_refs(&live_drop_scene_model,
                                       dirinfo.header.sprite_start,
                                       scene_refs,
                                       sizeof(scene_refs) / sizeof(scene_refs[0]),
                                       &scene_ref_count) == 0);
    CHECK(scene_ref_count == 1);
    CHECK(scene_refs[0].kind == WL_SCENE_SPRITE_STATIC);
    CHECK(scene_refs[0].source_index == 28);
    CHECK(scene_refs[0].vswap_chunk_index == 134);
    const uint16_t live_drop_chunks[] = { scene_refs[0].vswap_chunk_index };
    unsigned char live_drop_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface live_drop_surface;
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               live_drop_chunks, 1, 0,
                                               live_drop_pixels,
                                               sizeof(live_drop_pixels),
                                               &live_drop_surface) == 0);
    const wl_indexed_surface *live_drop_surfaces[] = { &live_drop_surface };
    const uint32_t live_drop_scene_x[] = { scene_refs[0].world_x };
    const uint32_t live_drop_scene_y[] = { scene_refs[0].world_y };
    const uint16_t live_drop_scene_ids[] = { scene_refs[0].source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&live_drop_scene_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_drop_surfaces,
                                                   live_drop_scene_x,
                                                   live_drop_scene_y,
                                                   live_drop_scene_ids,
                                                   scene_ref_count, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    uint32_t live_drop_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(live_drop_scene_hash == 0x707dbe4e);
    CHECK(live_drop_scene_hash != picked_static_scene_hash);

    wl_actor_death_state live_death_state;
    CHECK(wl_start_actor_death_state(&live_drop_actor_state,
                                     &live_drop_tick.actor_damage,
                                     &live_death_state) == 0);
    CHECK(live_death_state.sprite_source_index == 91);
    wl_scene_sprite_ref live_death_ref;
    CHECK(wl_build_actor_death_scene_ref(&live_drop_actor_state,
                                         &live_death_state,
                                         dirinfo.header.sprite_start, 3,
                                         &live_death_ref) == 0);
    CHECK(live_death_ref.kind == WL_SCENE_SPRITE_ACTOR);
    CHECK(live_death_ref.source_index == 91);
    CHECK(live_death_ref.vswap_chunk_index == 197);
    const uint16_t live_death_chunks[] = { live_death_ref.vswap_chunk_index };
    unsigned char live_death_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface live_death_surface;
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               live_death_chunks, 1, 0,
                                               live_death_pixels,
                                               sizeof(live_death_pixels),
                                               &live_death_surface) == 0);
    const wl_indexed_surface *live_death_surfaces[] = { &live_death_surface };
    const uint32_t live_death_scene_x[] = { live_death_ref.world_x };
    const uint32_t live_death_scene_y[] = { live_death_ref.world_y };
    const uint16_t live_death_scene_ids[] = { live_death_ref.source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&live_drop_scene_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   live_death_surfaces,
                                                   live_death_scene_x,
                                                   live_death_scene_y,
                                                   live_death_scene_ids,
                                                   1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    uint32_t live_death_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(live_death_scene_hash == 0x2e8b4819);
    CHECK(live_death_scene_hash != live_drop_scene_hash);

    wl_game_model full_death_model;
    memset(&full_death_model, 0, sizeof(full_death_model));
    full_death_model.tilemap[7 + 4 * WL_MAP_SIDE] = 2;
    full_death_model.actor_count = 6;
    full_death_model.actors[5].kind = WL_ACTOR_GUARD;
    full_death_model.actors[5].mode = WL_ACTOR_STAND;
    full_death_model.actors[5].shootable = 1;
    full_death_model.actors[5].tile_x = 5;
    full_death_model.actors[5].tile_y = 4;
    wl_actor_desc full_death_actor_desc;
    memset(&full_death_actor_desc, 0, sizeof(full_death_actor_desc));
    full_death_actor_desc.kind = WL_ACTOR_GUARD;
    full_death_actor_desc.shootable = 1;
    full_death_actor_desc.tile_x = 5;
    full_death_actor_desc.tile_y = 4;
    wl_actor_combat_state full_death_actor;
    CHECK(wl_init_actor_combat_state(&full_death_actor_desc,
                                     WL_DIFFICULTY_HARD,
                                     &full_death_actor) == 0);
    wl_player_gameplay_state full_death_player;
    CHECK(wl_init_player_gameplay_state(&full_death_player, 100, 3, 0,
                                        WL_EXTRA_POINTS) == 0);
    wl_player_motion_state full_death_motion = {
        (3u << 16) + 0x8000u,
        (4u << 16) + 0x8000u,
        3,
        4,
    };
    wl_live_full_combat_tick_result full_death_tick;
    CHECK(wl_step_live_full_combat_tick(&full_death_player, &full_death_model,
                                        use_wall, use_info, WL_MAP_PLANE_WORDS,
                                        &full_death_motion, 0, 0,
                                        0x10000, 0, WL_DIR_EAST, 0, 0,
                                        NULL, NULL, &full_death_actor, 25,
                                        5, dirinfo.header.sprite_start,
                                        WL_DIFFICULTY_HARD,
                                        0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 1,
                                        &full_death_tick) == 0);
    CHECK(full_death_tick.death_started == 1);
    CHECK(full_death_tick.death_ref_built == 1);
    CHECK(full_death_tick.actor_death_ref.source_index == 91);
    CHECK(full_death_tick.actor_death_ref.vswap_chunk_index == 197);
    CHECK(full_death_tick.actor_death_ref.model_index == 5);
    const uint16_t full_death_chunks[] = {
        full_death_tick.actor_death_ref.vswap_chunk_index,
    };
    unsigned char full_death_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface full_death_surface;
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               full_death_chunks, 1, 0,
                                               full_death_pixels,
                                               sizeof(full_death_pixels),
                                               &full_death_surface) == 0);
    const wl_indexed_surface *full_death_surfaces[] = { &full_death_surface };
    const uint32_t full_death_scene_x[] = { full_death_tick.actor_death_ref.world_x };
    const uint32_t full_death_scene_y[] = { full_death_tick.actor_death_ref.world_y };
    const uint16_t full_death_scene_ids[] = { full_death_tick.actor_death_ref.source_index };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&full_death_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   full_death_surfaces,
                                                   full_death_scene_x,
                                                   full_death_scene_y,
                                                   full_death_scene_ids,
                                                   1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == live_death_scene_hash);

    wl_actor_death_state active_full_death = full_death_tick.actor_death;
    wl_live_full_combat_death_tick_result combined_death_tick;
    CHECK(wl_step_live_full_combat_death_tick(&full_death_player,
                                              &full_death_model,
                                              use_wall, use_info,
                                              WL_MAP_PLANE_WORDS,
                                              &full_death_motion, 0, 0,
                                              0x10000, 0, WL_DIR_EAST, 0, 0,
                                              NULL, NULL, NULL, 0,
                                              0, dirinfo.header.sprite_start,
                                              WL_DIFFICULTY_HARD,
                                              0, 0, 0, 0, 0, 0,
                                              0, 0, 0, 0, 0, 45,
                                              5, &active_full_death,
                                              &combined_death_tick) == 0);
    CHECK(combined_death_tick.combat.actor_damaged == 0);
    CHECK(combined_death_tick.combat.drop_static_index == 1);
    CHECK(combined_death_tick.death_stepped == 1);
    CHECK(combined_death_tick.death.step.finished == 1);
    CHECK(combined_death_tick.death.death_ref.source_index == 95);
    CHECK(combined_death_tick.death.death_ref.vswap_chunk_index == 201);
    CHECK(combined_death_tick.death.final_frame_applied == 1);
    CHECK(full_death_model.actors[5].scene_source_override == 1);
    CHECK(full_death_model.actors[5].scene_source_index == 95);
    const uint16_t final_death_chunks[] = {
        combined_death_tick.death.death_ref.vswap_chunk_index,
    };
    unsigned char final_death_pixels[WL_MAP_PLANE_WORDS];
    wl_indexed_surface final_death_surface;
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               final_death_chunks, 1, 0,
                                               final_death_pixels,
                                               sizeof(final_death_pixels),
                                               &final_death_surface) == 0);
    const wl_indexed_surface *final_death_surfaces[] = { &final_death_surface };
    const uint32_t final_death_scene_x[] = {
        combined_death_tick.death.death_ref.world_x,
    };
    const uint32_t final_death_scene_y[] = {
        combined_death_tick.death.death_ref.world_y,
    };
    const uint16_t final_death_scene_ids[] = {
        combined_death_tick.death.death_ref.source_index,
    };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&full_death_model,
                                                   dirinfo.header.sprite_start,
                                                   (3u << 16) + 0x8000u,
                                                   (4u << 16) + 0x8000u,
                                                   0x10000, 0, 0, -0x8000, 39, 1, 3,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   final_death_surfaces,
                                                   final_death_scene_x,
                                                   final_death_scene_y,
                                                   final_death_scene_ids,
                                                   1, 0,
                                                   &canvas, runtime_dirs_x,
                                                   runtime_dirs_y, runtime_view_hits,
                                                   runtime_view_strips, sprites,
                                                   wall_heights) == 0);
    uint32_t combined_death_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(combined_death_scene_hash == 0x81c10dcf);
    CHECK(combined_death_scene_hash != live_death_scene_hash);

    const wl_indexed_surface *bad_scene_sprites[] = { NULL };
    CHECK(wl_render_camera_scene_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                      player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                      view_pages, view_page_sizes, 18,
                                      bad_scene_sprites, scene_sprite_x, scene_sprite_y,
                                      scene_sprite_ids, 1, 0, &canvas,
                                      camera_dirs_x, camera_dirs_y, view_hits,
                                      view_strips, sprites, wall_heights) == -1);
    const uint16_t visible_ref_chunks[] = {
        scene_refs[113].vswap_chunk_index,
        scene_refs[114].vswap_chunk_index,
    };
    unsigned char visible_ref_pixels[2 * WL_MAP_PLANE_WORDS];
    wl_indexed_surface visible_ref_surfaces[2];
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               visible_ref_chunks, 2, 0,
                                               visible_ref_pixels,
                                               sizeof(visible_ref_pixels),
                                               visible_ref_surfaces) == 0);
    CHECK(fnv1a_bytes(visible_ref_pixels, sizeof(visible_ref_pixels)) == 0xd53b06f5);
    CHECK(fnv1a_bytes(visible_ref_surfaces[0].pixels,
                      visible_ref_surfaces[0].pixel_count) == 0x442facd4);
    CHECK(fnv1a_bytes(visible_ref_surfaces[1].pixels,
                      visible_ref_surfaces[1].pixel_count) == 0xd363bf0c);
    const wl_indexed_surface *visible_scene_sprites[] = {
        &visible_ref_surfaces[0],
        &visible_ref_surfaces[1],
    };
    const uint32_t visible_scene_x[] = {
        scene_refs[113].world_x,
        scene_refs[114].world_x,
    };
    const uint32_t visible_scene_y[] = {
        scene_refs[113].world_y,
        scene_refs[114].world_y,
    };
    const uint16_t visible_scene_ids[] = {
        scene_refs[113].source_index,
        scene_refs[114].source_index,
    };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_camera_scene_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                      player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                      view_pages, view_page_sizes, 18,
                                      visible_scene_sprites, visible_scene_x,
                                      visible_scene_y, visible_scene_ids, 2, 0,
                                      &canvas, camera_dirs_x, camera_dirs_y,
                                      view_hits, view_strips, sprites,
                                      wall_heights) == 0);
    CHECK(sprites[0].source_index == scene_refs[114].source_index);
    CHECK(sprites[0].surface_index == 1);
    CHECK(sprites[0].view_x == 39);
    CHECK(sprites[0].scaled_height == 26);
    CHECK(sprites[1].source_index == scene_refs[113].source_index);
    CHECK(sprites[1].surface_index == 0);
    CHECK(sprites[1].view_x == 39);
    CHECK(sprites[1].scaled_height == 42);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x61f7f78b);
    const size_t broad_ref_indices[] = { 110, 111, 113, 114, 115 };
    uint16_t broad_ref_chunks[5];
    uint32_t broad_scene_x[5];
    uint32_t broad_scene_y[5];
    uint16_t broad_scene_ids[5];
    for (size_t i = 0; i < 5; ++i) {
        broad_ref_chunks[i] = scene_refs[broad_ref_indices[i]].vswap_chunk_index;
        broad_scene_x[i] = scene_refs[broad_ref_indices[i]].world_x;
        broad_scene_y[i] = scene_refs[broad_ref_indices[i]].world_y;
        broad_scene_ids[i] = scene_refs[broad_ref_indices[i]].source_index;
    }
    unsigned char broad_ref_pixels[5 * WL_MAP_PLANE_WORDS];
    wl_indexed_surface broad_ref_surfaces[5];
    CHECK(wl_decode_vswap_sprite_surface_cache(vswap_path, &sprite_dirinfo,
                                               broad_ref_chunks, 5, 0,
                                               broad_ref_pixels,
                                               sizeof(broad_ref_pixels),
                                               broad_ref_surfaces) == 0);
    CHECK(fnv1a_bytes(broad_ref_pixels, sizeof(broad_ref_pixels)) == 0x61a879ca);
    CHECK(fnv1a_bytes(broad_ref_surfaces[0].pixels,
                      broad_ref_surfaces[0].pixel_count) == 0x442facd4);
    CHECK(fnv1a_bytes(broad_ref_surfaces[1].pixels,
                      broad_ref_surfaces[1].pixel_count) == 0x853fcce9);
    CHECK(fnv1a_bytes(broad_ref_surfaces[2].pixels,
                      broad_ref_surfaces[2].pixel_count) == 0x442facd4);
    CHECK(fnv1a_bytes(broad_ref_surfaces[3].pixels,
                      broad_ref_surfaces[3].pixel_count) == 0xd363bf0c);
    CHECK(fnv1a_bytes(broad_ref_surfaces[4].pixels,
                      broad_ref_surfaces[4].pixel_count) == 0x5ff159ef);
    const wl_indexed_surface *broad_scene_sprites[] = {
        &broad_ref_surfaces[0],
        &broad_ref_surfaces[1],
        &broad_ref_surfaces[2],
        &broad_ref_surfaces[3],
        &broad_ref_surfaces[4],
    };
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_camera_scene_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                      player_y, 0x10000, 0, 0, -0x8000, 30, 1, 5,
                                      view_pages, view_page_sizes, 18,
                                      broad_scene_sprites, broad_scene_x,
                                      broad_scene_y, broad_scene_ids, 5, 0,
                                      &canvas, camera_dirs_x, camera_dirs_y,
                                      view_hits, view_strips, sprites,
                                      wall_heights) == 0);
    CHECK(sprites[0].source_index == scene_refs[115].source_index);
    CHECK(sprites[0].surface_index == 4);
    CHECK(sprites[0].view_x == 39);
    CHECK(sprites[0].scaled_height == 19);
    CHECK(sprites[1].source_index == scene_refs[111].source_index);
    CHECK(sprites[1].surface_index == 1);
    CHECK(sprites[1].view_x == 10);
    CHECK(sprites[1].scaled_height == 23);
    CHECK(sprites[2].source_index == scene_refs[114].source_index);
    CHECK(sprites[2].surface_index == 3);
    CHECK(sprites[2].view_x == 39);
    CHECK(sprites[2].scaled_height == 26);
    CHECK(sprites[3].source_index == scene_refs[110].source_index);
    CHECK(sprites[3].surface_index == 0);
    CHECK(sprites[3].view_x == -14);
    CHECK(sprites[3].scaled_height == 42);
    CHECK(sprites[4].source_index == scene_refs[113].source_index);
    CHECK(sprites[4].surface_index == 2);
    CHECK(sprites[4].view_x == 39);
    CHECK(sprites[4].scaled_height == 42);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0xb92e568b);
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&model,
                                                   dirinfo.header.sprite_start,
                                                   player_x, player_y,
                                                   0x10000, 0, 0, -0x8000,
                                                   30, 1, 5,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   broad_scene_sprites,
                                                   broad_scene_x, broad_scene_y,
                                                   broad_scene_ids, 5, 0,
                                                   &canvas, camera_dirs_x,
                                                   camera_dirs_y, view_hits,
                                                   view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(view_hits[0].tile_x == 32);
    CHECK(view_hits[0].tile_y == 57);
    CHECK(view_hits[0].wall_page_index == 99);
    CHECK(sprites[0].source_index == scene_refs[115].source_index);
    CHECK(sprites[4].source_index == scene_refs[113].source_index);
    uint32_t live_closed_ref_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    uint16_t live_door_tile = model.tilemap[32 + 57 * WL_MAP_SIDE];
    model.tilemap[32 + 57 * WL_MAP_SIDE] = 0;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_runtime_door_camera_scene_view(&model,
                                                   dirinfo.header.sprite_start,
                                                   player_x, player_y,
                                                   0x10000, 0, 0, -0x8000,
                                                   30, 1, 5,
                                                   runtime_door_pages,
                                                   runtime_door_page_sizes, 106,
                                                   broad_scene_sprites,
                                                   broad_scene_x, broad_scene_y,
                                                   broad_scene_ids, 5, 0,
                                                   &canvas, camera_dirs_x,
                                                   camera_dirs_y, view_hits,
                                                   view_strips, sprites,
                                                   wall_heights) == 0);
    CHECK(view_hits[0].tile_x == 41);
    CHECK(view_hits[0].wall_page_index == 15);
    uint32_t live_open_ref_scene_hash = fnv1a_bytes(canvas.pixels, canvas.pixel_count);
    CHECK(live_closed_ref_scene_hash == 0x21495346);
    CHECK(live_open_ref_scene_hash == 0x2e4660d2);
    model.tilemap[32 + 57 * WL_MAP_SIDE] = live_door_tile;
    memset(canvas_pixels, 0x2a, sizeof(canvas_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, canvas_pixels, sizeof(canvas_pixels),
                                  &canvas) == 0);
    CHECK(wl_render_camera_scene_view(wall_plane, WL_MAP_PLANE_WORDS, player_x,
                                      player_y, 0x10000, -0x4000, 0, -0x8000,
                                      30, 1, 5, view_pages, view_page_sizes, 18,
                                      broad_scene_sprites, broad_scene_x,
                                      broad_scene_y, broad_scene_ids, 5, 0,
                                      &canvas, camera_dirs_x, camera_dirs_y,
                                      view_hits, view_strips, sprites,
                                      wall_heights) == 0);
    CHECK(sprites[0].source_index == scene_refs[115].source_index);
    CHECK(sprites[0].surface_index == 4);
    CHECK(sprites[0].view_x == 52);
    CHECK(sprites[0].scaled_height == 19);
    CHECK(sprites[0].distance == 0xb1c70);
    CHECK(sprites[1].source_index == scene_refs[111].source_index);
    CHECK(sprites[1].surface_index == 1);
    CHECK(sprites[1].view_x == 25);
    CHECK(sprites[1].scaled_height == 20);
    CHECK(sprites[1].distance == 0xa5c70);
    CHECK(sprites[2].source_index == scene_refs[114].source_index);
    CHECK(sprites[2].surface_index == 3);
    CHECK(sprites[2].view_x == 52);
    CHECK(sprites[2].scaled_height == 26);
    CHECK(sprites[2].distance == 0x81c70);
    CHECK(sprites[3].source_index == scene_refs[110].source_index);
    CHECK(sprites[3].surface_index == 0);
    CHECK(sprites[3].view_x == 8);
    CHECK(sprites[3].scaled_height == 33);
    CHECK(sprites[3].distance == 0x65c70);
    CHECK(sprites[4].source_index == scene_refs[113].source_index);
    CHECK(sprites[4].surface_index == 2);
    CHECK(sprites[4].view_x == 52);
    CHECK(sprites[4].scaled_height == 42);
    CHECK(sprites[4].distance == 0x51c70);
    CHECK(fnv1a_bytes(canvas.pixels, canvas.pixel_count) == 0x4668f191);
    CHECK(wl_project_world_sprite(106, player_x, player_y, player_x, player_y,
                                  0, 0, 80, 128, &sprites[0]) == -1);
    CHECK(wl_sort_projected_sprites_far_to_near(NULL, 0) == 0);
    CHECK(wl_sort_projected_sprites_far_to_near(NULL, 1) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 107, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 1556);
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[107].kind,
                                         &shape) == 0);
    CHECK(shape.leftpix == 1);
    CHECK(shape.rightpix == 62);
    CHECK(shape.visible_columns == 62);
    CHECK(shape.post_count == 85);
    CHECK(shape.terminal_count == 62);
    CHECK(shape.min_posts_per_column == 1);
    CHECK(shape.max_posts_per_column == 3);
    CHECK(shape.max_post_span == 36);
    CHECK(shape.max_post_start == 32);
    CHECK(shape.max_post_end == 36);
    CHECK(shape.min_source_offset == 113);
    CHECK(shape.max_source_offset == 904);
    CHECK(shape.total_post_span == 1586);
    CHECK(wl_decode_sprite_shape_surface(chunk_buf, chunk_bytes, 0, indexed_buf,
                                         sizeof(indexed_buf), &surface) == 0);
    CHECK(count_byte_not_value(surface.pixels, surface.pixel_count, 0) == 384);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x88a2d1b4);
    CHECK(wl_decode_sprite_shape_surface(chunk_buf, chunk_bytes, 0, indexed_buf,
                                         4095, &surface) == -1);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 542, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xaee73350);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 662, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 184);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xfba68c74);
    return 0;
}

static int check_optional_sod(const char *dir) {
    char sod_dir[1024];
    CHECK(wl_join_path(sod_dir, sizeof(sod_dir), dir, "m1") == 0);

    char maphead_path[1024];
    char gamemaps_path[1024];
    char vswap_path[1024];
    char vgahead_path[1024];
    char vgadict_path[1024];
    char vgagraph_path[1024];
    if (path(maphead_path, sizeof(maphead_path), sod_dir, "MAPHEAD.SOD") != 0 ||
        path(gamemaps_path, sizeof(gamemaps_path), sod_dir, "GAMEMAPS.SOD") != 0 ||
        path(vswap_path, sizeof(vswap_path), sod_dir, "VSWAP.SOD") != 0 ||
        path(vgahead_path, sizeof(vgahead_path), sod_dir, "VGAHEAD.SOD") != 0 ||
        path(vgadict_path, sizeof(vgadict_path), sod_dir, "VGADICT.SOD") != 0 ||
        path(vgagraph_path, sizeof(vgagraph_path), sod_dir, "VGAGRAPH.SOD") != 0) {
        return 0;
    }

    size_t sod_size = 0;
    if (wl_file_size(maphead_path, &sod_size) != 0) {
        printf("SOD data absent; skipping optional SOD assertions\n");
        return 0;
    }

    wl_maphead mh;
    CHECK(wl_read_maphead(maphead_path, &mh) == 0);
    CHECK(mh.file_size == 402);
    CHECK(mh.rlew_tag == 0xabcd);
    CHECK(mh.offsets[0] == 2097);

    wl_vswap_header vs;
    CHECK(wl_read_vswap_header(vswap_path, &vs) == 0);
    CHECK(vs.chunks_in_file == 666);
    CHECK(vs.sprite_start == 134);
    CHECK(vs.sound_start == 555);

    wl_map_header map0;
    CHECK(wl_read_map_header(gamemaps_path, mh.offsets[0], &map0) == 0);
    CHECK(strcmp(map0.name, "Tunnels 1") == 0);
    CHECK(map0.width == 64);
    CHECK(map0.height == 64);

    uint16_t sod_wall_plane[WL_MAP_PLANE_WORDS];
    uint16_t sod_info_plane[WL_MAP_PLANE_WORDS];
    wl_game_model sod_model;
    const struct {
        size_t map_index;
        const char *name;
        uint16_t player_x;
        uint16_t player_y;
        size_t door_count;
        size_t static_count;
        size_t actor_count;
        size_t kill_total;
        size_t treasure_total;
        size_t secret_total;
        size_t unknown_info_tiles;
        uint32_t unknown_info_hash;
        uint16_t first_unknown_tile;
        uint16_t first_unknown_x;
        uint16_t first_unknown_y;
        uint16_t last_unknown_tile;
        uint16_t last_unknown_x;
        uint16_t last_unknown_y;
        size_t scene_ref_count;
        uint32_t scene_ref_hash;
        uint16_t first_scene_source;
        uint16_t last_scene_source;
        size_t spear_pillar_count;
        size_t spear_truck_count;
    } sod_model_gaps[] = {
        { 0, "Tunnels 1", 32, 59, 17, 149, 8, 8, 45, 5, 0,
          0, 0, 0, 0, 0, 0, 0, 157, 0x44715246, 10, 50, 2, 0 },
        { 4, "Tunnel Boss", 50, 31, 18, 189, 13, 13, 42, 12, 0,
          0, 0, 0, 0, 0, 0, 0, 201, 0x1262857c, 16, 138, 15, 0 },
        { 17, "Death Knight", 30, 41, 9, 130, 11, 11, 2, 1, 0,
          0, 0, 0, 0, 0, 0, 0, 140, 0xac7a93d1, 28, 187, 38, 1 },
        { 20, "Angel of Death", 31, 22, 1, 263, 38, 38, 14, 5, 0,
          0, 0, 0, 0, 0, 0, 0, 263, 0x9b3f417e, 50, 35, 83, 0 },
    };
    for (size_t i = 0; i < sizeof(sod_model_gaps) / sizeof(sod_model_gaps[0]); ++i) {
        wl_map_header sod_map;
        CHECK(wl_read_map_header(gamemaps_path, mh.offsets[sod_model_gaps[i].map_index],
                                 &sod_map) == 0);
        CHECK(strcmp(sod_map.name, sod_model_gaps[i].name) == 0);
        CHECK(wl_read_map_plane(gamemaps_path, &sod_map, 0, mh.rlew_tag,
                                sod_wall_plane, WL_MAP_PLANE_WORDS) == 0);
        CHECK(wl_read_map_plane(gamemaps_path, &sod_map, 1, mh.rlew_tag,
                                sod_info_plane, WL_MAP_PLANE_WORDS) == 0);
        CHECK(wl_build_game_model(sod_wall_plane, sod_info_plane, WL_MAP_PLANE_WORDS,
                                  WL_DIFFICULTY_EASY, &sod_model) == 0);
        CHECK(sod_model.player.present == 1);
        CHECK(sod_model.player.x == sod_model_gaps[i].player_x);
        CHECK(sod_model.player.y == sod_model_gaps[i].player_y);
        CHECK(sod_model.door_count == sod_model_gaps[i].door_count);
        CHECK(sod_model.static_count == sod_model_gaps[i].static_count);
        CHECK(sod_model.actor_count == sod_model_gaps[i].actor_count);
        CHECK(sod_model.kill_total == sod_model_gaps[i].kill_total);
        CHECK(sod_model.treasure_total == sod_model_gaps[i].treasure_total);
        CHECK(sod_model.secret_total == sod_model_gaps[i].secret_total);
        CHECK(sod_model.unknown_info_tiles == sod_model_gaps[i].unknown_info_tiles);
        CHECK(sod_model.unknown_info_hash == sod_model_gaps[i].unknown_info_hash);
        CHECK(sod_model.first_unknown_info_tile == sod_model_gaps[i].first_unknown_tile);
        CHECK(sod_model.first_unknown_info_x == sod_model_gaps[i].first_unknown_x);
        CHECK(sod_model.first_unknown_info_y == sod_model_gaps[i].first_unknown_y);
        CHECK(sod_model.last_unknown_info_tile == sod_model_gaps[i].last_unknown_tile);
        CHECK(sod_model.last_unknown_info_x == sod_model_gaps[i].last_unknown_x);
        CHECK(sod_model.last_unknown_info_y == sod_model_gaps[i].last_unknown_y);

        wl_scene_sprite_ref sod_scene_refs[WL_MAX_STATS + WL_MAX_ACTORS];
        size_t sod_scene_ref_count = 0;
        CHECK(wl_collect_scene_sprite_refs(&sod_model, vs.sprite_start,
                                           sod_scene_refs,
                                           sizeof(sod_scene_refs) / sizeof(sod_scene_refs[0]),
                                           &sod_scene_ref_count) == 0);
        CHECK(sod_scene_ref_count == sod_model_gaps[i].scene_ref_count);
        CHECK(fnv1a_scene_sprite_refs(sod_scene_refs, sod_scene_ref_count) ==
              sod_model_gaps[i].scene_ref_hash);
        CHECK(sod_scene_refs[0].source_index == sod_model_gaps[i].first_scene_source);
        CHECK(sod_scene_refs[sod_scene_ref_count - 1].source_index ==
              sod_model_gaps[i].last_scene_source);
        size_t spear_pillars = 0;
        size_t spear_trucks = 0;
        for (size_t j = 0; j < sod_model.static_count; ++j) {
            if (sod_model.statics[j].type == 49) {
                ++spear_pillars;
            } else if (sod_model.statics[j].type == 51) {
                ++spear_trucks;
            }
        }
        CHECK(spear_pillars == sod_model_gaps[i].spear_pillar_count);
        CHECK(spear_trucks == sod_model_gaps[i].spear_truck_count);
    }

    unsigned unknown_tile_counts[512] = {0};
    size_t unknown_tile_total = 0;
    size_t unknown_tile_unique = 0;
    uint16_t single_unknown_wall[WL_MAP_PLANE_WORDS];
    uint16_t single_unknown_info[WL_MAP_PLANE_WORDS];
    for (size_t i = 0; i < WL_MAP_PLANE_WORDS; ++i) {
        single_unknown_wall[i] = WL_AREATILE;
        single_unknown_info[i] = 0;
    }
    for (size_t map_index = 0; map_index < 21; ++map_index) {
        wl_map_header sod_map;
        CHECK(wl_read_map_header(gamemaps_path, mh.offsets[map_index], &sod_map) == 0);
        CHECK(wl_read_map_plane(gamemaps_path, &sod_map, 1, mh.rlew_tag,
                                sod_info_plane, WL_MAP_PLANE_WORDS) == 0);
        for (size_t i = 0; i < WL_MAP_PLANE_WORDS; ++i) {
            uint16_t tile = sod_info_plane[i];
            if (tile == 0 || tile >= sizeof(unknown_tile_counts) / sizeof(unknown_tile_counts[0])) {
                continue;
            }
            single_unknown_info[WL_MAP_SIDE + 1] = tile;
            CHECK(wl_build_game_model(single_unknown_wall, single_unknown_info,
                                      WL_MAP_PLANE_WORDS, WL_DIFFICULTY_EASY,
                                      &sod_model) == 0);
            single_unknown_info[WL_MAP_SIDE + 1] = 0;
            if (sod_model.unknown_info_tiles != 0) {
                ++unknown_tile_counts[tile];
                ++unknown_tile_total;
            }
        }
    }
    for (size_t i = 0; i < sizeof(unknown_tile_counts) / sizeof(unknown_tile_counts[0]); ++i) {
        if (unknown_tile_counts[i] != 0) {
            ++unknown_tile_unique;
        }
    }
    CHECK(unknown_tile_total == 0);
    CHECK(unknown_tile_unique == 0);
    CHECK(unknown_tile_counts[72] == 0);
    CHECK(unknown_tile_counts[73] == 0);
    CHECK(unknown_tile_counts[74] == 0);
    CHECK(unknown_tile_counts[106] == 0);
    CHECK(unknown_tile_counts[107] == 0);
    CHECK(unknown_tile_counts[125] == 0);
    CHECK(unknown_tile_counts[142] == 0);
    CHECK(unknown_tile_counts[143] == 0);
    CHECK(unknown_tile_counts[161] == 0);

    wl_graphics_header gh;
    wl_huffman_node huff[WL_HUFFMAN_NODE_COUNT];
    unsigned char graphics_buf[65536];
    unsigned char indexed_buf[65536];
    unsigned char scaler_pixels[80 * 128];
    wl_indexed_surface surface;
    wl_indexed_surface scaler;
    size_t graphics_bytes = 0;
    size_t compressed_bytes = 0;
    CHECK(wl_read_graphics_header(vgahead_path, &gh) == 0);
    CHECK(gh.chunk_count == 169);
    CHECK(gh.file_size == 510);
    CHECK(gh.offsets[0] == 0);
    CHECK(gh.offsets[1] == 383);
    CHECK(gh.offsets[169] == 947979);
    CHECK(wl_read_huffman_dictionary(vgadict_path, huff) == 0);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 0, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 383);
    CHECK(graphics_bytes == 588);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x43f617ea);
    wl_picture_table_metadata pictures;
    CHECK(wl_decode_picture_table(graphics_buf, graphics_bytes, &pictures) == 0);
    CHECK(pictures.picture_count == 147);
    CHECK(pictures.min_width == 8);
    CHECK(pictures.max_width == 320);
    CHECK(pictures.min_height == 8);
    CHECK(pictures.max_height == 200);
    CHECK(pictures.total_pixels == 1105792);
    CHECK(pictures.pictures[0].width == 320);
    CHECK(pictures.pictures[0].height == 200);
    CHECK(pictures.pictures[1].width == 104);
    CHECK(pictures.pictures[1].height == 16);
    CHECK(pictures.pictures[84].width == 320);
    CHECK(pictures.pictures[84].height == 200);
    CHECK(pictures.pictures[90].width == 320);
    CHECK(pictures.pictures[90].height == 80);
    CHECK(pictures.pictures[91].width == 320);
    CHECK(pictures.pictures[91].height == 120);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 1, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 4448);
    CHECK(graphics_bytes == 8300);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xdb48ce2b);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 3, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 42248);
    CHECK(graphics_bytes == 64000);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0x3a6afac3);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[0].width,
                                           pictures.pictures[0].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.format == WL_SURFACE_INDEXED8);
    CHECK(surface.width == 320);
    CHECK(surface.height == 200);
    CHECK(surface.stride == 320);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(surface.pixels == indexed_buf);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x5e85d9c1);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 90, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 10561);
    CHECK(graphics_bytes == 12800);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xa5d5a6f7);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[87].width,
                                           pictures.pictures[87].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.width == 320);
    CHECK(surface.height == 40);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0xff61711d);
    CHECK(wl_read_graphics_chunk(vgagraph_path, &gh, huff, 149, graphics_buf,
                                 sizeof(graphics_buf), &graphics_bytes,
                                 &compressed_bytes) == 0);
    CHECK(compressed_bytes == 6243);
    CHECK(graphics_bytes == 10752);
    CHECK(fnv1a_bytes(graphics_buf, graphics_bytes) == 0xeb393cc0);
    CHECK(wl_decode_planar_picture_surface(graphics_buf, graphics_bytes,
                                           pictures.pictures[146].width,
                                           pictures.pictures[146].height,
                                           indexed_buf, sizeof(indexed_buf),
                                           &surface) == 0);
    CHECK(surface.width == 224);
    CHECK(surface.height == 48);
    CHECK(surface.pixel_count == graphics_bytes);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x46e4bd08);

    wl_vswap_directory dirinfo;
    CHECK(wl_read_vswap_directory(vswap_path, &dirinfo) == 0);
    CHECK(dirinfo.header.file_size == 1616544);
    CHECK(dirinfo.data_start == 4002);
    CHECK(dirinfo.max_chunk_end == 1616544);
    CHECK(dirinfo.wall_count == 134);
    CHECK(dirinfo.sprite_count == 421);
    CHECK(dirinfo.sound_count == 111);
    CHECK(dirinfo.sparse_count == 0);
    CHECK(dirinfo.chunks[133].offset == 548864);
    CHECK(dirinfo.chunks[133].length == 4096);
    CHECK(dirinfo.chunks[134].offset == 552960);
    CHECK(dirinfo.chunks[134].length == 1306);
    CHECK(dirinfo.chunks[134].kind == WL_VSWAP_CHUNK_SPRITE);
    CHECK(dirinfo.chunks[555].offset == 1233408);
    CHECK(dirinfo.chunks[555].length == 4096);
    CHECK(dirinfo.chunks[665].offset == 1616384);
    CHECK(dirinfo.chunks[665].length == 160);

    unsigned char chunk_buf[4096];
    unsigned char wall0_buf[4096];
    unsigned char wall105_buf[4096];
    unsigned char column_buf[64];
    unsigned char surface_column_buf[64];
    size_t chunk_bytes = 0;
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 0, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x98d020a5);
    memcpy(wall0_buf, chunk_buf, sizeof(wall0_buf));
    wl_vswap_shape_metadata shape;
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[0].kind,
                                         &shape) == 0);
    CHECK(shape.kind == WL_VSWAP_CHUNK_WALL);
    CHECK(shape.width == 64);
    CHECK(shape.height == 64);
    CHECK(shape.visible_columns == 64);
    wl_wall_page_metadata wallmeta;
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 7);
    CHECK(wallmeta.max_color == 31);
    CHECK(wallmeta.unique_color_count == 18);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(surface.width == 64);
    CHECK(surface.height == 64);
    CHECK(surface.pixel_count == 4096);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x8fe4d8ff);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0800, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x2fbb79bb);
    CHECK(wl_sample_indexed_surface_column(&surface, 32, surface_column_buf,
                                           sizeof(surface_column_buf)) == 0);
    CHECK(memcmp(column_buf, surface_column_buf, sizeof(column_buf)) == 0);
    memset(scaler_pixels, 0x2a, sizeof(scaler_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, scaler_pixels, sizeof(scaler_pixels),
                                  &scaler) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &scaler,
                                          7, 64) == 0);
    CHECK(fnv1a_bytes(scaler.pixels, scaler.pixel_count) == 0x78547277);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 105, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xbbbf5c67);
    memcpy(wall105_buf, chunk_buf, sizeof(wall105_buf));
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 0);
    CHECK(wallmeta.max_color == 237);
    CHECK(wallmeta.unique_color_count == 26);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x997d475d);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0000, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0xd61f9cbd);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0fc0, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(fnv1a_bytes(column_buf, sizeof(column_buf)) == 0x3e5f4efd);
    CHECK(wl_sample_indexed_surface_column(&surface, 63, surface_column_buf,
                                           sizeof(surface_column_buf)) == 0);
    CHECK(memcmp(column_buf, surface_column_buf, sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &scaler,
                                          29, 160) == 0);
    CHECK(wl_sample_wall_page_column(chunk_buf, chunk_bytes, 0x0000, column_buf,
                                     sizeof(column_buf)) == 0);
    CHECK(wl_scale_wall_column_to_surface(column_buf, sizeof(column_buf), &scaler,
                                          13, 96) == 0);
    CHECK(fnv1a_bytes(scaler.pixels, scaler.pixel_count) == 0x60ddb236);
    memset(scaler_pixels, 0x2a, sizeof(scaler_pixels));
    CHECK(wl_wrap_indexed_surface(80, 128, scaler_pixels, sizeof(scaler_pixels),
                                  &scaler) == 0);
    const wl_wall_strip sod_strips[] = {
        { wall0_buf, sizeof(wall0_buf), 0x0800, 7, 64 },
        { wall105_buf, sizeof(wall105_buf), 0x0fc0, 29, 160 },
        { wall105_buf, sizeof(wall105_buf), 0x0000, 13, 96 },
    };
    CHECK(wl_render_wall_strip_viewport(sod_strips, 3, &scaler) == 0);
    CHECK(fnv1a_bytes(scaler.pixels, scaler.pixel_count) == 0x60ddb236);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 133, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x33b7f33f);
    CHECK(wl_decode_wall_page_metadata(chunk_buf, chunk_bytes, &wallmeta) == 0);
    CHECK(wallmeta.min_color == 0);
    CHECK(wallmeta.max_color == 31);
    CHECK(wallmeta.unique_color_count == 11);
    CHECK(wl_decode_wall_page_surface(chunk_buf, chunk_bytes, indexed_buf,
                                      sizeof(indexed_buf), &surface) == 0);
    CHECK(fnv1a_bytes(surface.pixels, surface.pixel_count) == 0x66874cf5);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 134, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 1306);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xbf4fcd99);
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[134].kind,
                                         &shape) == 0);
    CHECK(shape.kind == WL_VSWAP_CHUNK_SPRITE);
    CHECK(shape.leftpix == 4);
    CHECK(shape.rightpix == 58);
    CHECK(shape.visible_columns == 55);
    CHECK(shape.first_column_offset == 800);
    CHECK(shape.last_column_offset == 1298);
    CHECK(shape.post_count == 66);
    CHECK(shape.terminal_count == 55);
    CHECK(shape.min_posts_per_column == 1);
    CHECK(shape.max_posts_per_column == 2);
    CHECK(shape.max_post_span == 40);
    CHECK(shape.min_source_offset == 108);
    CHECK(shape.max_source_offset == 782);
    CHECK(shape.total_post_span == 1372);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 135, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 1556);
    CHECK(wl_decode_vswap_shape_metadata(chunk_buf, chunk_bytes, dirinfo.chunks[135].kind,
                                         &shape) == 0);
    CHECK(shape.leftpix == 1);
    CHECK(shape.rightpix == 62);
    CHECK(shape.visible_columns == 62);
    CHECK(shape.post_count == 85);
    CHECK(shape.terminal_count == 62);
    CHECK(shape.max_posts_per_column == 3);
    CHECK(shape.max_post_span == 36);
    CHECK(shape.max_source_offset == 904);
    CHECK(shape.total_post_span == 1586);

    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 555, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 4096);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xaee73350);
    CHECK(wl_read_vswap_chunk(vswap_path, &dirinfo, 665, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 160);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xbb53ed59);
    return 0;
}

static int check_audio_wl6(const char *dir) {
    char audiohed_path[512];
    char audiot_path[512];
    wl_audio_header audio;
    unsigned char chunk_buf[65536];
    size_t chunk_bytes = 0;
    wl_audio_chunk_metadata audio_meta;
    wl_audio_range_summary audio_range;
    wl_sound_priority_decision priority_decision;
    wl_sound_channel_decision channel_decision;
    wl_sound_channel_state sound_channel;
    wl_sound_channel_start_result sound_start;
    wl_sound_channel_schedule_result sound_schedule;
    wl_sound_channel_advance_result sound_advance;
    wl_sound_channel_tick_result sound_tick;
    wl_sound_channel_schedule_tick_result sound_schedule_tick;
    wl_sound_channel_schedule_advance_result sound_schedule_advance;
    wl_sound_channel_schedule_window_result sound_schedule_window;
    wl_sound_channel_schedule_position_result sound_schedule_position;
    wl_sound_channel_schedule_progress_result sound_schedule_progress;
    wl_sound_channel_schedule_progress_window_result sound_schedule_progress_window;
    wl_sound_channel_schedule_tick_progress_window_result sound_schedule_tick_progress_window;
    wl_pc_speaker_sound_metadata pc_meta;
    wl_pc_speaker_playback_cursor pc_cursor;
    wl_adlib_sound_metadata adlib_meta;
    wl_adlib_instrument_registers adlib_regs;
    wl_adlib_playback_cursor adlib_cursor;
    wl_sample_playback_window sample_window;
    wl_sample_playback_position sample_position;
    wl_sound_channel_progress sound_progress;
    wl_sound_channel_progress_window sound_progress_window;
    size_t sound_sample_count = 0;
    uint8_t pc_sample = 0;
    uint8_t adlib_instrument_byte = 0;
    uint8_t adlib_sample = 0;
    wl_imf_music_metadata imf_meta;
    wl_imf_music_command imf_command;
    wl_imf_playback_window imf_window;
    wl_imf_playback_position imf_position;
    wl_imf_playback_cursor imf_cursor;

    CHECK(wl_join_path(audiohed_path, sizeof(audiohed_path), dir, "AUDIOHED.WL6") == 0);
    CHECK(wl_join_path(audiot_path, sizeof(audiot_path), dir, "AUDIOT.WL6") == 0);
    CHECK(wl_read_audio_header(audiohed_path, &audio) == 0);

    /* AUDIOHED.WL6 is 1156 bytes = 289 uint32 offsets = 288 chunks + 1 sentinel */
    CHECK(audio.file_size == 1156);
    CHECK(audio.chunk_count == 288);

    /* First offsets from the file: 0, 15, 28, 44, 102 */
    CHECK(audio.offsets[0] == 0);
    CHECK(audio.offsets[1] == 15);
    CHECK(audio.offsets[2] == 28);
    CHECK(audio.offsets[3] == 44);
    CHECK(audio.offsets[4] == 102);

    CHECK(wl_summarize_audio_range(&audio, 0, 87, &audio_range) == 0);
    CHECK(audio_range.start_chunk == 0);
    CHECK(audio_range.chunk_count == 87);
    CHECK(audio_range.non_empty_chunks == 87);
    CHECK(audio_range.total_bytes == 9986);
    CHECK(audio_range.first_non_empty_chunk == 0);
    CHECK(audio_range.last_non_empty_chunk == 86);
    CHECK(audio_range.largest_chunk == 50);
    CHECK(audio_range.largest_chunk_bytes == 313);
    CHECK(wl_summarize_audio_range(&audio, 87, 87, &audio_range) == 0);
    CHECK(audio_range.start_chunk == 87);
    CHECK(audio_range.chunk_count == 87);
    CHECK(audio_range.non_empty_chunks == 87);
    CHECK(audio_range.total_bytes == 12969);
    CHECK(audio_range.first_non_empty_chunk == 87);
    CHECK(audio_range.last_non_empty_chunk == 173);
    CHECK(audio_range.largest_chunk == 137);
    CHECK(audio_range.largest_chunk_bytes == 339);
    CHECK(wl_summarize_audio_range(&audio, 174, 87, &audio_range) == 0);
    CHECK(audio_range.start_chunk == 174);
    CHECK(audio_range.chunk_count == 87);
    CHECK(audio_range.non_empty_chunks == 1);
    CHECK(audio_range.total_bytes == 4);
    CHECK(audio_range.first_non_empty_chunk == 260);
    CHECK(audio_range.last_non_empty_chunk == 260);
    CHECK(audio_range.largest_chunk == 260);
    CHECK(audio_range.largest_chunk_bytes == 4);
    CHECK(wl_summarize_audio_range(&audio, 261, 27, &audio_range) == 0);
    CHECK(audio_range.start_chunk == 261);
    CHECK(audio_range.chunk_count == 27);
    CHECK(audio_range.non_empty_chunks == 27);
    CHECK(audio_range.total_bytes == 297250);
    CHECK(audio_range.first_non_empty_chunk == 261);
    CHECK(audio_range.last_non_empty_chunk == 287);
    CHECK(audio_range.largest_chunk == 280);
    CHECK(audio_range.largest_chunk_bytes == 22578);
    CHECK(wl_summarize_audio_range(&audio, 288, 1, &audio_range) == -1);
    CHECK(wl_describe_sound_priority_decision(0, 99, 1, &priority_decision) == 0);
    CHECK(priority_decision.current_active == 0);
    CHECK(priority_decision.current_priority == 99);
    CHECK(priority_decision.candidate_priority == 1);
    CHECK(priority_decision.should_start == 1);
    CHECK(wl_describe_sound_priority_decision(1, 4, 3, &priority_decision) == 0);
    CHECK(priority_decision.should_start == 0);
    CHECK(wl_describe_sound_priority_decision(1, 4, 4, &priority_decision) == 0);
    CHECK(priority_decision.should_start == 1);
    CHECK(wl_describe_sound_priority_decision(1, 4, 5, &priority_decision) == 0);
    CHECK(priority_decision.should_start == 1);
    CHECK(wl_describe_sound_priority_decision(2, 4, 5, &priority_decision) == -1);
    CHECK(wl_describe_sound_priority_decision(1, 4, 5, NULL) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 12;
    sound_channel.priority = 99;
    sound_channel.sample_position = 5;
    CHECK(wl_start_sound_channel(&sound_channel, 1, 1, &sound_start) == 0);
    CHECK(sound_start.started == 1);
    CHECK(sound_start.replaced == 0);
    CHECK(sound_start.state.active == 1);
    CHECK(sound_start.state.sound_index == 1);
    CHECK(sound_start.state.priority == 1);
    CHECK(sound_start.state.sample_position == 0);
    sound_channel = sound_start.state;
    sound_channel.sample_position = 7;
    CHECK(wl_start_sound_channel(&sound_channel, 2, 0, &sound_start) == 0);
    CHECK(sound_start.started == 0);
    CHECK(sound_start.replaced == 0);
    CHECK(sound_start.state.active == 1);
    CHECK(sound_start.state.sound_index == 1);
    CHECK(sound_start.state.priority == 1);
    CHECK(sound_start.state.sample_position == 7);

    CHECK(wl_start_sound_channel(&sound_channel, 3, 1, &sound_start) == 0);
    CHECK(sound_start.started == 1);
    CHECK(sound_start.replaced == 1);
    CHECK(sound_start.state.active == 1);
    CHECK(sound_start.state.sound_index == 3);
    CHECK(sound_start.state.priority == 1);
    CHECK(sound_start.state.sample_position == 0);
    CHECK(wl_start_sound_channel(NULL, 4, 5, &sound_start) == -1);
    CHECK(wl_start_sound_channel(&sound_channel, 4, 5, NULL) == -1);
    sound_channel.active = 2;
    CHECK(wl_start_sound_channel(&sound_channel, 4, 5, &sound_start) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 11;
    sound_channel.priority = 3;
    sound_channel.sample_position = 4;
    CHECK(wl_advance_sound_channel(&sound_channel, 8, 2, &sound_advance) == 0);
    CHECK(sound_advance.completed == 0);
    CHECK(sound_advance.samples_consumed == 0);
    CHECK(sound_advance.state.active == 0);
    CHECK(sound_advance.state.sample_position == 4);

    sound_channel.active = 1;
    sound_channel.sample_position = 2;
    CHECK(wl_advance_sound_channel(&sound_channel, 8, 3, &sound_advance) == 0);
    CHECK(sound_advance.completed == 0);
    CHECK(sound_advance.samples_consumed == 3);
    CHECK(sound_advance.state.active == 1);
    CHECK(sound_advance.state.sound_index == 11);
    CHECK(sound_advance.state.priority == 3);
    CHECK(sound_advance.state.sample_position == 5);
    CHECK(wl_advance_sound_channel(&sound_advance.state, 8, 10, &sound_advance) == 0);
    CHECK(sound_advance.completed == 1);
    CHECK(sound_advance.samples_consumed == 3);
    CHECK(sound_advance.state.active == 0);
    CHECK(sound_advance.state.sample_position == 8);
    sound_channel.sample_position = 9;
    CHECK(wl_advance_sound_channel(&sound_channel, 8, 1, &sound_advance) == -1);
    sound_channel.sample_position = 8;
    CHECK(wl_advance_sound_channel(&sound_channel, 8, 0, &sound_advance) == 0);
    CHECK(sound_advance.completed == 1);
    CHECK(sound_advance.samples_consumed == 0);
    CHECK(sound_advance.state.active == 0);
    sound_channel.active = 2;
    CHECK(wl_advance_sound_channel(&sound_channel, 8, 1, &sound_advance) == -1);
    CHECK(wl_advance_sound_channel(NULL, 8, 1, &sound_advance) == -1);
    CHECK(wl_advance_sound_channel(&sound_channel, 8, 1, NULL) == -1);

    /* Offsets must be non-decreasing */
    for (size_t i = 1; i <= audio.chunk_count; ++i) {
        CHECK(audio.offsets[i] >= audio.offsets[i - 1]);
    }

    /* Last offset (sentinel) should be <= AUDIOT file size */
    size_t audiot_size = 0;
    CHECK(wl_file_size(audiot_path, &audiot_size) == 0);
    CHECK(audiot_size == 320209);
    CHECK(audio.offsets[audio.chunk_count] <= audiot_size);

    /* Read a few representative chunks and hash them */

    /* PC speaker sound 0 (HITWALLSND) */
    CHECK(wl_read_audio_chunk(audiot_path, &audio, 0, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 15);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x5971ec53);
    CHECK(wl_describe_audio_chunk(0, chunk_buf, chunk_bytes, &audio_meta) == 0);
    CHECK(audio_meta.kind == WL_AUDIO_CHUNK_PC_SPEAKER);
    CHECK(audio_meta.is_empty == 0);
    CHECK(audio_meta.declared_length == 8);
    CHECK(audio_meta.priority == 1);
    CHECK(audio_meta.payload_offset == 6);
    CHECK(audio_meta.payload_size == 9);
    CHECK(wl_describe_sound_channel_decision(0, 50, 99, 0, &audio_meta,
                                             &channel_decision) == 0);
    CHECK(channel_decision.current_active == 0);
    CHECK(channel_decision.current_chunk == 50);
    CHECK(channel_decision.current_priority == 99);
    CHECK(channel_decision.candidate_chunk == 0);
    CHECK(channel_decision.candidate_kind == WL_AUDIO_CHUNK_PC_SPEAKER);
    CHECK(channel_decision.candidate_priority == 1);
    CHECK(channel_decision.should_start == 1);
    CHECK(channel_decision.next_active == 1);
    CHECK(channel_decision.next_chunk == 0);
    CHECK(channel_decision.next_priority == 1);
    memset(&sound_channel, 0, sizeof(sound_channel));
    CHECK(wl_start_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                            &sound_start) == 0);
    CHECK(sound_start.started == 1);
    CHECK(sound_start.replaced == 0);
    CHECK(sound_start.state.sound_index == 0);
    CHECK(sound_start.state.priority == 1);
    CHECK(sound_start.state.sample_position == 0);

    CHECK(wl_describe_sound_channel_decision(1, 50, 2, 0, &audio_meta,
                                             &channel_decision) == 0);
    CHECK(channel_decision.should_start == 0);
    CHECK(channel_decision.next_active == 1);
    CHECK(channel_decision.next_chunk == 50);
    CHECK(channel_decision.next_priority == 2);
    CHECK(wl_describe_sound_channel_decision(2, 50, 2, 0, &audio_meta,
                                             &channel_decision) == -1);
    CHECK(wl_describe_sound_channel_decision(1, 50, 2, 0, NULL,
                                             &channel_decision) == -1);
    CHECK(wl_describe_sound_channel_decision(1, 50, 2, 0, &audio_meta,
                                             NULL) == -1);
    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 50;
    sound_channel.priority = 99;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_sound_channel(&sound_channel, 0, &audio_meta, &sound_schedule) == 0);
    CHECK(sound_schedule.started == 1);
    CHECK(sound_schedule.replaced == 0);
    CHECK(sound_schedule.held == 0);
    CHECK(sound_schedule.candidate_kind == WL_AUDIO_CHUNK_PC_SPEAKER);
    CHECK(sound_schedule.candidate_priority == 1);
    CHECK(sound_schedule.state.active == 1);
    CHECK(sound_schedule.state.sound_index == 0);
    CHECK(sound_schedule.state.priority == 1);
    CHECK(sound_schedule.state.sample_position == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 2;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_sound_channel(&sound_channel, 0, &audio_meta, &sound_schedule) == 0);
    CHECK(sound_schedule.started == 0);
    CHECK(sound_schedule.replaced == 0);
    CHECK(sound_schedule.held == 1);
    CHECK(sound_schedule.state.active == 1);
    CHECK(sound_schedule.state.sound_index == 50);
    CHECK(sound_schedule.state.priority == 2);
    CHECK(sound_schedule.state.sample_position == 4);
    CHECK(wl_schedule_sound_channel(NULL, 0, &audio_meta, &sound_schedule) == -1);
    CHECK(wl_schedule_sound_channel(&sound_channel, 0, NULL, &sound_schedule) == -1);
    CHECK(wl_schedule_sound_channel(&sound_channel, 0, &audio_meta, NULL) == -1);
    sound_channel.active = 2;
    CHECK(wl_schedule_sound_channel(&sound_channel, 0, &audio_meta, &sound_schedule) == -1);
    CHECK(wl_describe_pc_speaker_sound(chunk_buf, chunk_bytes, &pc_meta) == 0);
    CHECK(pc_meta.sample_count == 8);
    CHECK(wl_describe_sound_sample_count_from_chunk(&audio_meta, chunk_buf,
                                                    chunk_bytes,
                                                    &sound_sample_count) == 0);
    CHECK(sound_sample_count == 8);
    CHECK(wl_describe_sound_sample_count_from_chunk(&audio_meta, chunk_buf, 6,
                                                    &sound_sample_count) == -1);
    CHECK(wl_describe_sound_sample_count_from_chunk(NULL, chunk_buf,
                                                    chunk_bytes,
                                                    &sound_sample_count) == -1);
    CHECK(pc_meta.first_sample == 0x83);
    CHECK(pc_meta.last_sample == 0x84);
    CHECK(pc_meta.terminator == 0);
    CHECK(pc_meta.trailing_bytes == 0);
    CHECK(wl_get_pc_speaker_sound_sample(chunk_buf, chunk_bytes, 0, &pc_sample) == 0);
    CHECK(pc_sample == 0x83);
    CHECK(wl_get_pc_speaker_sound_sample(chunk_buf, chunk_bytes, 7, &pc_sample) == 0);
    CHECK(pc_sample == 0x84);
    CHECK(wl_get_pc_speaker_sound_sample(chunk_buf, chunk_bytes, 8, &pc_sample) == -1);
    CHECK(wl_advance_pc_speaker_playback_cursor(chunk_buf, chunk_bytes, 0, 0, &pc_cursor) == 0);
    CHECK(pc_cursor.sample_index == 0);
    CHECK(pc_cursor.current_sample == 0x83);
    CHECK(pc_cursor.samples_consumed == 0);
    CHECK(pc_cursor.completed == 0);
    CHECK(wl_advance_pc_speaker_playback_cursor(chunk_buf, chunk_bytes, 6, 1, &pc_cursor) == 0);
    CHECK(pc_cursor.sample_index == 7);
    CHECK(pc_cursor.current_sample == 0x84);
    CHECK(pc_cursor.samples_consumed == 1);
    CHECK(pc_cursor.completed == 0);
    CHECK(wl_advance_pc_speaker_playback_cursor(chunk_buf, chunk_bytes, 6, 9, &pc_cursor) == 0);
    CHECK(pc_cursor.sample_index == 8);
    CHECK(pc_cursor.samples_consumed == 2);
    CHECK(pc_cursor.completed == 1);
    CHECK(wl_advance_pc_speaker_playback_cursor(chunk_buf, chunk_bytes, 8, 0, &pc_cursor) == 0);
    CHECK(pc_cursor.completed == 1);
    CHECK(wl_advance_pc_speaker_playback_cursor(chunk_buf, chunk_bytes, 8, 1, &pc_cursor) == -1);
    CHECK(wl_describe_pc_speaker_playback_window(chunk_buf, chunk_bytes, 2, 3,
                                                 &sample_window) == 0);
    CHECK(sample_window.start_sample == 2);
    CHECK(sample_window.samples_available == 6);
    CHECK(sample_window.samples_in_window == 3);
    CHECK(sample_window.next_sample == 5);
    CHECK(sample_window.first_sample == 0x82);
    CHECK(sample_window.last_sample == 0x8a);
    CHECK(sample_window.completed == 0);
    CHECK(wl_describe_pc_speaker_playback_window(chunk_buf, chunk_bytes, 7, 4,
                                                 &sample_window) == 0);
    CHECK(sample_window.samples_in_window == 1);
    CHECK(sample_window.next_sample == 8);
    CHECK(sample_window.completed == 1);
    CHECK(wl_describe_pc_speaker_playback_window(chunk_buf, chunk_bytes, 9, 1,
                                                 &sample_window) == -1);
    CHECK(wl_describe_sound_playback_window_from_chunk(&audio_meta, chunk_buf,
                                                       chunk_bytes, 2, 3,
                                                       &sample_window) == 0);
    CHECK(sample_window.start_sample == 2);
    CHECK(sample_window.samples_in_window == 3);
    CHECK(sample_window.next_sample == 5);
    CHECK(sample_window.first_sample == 0x82);
    CHECK(sample_window.last_sample == 0x8a);
    audio_meta.kind = WL_AUDIO_CHUNK_MUSIC;
    CHECK(wl_describe_sound_playback_window_from_chunk(&audio_meta, chunk_buf,
                                                       chunk_bytes, 2, 3,
                                                       &sample_window) == -1);
    audio_meta.kind = WL_AUDIO_CHUNK_PC_SPEAKER;
    CHECK(wl_describe_pc_speaker_playback_position(chunk_buf, chunk_bytes, 0,
                                                   &sample_position) == 0);
    CHECK(sample_position.sample_position == 0);
    CHECK(sample_position.sample_count == 8);
    CHECK(sample_position.current_sample == 0x83);
    CHECK(sample_position.completed == 0);
    CHECK(wl_describe_pc_speaker_playback_position(chunk_buf, chunk_bytes, 7,
                                                   &sample_position) == 0);
    CHECK(sample_position.current_sample == 0x84);
    CHECK(sample_position.completed == 0);
    CHECK(wl_describe_pc_speaker_playback_position(chunk_buf, chunk_bytes, 8,
                                                   &sample_position) == 0);
    CHECK(sample_position.sample_position == 8);
    CHECK(sample_position.completed == 1);
    CHECK(wl_describe_pc_speaker_playback_position(chunk_buf, chunk_bytes, 9,
                                                   &sample_position) == -1);
    CHECK(wl_describe_sound_playback_position_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                         7, &sample_position) == 0);
    CHECK(sample_position.sample_count == 8);
    CHECK(sample_position.current_sample == 0x84);
    CHECK(sample_position.completed == 0);
    CHECK(wl_describe_sound_playback_position_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                         8, &sample_position) == 0);
    CHECK(sample_position.completed == 1);
    CHECK(wl_describe_sound_playback_position_from_chunk(&audio_meta, chunk_buf, 6,
                                                         0, &sample_position) == -1);

    sound_channel.active = 1;
    sound_channel.sound_index = 0;
    sound_channel.priority = 1;
    sound_channel.sample_position = 7;
    CHECK(wl_describe_sound_channel_position_from_chunk(&sound_channel, &audio_meta,
                                                        chunk_buf, chunk_bytes,
                                                        &sample_position) == 0);
    CHECK(sample_position.sample_position == 7);
    CHECK(sample_position.current_sample == 0x84);
    CHECK(sample_position.completed == 0);
    CHECK(wl_describe_sound_channel_remaining_from_chunk(&sound_channel, &audio_meta,
                                                         chunk_buf, chunk_bytes,
                                                         &sound_sample_count) == 0);
    CHECK(sound_sample_count == 1);
    CHECK(wl_describe_sound_channel_window_from_chunk(&sound_channel, &audio_meta,
                                                      chunk_buf, chunk_bytes, 4,
                                                      &sample_window) == 0);
    CHECK(sample_window.start_sample == 7);
    CHECK(sample_window.samples_available == 1);
    CHECK(sample_window.samples_in_window == 1);
    CHECK(sample_window.next_sample == 8);
    CHECK(sample_window.first_sample == 0x84);
    CHECK(sample_window.last_sample == 0x84);
    CHECK(sample_window.completed == 1);
    sound_channel.active = 0;
    CHECK(wl_describe_sound_channel_position_from_chunk(&sound_channel, &audio_meta,
                                                        chunk_buf, chunk_bytes,
                                                        &sample_position) == -1);
    CHECK(wl_describe_sound_channel_remaining_from_chunk(&sound_channel, &audio_meta,
                                                         chunk_buf, chunk_bytes,
                                                         &sound_sample_count) == -1);
    CHECK(wl_describe_sound_channel_window_from_chunk(&sound_channel, &audio_meta,
                                                      chunk_buf, chunk_bytes, 1,
                                                      &sample_window) == -1);

    sound_channel.active = 1;
    sound_channel.sound_index = 0;
    sound_channel.priority = 1;
    sound_channel.sample_position = 6;
    CHECK(wl_advance_sound_channel_from_chunk(&sound_channel, &audio_meta,
                                              chunk_buf, chunk_bytes, 3,
                                              &sound_advance) == 0);
    CHECK(sound_advance.completed == 1);
    CHECK(sound_advance.samples_consumed == 2);
    CHECK(sound_advance.state.active == 0);
    CHECK(sound_advance.state.sample_position == 8);
    CHECK(wl_advance_sound_channel_from_chunk(&sound_channel, &audio_meta,
                                              chunk_buf, 6, 1,
                                              &sound_advance) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 1;
    sound_channel.sound_index = 0;
    sound_channel.priority = 1;
    sound_channel.sample_position = 6;
    CHECK(wl_tick_sound_channel(&sound_channel, WL_AUDIO_CHUNK_PC_SPEAKER,
                                chunk_buf, chunk_bytes, 1, &sound_tick) == 0);
    CHECK(sound_tick.state.active == 1);
    CHECK(sound_tick.state.sound_index == 0);
    CHECK(sound_tick.state.priority == 1);
    CHECK(sound_tick.state.sample_position == 7);
    CHECK(sound_tick.samples_consumed == 1);
    CHECK(sound_tick.current_sample == 0x84);
    CHECK(sound_tick.completed == 0);
    CHECK(wl_tick_sound_channel(&sound_tick.state, WL_AUDIO_CHUNK_PC_SPEAKER,
                                chunk_buf, chunk_bytes, 9, &sound_tick) == 0);
    CHECK(sound_tick.state.active == 0);
    CHECK(sound_tick.state.sample_position == 8);
    CHECK(sound_tick.samples_consumed == 1);
    CHECK(sound_tick.completed == 1);
    sound_channel.active = 1;
    sound_channel.sound_index = 0;
    sound_channel.priority = 1;
    sound_channel.sample_position = 6;
    CHECK(wl_tick_sound_channel_from_chunk(&sound_channel, &audio_meta,
                                           chunk_buf, chunk_bytes, 2,
                                           &sound_tick) == 0);
    CHECK(sound_tick.state.active == 0);
    CHECK(sound_tick.state.sample_position == 8);
    CHECK(sound_tick.samples_consumed == 2);
    CHECK(sound_tick.completed == 1);
    CHECK(wl_tick_sound_channel_from_chunk(&sound_channel, NULL,
                                           chunk_buf, chunk_bytes, 1,
                                           &sound_tick) == -1);
    audio_meta.is_empty = 1;
    CHECK(wl_tick_sound_channel_from_chunk(&sound_channel, &audio_meta,
                                           chunk_buf, chunk_bytes, 1,
                                           &sound_tick) == -1);
    audio_meta.is_empty = 0;
    audio_meta.kind = WL_AUDIO_CHUNK_MUSIC;
    CHECK(wl_tick_sound_channel_from_chunk(&sound_channel, &audio_meta,
                                           chunk_buf, chunk_bytes, 1,
                                           &sound_tick) == -1);
    audio_meta.kind = WL_AUDIO_CHUNK_PC_SPEAKER;
    memset(&sound_channel, 0, sizeof(sound_channel));
    CHECK(wl_tick_sound_channel(&sound_channel, WL_AUDIO_CHUNK_PC_SPEAKER,
                                chunk_buf, chunk_bytes, 5, &sound_tick) == 0);
    CHECK(sound_tick.state.active == 0);
    CHECK(sound_tick.samples_consumed == 0);
    sound_channel.active = 2;
    CHECK(wl_tick_sound_channel(&sound_channel, WL_AUDIO_CHUNK_PC_SPEAKER,
                                chunk_buf, chunk_bytes, 1, &sound_tick) == -1);
    sound_channel.active = 1;
    CHECK(wl_tick_sound_channel(&sound_channel, WL_AUDIO_CHUNK_MUSIC,
                                chunk_buf, chunk_bytes, 1, &sound_tick) == -1);
    CHECK(wl_tick_sound_channel(&sound_channel, WL_AUDIO_CHUNK_PC_SPEAKER,
                                chunk_buf, 6, 1, &sound_tick) == -1);
    CHECK(wl_tick_sound_channel(&sound_channel, WL_AUDIO_CHUNK_PC_SPEAKER,
                                chunk_buf, chunk_bytes, 1, NULL) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 50;
    sound_channel.priority = 99;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_tick_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                                    chunk_buf, chunk_bytes, 2,
                                                    &sound_schedule_tick) == 0);
    CHECK(sound_schedule_tick.schedule.started == 1);
    CHECK(sound_schedule_tick.schedule.replaced == 0);
    CHECK(sound_schedule_tick.schedule.held == 0);
    CHECK(sound_schedule_tick.ticked == 1);
    CHECK(sound_schedule_tick.tick.state.active == 1);
    CHECK(sound_schedule_tick.tick.state.sound_index == 0);
    CHECK(sound_schedule_tick.tick.state.priority == 1);
    CHECK(sound_schedule_tick.tick.state.sample_position == 2);
    CHECK(sound_schedule_tick.tick.samples_consumed == 2);
    CHECK(sound_schedule_tick.tick.current_sample == 0x82);
    CHECK(sound_schedule_tick.tick.completed == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 2;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_tick_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                                    chunk_buf, chunk_bytes, 2,
                                                    &sound_schedule_tick) == 0);
    CHECK(sound_schedule_tick.schedule.started == 0);
    CHECK(sound_schedule_tick.schedule.held == 1);
    CHECK(sound_schedule_tick.ticked == 0);
    CHECK(sound_schedule_tick.tick.state.active == 1);
    CHECK(sound_schedule_tick.tick.state.sound_index == 50);
    CHECK(sound_schedule_tick.tick.state.priority == 2);
    CHECK(sound_schedule_tick.tick.state.sample_position == 4);
    CHECK(wl_schedule_tick_sound_channel_from_chunk(&sound_channel, 70000, &audio_meta,
                                                    chunk_buf, chunk_bytes, 1,
                                                    &sound_schedule_tick) == -1);
    CHECK(wl_schedule_tick_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                                    chunk_buf, 6, 1,
                                                    &sound_schedule_tick) == -1);
    CHECK(wl_schedule_tick_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                                    chunk_buf, chunk_bytes, 1,
                                                    NULL) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 50;
    sound_channel.priority = 99;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_advance_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                                       chunk_buf, chunk_bytes, 3,
                                                       &sound_schedule_advance) == 0);
    CHECK(sound_schedule_advance.schedule.started == 1);
    CHECK(sound_schedule_advance.schedule.held == 0);
    CHECK(sound_schedule_advance.advanced == 1);
    CHECK(sound_schedule_advance.advance.state.active == 1);
    CHECK(sound_schedule_advance.advance.state.sound_index == 0);
    CHECK(sound_schedule_advance.advance.state.priority == 1);
    CHECK(sound_schedule_advance.advance.state.sample_position == 3);
    CHECK(sound_schedule_advance.advance.samples_consumed == 3);
    CHECK(sound_schedule_advance.advance.completed == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 2;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_advance_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                                       chunk_buf, chunk_bytes, 3,
                                                       &sound_schedule_advance) == 0);
    CHECK(sound_schedule_advance.schedule.started == 0);
    CHECK(sound_schedule_advance.schedule.held == 1);
    CHECK(sound_schedule_advance.advanced == 0);
    CHECK(sound_schedule_advance.advance.state.active == 1);
    CHECK(sound_schedule_advance.advance.state.sound_index == 50);
    CHECK(sound_schedule_advance.advance.state.priority == 2);
    CHECK(sound_schedule_advance.advance.state.sample_position == 4);
    CHECK(wl_schedule_advance_sound_channel_from_chunk(&sound_channel, 70000, &audio_meta,
                                                       chunk_buf, chunk_bytes, 1,
                                                       &sound_schedule_advance) == -1);
    CHECK(wl_schedule_advance_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                                       chunk_buf, 6, 1,
                                                       &sound_schedule_advance) == -1);
    CHECK(wl_schedule_advance_sound_channel_from_chunk(&sound_channel, 0, &audio_meta,
                                                       chunk_buf, chunk_bytes, 1,
                                                       NULL) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 50;
    sound_channel.priority = 99;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_describe_sound_channel_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 3,
              &sound_schedule_window) == 0);
    CHECK(sound_schedule_window.schedule.started == 1);
    CHECK(sound_schedule_window.schedule.held == 0);
    CHECK(sound_schedule_window.described == 1);
    CHECK(sound_schedule_window.window.start_sample == 0);
    CHECK(sound_schedule_window.window.samples_in_window == 3);
    CHECK(sound_schedule_window.window.next_sample == 3);
    CHECK(sound_schedule_window.window.first_sample == 0x83);
    CHECK(sound_schedule_window.window.last_sample == 0x82);
    CHECK(sound_schedule_window.window.completed == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 2;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_describe_sound_channel_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 3,
              &sound_schedule_window) == 0);
    CHECK(sound_schedule_window.schedule.started == 0);
    CHECK(sound_schedule_window.schedule.held == 1);
    CHECK(sound_schedule_window.described == 0);
    CHECK(sound_schedule_window.schedule.state.sound_index == 50);
    CHECK(sound_schedule_window.schedule.state.sample_position == 4);
    CHECK(wl_schedule_describe_sound_channel_window_from_chunk(
              &sound_channel, 70000, &audio_meta, chunk_buf, chunk_bytes, 1,
              &sound_schedule_window) == -1);
    CHECK(wl_schedule_describe_sound_channel_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, 6, 1,
              &sound_schedule_window) == -1);
    CHECK(wl_schedule_describe_sound_channel_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 1,
              NULL) == -1);
    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 50;
    sound_channel.priority = 99;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_describe_sound_channel_position_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes,
              &sound_schedule_position) == 0);
    CHECK(sound_schedule_position.schedule.started == 1);
    CHECK(sound_schedule_position.schedule.held == 0);
    CHECK(sound_schedule_position.described == 1);
    CHECK(sound_schedule_position.position.sample_position == 0);
    CHECK(sound_schedule_position.position.sample_count == 8);
    CHECK(sound_schedule_position.position.current_sample == 0x83);
    CHECK(sound_schedule_position.position.completed == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 2;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_describe_sound_channel_position_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes,
              &sound_schedule_position) == 0);
    CHECK(sound_schedule_position.schedule.started == 0);
    CHECK(sound_schedule_position.schedule.held == 1);
    CHECK(sound_schedule_position.described == 0);
    CHECK(sound_schedule_position.schedule.state.sound_index == 50);
    CHECK(sound_schedule_position.schedule.state.sample_position == 4);
    CHECK(wl_schedule_describe_sound_channel_position_from_chunk(
              &sound_channel, 70000, &audio_meta, chunk_buf, chunk_bytes,
              &sound_schedule_position) == -1);
    CHECK(wl_schedule_describe_sound_channel_position_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, 6,
              &sound_schedule_position) == -1);
    CHECK(wl_schedule_describe_sound_channel_position_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes,
              NULL) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 50;
    sound_channel.priority = 99;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_describe_sound_channel_progress_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes,
              &sound_schedule_progress) == 0);
    CHECK(sound_schedule_progress.schedule.started == 1);
    CHECK(sound_schedule_progress.schedule.held == 0);
    CHECK(sound_schedule_progress.described == 1);
    CHECK(sound_schedule_progress.progress.sound_index == 0);
    CHECK(sound_schedule_progress.progress.priority == 1);
    CHECK(sound_schedule_progress.progress.kind == WL_AUDIO_CHUNK_PC_SPEAKER);
    CHECK(sound_schedule_progress.progress.sample_position == 0);
    CHECK(sound_schedule_progress.progress.sample_count == 8);
    CHECK(sound_schedule_progress.progress.remaining_samples == 8);
    CHECK(sound_schedule_progress.progress.current_sample == 0x83);
    CHECK(sound_schedule_progress.progress.completed == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 2;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_describe_sound_channel_progress_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes,
              &sound_schedule_progress) == 0);
    CHECK(sound_schedule_progress.schedule.started == 0);
    CHECK(sound_schedule_progress.schedule.held == 1);
    CHECK(sound_schedule_progress.described == 0);
    CHECK(sound_schedule_progress.schedule.state.sound_index == 50);
    CHECK(sound_schedule_progress.schedule.state.sample_position == 4);
    CHECK(wl_schedule_describe_sound_channel_progress_from_chunk(
              &sound_channel, 70000, &audio_meta, chunk_buf, chunk_bytes,
              &sound_schedule_progress) == -1);
    CHECK(wl_schedule_describe_sound_channel_progress_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, 6,
              &sound_schedule_progress) == -1);
    CHECK(wl_schedule_describe_sound_channel_progress_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes,
              NULL) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 0;
    sound_channel.sound_index = 50;
    sound_channel.priority = 99;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 3,
              &sound_schedule_progress_window) == 0);
    CHECK(sound_schedule_progress_window.schedule.started == 1);
    CHECK(sound_schedule_progress_window.schedule.held == 0);
    CHECK(sound_schedule_progress_window.described == 1);
    CHECK(sound_schedule_progress_window.progress_window.progress.sound_index == 0);
    CHECK(sound_schedule_progress_window.progress_window.progress.sample_position == 0);
    CHECK(sound_schedule_progress_window.progress_window.progress.remaining_samples == 8);
    CHECK(sound_schedule_progress_window.progress_window.window.start_sample == 0);
    CHECK(sound_schedule_progress_window.progress_window.window.samples_in_window == 3);
    CHECK(sound_schedule_progress_window.progress_window.window.next_sample == 3);
    CHECK(sound_schedule_progress_window.progress_window.window.first_sample == 0x83);
    CHECK(sound_schedule_progress_window.progress_window.window.last_sample == 0x82);
    CHECK(sound_schedule_progress_window.progress_window.window.completed == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 2;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 3,
              &sound_schedule_progress_window) == 0);
    CHECK(sound_schedule_progress_window.schedule.started == 0);
    CHECK(sound_schedule_progress_window.schedule.held == 1);
    CHECK(sound_schedule_progress_window.described == 0);
    CHECK(sound_schedule_progress_window.schedule.state.sound_index == 50);
    CHECK(sound_schedule_progress_window.schedule.state.sample_position == 4);
    CHECK(wl_schedule_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 70000, &audio_meta, chunk_buf, chunk_bytes, 3,
              &sound_schedule_progress_window) == -1);
    CHECK(wl_schedule_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, 6, 3,
              &sound_schedule_progress_window) == -1);
    CHECK(wl_schedule_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 3,
              NULL) == -1);

    memset(&sound_channel, 0, sizeof(sound_channel));
    CHECK(wl_schedule_tick_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 3, 2,
              &sound_schedule_tick_progress_window) == 0);
    CHECK(sound_schedule_tick_progress_window.tick.schedule.started == 1);
    CHECK(sound_schedule_tick_progress_window.tick.ticked == 1);
    CHECK(sound_schedule_tick_progress_window.tick.tick.state.sample_position == 3);
    CHECK(sound_schedule_tick_progress_window.described == 1);
    CHECK(sound_schedule_tick_progress_window.progress_window.progress.sample_position == 3);
    CHECK(sound_schedule_tick_progress_window.progress_window.progress.remaining_samples == 5);
    CHECK(sound_schedule_tick_progress_window.progress_window.window.start_sample == 3);
    CHECK(sound_schedule_tick_progress_window.progress_window.window.samples_in_window == 2);
    CHECK(sound_schedule_tick_progress_window.progress_window.window.next_sample == 5);
    CHECK(sound_schedule_tick_progress_window.progress_window.window.first_sample == 0x8e);
    CHECK(sound_schedule_tick_progress_window.progress_window.window.last_sample == 0x8a);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 2;
    sound_channel.sample_position = 4;
    CHECK(wl_schedule_tick_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 3, 2,
              &sound_schedule_tick_progress_window) == 0);
    CHECK(sound_schedule_tick_progress_window.tick.schedule.held == 1);
    CHECK(sound_schedule_tick_progress_window.tick.ticked == 0);
    CHECK(sound_schedule_tick_progress_window.described == 0);
    CHECK(sound_schedule_tick_progress_window.tick.tick.state.sound_index == 50);
    CHECK(wl_schedule_tick_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 70000, &audio_meta, chunk_buf, chunk_bytes, 3, 2,
              &sound_schedule_tick_progress_window) == -1);
    CHECK(wl_schedule_tick_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, 6, 3, 2,
              &sound_schedule_tick_progress_window) == -1);
    CHECK(wl_schedule_tick_describe_sound_channel_progress_window_from_chunk(
              &sound_channel, 0, &audio_meta, chunk_buf, chunk_bytes, 3, 2,
              NULL) == -1);


    /* PC speaker sound 1 (SELECTWPNSND) */
    CHECK(wl_read_audio_chunk(audiot_path, &audio, 1, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 13);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x21985d89);
    CHECK(wl_describe_pc_speaker_sound(chunk_buf, chunk_bytes, &pc_meta) == 0);
    CHECK(pc_meta.sample_count == 6);
    CHECK(pc_meta.first_sample == 0x2f);
    CHECK(pc_meta.last_sample == 0x2f);
    CHECK(pc_meta.terminator == 0);
    CHECK(wl_get_pc_speaker_sound_sample(chunk_buf, chunk_bytes, 5, &pc_sample) == 0);
    CHECK(pc_sample == 0x2f);
    CHECK(wl_describe_pc_speaker_sound(chunk_buf, 6, &pc_meta) != 0);
    CHECK(wl_get_pc_speaker_sound_sample(chunk_buf, 6, 0, &pc_sample) == -1);
    CHECK(wl_advance_pc_speaker_playback_cursor(chunk_buf, 6, 0, 1, &pc_cursor) == -1);

    /* Adlib sound 87 (first adlib = STARTADLIBSOUNDS) */
    CHECK(wl_read_audio_chunk(audiot_path, &audio, 87, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 41);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x799f60b1);
    CHECK(wl_describe_audio_chunk(87, chunk_buf, chunk_bytes, &audio_meta) == 0);
    CHECK(audio_meta.kind == WL_AUDIO_CHUNK_ADLIB);
    CHECK(wl_describe_audio_chunk_with_ranges(87, 87, 87, 87,
                                              chunk_buf, chunk_bytes, &audio_meta) == 0);
    CHECK(audio_meta.kind == WL_AUDIO_CHUNK_ADLIB);
    CHECK(wl_describe_audio_chunk_with_ranges(87, (size_t)-1, 1, 1,
                                              chunk_buf, chunk_bytes, &audio_meta) == -1);
    CHECK(audio_meta.is_empty == 0);
    CHECK(audio_meta.declared_length == 8);
    CHECK(audio_meta.priority == 1);
    CHECK(audio_meta.payload_offset == 6);
    CHECK(audio_meta.payload_size == 35);
    CHECK(wl_describe_sound_channel_decision(1, 50, 1, 87, &audio_meta,
                                             &channel_decision) == 0);
    CHECK(channel_decision.candidate_kind == WL_AUDIO_CHUNK_ADLIB);
    CHECK(channel_decision.should_start == 1);
    CHECK(channel_decision.next_chunk == 87);
    CHECK(channel_decision.next_priority == 1);
    memset(&sound_channel, 0, sizeof(sound_channel));
    CHECK(wl_start_sound_channel_from_chunk(&sound_channel, 87, &audio_meta,
                                            &sound_start) == 0);
    CHECK(sound_start.started == 1);
    CHECK(sound_start.state.sound_index == 87);
    CHECK(sound_start.state.priority == 1);
    CHECK(sound_start.state.sample_position == 0);
    CHECK(wl_start_sound_channel_from_chunk(&sound_channel, 70000, &audio_meta,
                                            &sound_start) == -1);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 1;
    sound_channel.sample_position = 5;
    CHECK(wl_schedule_sound_channel(&sound_channel, 87, &audio_meta, &sound_schedule) == 0);
    CHECK(sound_schedule.started == 1);
    CHECK(sound_schedule.replaced == 1);
    CHECK(sound_schedule.held == 0);
    CHECK(sound_schedule.candidate_kind == WL_AUDIO_CHUNK_ADLIB);
    CHECK(sound_schedule.candidate_priority == 1);
    CHECK(sound_schedule.state.active == 1);
    CHECK(sound_schedule.state.sound_index == 87);
    CHECK(sound_schedule.state.priority == 1);
    CHECK(sound_schedule.state.sample_position == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 50;
    sound_channel.priority = 1;
    sound_channel.sample_position = 5;
    CHECK(wl_schedule_sound_channel_from_chunk(&sound_channel, 87, &audio_meta,
                                               &sound_schedule) == 0);
    CHECK(sound_schedule.started == 1);
    CHECK(sound_schedule.replaced == 1);
    CHECK(sound_schedule.held == 0);
    CHECK(sound_schedule.candidate_kind == WL_AUDIO_CHUNK_ADLIB);
    CHECK(sound_schedule.candidate_priority == 1);
    CHECK(sound_schedule.state.active == 1);
    CHECK(sound_schedule.state.sound_index == 87);
    CHECK(sound_schedule.state.priority == 1);
    CHECK(sound_schedule.state.sample_position == 0);
    CHECK(wl_schedule_sound_channel_from_chunk(&sound_channel, 70000, &audio_meta,
                                               &sound_schedule) == -1);
    CHECK(wl_describe_adlib_sound(chunk_buf, chunk_bytes, &adlib_meta) == 0);
    CHECK(adlib_meta.sample_count == 8);
    CHECK(wl_describe_sound_sample_count_from_chunk(&audio_meta, chunk_buf,
                                                    chunk_bytes,
                                                    &sound_sample_count) == 0);
    CHECK(sound_sample_count == 8);
    CHECK(adlib_meta.priority == 1);
    CHECK(adlib_meta.instrument_bytes == 16);
    CHECK(adlib_meta.first_instrument_byte == 0x70);
    CHECK(adlib_meta.last_instrument_byte == 0x03);
    CHECK(adlib_meta.first_sample == 0x04);
    CHECK(adlib_meta.last_sample == 0x2e);
    CHECK(adlib_meta.trailing_bytes == 11);
    CHECK(wl_get_adlib_instrument_byte(chunk_buf, chunk_bytes, 0, &adlib_instrument_byte) == 0);
    CHECK(adlib_instrument_byte == 0x70);
    CHECK(wl_get_adlib_instrument_byte(chunk_buf, chunk_bytes, 15, &adlib_instrument_byte) == 0);
    CHECK(adlib_instrument_byte == 0x03);
    CHECK(wl_get_adlib_instrument_byte(chunk_buf, chunk_bytes, 16, &adlib_instrument_byte) == -1);
    CHECK(wl_describe_adlib_instrument_registers(chunk_buf, chunk_bytes, &adlib_regs) == 0);
    CHECK(adlib_regs.modulator_regs[0] == 0x20);
    CHECK(adlib_regs.modulator_values[0] == 0x70);
    CHECK(adlib_regs.carrier_regs[0] == 0x23);
    CHECK(adlib_regs.carrier_values[0] == 0x75);
    CHECK(adlib_regs.modulator_regs[4] == 0xe0);
    CHECK(adlib_regs.modulator_values[4] == 0x00);
    CHECK(adlib_regs.carrier_regs[4] == 0xe3);
    CHECK(adlib_regs.carrier_values[4] == 0x00);
    CHECK(adlib_regs.feedback_reg == 0xc0);
    CHECK(adlib_regs.feedback_value == 0x01);
    CHECK(adlib_regs.voice == 0x32);
    CHECK(adlib_regs.mode == 0xd5);
    CHECK(wl_describe_adlib_instrument_registers(chunk_buf, 21, &adlib_regs) == -1);
    CHECK(wl_describe_adlib_instrument_registers(chunk_buf, chunk_bytes, NULL) == -1);
    CHECK(wl_describe_adlib_instrument_registers_from_chunk(&audio_meta, chunk_buf,
                                                            chunk_bytes,
                                                            &adlib_regs) == 0);
    CHECK(adlib_regs.modulator_regs[0] == 0x20);
    CHECK(adlib_regs.modulator_values[0] == 0x70);
    CHECK(adlib_regs.carrier_regs[0] == 0x23);
    CHECK(adlib_regs.carrier_values[0] == 0x75);
    CHECK(adlib_regs.feedback_value == 0x01);
    CHECK(adlib_regs.voice == 0x32);
    audio_meta.kind = WL_AUDIO_CHUNK_PC_SPEAKER;
    CHECK(wl_describe_adlib_instrument_registers_from_chunk(&audio_meta, chunk_buf,
                                                            chunk_bytes,
                                                            &adlib_regs) == -1);
    audio_meta.kind = WL_AUDIO_CHUNK_ADLIB;
    CHECK(wl_describe_adlib_instrument_registers_from_chunk(&audio_meta, chunk_buf,
                                                            21,
                                                            &adlib_regs) == -1);
    CHECK(wl_get_adlib_sound_sample(chunk_buf, chunk_bytes, 0, &adlib_sample) == 0);
    CHECK(adlib_sample == 0x04);
    CHECK(wl_get_adlib_sound_sample(chunk_buf, chunk_bytes, 7, &adlib_sample) == 0);
    CHECK(adlib_sample == 0x2e);
    CHECK(wl_get_adlib_sound_sample(chunk_buf, chunk_bytes, 8, &adlib_sample) == -1);
    CHECK(wl_advance_adlib_playback_cursor(chunk_buf, chunk_bytes, 0, 0, &adlib_cursor) == 0);
    CHECK(adlib_cursor.sample_index == 0);
    CHECK(adlib_cursor.current_sample == 0x04);
    CHECK(adlib_cursor.samples_consumed == 0);
    CHECK(adlib_cursor.completed == 0);
    CHECK(wl_advance_adlib_playback_cursor(chunk_buf, chunk_bytes, 6, 1, &adlib_cursor) == 0);
    CHECK(adlib_cursor.sample_index == 7);
    CHECK(adlib_cursor.current_sample == 0x2e);
    CHECK(adlib_cursor.samples_consumed == 1);
    CHECK(adlib_cursor.completed == 0);
    CHECK(wl_advance_adlib_playback_cursor(chunk_buf, chunk_bytes, 6, 9, &adlib_cursor) == 0);
    CHECK(adlib_cursor.sample_index == 8);
    CHECK(adlib_cursor.samples_consumed == 2);
    CHECK(adlib_cursor.completed == 1);
    CHECK(wl_advance_adlib_playback_cursor(chunk_buf, chunk_bytes, 8, 0, &adlib_cursor) == 0);
    CHECK(adlib_cursor.completed == 1);
    CHECK(wl_advance_adlib_playback_cursor(chunk_buf, chunk_bytes, 8, 1, &adlib_cursor) == -1);
    CHECK(wl_describe_adlib_sound(chunk_buf, 22, &adlib_meta) == -1);
    CHECK(wl_get_adlib_instrument_byte(chunk_buf, 21, 0, &adlib_instrument_byte) == -1);
    CHECK(wl_get_adlib_sound_sample(chunk_buf, 22, 0, &adlib_sample) == -1);
    CHECK(wl_advance_adlib_playback_cursor(chunk_buf, chunk_bytes, 5, 2, &adlib_cursor) == 0);
    CHECK(adlib_cursor.sample_index == 7);
    CHECK(adlib_cursor.current_sample == 0x2e);
    CHECK(adlib_cursor.samples_consumed == 2);
    CHECK(adlib_cursor.completed == 0);
    CHECK(wl_advance_adlib_playback_cursor(chunk_buf, chunk_bytes, 5, 9, &adlib_cursor) == 0);
    CHECK(adlib_cursor.sample_index == 8);
    CHECK(adlib_cursor.samples_consumed == 3);
    CHECK(adlib_cursor.completed == 1);
    CHECK(wl_advance_adlib_playback_cursor(chunk_buf, 22, 0, 1, &adlib_cursor) == -1);
    CHECK(wl_describe_adlib_playback_window(chunk_buf, chunk_bytes, 3, 4,
                                            &sample_window) == 0);
    CHECK(sample_window.start_sample == 3);
    CHECK(sample_window.samples_available == 5);
    CHECK(sample_window.samples_in_window == 4);
    CHECK(sample_window.next_sample == 7);
    CHECK(sample_window.first_sample == 0x35);
    CHECK(sample_window.last_sample == 0x29);
    CHECK(sample_window.completed == 0);
    CHECK(wl_describe_adlib_playback_window(chunk_buf, chunk_bytes, 8, 1,
                                            &sample_window) == 0);
    CHECK(sample_window.samples_in_window == 0);
    CHECK(sample_window.completed == 1);
    CHECK(wl_describe_adlib_playback_window(chunk_buf, chunk_bytes, 9, 1,
                                            &sample_window) == -1);
    CHECK(wl_describe_sound_playback_window_from_chunk(&audio_meta, chunk_buf,
                                                       chunk_bytes, 3, 4,
                                                       &sample_window) == 0);
    CHECK(sample_window.start_sample == 3);
    CHECK(sample_window.samples_in_window == 4);
    CHECK(sample_window.next_sample == 7);
    CHECK(sample_window.first_sample == 0x35);
    CHECK(sample_window.last_sample == 0x29);
    CHECK(wl_describe_sound_playback_window_from_chunk(NULL, chunk_buf,
                                                       chunk_bytes, 3, 4,
                                                       &sample_window) == -1);
    CHECK(wl_describe_adlib_playback_position(chunk_buf, chunk_bytes, 0,
                                              &sample_position) == 0);
    CHECK(sample_position.sample_position == 0);
    CHECK(sample_position.sample_count == 8);
    CHECK(sample_position.current_sample == 0x04);
    CHECK(sample_position.completed == 0);
    CHECK(wl_describe_adlib_playback_position(chunk_buf, chunk_bytes, 7,
                                              &sample_position) == 0);
    CHECK(sample_position.current_sample == 0x2e);
    CHECK(sample_position.completed == 0);
    CHECK(wl_describe_adlib_playback_position(chunk_buf, chunk_bytes, 8,
                                              &sample_position) == 0);
    CHECK(sample_position.sample_position == 8);
    CHECK(sample_position.completed == 1);
    CHECK(wl_describe_adlib_playback_position(chunk_buf, chunk_bytes, 9,
                                              &sample_position) == -1);
    CHECK(wl_describe_sound_playback_position_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                         7, &sample_position) == 0);
    CHECK(sample_position.sample_count == 8);
    CHECK(sample_position.current_sample == 0x2e);
    CHECK(sample_position.completed == 0);
    sound_channel.active = 1;
    sound_channel.sound_index = 87;
    sound_channel.priority = 1;
    sound_channel.sample_position = 7;
    CHECK(wl_describe_sound_channel_position_from_chunk(&sound_channel, &audio_meta,
                                                        chunk_buf, chunk_bytes,
                                                        &sample_position) == 0);
    CHECK(sample_position.sample_count == 8);
    CHECK(sample_position.current_sample == 0x2e);
    CHECK(sample_position.completed == 0);
    CHECK(wl_describe_sound_channel_remaining_from_chunk(&sound_channel, &audio_meta,
                                                         chunk_buf, chunk_bytes,
                                                         &sound_sample_count) == 0);
    CHECK(sound_sample_count == 1);
    CHECK(wl_describe_sound_channel_progress_from_chunk(&sound_channel, &audio_meta,
                                                        chunk_buf, chunk_bytes,
                                                        &sound_progress) == 0);
    CHECK(sound_progress.sound_index == 87);
    CHECK(sound_progress.priority == 1);
    CHECK(sound_progress.kind == WL_AUDIO_CHUNK_ADLIB);
    CHECK(sound_progress.sample_position == 7);
    CHECK(sound_progress.sample_count == 8);
    CHECK(sound_progress.remaining_samples == 1);
    CHECK(sound_progress.current_sample == 0x2e);
    CHECK(sound_progress.completed == 0);
    CHECK(wl_describe_sound_channel_progress_window_from_chunk(&sound_channel, &audio_meta,
                                                               chunk_buf, chunk_bytes, 2,
                                                               &sound_progress_window) == 0);
    CHECK(sound_progress_window.progress.sound_index == 87);
    CHECK(sound_progress_window.progress.sample_position == 7);
    CHECK(sound_progress_window.progress.remaining_samples == 1);
    CHECK(sound_progress_window.window.start_sample == 7);
    CHECK(sound_progress_window.window.samples_available == 1);
    CHECK(sound_progress_window.window.samples_in_window == 1);
    CHECK(sound_progress_window.window.next_sample == 8);
    CHECK(sound_progress_window.window.first_sample == 0x2e);
    CHECK(sound_progress_window.window.last_sample == 0x2e);
    CHECK(sound_progress_window.window.completed == 1);
    sound_channel.sample_position = 8;
    CHECK(wl_describe_sound_channel_progress_from_chunk(&sound_channel, &audio_meta,
                                                        chunk_buf, chunk_bytes,
                                                        &sound_progress) == 0);
    CHECK(sound_progress.remaining_samples == 0);
    CHECK(sound_progress.completed == 1);
    sound_channel.sample_position = 7;
    sound_channel.active = 2;
    CHECK(wl_describe_sound_channel_position_from_chunk(&sound_channel, &audio_meta,
                                                        chunk_buf, chunk_bytes,
                                                        &sample_position) == -1);
    CHECK(wl_describe_sound_channel_remaining_from_chunk(&sound_channel, &audio_meta,
                                                         chunk_buf, chunk_bytes,
                                                         &sound_sample_count) == -1);
    CHECK(wl_describe_sound_channel_progress_from_chunk(&sound_channel, &audio_meta,
                                                        chunk_buf, chunk_bytes,
                                                        &sound_progress) == -1);
    CHECK(wl_describe_sound_channel_progress_window_from_chunk(&sound_channel, &audio_meta,
                                                               chunk_buf, chunk_bytes, 2,
                                                               &sound_progress_window) == -1);
    sound_channel.active = 1;
    sound_channel.sample_position = 9;
    CHECK(wl_describe_sound_channel_remaining_from_chunk(&sound_channel, &audio_meta,
                                                         chunk_buf, chunk_bytes,
                                                         &sound_sample_count) == -1);
    sound_channel.active = 1;
    sound_channel.sound_index = 87;
    sound_channel.priority = 1;
    sound_channel.sample_position = 5;
    CHECK(wl_advance_sound_channel_from_chunk(&sound_channel, &audio_meta,
                                              chunk_buf, chunk_bytes, 2,
                                              &sound_advance) == 0);
    CHECK(sound_advance.completed == 0);
    CHECK(sound_advance.samples_consumed == 2);
    CHECK(sound_advance.state.active == 1);
    CHECK(sound_advance.state.sample_position == 7);
    CHECK(wl_advance_sound_channel_from_chunk(&sound_advance.state, &audio_meta,
                                              chunk_buf, chunk_bytes, 2,
                                              &sound_advance) == 0);
    CHECK(sound_advance.completed == 1);
    CHECK(sound_advance.samples_consumed == 1);
    CHECK(sound_advance.state.active == 0);
    CHECK(sound_advance.state.sample_position == 8);
    audio_meta.kind = WL_AUDIO_CHUNK_MUSIC;
    CHECK(wl_describe_sound_playback_position_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                         0, &sample_position) == -1);
    CHECK(wl_advance_sound_channel_from_chunk(&sound_channel, &audio_meta,
                                              chunk_buf, chunk_bytes, 1,
                                              &sound_advance) == -1);
    audio_meta.kind = WL_AUDIO_CHUNK_ADLIB;

    memset(&sound_channel, 0, sizeof(sound_channel));
    sound_channel.active = 1;
    sound_channel.sound_index = 87;
    sound_channel.priority = 1;
    sound_channel.sample_position = 5;
    CHECK(wl_tick_sound_channel(&sound_channel, WL_AUDIO_CHUNK_ADLIB,
                                chunk_buf, chunk_bytes, 2, &sound_tick) == 0);
    CHECK(sound_tick.state.active == 1);
    CHECK(sound_tick.state.sound_index == 87);
    CHECK(sound_tick.state.sample_position == 7);
    CHECK(sound_tick.samples_consumed == 2);
    CHECK(sound_tick.current_sample == 0x2e);
    CHECK(sound_tick.completed == 0);
    CHECK(wl_tick_sound_channel(&sound_tick.state, WL_AUDIO_CHUNK_ADLIB,
                                chunk_buf, chunk_bytes, 2, &sound_tick) == 0);
    CHECK(sound_tick.state.active == 0);
    CHECK(sound_tick.state.sample_position == 8);
    CHECK(sound_tick.samples_consumed == 1);
    CHECK(sound_tick.completed == 1);

    /* Digital sound 174 (first digi = STARTDIGISOUNDS) - empty in WL6 shareware */
    CHECK(wl_read_audio_chunk(audiot_path, &audio, 174, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 0);
    CHECK(wl_describe_audio_chunk(174, chunk_buf, chunk_bytes, &audio_meta) == 0);
    CHECK(audio_meta.kind == WL_AUDIO_CHUNK_DIGITAL);
    CHECK(audio_meta.is_empty == 1);
    CHECK(audio_meta.payload_size == 0);
    CHECK(wl_start_sound_channel_from_chunk(&sound_channel, 174, &audio_meta,
                                            &sound_start) == -1);

    /* Music 261 (first music = STARTMUSIC, CORNER_MUS) */
    CHECK(wl_read_audio_chunk(audiot_path, &audio, 261, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 7546);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xea0d69d8);
    CHECK(wl_describe_audio_chunk(261, chunk_buf, chunk_bytes, &audio_meta) == 0);
    CHECK(audio_meta.kind == WL_AUDIO_CHUNK_MUSIC);
    CHECK(audio_meta.is_empty == 0);
    CHECK(audio_meta.declared_length == 7456);
    CHECK(audio_meta.priority == 0);
    CHECK(audio_meta.payload_offset == 4);
    CHECK(audio_meta.payload_size == 7542);
    CHECK(wl_describe_imf_music_chunk(chunk_buf, chunk_bytes, &imf_meta) == 0);
    CHECK(imf_meta.declared_bytes == 7456);
    CHECK(imf_meta.command_count == 1864);
    CHECK(imf_meta.first_register == 0);
    CHECK(imf_meta.first_value == 0);
    CHECK(imf_meta.first_delay == 189);
    CHECK(imf_meta.last_register == 0);
    CHECK(imf_meta.last_value == 0);
    CHECK(imf_meta.last_delay == 1);
    CHECK(imf_meta.max_delay == 64098);
    CHECK(imf_meta.zero_delay_count == 0);
    CHECK(imf_meta.total_delay == 25697407);
    CHECK(imf_meta.trailing_bytes == 86);
    CHECK(wl_get_imf_music_command(chunk_buf, chunk_bytes, 0, &imf_command) == 0);
    CHECK(imf_command.reg == 0);
    CHECK(imf_command.value == 0);
    CHECK(imf_command.delay == 189);
    CHECK(wl_get_imf_music_command(chunk_buf, chunk_bytes, 1863, &imf_command) == 0);
    CHECK(imf_command.reg == 0);
    CHECK(imf_command.value == 0);
    CHECK(imf_command.delay == 1);
    CHECK(wl_get_imf_music_command(chunk_buf, chunk_bytes, 1864, &imf_command) == -1);
    CHECK(wl_describe_imf_playback_window(chunk_buf, chunk_bytes, 0, 189, &imf_window) == 0);
    CHECK(imf_window.start_command == 0);
    CHECK(imf_window.commands_available == 1864);
    CHECK(imf_window.commands_in_window == 1);
    CHECK(imf_window.next_command == 1);
    CHECK(imf_window.elapsed_delay == 189);
    CHECK(imf_window.completed == 0);
    CHECK(wl_describe_imf_playback_window(chunk_buf, chunk_bytes, 1, 197, &imf_window) == 0);
    CHECK(imf_window.commands_in_window == 1);
    CHECK(imf_window.next_command == 2);
    CHECK(imf_window.elapsed_delay == 8);
    CHECK(wl_describe_imf_playback_window(chunk_buf, chunk_bytes, 0, 20485, &imf_window) == 0);
    CHECK(imf_window.commands_in_window == 3);
    CHECK(imf_window.next_command == 3);
    CHECK(imf_window.elapsed_delay == 20485);
    CHECK(wl_describe_imf_playback_window(chunk_buf, chunk_bytes, 1864, 1, &imf_window) == 0);
    CHECK(imf_window.commands_available == 0);
    CHECK(imf_window.commands_in_window == 0);
    CHECK(imf_window.completed == 1);
    CHECK(wl_describe_imf_playback_window(chunk_buf, chunk_bytes, 1865, 1, &imf_window) == -1);
    CHECK(wl_describe_imf_playback_position(chunk_buf, chunk_bytes, 0, &imf_position) == 0);
    CHECK(imf_position.command_index == 0);
    CHECK(imf_position.command_delay == 189);
    CHECK(imf_position.delay_elapsed == 0);
    CHECK(imf_position.delay_remaining == 189);
    CHECK(imf_position.completed == 0);
    CHECK(wl_describe_imf_playback_position(chunk_buf, chunk_bytes, 188, &imf_position) == 0);
    CHECK(imf_position.command_index == 0);
    CHECK(imf_position.delay_elapsed == 188);
    CHECK(imf_position.delay_remaining == 1);
    CHECK(wl_describe_imf_playback_position(chunk_buf, chunk_bytes, 189, &imf_position) == 0);
    CHECK(imf_position.command_index == 1);
    CHECK(imf_position.command_delay == 8);
    CHECK(imf_position.delay_elapsed == 0);
    CHECK(imf_position.delay_remaining == 8);
    CHECK(wl_describe_imf_playback_position_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                       189, &imf_position) == 0);
    CHECK(imf_position.command_index == 1);
    CHECK(imf_position.command_delay == 8);
    CHECK(imf_position.delay_remaining == 8);
    CHECK(wl_describe_imf_playback_position(chunk_buf, chunk_bytes, 196, &imf_position) == 0);
    CHECK(imf_position.command_index == 1);
    CHECK(imf_position.delay_elapsed == 7);
    CHECK(imf_position.delay_remaining == 1);
    CHECK(wl_describe_imf_playback_position(chunk_buf, chunk_bytes, 20485, &imf_position) == 0);
    CHECK(imf_position.command_index == 3);
    CHECK(imf_position.completed == 0);
    CHECK(wl_describe_imf_playback_position(chunk_buf, chunk_bytes, imf_meta.total_delay, &imf_position) == 0);
    CHECK(imf_position.command_index == 1864);
    CHECK(imf_position.completed == 1);
    CHECK(wl_advance_imf_playback_cursor(chunk_buf, chunk_bytes, 0, 0, 188, &imf_cursor) == 0);
    CHECK(imf_cursor.command_index == 0);
    CHECK(imf_cursor.command_delay == 189);
    CHECK(imf_cursor.delay_elapsed == 188);
    CHECK(imf_cursor.delay_remaining == 1);
    CHECK(imf_cursor.commands_advanced == 0);
    CHECK(imf_cursor.completed == 0);
    CHECK(wl_advance_imf_playback_cursor(chunk_buf, chunk_bytes, 0, 188, 1, &imf_cursor) == 0);
    CHECK(imf_cursor.command_index == 1);
    CHECK(imf_cursor.command_delay == 8);
    CHECK(imf_cursor.delay_elapsed == 0);
    CHECK(imf_cursor.delay_remaining == 8);
    CHECK(imf_cursor.ticks_consumed == 1);
    CHECK(imf_cursor.commands_advanced == 1);
    CHECK(wl_advance_imf_playback_cursor(chunk_buf, chunk_bytes, 1, 7, 20289, &imf_cursor) == 0);
    CHECK(imf_cursor.command_index == 3);
    CHECK(imf_cursor.delay_elapsed == 0);
    CHECK(imf_cursor.commands_advanced == 2);
    CHECK(imf_cursor.completed == 0);
    CHECK(wl_advance_imf_playback_cursor_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                    1, 7, 20289, &imf_cursor) == 0);
    CHECK(imf_cursor.command_index == 3);
    CHECK(imf_cursor.commands_advanced == 2);
    CHECK(imf_cursor.completed == 0);
    CHECK(wl_advance_imf_playback_cursor(chunk_buf, chunk_bytes, 1863, 0, 1, &imf_cursor) == 0);
    CHECK(imf_cursor.command_index == 1864);
    CHECK(imf_cursor.commands_advanced == 1);
    CHECK(imf_cursor.completed == 1);
    CHECK(wl_advance_imf_playback_cursor(chunk_buf, chunk_bytes, 1864, 0, 99, &imf_cursor) == 0);
    CHECK(imf_cursor.command_index == 1864);
    CHECK(imf_cursor.ticks_consumed == 0);
    CHECK(imf_cursor.completed == 1);
    CHECK(wl_advance_imf_playback_cursor(chunk_buf, chunk_bytes, 0, 190, 1, &imf_cursor) == -1);
    CHECK(wl_advance_imf_playback_cursor(chunk_buf, chunk_bytes, 1864, 1, 1, &imf_cursor) == -1);
    audio_meta.kind = WL_AUDIO_CHUNK_ADLIB;
    CHECK(wl_describe_imf_playback_position_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                       189, &imf_position) == -1);
    CHECK(wl_advance_imf_playback_cursor_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                    1, 7, 20289, &imf_cursor) == -1);
    audio_meta.kind = WL_AUDIO_CHUNK_MUSIC;
    audio_meta.payload_size = chunk_bytes + 1u;
    CHECK(wl_describe_imf_playback_position_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                       189, &imf_position) == -1);
    CHECK(wl_advance_imf_playback_cursor_from_chunk(&audio_meta, chunk_buf, chunk_bytes,
                                                    1, 7, 20289, &imf_cursor) == -1);

    /* Boundary: last chunk */
    CHECK(wl_read_audio_chunk(audiot_path, &audio, 287, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 20926);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x65998666);
    CHECK(wl_describe_imf_music_chunk(chunk_buf, chunk_bytes, &imf_meta) == 0);
    CHECK(imf_meta.declared_bytes == 20836);
    CHECK(imf_meta.command_count == 5209);
    CHECK(imf_meta.first_delay == 189);
    CHECK(imf_meta.last_register == 0);
    CHECK(imf_meta.last_value == 0);
    CHECK(imf_meta.last_delay == 1);
    CHECK(imf_meta.max_delay == 65429);
    CHECK(imf_meta.zero_delay_count == 0);
    CHECK(imf_meta.total_delay == 71494600);
    CHECK(imf_meta.trailing_bytes == 86);

    /* Out of range */
    CHECK(wl_read_audio_chunk(audiot_path, &audio, 288, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == -1);

    return 0;
}

static int check_audio_optional_sod(const char *dir) {
    char sod_dir[1024];
    char audiohed_path[1024];
    char audiot_path[1024];
    wl_audio_header audio;
    wl_audio_range_summary audio_range;
    wl_audio_chunk_metadata audio_meta;
    wl_imf_music_metadata imf_meta;
    wl_imf_playback_window imf_window;
    wl_imf_playback_position imf_position;
    wl_imf_playback_cursor imf_cursor;
    unsigned char chunk_buf[65536];
    size_t chunk_bytes = 0;
    size_t audiohed_size = 0;
    size_t audiot_size = 0;

    CHECK(wl_join_path(sod_dir, sizeof(sod_dir), dir, "m1") == 0);
    if (path(audiohed_path, sizeof(audiohed_path), sod_dir, "AUDIOHED.SOD") != 0 ||
        path(audiot_path, sizeof(audiot_path), sod_dir, "AUDIOT.SOD") != 0) {
        return 0;
    }
    if (wl_file_size(audiohed_path, &audiohed_size) != 0) {
        printf("SOD audio data absent; skipping optional SOD audio assertions\n");
        return 0;
    }

    CHECK(wl_read_audio_header(audiohed_path, &audio) == 0);
    CHECK(audio.file_size == 1072);
    CHECK(audio.chunk_count == 267);
    CHECK(audio.offsets[0] == 0);
    CHECK(audio.offsets[1] == 15);
    CHECK(audio.offsets[2] == 66);
    CHECK(audio.offsets[3] == 82);
    CHECK(audio.offsets[4] == 174);

    for (size_t i = 1; i <= audio.chunk_count; ++i) {
        CHECK(audio.offsets[i] >= audio.offsets[i - 1]);
    }

    CHECK(wl_file_size(audiot_path, &audiot_size) == 0);
    CHECK(audiot_size == 328620);
    CHECK(audio.offsets[audio.chunk_count] == audiot_size);

    CHECK(wl_summarize_audio_range(&audio, 0, 81, &audio_range) == 0);
    CHECK(audio_range.non_empty_chunks == 81);
    CHECK(audio_range.total_bytes == 6989);
    CHECK(audio_range.first_non_empty_chunk == 0);
    CHECK(audio_range.last_non_empty_chunk == 80);
    CHECK(audio_range.largest_chunk == 17);
    CHECK(audio_range.largest_chunk_bytes == 306);
    CHECK(wl_summarize_audio_range(&audio, 81, 81, &audio_range) == 0);
    CHECK(audio_range.non_empty_chunks == 81);
    CHECK(audio_range.total_bytes == 10607);
    CHECK(audio_range.first_non_empty_chunk == 81);
    CHECK(audio_range.last_non_empty_chunk == 161);
    CHECK(audio_range.largest_chunk == 119);
    CHECK(audio_range.largest_chunk_bytes == 338);
    CHECK(wl_summarize_audio_range(&audio, 162, 81, &audio_range) == 0);
    CHECK(audio_range.non_empty_chunks == 1);
    CHECK(audio_range.total_bytes == 4);
    CHECK(audio_range.first_non_empty_chunk == 242);
    CHECK(audio_range.last_non_empty_chunk == 242);
    CHECK(audio_range.largest_chunk == 242);
    CHECK(audio_range.largest_chunk_bytes == 4);
    CHECK(wl_summarize_audio_range(&audio, 243, 24, &audio_range) == 0);
    CHECK(audio_range.non_empty_chunks == 24);
    CHECK(audio_range.total_bytes == 311020);
    CHECK(audio_range.first_non_empty_chunk == 243);
    CHECK(audio_range.last_non_empty_chunk == 266);
    CHECK(audio_range.largest_chunk == 258);
    CHECK(audio_range.largest_chunk_bytes == 22578);

    CHECK(wl_read_audio_chunk(audiot_path, &audio, 0, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 15);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x5971ec53);

    CHECK(wl_read_audio_chunk(audiot_path, &audio, 1, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 51);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xfafa57eb);

    CHECK(wl_read_audio_chunk(audiot_path, &audio, 87, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 69);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0xf0dfcb70);

    CHECK(wl_read_audio_chunk(audiot_path, &audio, 174, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 0);

    CHECK(wl_read_audio_chunk(audiot_path, &audio, 261, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 13286);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x04a8dbe2);

    CHECK(wl_read_audio_chunk(audiot_path, &audio, 243, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 21730);
    CHECK(wl_describe_audio_chunk_with_ranges(243, 81, 81, 81,
                                              chunk_buf, chunk_bytes, &audio_meta) == 0);
    CHECK(audio_meta.kind == WL_AUDIO_CHUNK_MUSIC);
    CHECK(audio_meta.declared_length == 21640);
    CHECK(audio_meta.payload_offset == 4);
    CHECK(audio_meta.payload_size == 21726);
    CHECK(wl_describe_audio_chunk(243, chunk_buf, chunk_bytes, &audio_meta) == 0);
    CHECK(audio_meta.kind == WL_AUDIO_CHUNK_DIGITAL);
    CHECK(wl_describe_imf_music_chunk(chunk_buf, chunk_bytes, &imf_meta) == 0);
    CHECK(imf_meta.declared_bytes == 21640);
    CHECK(imf_meta.command_count == 5410);
    CHECK(imf_meta.first_register == 0);
    CHECK(imf_meta.first_value == 0);
    CHECK(imf_meta.first_delay == 189);
    CHECK(imf_meta.last_register == 0);
    CHECK(imf_meta.last_value == 0);
    CHECK(imf_meta.last_delay == 1);
    CHECK(imf_meta.max_delay == 65419);
    CHECK(imf_meta.zero_delay_count == 0);
    CHECK(imf_meta.total_delay == 65434029);
    CHECK(imf_meta.trailing_bytes == 86);
    CHECK(wl_describe_imf_playback_window(chunk_buf, chunk_bytes, 0, 100,
                                          &imf_window) == 0);
    CHECK(imf_window.commands_available == 5410);
    CHECK(imf_window.commands_in_window == 1);
    CHECK(imf_window.next_command == 1);
    CHECK(imf_window.elapsed_delay == 189);
    CHECK(imf_window.completed == 0);
    CHECK(wl_describe_imf_playback_position(chunk_buf, chunk_bytes, 500,
                                            &imf_position) == 0);
    CHECK(imf_position.command_index == 2);
    CHECK(imf_position.command_delay == 20288);
    CHECK(imf_position.delay_elapsed == 303);
    CHECK(imf_position.delay_remaining == 19985);
    CHECK(imf_position.completed == 0);
    CHECK(wl_advance_imf_playback_cursor(chunk_buf, chunk_bytes, 0, 0, 500,
                                         &imf_cursor) == 0);
    CHECK(imf_cursor.command_index == 2);
    CHECK(imf_cursor.command_delay == 20288);
    CHECK(imf_cursor.delay_elapsed == 303);
    CHECK(imf_cursor.delay_remaining == 19985);
    CHECK(imf_cursor.ticks_consumed == 500);
    CHECK(imf_cursor.commands_advanced == 2);
    CHECK(imf_cursor.completed == 0);

    CHECK(wl_read_audio_chunk(audiot_path, &audio, 266, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == 0);
    CHECK(chunk_bytes == 6302);
    CHECK(fnv1a_bytes(chunk_buf, chunk_bytes) == 0x0fdb4632);

    CHECK(wl_read_audio_chunk(audiot_path, &audio, 267, chunk_buf, sizeof(chunk_buf),
                              &chunk_bytes) == -1);

    return 0;
}

int main(void) {
    const char *dir = wl_default_data_dir();
    CHECK(check_gameplay_events() == 0);
    CHECK(check_decode_helpers() == 0);
    CHECK(check_wl6(dir) == 0);
    CHECK(check_optional_sod(dir) == 0);
    CHECK(check_audio_wl6(dir) == 0);
    CHECK(check_audio_optional_sod(dir) == 0);
    printf("asset/decompression/semantics/model/vswap/runtime-present-chase-attack-frame/audio+sod-audio tests passed for %s\n", dir);
    return 0;
}
