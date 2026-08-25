#include "ssh_terminal.h"
#include "settings.h"
#include "network.h"
#include <WiFi.h>
#include <libssh/libssh.h>

static SemaphoreHandle_t terminal_mutex;
static String terminal_output;
static String pending_command;
static TaskHandle_t terminal_task_handle;
static constexpr int terminal_columns = 80;
static constexpr int terminal_rows = 24;
static char terminal_screen[terminal_rows][terminal_columns];
static int terminal_cursor_x;
static int terminal_cursor_y;
static int terminal_saved_x;
static int terminal_saved_y;
static int terminal_escape_state;
static String terminal_csi;

static void terminal_clear_line(int row, int first, int last) {
  for (int column = first; column <= last; column++) terminal_screen[row][column] = ' ';
}
static void terminal_clear_screen() {
  for (int row = 0; row < terminal_rows; row++) terminal_clear_line(row, 0, terminal_columns - 1);
  terminal_cursor_x = terminal_cursor_y = 0;
}
static int csi_value(int index, int fallback) {
  int start = 0;
  int current = 0;
  for (int position = 0; position <= terminal_csi.length(); position++) {
    if (position == terminal_csi.length() || terminal_csi[position] == ';') {
      if (current == index) {
        String value = terminal_csi.substring(start, position);
        return value.length() ? value.toInt() : fallback;
      }
      current++;
      start = position + 1;
    }
  }
  return fallback;
}
static void terminal_scroll() {
  if (terminal_cursor_y < terminal_rows) return;
  for (int row = 1; row < terminal_rows; row++) memcpy(terminal_screen[row - 1], terminal_screen[row], terminal_columns);
  terminal_clear_line(terminal_rows - 1, 0, terminal_columns - 1);
  terminal_cursor_y = terminal_rows - 1;
}
static void terminal_csi_command(char command) {
  int value = csi_value(0, 1);
  if (command == 'A') terminal_cursor_y = max(0, terminal_cursor_y - value);
  else if (command == 'B') terminal_cursor_y = min(terminal_rows - 1, terminal_cursor_y + value);
  else if (command == 'C') terminal_cursor_x = min(terminal_columns - 1, terminal_cursor_x + value);
  else if (command == 'D') terminal_cursor_x = max(0, terminal_cursor_x - value);
  else if (command == 'G') terminal_cursor_x = min(terminal_columns - 1, max(0, value - 1));
  else if (command == 'd') terminal_cursor_y = min(terminal_rows - 1, max(0, value - 1));
  else if (command == 'H' || command == 'f') {
    terminal_cursor_y = min(terminal_rows - 1, max(0, csi_value(0, 1) - 1));
    terminal_cursor_x = min(terminal_columns - 1, max(0, csi_value(1, 1) - 1));
  } else if (command == 'J') {
    if (value == 2 || value == 3) terminal_clear_screen();
    else if (value == 0) { terminal_clear_line(terminal_cursor_y, terminal_cursor_x, terminal_columns - 1); for (int row = terminal_cursor_y + 1; row < terminal_rows; row++) terminal_clear_line(row, 0, terminal_columns - 1); }
  } else if (command == 'K') {
    if (value == 2) terminal_clear_line(terminal_cursor_y, 0, terminal_columns - 1);
    else if (value == 1) terminal_clear_line(terminal_cursor_y, 0, terminal_cursor_x);
    else terminal_clear_line(terminal_cursor_y, terminal_cursor_x, terminal_columns - 1);
  } else if (command == 'P') {
    int count = min(value, terminal_columns - terminal_cursor_x);
    memmove(&terminal_screen[terminal_cursor_y][terminal_cursor_x], &terminal_screen[terminal_cursor_y][terminal_cursor_x + count], terminal_columns - terminal_cursor_x - count);
    terminal_clear_line(terminal_cursor_y, terminal_columns - count, terminal_columns - 1);
  } else if (command == 's') { terminal_saved_x = terminal_cursor_x; terminal_saved_y = terminal_cursor_y; }
  else if (command == 'u') { terminal_cursor_x = terminal_saved_x; terminal_cursor_y = terminal_saved_y; }
}
static void terminal_put(char character) {
  if (terminal_escape_state == 1) {
    if (character == '[') { terminal_escape_state = 2; terminal_csi = ""; }
    else if (character == '7') { terminal_saved_x = terminal_cursor_x; terminal_saved_y = terminal_cursor_y; terminal_escape_state = 0; }
    else if (character == '8') { terminal_cursor_x = terminal_saved_x; terminal_cursor_y = terminal_saved_y; terminal_escape_state = 0; }
    else terminal_escape_state = 0;
    return;
  }
  if (terminal_escape_state == 2) {
    if (character >= '@' && character <= '~') { terminal_csi_command(character); terminal_escape_state = 0; }
    else if (terminal_csi.length() < 24) terminal_csi += character;
    return;
  }
  if (character == '\x1b') { terminal_escape_state = 1; return; }
  if (character == '\r') { terminal_cursor_x = 0; return; }
  if (character == '\n') { terminal_cursor_y++; terminal_scroll(); return; }
  if (character == '\b') { terminal_cursor_x = max(0, terminal_cursor_x - 1); return; }
  if (character == '\t') { terminal_cursor_x = min(terminal_columns - 1, (terminal_cursor_x / 8 + 1) * 8); return; }
  if (character < 0x20) return;
  terminal_screen[terminal_cursor_y][terminal_cursor_x] = character;
  terminal_cursor_x++;
  if (terminal_cursor_x >= terminal_columns) { terminal_cursor_x = 0; terminal_cursor_y++; terminal_scroll(); }
}
static void append_output(const char *text, size_t length) {
  for (size_t index = 0; index < length; index++) terminal_put(text[index]);
  String rendered;
  for (int row = 0; row < terminal_rows; row++) {
    int last = terminal_columns - 1;
    while (last >= 0 && terminal_screen[row][last] == ' ') last--;
    if (last >= 0) {
      char line[terminal_columns + 1];
      memcpy(line, terminal_screen[row], last + 1);
      line[last + 1] = '\0';
      rendered += line;
    }
    if (row < terminal_rows - 1) rendered += '\n';
  }
  if (xSemaphoreTake(terminal_mutex, portMAX_DELAY) == pdTRUE) {
    terminal_output = rendered;
    xSemaphoreGive(terminal_mutex);
  }
}

