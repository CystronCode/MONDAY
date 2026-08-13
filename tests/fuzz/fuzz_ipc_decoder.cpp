// tests/fuzz/fuzz_ipc_decoder.cpp
#include "src/generated/monday_ipc_v1.pb.h"
#include <stddef.h>
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    monday::ipc::v1::MondayEnvelope envelope;
    // Attempt to parse the raw byte buffer as a Protobuf message.
    // This will exercise Protobuf parsing logic and detect buffer overflows/crashes.
    (void)envelope.ParseFromArray(data, size);
    return 0;
}
