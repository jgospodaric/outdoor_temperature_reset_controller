#include <MenuSystem.h>
#include <OneWire.h>
#include <EEPROM.h>

#define ADDRESS_SIZE (8)
#define LAST_ADDRESS_BYTE (ADDRESS_SIZE - 1)
#define DATA_SIZE (12)
#define EEPROM_SIZE (32)

// Menu variables
MenuSystem menu;

Menu menu_root("Outdoor Temperature Reset Controller");

MenuItem menu_scan_temperature_sensors("Scan temperature sensors");

Menu menu_select_temperature_sensors("Select temperature sensors");
MenuItem menu_set_sensor_0_as_outdoor("Set sensor 0 as outdoor");
MenuItem menu_set_sensor_0_as_boiler("Set sensor 0 as boiler");
MenuItem menu_set_sensor_1_as_outdoor("Set sensor 1 as outdoor");
MenuItem menu_set_sensor_1_as_boiler("Set sensor 1 as boiler");

MenuItem menu_reset_eeprom("Reset EEPROM");

MenuItem menu_print_status("Print temperatures and relay status");

OneWire  ds(2);

int boiler_sensor_addr = 1;
int outdoor_sensor_addr = boiler_sensor_addr + ADDRESS_SIZE;
int temporary_rom_addr = outdoor_sensor_addr + ADDRESS_SIZE;

int burner_relay = 3;
int pump_relay = 4;
int room_pump_request_status = 5;

void scan_temperature_sensors(MenuItem* p_menu_item)
{
  int i;
  byte addr[ADDRESS_SIZE];
  int temporary_rom;
  int number_of_sensors;
  
  Serial.println("Searching sensors");
  
  temporary_rom = temporary_rom_addr;
  number_of_sensors = 0;

  while(ds.search(addr)) {
    Serial.print("Found ROM[sensor ");
    Serial.print(number_of_sensors, DEC);
    Serial.print("] = ");
    Serial.println();
    
    for(i = 0; i < ADDRESS_SIZE; i++) {
      Serial.write(" ");

      Serial.print(addr[i], HEX);
      
      EEPROM.write(temporary_rom + i, addr[i]);
    }
    Serial.println();
    
    temporary_rom += ADDRESS_SIZE;
    number_of_sensors += 1;
    
  }

  Serial.print("Found ");
  Serial.print(number_of_sensors, DEC);
  Serial.println(" sensors.");
  Serial.println();
}

