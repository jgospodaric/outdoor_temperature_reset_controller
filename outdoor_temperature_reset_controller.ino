#include <MenuSystem.h>
#include <OneWire.h>
#include <EEPROM.h>

#define ADDRESS_SIZE (8)
#define ADDRESS_FIRST_BYTE_INDEX (0)
#define ADDRESS_LAST_BYTE_INDEX (ADDRESS_SIZE - 1)
#define DATA_SIZE (12)
#define EEPROM_SIZE (32)

MenuSystem menu;

Menu menu_root("Outdoor Temperature Reset Controller");

MenuItem menu_scan_temperature_sensors("Scan temperature sensors");

Menu menu_select_temperature_sensors("Set temperature sensors");
MenuItem menu_set_sensor_0_as_boiler("Set sensor 0 as boiler");
MenuItem menu_set_sensor_0_as_outdoor("Set sensor 0 as outdoor");
MenuItem menu_set_sensor_1_as_boiler("Set sensor 1 as boiler");
MenuItem menu_set_sensor_1_as_outdoor("Set sensor 1 as outdoor");

MenuItem menu_reset_eeprom("Reset EEPROM");

MenuItem menu_print_status("Print temperatures and relay status");

OneWire ds(2);

int boiler_sensor_eeprom_address_begin = 1;
int outdoor_sensor_eeprom_address_begin = boiler_sensor_eeprom_address_begin + ADDRESS_SIZE;
int sensor_0_eeprom_address_begin = outdoor_sensor_eeprom_address_begin + ADDRESS_SIZE;
int sensor_1_eeprom_address_begin = sensor_0_eeprom_address_begin + ADDRESS_SIZE;

int burner_relay_pin = 3;
int pump_relay_pin = 4;
int room_pump_request_status_pin = 5;

void setup()
{
  Serial.begin(9600);

  pinMode(burner_relay_pin, OUTPUT);
  digitalWrite(burner_relay_pin, HIGH);
  pinMode(pump_relay_pin, OUTPUT);
  digitalWrite(pump_relay_pin, HIGH);
  pinMode(room_pump_request_status_pin, INPUT);

  menu_root.add_item(&menu_scan_temperature_sensors, &scan_temperature_sensors);

  menu_root.add_menu(&menu_select_temperature_sensors);
  menu_select_temperature_sensors.add_item(&menu_set_sensor_0_as_boiler, set_sensor_0_as_boiler);
  menu_select_temperature_sensors.add_item(&menu_set_sensor_0_as_outdoor, set_sensor_0_as_outdoor);
  menu_select_temperature_sensors.add_item(&menu_set_sensor_1_as_boiler, set_sensor_1_as_boiler);
  menu_select_temperature_sensors.add_item(&menu_set_sensor_1_as_outdoor, set_sensor_1_as_outdoor);
  
  menu_root.add_item(&menu_reset_eeprom, &reset_eeprom_addresses);
  
  menu_root.add_item(&menu_print_status, &print_status);

  menu.set_root_menu(&menu_root);

  display_menu();
}

void scan_temperature_sensors(MenuItem* p_menu_item)
{
  byte address[ADDRESS_SIZE];
  int sensor_eeprom_address_begin = 0;
  int number_of_sensors = 0;
  
  Serial.println("Searching sensors");

  while(ds.search(address))
  {
    switch(number_of_sensors)
    {
    case 0:
      sensor_eeprom_address_begin = sensor_0_eeprom_address_begin;
    case 1:
      sensor_eeprom_address_begin = sensor_1_eeprom_address_begin;
    }

    Serial.print("Found ROM[sensor ");
    Serial.print(number_of_sensors, DEC);
    Serial.print("]");
    Serial.println();
    
    put_address_to_eeprom(address, sensor_eeprom_address_begin);
    print_address(address);

    number_of_sensors += 1;    
  }

  Serial.print("Found ");
  Serial.print(number_of_sensors, DEC);
  Serial.println(" sensors.");
  Serial.println();
}

