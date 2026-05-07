#include "device_out.h"
#include "host_reader.h"
#include "mode.h"
#include "status_hid.h"

int main() {
  mode::Init();
  device_out::Init();
  status_hid::Init();
  host_reader::Init();

  while (true) {
    device_out::Task();
    host_reader::Task();
    status_hid::Task();
  }
}
