#include <util/atomic.h>
#include <Adafruit_NeoPixel.h>
#include <TaskAction.h>

#include "check-and-clear.h"

extern "C"{
#include "button.h"
}

#include "fixed-length-accumulator.h"
#include "very-tiny-http.h"

#include "game-ethernet.h"

static const int8_t MAX_DEBOUNCE = 5;
static bool s_pressed_flag = false;

static const uint8_t NEOPIXEL_PIN = 2;
static const uint8_t BUTTON_PIN = 3;

static Adafruit_NeoPixel s_pixels = Adafruit_NeoPixel(2, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

static void send_standard_erm_response()
{
    http_server_set_response_code("200 OK");
    http_server_set_header("Access-Control-Allow-Origin", "*");
    http_server_finish_headers();
}

static void debug_task_fn(TaskAction*task)
{

}
static TaskAction s_debug_task(debug_task_fn, 500, INFINITE_TICKS);

static void on_button_change(BTN_STATE_ENUM state)
{
    if (state == BTN_STATE_ACTIVE)
    {
        Serial.println("Button pressed");
    }
    else
    {
        Serial.println("Button released");
    } 
}

static BTN s_button = {
    .current_state = BTN_STATE_INACTIVE,
    .change_state_callback = on_button_change,
    .repeat_callback = NULL,
    .repeat_count = 0,
    .max_repeat_count = 0,
    .debounce_count = 0,
    .max_debounce_count = MAX_DEBOUNCE
};

static void debounce_task_fn(TaskAction*task)
{
    bool pressed_now = (digitalRead(BUTTON_PIN) == LOW);

    if (pressed_now)
    {
        s_pressed_flag = true;
    }

    BTN_Update(&s_button, pressed_now ? BTN_STATE_ACTIVE : BTN_STATE_INACTIVE);
}

static TaskAction s_debounce_task(debounce_task_fn, 25, INFINITE_TICKS);

static void switch_state_req_handler(char const * const url)
{
    (void)url;
    Serial.println(F("Handling /button/status"));
    send_standard_erm_response();
    http_server_add_body(check_and_clear(s_pressed_flag) ? "PRESSED" : "NOT PRESSED");
}

static http_get_handler s_handlers[] = 
{
    {"/button/status", switch_state_req_handler},
    {"", NULL}
};

void setup()
{
    ethernet_setup(s_handlers);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    BTN_InitHandler(&s_button);

    Serial.begin(115200);

    s_pixels.begin();

    Serial.println("Cave Escape Ethernet Button");
    delay(200);
}

void loop()
{
    s_debug_task.tick();
    s_debounce_task.tick();
    ethernet_tick();    
}

static void handle_serial_cmd(char const * const cmd)
{
    if (cmd[0] == '/')
    {
        http_get_handler * pHandler = http_server_match_handler_url(cmd, s_handlers);
        if (pHandler)
        {
            pHandler->fn(cmd);
        }
        else
        {
            Serial.println("No matching URL");
        }
    }
    else
    {
        Serial.print("Command '");
        Serial.print(cmd);
        Serial.println("' unknown");
    }
}

static char s_serial_buffer[64];
static uint8_t s_bufidx = 0;

void serialEvent()
{
    while (Serial.available())
    {
        char c  = Serial.read();
        if (c == '\n')
        {
            handle_serial_cmd(s_serial_buffer);
            s_bufidx = 0;
            s_serial_buffer[0] = '\0';
        }
        else
        {
            s_serial_buffer[s_bufidx++] = c;
            s_serial_buffer[s_bufidx] = '\0';
        }
    }
}