float get_temperature(byte* addr) {
  byte data[DATA_SIZE];
  byte type_s;
  byte present = 0;
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 0);
  int i;
  
  if(OneWire::crc8(addr, LAST_ADDRESS_BYTE) != addr[LAST_ADDRESS_BYTE]) {
      Serial.println("Addr. CRC is not valid!");
      return 0.0;
  }
  
  present = ds.reset();
  ds.select(addr);
  // Read Scratchpad
  ds.write(0xBE);

  // We need 9 bytes
  for(i = 0; i < 9; i++) {
    data[i] = ds.read();
  }
  
  switch(addr[0]) {
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
  if(type_s) {
    raw = raw << 3; // 9 bit resolution default
    if(data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if(cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if(cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if(cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }

  return (float)raw / 16.0; 
}

void get_address_from_eeprom(int eeprom_addr_source, byte* address_target)
{
  int byte_index;

  for(byte_index = 0; byte_index < ADDRESS_SIZE; byte_index++)
  {
      address_target[byte_index] = EEPROM.read(eeprom_address_source + byte_index);
  }
}


void put_address_to_eeprom(byte* address_source, int eeprom_address_target)
{
  int byte_index;

  for(byte_index = 0; byte_index < ADDRESS_SIZE; byte_index++)
  {
    EEPROM.write(eeprom_address_target + byte_index, address_source[byte_index]);
  }
}


void set_sensor_0_as_outdoor(MenuItem* p_menu_item)
{
  int i;
  byte addr[ADDRESS_SIZE];

  get_address_from_eeprom(temporary_rom_addr, addr);
  put_address_to_eeprom(addr, outdoor_sensor_addr);

  Serial.println("Sensor 0 set as outdoor");
}

void set_sensor_0_as_boiler(MenuItem* p_menu_item)
{
  int i;
  byte addr[ADDRESS_SIZE];
  
  for(i = 0; i < ADDRESS_SIZE; i++)
  {
      addr[i] = EEPROM.read(temporary_rom_addr + i);
      EEPROM.write(boiler_sensor_addr + i, addr[i]);
  }

  Serial.println("Sensor 0 set as boiler");
}

void set_sensor_1_as_outdoor(MenuItem* p_menu_item)
{
  int i;
  byte addr[ADDRESS_SIZE];
  
  for(i = 0; i < ADDRESS_SIZE; i++)
  {
      addr[i] = EEPROM.read(temporary_rom_addr + 8 + i);
      EEPROM.write(outdoor_sensor_addr + i, addr[i]);
  }

  Serial.println("Sensor 1 set as outdoor");
}

void set_sensor_1_as_boiler(MenuItem* p_menu_item)
{
  int i;
  byte addr[ADDRESS_SIZE];
  
  for(i = 0; i < ADDRESS_SIZE; i++)
  {
      addr[i] = EEPROM.read(temporary_rom_addr + 8 + i);
      EEPROM.write(boiler_sensor_addr + i, addr[i]);
  }

  Serial.println("Sensor 1 set as boiler");
}

void print_eeprom()
{
  int i;
  byte value;
  
  for(i = 0; i < EEPROM_SIZE; i++)
  {
    if(i > 0 && (i % ADDRESS_SIZE == 0))
    {
      Serial.println();
    }
    value = EEPROM.read(boiler_sensor_addr + i);
    Serial.print(value, HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void reset_eeprom(MenuItem* p_menu_item)
{
  int i;
  byte empty_value = 0x00;
  byte empty_address[8] = {0x00};

  put_address_to_eeprom(boiler_sensor_addr, empty_address);
  put_address_to_eeprom(outdoor_sensor_addr, empty_address);
  put_address_to_eeprom(temporary_rom_address, empty_address);
  put_address_to_eeprom(temporary_rom_address + ADDRESS_SIZE, empty_address);

  Serial.println("EEPROM reset");
}

void print_status(MenuItem* p_menu_item)
{
  int i;
  int j;
  byte addr[ADDRESS_SIZE];
  
  print_eeprom();
  
  Serial.println("Temperature and relay status");

  get_address_from_eeprom(boiler_sensor_addr, addr);
  Serial.println("Boiler temperature");
  Serial.print(get_temperature(addr));
  Serial.println();

  get_address_from_eeprom(outdoor_sensor_addr, addr);
  Serial.println("Outdoor temperature = ");
  Serial.print(get_temperature(addr));
  Serial.println();
  
  Serial.println("Pump request status");
  Serial.print(digitalRead(room_pump_request_status), HEX);
  Serial.println();
  
  Serial.println("Burner relay status");
  Serial.print(!digitalRead(burner_relay), HEX);
  Serial.println();

  Serial.println("Pump relay status");
  Serial.print(!digitalRead(pump_relay), HEX);
  Serial.println();  
}

// Standard arduino functions

void setup()
{
  Serial.begin(9600);
  serialPrintHelp();
  pinMode(burner_relay, OUTPUT);
  digitalWrite(burner_relay, HIGH);
  pinMode(pump_relay, OUTPUT);
  digitalWrite(pump_relay, HIGH);
  pinMode(room_pump_request_status, INPUT);

  menu_root.add_item(&menu_scan_temperature_sensors, &scan_temperature_sensors);

  menu_root.add_menu(&menu_select_temperature_sensors);
  menu_select_temperature_sensors.add_item(&menu_set_sensor_0_as_outdoor, set_sensor_0_as_outdoor);
  menu_select_temperature_sensors.add_item(&menu_set_sensor_0_as_boiler, set_sensor_0_as_boiler);
  menu_select_temperature_sensors.add_item(&menu_set_sensor_1_as_outdoor, set_sensor_1_as_outdoor);
  menu_select_temperature_sensors.add_item(&menu_set_sensor_1_as_boiler, set_sensor_1_as_boiler);
  
  menu_root.add_item(&menu_reset_eeprom, &reset_eeprom);
  
  menu_root.add_item(&menu_print_status, &print_status);

  menu.set_root_menu(&menu_root);

  displayMenu();
}

void loop()
{
  // Handle serial commands
  serialHandler();
  
  two_step_controller();

  // Wait for two seconds so the output is viewable
  delay(2000);
}

void displayMenu() {
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
      Serial.print("<<< ");

    Serial.println("");
  }
}

void serialHandler() {
  char inChar;
  if((inChar = Serial.read())>0) {
    switch(inChar) {
    case 'w': // Previus item
      menu.prev();
      displayMenu();
      break;
    case 's': // Next item
      menu.next();
      displayMenu();
      break;
    case 'a': // Back presed
      menu.back();
      displayMenu();
      break;
    case 'd': // Select presed
      menu.select();
      displayMenu();
      break;
    case '?':
    case 'h': // Display help
      serialPrintHelp();
      break;
    default:
      break;
    }
  }
}

void serialPrintHelp() {
  Serial.println("***************");
  Serial.println("w: go to previus item (up)");
  Serial.println("s: go to next item (down)");
  Serial.println("a: go back (right)");
  Serial.println("d: select \"selected\" item");
  Serial.println("?: print this help");
  Serial.println("h: print this help");
  Serial.println("***************");
}

void two_step_controller()
{
  bool pump_requested_status;
  byte addr[ADDRESS_SIZE];
  int i;
  float outdoor_temperature;
  float set_temperature;
  float boiler_temperature;
  float boiler_set_temperature_ratio;
  float room_set_temperature = 22.0;
  float step_treshhold = 0.05;
  
  pump_requested_status = digitalRead(room_pump_request_status);
  if(pump_requested_status == LOW)
  {
    digitalWrite(burner_relay, HIGH);
    digitalWrite(pump_relay, HIGH);

    return;
  }
  
  get_address_from_eeprom(outdoor_sensor_addr, addr);
  outdoor_temperature = get_temperature(addr);
  
  set_temperature = get_set_temperature(outdoor_temperature, room_set_temperature);

  get_address_from_eeprom(boiler_sensor_addr, addr);
  boiler_temperature = get_temperature(addr);

  boiler_set_temperature_ratio = boiler_temperature / set_temperature;
  boiler_set_temperature_ratio -= 1.0;
  if(boiler_set_temperature_ratio > step_treshhold)
  {
    digitalWrite(burner_relay, HIGH);
  }
  else
  {
    digitalWrite(burner_relay, LOW);
  }

  digitalWrite(pump_relay, !pump_requested_status);
}

float get_set_temperature(float outdoor_temperature, float room_set_temperature)
{
  return -1.1 * outdoor_temperature + 42.0 + 0.97 * room_set_temperature - 19.30;
}