static void ssh_task(void *) {
  for (;;) {
    // Wait for user to click Connect button
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    if (!g_settings.sshHost.length() || WiFi.status() != WL_CONNECTED) {
      append_output("\nSSH is not configured or WiFi not connected\n", 45);
      continue;
    }
    
    network_lock();
    ssh_session session = ssh_new();
    ssh_channel channel = nullptr;
    bool connected = false;
    if (session) {
      int port = g_settings.sshPort;
      ssh_options_set(session, SSH_OPTIONS_HOST, g_settings.sshHost.c_str());
      ssh_options_set(session, SSH_OPTIONS_PORT, &port);
      ssh_options_set(session, SSH_OPTIONS_USER, g_settings.sshUser.c_str());
      if (ssh_connect(session) == SSH_OK && ssh_userauth_password(session, nullptr, g_settings.sshPassword.c_str()) == SSH_AUTH_SUCCESS) {
        channel = ssh_channel_new(session);
        connected = channel && ssh_channel_open_session(channel) == SSH_OK &&
                    ssh_channel_request_pty_size(channel, "xterm", 80, 24) == SSH_OK &&
                    ssh_channel_request_shell(channel) == SSH_OK;
      }
    }
    if (connected) {
      append_output("\nSSH connected\n", 16);
      for (;;) {
        String command;
        if (xSemaphoreTake(terminal_mutex, portMAX_DELAY) == pdTRUE) {
          command = pending_command;
          pending_command = "";
          xSemaphoreGive(terminal_mutex);
        }
        if (command.length()) {
          command += "\n";
          ssh_channel_write(channel, command.c_str(), command.length());
        }
        char buffer[256];
        int length = ssh_channel_read_timeout(channel, buffer, sizeof(buffer) - 1, 0, 50);
        if (length > 0) { buffer[length] = '\0'; append_output(buffer, length); }
        if (ssh_channel_is_eof(channel)) break;
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    } else {
      append_output("\nSSH connection failed\n", 23);
    }
    if (channel) { ssh_channel_send_eof(channel); ssh_channel_close(channel); ssh_channel_free(channel); }
    if (session) { ssh_disconnect(session); ssh_free(session); }
    // hold the lock for the whole session so weather/stocks/calendar can't race SSH crypto ops for heap
    network_unlock();
  }
}

void ssh_terminal_begin() {
  terminal_mutex = xSemaphoreCreateMutex();
  terminal_clear_screen();
  terminal_output = "SSH terminal\nConfigure host and credentials, then click CONNECT button.";
  xTaskCreatePinnedToCore(ssh_task, "ssh_terminal", 16384, nullptr, 1, &terminal_task_handle, 0);
}
void ssh_terminal_tick() {}
String ssh_terminal_output() {
  String copy;
  if (terminal_mutex && xSemaphoreTake(terminal_mutex, portMAX_DELAY) == pdTRUE) { copy = terminal_output; xSemaphoreGive(terminal_mutex); }
  return copy;
}
void ssh_terminal_send(const String &command) {
  if (terminal_mutex && xSemaphoreTake(terminal_mutex, portMAX_DELAY) == pdTRUE) { pending_command = command; xSemaphoreGive(terminal_mutex); }
}
void ssh_terminal_connect_now() {
  if (terminal_task_handle) xTaskNotifyGive(terminal_task_handle);
}