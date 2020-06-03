#include <cassert>
#include <cstdlib>
#include <cstring>
#include <zmq.h>
#include <cstdio>

// Read one event off the monitor socket; return value and address
// by reference, if not null, and event number by value. Returns -1
// in case of error.

static void close_zero_linger(void *socket) {
  int linger = 0;
  zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger));
  zmq_close(socket);
}

static void bounce(void *client, void *server) {
  char send_buf[] = "Hello";
  char recv_buf[sizeof(send_buf) / sizeof(send_buf[0])];

  zmq_send(client, send_buf, sizeof(send_buf), 0);
  zmq_recv(server, recv_buf, sizeof(recv_buf), 0);
  zmq_send(server, send_buf, sizeof(send_buf), 0);
  zmq_recv(client, recv_buf, sizeof(recv_buf), 0);
}

static int get_monitor_event(void *monitor, int *value, char **address) {
  // First frame in message contains event number and value
  zmq_msg_t msg;
  zmq_msg_init(&msg);
  if (zmq_msg_recv(&msg, monitor, 0) == -1)
    return -1; // Interrupted, presumably
  assert(zmq_msg_more(&msg));

  uint8_t *data = (uint8_t *)zmq_msg_data(&msg);
  uint16_t event = *(uint16_t *)(data);
  if (value)
    *value = *(uint32_t *)(data + 2);
  //printf("value=%d\n", *(uint32_t *)(data + 2));

  // Second frame in message contains event address
  zmq_msg_init(&msg);
  if (zmq_msg_recv(&msg, monitor, 0) == -1)
    return -1; // Interrupted, presumably
  assert(!zmq_msg_more(&msg));

  if (address) {
    uint8_t *data = (uint8_t *)zmq_msg_data(&msg);
    size_t size = zmq_msg_size(&msg);
    *address = (char *)malloc(size + 1);
    memcpy(*address, data, size);
    (*address)[size] = 0;
  }
  //printf("address=%s\n", data);
  return event;
}

int main(void) {
  void *ctx = zmq_ctx_new();
  assert(ctx);

  // We'll monitor these two sockets
  void *client = zmq_socket(ctx, ZMQ_DEALER);
  assert(client);
  void *server = zmq_socket(ctx, ZMQ_DEALER);
  assert(server);

  // Monitor all events on client and server sockets
  int rc = zmq_socket_monitor(client, "inproc://monitor-client", ZMQ_EVENT_ALL);
  assert(rc == 0);
  rc = zmq_socket_monitor(server, "inproc://monitor-server", ZMQ_EVENT_ALL);
  assert(rc == 0);

  // Create two sockets for collecting monitor events
  void *client_mon = zmq_socket(ctx, ZMQ_PAIR);
  assert(client_mon);
  void *server_mon = zmq_socket(ctx, ZMQ_PAIR);
  assert(server_mon);

  // Connect these to the inproc endpoints so they'll get events
  rc = zmq_connect(client_mon, "inproc://monitor-client");
  assert(rc == 0);
  rc = zmq_connect(server_mon, "inproc://monitor-server");
  assert(rc == 0);

  // Now do a basic ping test
  rc = zmq_bind(server, "tcp://127.0.0.1:9998");
  assert(rc == 0);
  rc = zmq_connect(client, "tcp://127.0.0.1:9998");
  assert(rc == 0);
  bounce(client, server);

  // Close client and server
  close_zero_linger(client);
  close_zero_linger(server);

  // Now collect and check events from both sockets
  int event = get_monitor_event(client_mon, NULL, NULL);
  if (event == ZMQ_EVENT_CONNECT_DELAYED)
    event = get_monitor_event(client_mon, NULL, NULL);
  assert(event == ZMQ_EVENT_CONNECTED);
  event = get_monitor_event(client_mon, NULL, NULL);
  assert(event == ZMQ_EVENT_MONITOR_STOPPED);

  // This is the flow of server events
  event = get_monitor_event(server_mon, NULL, NULL);
  assert(event == ZMQ_EVENT_LISTENING);
  event = get_monitor_event(server_mon, NULL, NULL);
  assert(event == ZMQ_EVENT_ACCEPTED);
  event = get_monitor_event(server_mon, NULL, NULL);
  assert(event == ZMQ_EVENT_CLOSED);
  event = get_monitor_event(server_mon, NULL, NULL);
  assert(event == ZMQ_EVENT_MONITOR_STOPPED);

  // Close down the sockets
  close_zero_linger(client_mon);
  close_zero_linger(server_mon);
  zmq_ctx_term(ctx);

  return 0;
}
