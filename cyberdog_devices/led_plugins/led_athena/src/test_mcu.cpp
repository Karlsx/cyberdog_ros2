
#include "common_device/common_device.hpp"
/*
class acc
{
public:
  u_int8_t OFF;
  u_int8_t ON;
  u_int8_t BREAKTH;
  u_int8_t BLINK;
};
*/
class acc
{
public:
  u_int8_t OFF;
  u_int8_t ON;
  u_int8_t BREAKTH;
  u_int8_t BLINK;
};


int main(int argc, char ** argv)
{
  //void(argc);
  //void(argv);


  // for_send mode
  common_device::device<acc> device_2("/home/mi/dev_ws/src/cyberdog_ros2/cyberdog_devices/led_plugins/led_athena/example.toml");
  device_2.LINK_VAR(device_2.GetData()->OFF);
  device_2.LINK_VAR(device_2.GetData()->ON);
  device_2.LINK_VAR(device_2.GetData()->BREAKTH);
  device_2.LINK_VAR(device_2.GetData()->BLINK);


  std::shared_ptr acc_data = device_2.GetData();
  acc_data->OFF= 0x00;
  acc_data->ON= 0x01;
  acc_data->BREAKTH= 0x02;
  acc_data->BLINK= 0x03;
  device_2.SendSelfData();

  return 0;
}