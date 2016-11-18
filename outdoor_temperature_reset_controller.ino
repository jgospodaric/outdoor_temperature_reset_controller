#include <MenuSystem.h>
#include <OneWire.h>
#include <EEPROM.h>

#define ADDRESS_SIZE (8)
#define ADDRESS_FIRST_BYTE_INDEX (0)
#define ADDRESS_LAST_BYTE_INDEX (ADDRESS_SIZE - 1)
#define DATA_SIZE (12)
#define EEPROM_SIZE (32)
#define SLEEP_MS (2000)
#define MIN_TIME_BURNER_STATE_OFF (600000L)

MenuSystem menu;

Menu menu_root("Outdoor Temperature Reset Controller");

MenuItem menu_scan_temperature_sensors("Scan for temperature sensors");

Menu menu_select_temperature_sensors("Set temperature sensors");
MenuItem menu_set_sensor_0_as_boiler("Set sensor 0 as boiler");
MenuItem menu_set_sensor_0_as_outdoor("Set sensor 0 as outdoor");
MenuItem menu_set_sensor_1_as_boiler("Set sensor 1 as boiler");
MenuItem menu_set_sensor_1_as_outdoor("Set sensor 1 as outdoor");

MenuItem menu_reset_eeprom("Reset EEPROM");

MenuItem menu_print_status("Print status");

OneWire ds(2);

int boiler_sensor_eeprom_address_begin = 1;
int outdoor_sensor_eeprom_address_begin = boiler_sensor_eeprom_address_begin + ADDRESS_SIZE;
int sensor_0_eeprom_address_begin = outdoor_sensor_eeprom_address_begin + ADDRESS_SIZE;
int sensor_1_eeprom_address_begin = sensor_0_eeprom_address_begin + ADDRESS_SIZE;

int burner_relay_pin = 3;
int pump_relay_pin = 4;
int room_pump_request_status_pin = 5;
long timer_burner_state_off_ms = 0L;

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
  byte address[ADDRESS_SIZE] = {0x00};
  int sensor_eeprom_address_begin = 0;
  int sensor_id, number_of_sensors = 0;
  char output_print_line[80] = {0x00};
  
  Serial.println("Searching sensors");

  while(ds.search(address))
  {
    switch(sensor_id)
    {
    case 0:
      sensor_eeprom_address_begin = sensor_0_eeprom_address_begin;
    case 1:
      sensor_eeprom_address_begin = sensor_1_eeprom_address_begin;
    }

    sprintf(output_print_line, "Found ROM[sensor %d]\n", number_of_sensors);
    Serial.print(output_print_line);
      
    put_address_to_eeprom(address, sensor_eeprom_address_begin);
    print_address(address);

    sensor_id += 1;
  }

  number_of_sensors = sensor_id;

  sprintf(output_print_line, "Found %d sensors.\n", number_of_sensors);
  Serial.print(output_print_line);
}

float get_temperature_from_sensor_ds18x20(byte* address) {
  byte data[DATA_SIZE] = {0x00};
  byte type_s = 0x00;
  byte present = 0x00;
  
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
  byte tmp_address[ADDRESS_SIZE] = {0x00};

  get_address_from_eeprom(eeprom_address_source, tmp_address);
  put_address_to_eeprom(tmp_address, eeprom_address_target);
}

void get_address_from_eeprom(int eeprom_address_source, byte* address_target)
{
  for(int byte_index = ADDRESS_FIRST_BYTE_INDEX; byte_index <= ADDRESS_LAST_BYTE_INDEX; byte_index++)
  {
      address_target[byte_index] = EEPROM.read(eeprom_address_source + byte_index);
  }
}

void put_address_to_eeprom(byte* address_source, int eeprom_address_target)
{
  for(int byte_index = ADDRESS_FIRST_BYTE_INDEX; byte_index <= ADDRESS_LAST_BYTE_INDEX; byte_index++)
  {
    EEPROM.write(eeprom_address_target + byte_index, address_source[byte_index]);
  }
}

void reset_eeprom_addresses(MenuItem* p_menu_item)
{
  byte empty_address[ADDRESS_SIZE] = {0x00};

  put_address_to_eeprom(empty_address, boiler_sensor_eeprom_address_begin);
  put_address_to_eeprom(empty_address, outdoor_sensor_eeprom_address_begin);
  put_address_to_eeprom(empty_address, sensor_0_eeprom_address_begin);
  put_address_to_eeprom(empty_address, sensor_1_eeprom_address_begin);

  Serial.println("EEPROM reset");
}

