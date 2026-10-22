#include <stdint.h>
#include "millis.h"
#include "game_config.h"
#include "input.h"
#include "display.h"
#include "game.h"
#include "renderer.h"

int main(void) {
    input_init();
    display_init();
    millis_init();
    game_init();

    uint32_t now = millis();
    uint32_t last_poll = now;
    uint32_t last_frame = now - RENDER_FRAME_MS; // Draw on the first iteration.

    while (1) {
        now = millis();

        // Unsigned elapsed-time subtraction handles millis() wraparound.
        if ((uint32_t)(now - last_poll) >= INPUT_POLL_MS) {
            last_poll = now;
            input_state_t input = input_poll(now);
            game_handle_input(input, now);
        }

        game_update(now);

        if ((uint32_t)(now - last_frame) >= RENDER_FRAME_MS) {
            last_frame = now;
            const game_t *game = game_get_state();
            renderer_draw(game, now);
        }
    }
}
