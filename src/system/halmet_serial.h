#ifndef HALMET_SRC_HALMET_SERIAL_H_
#define HALMET_SRC_HALMET_SERIAL_H_

#include <esp_mac.h>

#include <cstdint>

namespace halmet {
    extern uint64_t GetBoardSerialNumber();
}

#endif  // HALMET_SRC_HALMET_SERIAL_H_