float get_temperature_from_sensor_ds18x20(byte* address) {
  byte data[DATA_SIZE];
  byte type_s;
  byte present = 0;
  
  ds.reset();
  ds.select(address);
  ds.write(0x44, 0);
  
  if(OneWire::crc8(address, ADDRESS_LAST_BYTE_INDEX) != address[ADDRESS_LAST_BYTE_INDEX])
  {
    Serial.println("Addr. CRC is not valid!");
    return 0.0;
  }
  
  present = ds.reset();
  ds.select(address);
  // Read Scratchpad
  ds.write(0xBE);

  // We need 9 bytes
  for(int i = 0; i < 9; i++)
  {
    data[i] = ds.read();
  }
  
  switch(address[0])
  {
  case 0x10:
    type_s = 1;
    break;
  case 0x28:
    type_s = 0;
    break;
  case 0x22:
    type_s = 0;
    break;
  default:
    Serial.println("Device is not a DS18x20 family device.");
    return 0.0;
  }
  
  int16_t raw = (data[1] << 8) | data[0];
  if(type_s)
  {
    raw = raw << 3; // 9 bit resolution default
    if(data[7] == 0x10)
    {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  else
  {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if(cfg == 0x00)
    {
      raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    }
    else if(cfg == 0x20)
    {
      raw = raw & ~3; // 10 bit res, 187.5 ms
    }
    else if(cfg == 0x40)
    {
      raw = raw & ~1; // 11 bit res, 375 ms
    }
    //// default is 12 bit resolution, 750 ms conversion time
  }

  return (float)raw / 16.0; 
}

void set_sensor_0_as_boiler(MenuItem* p_menu_item)
{
  copy_address_in_eeprom(sensor_0_eeprom_address_begin, boiler_sensor_eeprom_address_begin);

  Serial.println("Sensor 0 set as boiler");
}

void set_sensor_0_as_outdoor(MenuItem* p_menu_item)
{
  copy_address_in_eeprom(sensor_0_eeprom_address_begin, outdoor_sensor_eeprom_address_begin);

  Serial.println("Sensor 0 set as outdoor");
}

void set_sensor_1_as_boiler(MenuItem* p_menu_item)
{
  copy_address_in_eeprom(sensor_1_eeprom_address_begin, boiler_sensor_eeprom_address_begin);

  Serial.println("Sensor 1 set as boiler");
}

void set_sensor_1_as_outdoor(MenuItem* p_menu_item)
{
  copy_address_in_eeprom(sensor_1_eeprom_address_begin, outdoor_sensor_eeprom_address_begin);

  Serial.println("Sensor 1 set as outdoor");
}

void copy_address_in_eeprom(int eeprom_address_source, int eeprom_address_target)
{
  byte tmp_address[ADDRESS_SIZE];

  get_address_from_eeprom(eeprom_address_source, tmp_address);
  put_address_to_eeprom(tmp_address, eeprom_address_target);
}

void get_address_from_eeprom(int eeprom_address_source, byte* address_target)
{
  int byte_index;

  for(byte_index = ADDRESS_FIRST_BYTE_INDEX; byte_index <= ADDRESS_LAST_BYTE_INDEX; byte_index++)
  {
      address_target[byte_index] = EEPROM.read(eeprom_address_source + byte_index);
  }
}

void put_address_to_eeprom(byte* address_source, int eeprom_address_target)
{
  int byte_index;

  for(byte_index = ADDRESS_FIRST_BYTE_INDEX; byte_index <= ADDRESS_LAST_BYTE_INDEX; byte_index++)
  {
    EEPROM.write(eeprom_address_target + byte_index, address_source[byte_index]);
  }
}

void reset_eeprom_addresses(MenuItem* p_menu_item)
{
  int i;
  byte empty_address[ADDRESS_SIZE] = {0x00};

  put_address_to_eeprom(empty_address, boiler_sensor_eeprom_address_begin);
  put_address_to_eeprom(empty_address, outdoor_sensor_eeprom_address_begin);
  put_address_to_eeprom(empty_address, sensor_0_eeprom_address_begin);
  put_address_to_eeprom(empty_address, sensor_1_eeprom_address_begin);

  Serial.println("EEPROM reset");
}

void print_status(MenuItem* p_menu_item)
{
  byte address[ADDRESS_SIZE];
  
  print_eeprom_addresses();
  
  Serial.println("Temperature and relay status");

  get_address_from_eeprom(boiler_sensor_eeprom_address_begin, address);
  Serial.println("Boiler temperature");
  Serial.print(get_temperature_from_sensor_ds18x20(address));
  Serial.println();

  get_address_from_eeprom(outdoor_sensor_eeprom_address_begin, address);
  Serial.println("Outdoor temperature");
  Serial.print(get_temperature_from_sensor_ds18x20(address));
  Serial.println();
  
  Serial.println("Pump room request status");
  Serial.print(digitalRead(room_pump_request_status_pin), HEX);
  Serial.println();
  
  Serial.println("Burner relay status");
  Serial.print(!digitalRead(burner_relay_pin), HEX);
  Serial.println();

  Serial.println("Pump relay status");
  Serial.print(!digitalRead(pump_relay_pin), HEX);
  Serial.println();  
}

void print_eeprom_addresses()
{
  byte address[ADDRESS_SIZE];

  get_address_from_eeprom(boiler_sensor_eeprom_address_begin, address);
  print_address(address);
  
  get_address_from_eeprom(outdoor_sensor_eeprom_address_begin, address);
  print_address(address);
  
  get_address_from_eeprom(sensor_0_eeprom_address_begin, address);
  print_address(address);
  
  get_address_from_eeprom(sensor_1_eeprom_address_begin, address);
  print_address(address);

  Serial.println();
}

void print_address(byte* address)
{
  int byte_index;

  for(byte_index = ADDRESS_FIRST_BYTE_INDEX; byte_index <= ADDRESS_LAST_BYTE_INDEX; byte_index++)
  {
    Serial.print(address[byte_index], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void loop()
{
  serial_handler();
  
  execute_two_step_outdoor_temperature_reset_controller();

  delay(2000);
}

void serial_handler() {
  char input_character;
  if((input_character = Serial.read()) > 0) {
    switch(input_character)
    {
    case 'w': // Previus item
      menu.prev();
      display_menu();
      break;
    case 's': // Next item
      menu.next();
      display_menu();
      break;
    case 'a': // Back presed
      menu.back();
      display_menu();
      break;
    case 'd': // Select presed
      menu.select();
      display_menu();
      break;
    case '?':
    case 'h': // Display help
      print_help();
      break;
    default:
      break;
    }
  }
}

void display_menu() {
  Serial.println("");
  // Display the menu
  Menu const* cp_menu = menu.get_current_menu();

  Serial.print("Current menu name: ");
  Serial.println(cp_menu->get_name());

  MenuComponent const* cp_menu_sel = cp_menu->get_selected();
  for(int i = 0; i < cp_menu->get_num_menu_components(); ++i)
  {
    MenuComponent const* cp_m_comp = cp_menu->get_menu_component(i);
    Serial.print(cp_m_comp->get_name());

    if(cp_menu_sel == cp_m_comp)
    {
      Serial.print("<<< ");
    }

    Serial.println("");
  }
}

void print_help() {
  Serial.println("***************");
  Serial.println("w: go to previus item (up)");
  Serial.println("s: go to next item (down)");
  Serial.println("a: go back (right)");
  Serial.println("d: select \"selected\" item");
  Serial.println("?: print this help");
  Serial.println("h: print this help");
  Serial.println("***************");
}

void execute_two_step_outdoor_temperature_reset_controller()
{
  bool pump_requested_status;
  byte address[ADDRESS_SIZE];
  float outdoor_temperature;
  float set_temperature;
  float boiler_temperature;
  float boiler_set_temperature_ratio;
  float room_set_temperature = 22.0;
  float step_treshhold = 0.05;
  
  pump_requested_status = digitalRead(room_pump_request_status_pin);
  if(pump_requested_status == LOW)
  {
    digitalWrite(burner_relay_pin, HIGH);
    digitalWrite(pump_relay_pin, HIGH);

    return;
  }
  
  get_address_from_eeprom(outdoor_sensor_eeprom_address_begin, address);
  outdoor_temperature = get_temperature_from_sensor_ds18x20(address);
  
  set_temperature = get_set_temperature(outdoor_temperature, room_set_temperature);

  get_address_from_eeprom(boiler_sensor_eeprom_address_begin, address);
  boiler_temperature = get_temperature_from_sensor_ds18x20(address);

  boiler_set_temperature_ratio = boiler_temperature / set_temperature;
  boiler_set_temperature_ratio -= 1.0;
  if(boiler_set_temperature_ratio > step_treshhold)
  {
    digitalWrite(burner_relay_pin, HIGH);
  }
  else
  {
    digitalWrite(burner_relay_pin, LOW);
  }

  digitalWrite(pump_relay_pin, !pump_requested_status);
}

float get_set_temperature(float outdoor_temperature, float room_set_temperature)
{
  // see doc/reset_curves_line_equations.ods in project
  return -1.1 * outdoor_temperature + 42.0 + 0.97 * room_set_temperature - 19.30;
}