void print_status(MenuItem* p_menu_item)
{
  byte address[ADDRESS_SIZE] = {0x00};
  char output_print_line[80] = {0x00};

  print_eeprom_addresses();
  
  Serial.println("Temperature and relay status");

  get_address_from_eeprom(boiler_sensor_eeprom_address_begin, address);
  sprintf(output_print_line, "Boiler temperature %0.2f\n", get_temperature_from_sensor_ds18x20(address));
  Serial.print(output_print_line);

  get_address_from_eeprom(outdoor_sensor_eeprom_address_begin, address);
  Serial.println("Outdoor temperature");
  sprintf(output_print_line, "Boiler temperature %0.2f\n", get_temperature_from_sensor_ds18x20(address));
  Serial.print(output_print_line);
  
  sprintf(output_print_line, "Pump room request status %s\n", boolean_to_on_off_string(is_pump_requested()));
  Serial.print(output_print_line);
  
  sprintf(output_print_line, "Burner relay status %s\n", boolean_to_on_off_string(!digitalRead(burner_relay_pin)));
  Serial.print(output_print_line);


  Serial.print(output_print_line);
}

const char * boolean_to_on_off_string(boolean boolean_var)
{
  return boolean_var ? "on" : "off";
}

void print_eeprom_addresses()
{
  byte address[ADDRESS_SIZE] = {0x00};

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
  char test[20];
  int byte_index = ADDRESS_FIRST_BYTE_INDEX;
  char output_print_line[80] = {0x00};
  
  for(; byte_index <= ADDRESS_LAST_BYTE_INDEX; byte_index++)
  {
    sprintf(output_print_line + byte_index * 3, "%02X ", address[byte_index]);
  }
  sprintf(output_print_line + byte_index * 3, "\n");
  Serial.print(output_print_line);
}

void loop()
{
  serial_handler();
  
  execute_two_step_outdoor_temperature_reset_controller();

  if(timer_burner_state_off_ms > 0L)
  {
    timer_burner_state_off_ms -= long SLEEP_MS;
  }

  delay(SLEEP_MS);
}

void serial_handler() {
  char input_character = 0x00;
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
  byte address[ADDRESS_SIZE] = {0x00};
  float outdoor_temperature = 0.0;
  float set_temperature = 0.0;
  float boiler_temperature = 0.0;
  float boiler_set_temperature_ratio = 0.0;
  float room_set_temperature = 22.0;
  float burner_step_treshold = 0.05;
  float pump_step_treshold = 0.02;

  if(is_pump_requested())
  {
    turn_off_burner();
    turn_off_pump();

    return;
  }
  
  get_address_from_eeprom(outdoor_sensor_eeprom_address_begin, address);
  outdoor_temperature = get_temperature_from_sensor_ds18x20(address);
  
  set_temperature = get_set_temperature(outdoor_temperature, room_set_temperature);

  get_address_from_eeprom(boiler_sensor_eeprom_address_begin, address);
  boiler_temperature = get_temperature_from_sensor_ds18x20(address);

  boiler_set_temperature_ratio = boiler_temperature / set_temperature;
  boiler_set_temperature_ratio -= 1.0;
  if(boiler_set_temperature_ratio >= burner_step_treshold)
  {
    turn_off_burner();
  }
  else
  {
    turn_on_burner();
  }

  if(is_pump_requested() && boiler_set_temperature_ratio >= pump_step_treshold)
  {
    turn_on_pump();
  }
  else if(!is_pump_requested())
  {
    turn_off_pump();
  }
}

float get_set_temperature(float outdoor_temperature, float room_set_temperature)
{
  float slope = -1.1;
  float constant_term_b = 46.2;
  float correction_slope = 0.967;
  float correction_constant_term_b = -19.33;

  // see doc/reset_curves_line_equations.ods in project
  return slope * outdoor_temperature + \
    constant_term_b + \
    correction_slope * room_set_temperature - \
    correction_constant_term_b;
}

void turn_on_burner()
{
  if(timer_burner_state_off_ms > 0)
  {
    return;
  }

  digitalWrite(burner_relay_pin, LOW);
}

void turn_off_burner()
{
  if(digitalRead(burner_relay_pin) == LOW)
  {
    timer_burner_state_off_ms = MIN_TIME_BURNER_STATE_OFF;
  }

  digitalWrite(burner_relay_pin, HIGH);
}

bool is_pump_requested()
{
  bool pump_requested_status = false;

  pump_requested_status = digitalRead(room_pump_request_status_pin);

  return pump_requested_status == HIGH;
}

void turn_on_pump()
{
  digitalWrite(pump_relay_pin, LOW);
}

void turn_off_pump()
{
  digitalWrite(pump_relay_pin, HIGH);
}
