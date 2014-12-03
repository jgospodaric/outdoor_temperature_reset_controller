#include <MenuSystem.h>
#include <OneWire.h>
#include <EEPROM.h>

// Menu variables
MenuSystem menu;

Menu menu_root("Outdoor Temperature Reset Controller");

MenuItem menu_scan_temperature_sensors("Scan temperature sensors");

MenuItem menu_select_temperature_sensors("Select temperature sensors");

MenuItem menu_print_status("Print temperatures and relay status");

Menu menu_manual_burner_relay("Manual burner relay control");
MenuItem menu_manual_burner_relay_on("On");
MenuItem menu_manual_burner_relay_off("Off");

Menu menu_manual_pump_relay("Manual pump relay control");
MenuItem menu_manual_pump_relay_on("On");
MenuItem menu_manual_pump_relay_off("Off");

OneWire  ds(2);

int boiler_sensor_addr = 1;
int outdoor_sensor_addr = boiler_sensor_addr + 7;
int temporary_rom_addr = outdoor_sensor_addr + 7;

int burner_relay = 3;
int pump_relay = 4;

// Menu callback function
// In this example all menu items use the same callback.

void scan_temperature_sensors(MenuItem* p_menu_item)
{
  int i;
  byte addr[8];
  int temporary_rom;
  
  Serial.println("Searching sensors");
  
  temporary_rom = temporary_rom_addr;
  
  while (ds.search(addr)) {
    Serial.print("Found ROM =");
    
    for( i = 0; i < 8; i++) {
      Serial.write(' ');
      
      Serial.print(addr[i], HEX);
      
      EEPROM.write(temporary_rom + i, addr[i]);
    }
    
    temporary_rom += 7;
    
  }
}

float get_temperature(byte* addr) {
  byte data[12];
  byte type_s;
  byte present = 0;
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 0);
  int i;
  
  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("Addr. CRC is not valid!");
      return 0.0;
  }
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }
  
  switch (addr[0]) {
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
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }

  return (float)raw / 16.0; 
}

void select_temperature_sensors(MenuItem* p_menu_item)
{
  Serial.println("Item2 Selected");
  
}

void print_status(MenuItem* p_menu_item)
{
  int i;
  int j;
  byte addr[8];
  
  Serial.println("Temperture and relay status");

  for( i = 0; i < 8; i++) {
    addr[i] = EEPROM.read(boiler_sensor_addr + i);
  }  

  Serial.print('Boiler temperature = ');
  Serial.println(get_temperature(addr));
  
  for( i = 0; i < 8; i++) {
    addr[i] = EEPROM.read(outdoor_sensor_addr + i);
  }
  
  Serial.print('Outdoor temperature = ');
  Serial.println(get_temperature(addr));
}

void manual_burner_relay_on(MenuItem* p_menu_item)
{
  Serial.println("Item3 Selected");
  digitalWrite(burner_relay, HIGH);
}

void manual_burner_relay_off(MenuItem* p_menu_item)
{
  Serial.println("Item3 Selected");
  digitalWrite(burner_relay, LOW);
}

void manual_pump_relay_on(MenuItem* p_menu_item)
{
  Serial.println("Item3 Selected");
  digitalWrite(pump_relay, HIGH);
}

void manual_pump_relay_off(MenuItem* p_menu_item)
{
  Serial.println("Item3 Selected");
  digitalWrite(pump_relay, LOW);
}

// Standard arduino functions

void setup()
{
  Serial.begin(9600);
  serialPrintHelp();
  pinMode(burner_relay, OUTPUT);
  pinMode(pump_relay, OUTPUT);
  

  menu_root.add_item(&menu_scan_temperature_sensors, &scan_temperature_sensors);
  menu_root.add_item(&menu_select_temperature_sensors, &select_temperature_sensors);
  menu_root.add_item(&menu_print_status, &print_status);
  
  menu_root.add_menu(&menu_manual_burner_relay);
  menu_manual_burner_relay.add_item(&menu_manual_burner_relay_on, manual_burner_relay_on);
  menu_manual_burner_relay.add_item(&menu_manual_burner_relay_on, manual_burner_relay_off);
  
  menu_root.add_menu(&menu_manual_pump_relay);
  menu_manual_pump_relay.add_item(&menu_manual_pump_relay_on, &manual_pump_relay_on);
  menu_manual_pump_relay.add_item(&menu_manual_pump_relay_off, &manual_pump_relay_off);
  
  menu.set_root_menu(&menu_root);

  displayMenu();
}

void loop()
{
  // Handle serial commands
  serialHandler();

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
  for (int i = 0; i < cp_menu->get_num_menu_components(); ++i)
  {
    MenuComponent const* cp_m_comp = cp_menu->get_menu_component(i);
    Serial.print(cp_m_comp->get_name());

    if (cp_menu_sel == cp_m_comp)
      Serial.print("<<< ");

    Serial.println("");
  }
}

void serialHandler() {
  char inChar;
  if((inChar = Serial.read())>0) {
    switch (inChar) {
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
