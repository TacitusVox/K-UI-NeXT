/* SPDX-License-Identifier: GPL-3.0-only */
#define MODEM_MODE_REMOTE 1
#define MODEM_SPEED_V8_AUTO 2
int modem_init(void);
void modem_shutdown(void);
int modem_set_mode(int mode, int speed);
int modem_is_connected(void);
int modem_is_connecting(void);
int modem_read_data(unsigned char *data, int size);
int modem_write_data(unsigned char *data, int size);
int modem_wait_dialtone(int timeout);
int modem_dial(const char *number);
unsigned long modem_get_connection_rate(void);
