#ifndef APP_H
#define APP_H

#include <stdint.h>

enum class NodeMode : uint8_t {
  Idle,
};

struct AppState {
  NodeMode mode;
};

void appInit();
void appTick(uint32_t nowMs);

#endif  // APP_H
