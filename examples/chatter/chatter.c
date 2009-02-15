/* -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-  */
/* vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0: */

#include <rts.h>

#define MSG_LEN 64

const char *messages[] = {
    "Mrs. Dalloway said she would buy the flowers herself.\n\0        ",
    "1801--I have just returned from a visit to my landlord.\n\0      ",
    "I am a sick man ... I am a spiteful man.\n\0                     ",
    "There was no possibility of taking a walk that day.\n\0          ",
    "Call me Ishmael.\n\0                                             ",
    "It is a truth universally acknowledged...\n\0                    ",
    "I was born in the Year 1632, in the City of York.\n\0            ",
    "Selden paused in surprise.\n\0                                   "
};

void send_message() {
    unsigned id = cpu_id();
    /* first message */
    unsigned flow = (id << 16) | ((3 - id) << 8) | 0;
    const char *src = messages[2 * id];
    unsigned xmit_id = send(flow, src, MSG_LEN);
    while (!transmission_done(xmit_id)); /* spin until xmit done */
    /* second message */
    flow = (id << 16) | ((3 - id) << 8) | 1;
    src = messages[2 * id + 1];
    xmit_id = send(flow, src, MSG_LEN);
    while (!transmission_done(xmit_id)); /* spin until xmit done */
}

void receive_and_block(unsigned qid, void *dst, unsigned len) {
    unsigned xmit_id = receive(qid, dst, len);
    while (!transmission_done(xmit_id)); /* spin until xmit done */
}

void receive_and_print(unsigned qid) {
    char buf[MSG_LEN + 11]; buf[MSG_LEN + 10] = '\0';
    unsigned flow = next_packet_flow(qid);
    for (int shift = 28, i = 0; i < 8; ++i, shift -= 4)
        buf[i] = '0' + ((flow >> shift) & 0xf);
    buf[8] = ':'; buf[9] = ' ';
    for (int remaining = next_packet_length(qid); remaining > 0;
         remaining -= MSG_LEN) {
        receive_and_block(qid, buf + 10,
                          remaining < MSG_LEN ? remaining : MSG_LEN);
        print_string(buf);
    }
}

void receive_and_print_loop() {
    for (;;) {
        unsigned qs = waiting_queues();
        if (qs) {
            unsigned q = 0;
            for (q = 0; !(qs & 1); qs >>= 1, ++q);
            receive_and_print(q);
        }
    }
}

int main(int argc, char **argv) {
    send_message();
    receive_and_print_loop();
}

