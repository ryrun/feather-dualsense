#include "device_out.h"
#include "host_reader.h"
#include "mode.h"

int main() {
  mode::Init();
  device_out::Init();
  host_reader::Init();

  while (true) {
    device_out::Task();
    host_reader::Task();
  }
}
