#include <avr/EEPROM.h>
#include <Interrupts.h>
#include <phys253.h>
#include <LiquidCrystal.h>
#include "vars.h"

// QRD Menu Class
class MenuItem
{
public:
	String    Name;
	uint16_t  Value;
	uint16_t* EEPROMAddress;
	static uint16_t MenuItemCount;
	MenuItem(String name)
	{
		MenuItemCount++;
		EEPROMAddress = (uint16_t*)(2 * MenuItemCount) + 73;
		Name = name;
		Value = eeprom_read_word(EEPROMAddress);
	}
	void Save()
	{
		eeprom_write_word(EEPROMAddress, Value);
	}
};

// IR Menu Class
class IRMenuItem
{
public:
	String    Name;
	uint16_t  Value;
	uint16_t* EEPROMAddress;
	static uint16_t MenuItemCount;
	IRMenuItem(String name)
	{
		MenuItemCount++;
		EEPROMAddress = (uint16_t*)(MenuItemCount) + 56; // offset the EEPROMAddress
		Name = name;
		Value = eeprom_read_word(EEPROMAddress);
	}
	void Save()
	{
		eeprom_write_word(EEPROMAddress, Value);
	}
};

// Main Menu
class MainMenuItem
{
public:
	String Name;
	static uint16_t MenuItemCount;
	MainMenuItem(String name)
	{
		MenuItemCount++;
		Name = name;
	}
	static void Open(int index)
	{
		switch (index) {
		case 0:
			// set pid mode for testing
			mode = modes[modeIndex];
			if (mode == modes[0]) {
				// tape following mode
				pidfn = tapePID;
			} else if (mode == modes[1]) {
				// ir following mode
				pidfn = irPID;
			}
			break;
		case 1:
			QRDMENU();
			break;
		case 2:
			IRMENU();
			break;
		case 3:
			pivot(val);
			break;
		case 4:
			travel(val,FORWARDS);
			break;
		}
	}
};

/* Add the menu items */
uint16_t MenuItem::MenuItemCount = 0;
MenuItem Speed        	  = MenuItem("Speed");
MenuItem ProportionalGain = MenuItem("P-gain");
MenuItem DerivativeGain   = MenuItem("D-gain");
MenuItem IntegralGain     = MenuItem("I-gain");
MenuItem ThresholdVoltage = MenuItem("T-volt");
MenuItem menuItems[]      = {Speed, ProportionalGain, DerivativeGain, IntegralGain, ThresholdVoltage};

uint16_t IRMenuItem::MenuItemCount = 0;
IRMenuItem IRProportionalGain = IRMenuItem("P-gain");
IRMenuItem IRDerivativeGain   = IRMenuItem("D-gain");
IRMenuItem IRIntegralGain     = IRMenuItem("I-gain");
IRMenuItem IRThreshold		  = IRMenuItem("Threshold");
IRMenuItem IRmenuItems[]      = {IRProportionalGain, IRDerivativeGain, IRIntegralGain, IRThreshold};

uint16_t MainMenuItem::MenuItemCount = 0;
MainMenuItem Sensors      = MainMenuItem("Sensors");
MainMenuItem TapePID      = MainMenuItem("Tape PID");
MainMenuItem IRPID        = MainMenuItem("IR PID");
MainMenuItem pivotTest      = MainMenuItem("pivotTest");
MainMenuItem travelTest        = MainMenuItem("travelTest");
MainMenuItem mainMenu[]   = {Sensors, TapePID, IRPID, pivotTest, travelTest};

void setup()
{
#include <phys253setup.txt>
	//Serial.begin(9600);
	LCD.clear(); LCD.home();

	// set gains
	base_speed = menuItems[0].Value;
	q_pro_gain = menuItems[1].Value;
	q_diff_gain = menuItems[2].Value;
	q_int_gain = menuItems[3].Value;
	q_threshold = menuItems[4].Value;
	ir_pro_gain = IRmenuItems[0].Value;
	ir_diff_gain = IRmenuItems[1].Value;
	ir_int_gain = IRmenuItems[2].Value;
	ir_threshold = IRmenuItems[3].Value;

	enableExternalInterrupt(INT1, RISING);
	enableExternalInterrupt(INT2, RISING);
	attachISR(INT1, LE);
	attachISR(INT2, RE);

	// default PID loop is QRD tape following
	pidfn = tapePID;

	LCD.print("RC2"); LCD.setCursor(0, 1);
	LCD.print("Press Start.");
	while (!startbutton()) {};
	LCD.clear();
	MainMenu();
}

void loop()
{
	// Check for menu command
	if (startbutton() && stopbutton()) {
		// Pause motors
		motor.stop_all();
		MainMenu();
		// Restart motors
		motor.speed(LEFT_MOTOR, base_speed);
		motor.speed(RIGHT_MOTOR, base_speed);
	}
	pidfn();
}

/* Control Loops */
void tapePID() {
	left_sensor = analogRead(QRD_L);
	right_sensor = analogRead(QRD_R);

	if (left_sensor > q_threshold && right_sensor > q_threshold)
		error = 0; // both sensors on black
	else if (left_sensor > q_threshold && right_sensor < q_threshold)
		error = -1;	// left sensor on black
	else if (left_sensor < q_threshold && right_sensor > q_threshold)
		error = 1; // right sensor on black
	else if (left_sensor < q_threshold && right_sensor < q_threshold)
	{
		// neither sensor on black. check last error to see which side we are on.
		if ( last_error > 0)
			error = 5;
		else if ( last_error < 0)
			error = -5;
	}

	if ( !(error == last_error))
	{
		recent_error = last_error;
		to = t;
		t = 1;
	}

	P_error = q_pro_gain * error;
	D_error = q_diff_gain * ((float)(error - recent_error) / (float)(t + to)); // time is present within the differential gain
	I_error += q_int_gain * error;
	net_error = P_error + D_error + I_error;

	// prevent adjusting errors from going over actual speed.
	net_error = constrain(net_error, -base_speed, base_speed);

	//if net error is positive, right_motor will be stronger, will turn to the left
	motor.speed(LEFT_MOTOR, base_speed + net_error);
	motor.speed(RIGHT_MOTOR, base_speed - net_error);

	if ( count == 100 ) {
		count = 0;
		LCD.clear(); LCD.home();
		LCD.print("LQ:"); LCD.print(left_sensor);
		LCD.print(" LM:"); LCD.print(base_speed + net_error);
		LCD.setCursor(0, 1);
		LCD.print("RQ:"); LCD.print(right_sensor);
		LCD.print(" RM:"); LCD.print(base_speed - net_error);
	}

	last_error = error;
	count++;
	t++;
}

void irPID() {
	left_sensor = analogRead(IR_L);
	right_sensor = analogRead(IR_R);
	difference = right_sensor - left_sensor;
	average = (left_sensor + right_sensor) >> 3;
	error = difference;

	P_error = (ir_pro_gain) * error;
	D_error = ir_diff_gain * (error - last_error);
	I_error += ir_int_gain * error;
	net_error = static_cast<int32_t>(P_error + D_error + I_error) >> 4;
	Serial.print(average); Serial.print(" ");
	Serial.println(net_error);
	//Serial.print(D_error); Serial.print(" ");

	// Limit max error
	//net_error = constrain(net_error, -base_speed, base_speed);
	// TODO: Divide gain by average.
	//if net error is positive, right_motor will be stronger, will turn to the left
	motor.speed(LEFT_MOTOR, base_speed + net_error);
	motor.speed(RIGHT_MOTOR, base_speed - net_error);

	if ( count == 100 ) {
		count = 0;
		LCD.clear(); LCD.home();
		LCD.print("L:"); LCD.print(left_sensor);
		LCD.print(" R:"); LCD.print(right_sensor);
		LCD.setCursor(0, 1);
		LCD.print("ERR:"); LCD.print(net_error);
		//LCD.print("LM:"); LCD.print(base_speed + net_error); LCD.print(" RM:"); LCD.print(base_speed - net_error);
	}

	last_error = error;
	count++;
}

// Set arm vertical height
void setArmVert(int V) {
	armControlV = V;
}

// Keep arm vertically in place. Should be run along with PID.
void armVertControl() {
	int currentV = analogRead(ARM_POT);
	int diff = currentV - armControlV;
	diff = constrain(diff, -100, 100);
	motor.speed(ARM_MOTOR, diff);
}

/* Helper Functions */
// Stop driving
void pauseDrive() {
	motor.stop(LEFT_MOTOR);
	motor.stop(RIGHT_MOTOR);
}

// Pivot the robot for a certain number of encoder
// counts on both. UNTESTED.
void pivot(int counts)
{
	pivotCount = counts;
	pivotEncountStart_L = encount_L;
	pivotEncountStart_R = encount_R;
	lastPivotTime = millis();
	if (counts < 0) {
		motor.speed(RIGHT_MOTOR, STABLE_SPEED);
		motor.speed(LEFT_MOTOR, -STABLE_SPEED);
		attachTimerInterrupt(2000, pivotCheck);
	} else if (counts > 0) {
		motor.speed(RIGHT_MOTOR, -STABLE_SPEED);
		motor.speed(LEFT_MOTOR, STABLE_SPEED);
		attachTimerInterrupt(2000, pivotCheck);
	}
}

// Pivot in a direction d for a time t.
void timedPivot(uint32_t t, int d) {
	if ( d == LEFT) {
		motor.speed(RIGHT_MOTOR, STABLE_SPEED);
		motor.speed(LEFT_MOTOR, -STABLE_SPEED);
	} else if (d == RIGHT) {
		motor.speed(RIGHT_MOTOR, -STABLE_SPEED);
		motor.speed(LEFT_MOTOR, STABLE_SPEED);
	}
	delay(t);
	pauseDrive(); //TODO: change to use a timer interrupt
}

// Travel in a direction d for a number of counts.
void travel(int counts, int d) {
	travelCount = counts;
	travelEncountStart_L = encount_L;
	travelEncountStart_R = encount_R;
	lastTravelTime = millis();
	// TODO: one motor may need a power offset to travel straight
	motor.speed(RIGHT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	motor.speed(LEFT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	attachTimerInterrupt(2000, travelCheck);
}

// Travel in a direction d for a time t.
void timedTravel( uint32_t t, int d) {
	motor.speed(RIGHT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	motor.speed(LEFT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	delay(t);
	pauseDrive(); //TODO: change to use a timer interrupt
}


/* ISRs */

// Encoder ISRs. S signifies measuring of speed
void LE() {
	encount_L++;
}

void RE() {
	encount_R++;
}

void LES() {
	encount_L++;
	s_L = millis() - time_L;
	time_L = millis();
}

void RES() {
	encount_R++;
	s_R = millis() - time_R;
	time_R = millis();
}

/* Timer ISRs */

void pivotCheck()
{
	if (encount_L - pivotEncountStart_L >= pivotCount) {
		motor.stop(LEFT_MOTOR);
	}
	if (encount_R - pivotEncountStart_R >= pivotCount) {
		motor.stop(RIGHT_MOTOR);
	}
	if (encount_L - pivotEncountStart_L >= pivotCount
	        && encount_R - pivotEncountStart_R >= pivotCount) {
		detachTimerInterrupt();
		lastPivotTime -= millis();
	}
}

void travelCheck()
{
	if (encount_L - travelEncountStart_L >= travelCount) {
		motor.stop(LEFT_MOTOR);
	}
	if (encount_R - travelEncountStart_R >= travelCount) {
		motor.stop(RIGHT_MOTOR);
	}
	if (encount_L - travelEncountStart_L >= travelCount
	        && encount_R - travelEncountStart_R >= travelCount) {
		detachTimerInterrupt();
		lastTravelTime -= millis();
	}
}

/* Menus */
void QRDMENU()
{
	LCD.clear(); LCD.home();
	LCD.print("Entering submenu");
	delay(500);

	while (true)
	{
		/* Show MenuItem value and knob value */
		int menuIndex = knob(6) * (MenuItem::MenuItemCount) >> 10;
		LCD.clear(); LCD.home();
		LCD.print(menuItems[menuIndex].Name); LCD.print(" "); LCD.print(menuItems[menuIndex].Value);
		LCD.setCursor(0, 1);
		LCD.print("Set to "); LCD.print(menuIndex != 0 ? knob(7) : knob(7) >> 2); LCD.print("?");
		delay(100);

		/* Press start button to save the new value */
		if (startbutton())
		{
			delay(100);
			int val = knob(7); // cache knob value to memory
			if (menuIndex == 0) {
				val = val >> 2;
				LCD.clear(); LCD.home();
				LCD.print("Speed set to "); LCD.print(val);
				delay(250);
			}

			menuItems[menuIndex].Value = val;
			menuItems[menuIndex].Save();
			delay(250);
		}


		/* Press stop button to exit menu */
		if (stopbutton())
		{
			delay(100);
			if (stopbutton())
			{
				LCD.clear(); LCD.home();
				LCD.print("Leaving menu");
				// Set values after exiting menu
				base_speed = menuItems[0].Value;
				q_pro_gain = menuItems[1].Value;
				q_diff_gain = menuItems[2].Value;
				q_int_gain = menuItems[3].Value;
				q_threshold = menuItems[4].Value;
				delay(500);
				return;
			}
		}
	}
}

void IRMENU()
{
	LCD.clear(); LCD.home();
	LCD.print("Entering submenu");
	delay(500);

	while (true)
	{
		/* Show IRMenuItem value and knob value */
		int menuIndex = knob(6) * (IRMenuItem::MenuItemCount) >> 10;
		LCD.clear(); LCD.home();
		LCD.print(IRmenuItems[menuIndex].Name); LCD.print(" "); LCD.print(IRmenuItems[menuIndex].Value);
		LCD.setCursor(0, 1);
		LCD.print("Set to "); LCD.print(knob(7)); LCD.print("?");
		delay(100);

		/* Press start button to save the new value */
		if (startbutton())
		{
			delay(100);
			int val = knob(7); // cache knob value to memory
			IRmenuItems[menuIndex].Value = val;
			IRmenuItems[menuIndex].Save();
			delay(250);
		}

		/* Press stop button to exit menu */
		if (stopbutton())
		{
			delay(100);
			if (stopbutton())
			{
				LCD.clear(); LCD.home();
				LCD.print("Leaving menu");
				// set values
				ir_pro_gain = IRmenuItems[0].Value;
				ir_diff_gain = IRmenuItems[1].Value;
				ir_int_gain = IRmenuItems[2].Value;
				ir_threshold = IRmenuItems[3].Value;
				delay(500);
				return;
			}
		}
	}
}

void MainMenu() {
	LCD.clear(); LCD.home();
	LCD.print("Entering Main");
	delay(500);

	while (true)
	{
		/* Display submenu or pid mode */
		int menuIndex = knob(6) * (MainMenuItem::MenuItemCount) >> 10;
		LCD.clear(); LCD.home();
		if (menuIndex == 0) {
			// mode switching handling
			if (mode == "qrd") {
				LCD.print("LQ:"); LCD.print(analogRead(QRD_L)); LCD.print(" RQ:"); LCD.print(analogRead(QRD_R));
			}  else if (mode == "ir") {
				LCD.print("L:"); LCD.print(analogRead(IR_L)); LCD.print(" R:"); LCD.print(analogRead(IR_R));
			} else {
				LCD.print("Error: no mode");
			}
			modeIndex = (knob(7) * (sizeof(modes) / sizeof(*modes))) >> 10; // sizeof(a)/sizeof(*a) gives length of array
			LCD.setCursor(0, 1);
			LCD.print(modes[modeIndex]); LCD.print("?");
		} else if (menuIndex == 3) {
			LCD.print(mainMenu[modeIndex].Name);
			LCD.setCursor(0, 1);
			val = (knob(7) >> 2) - 128;
			LCD.print(val); LCD.print("?");
		} else if ( menuIndex == 4) {
			LCD.print(mainMenu[modeIndex].Name);
			LCD.setCursor(0, 1);
			val = map(knob(7), 0, 1023, 0, 3069);
			LCD.print(val); LCD.print("?");
		} else {
			// generic submenu handling
			LCD.print(mainMenu[menuIndex].Name);
			LCD.setCursor(0, 1);
			LCD.print("Start to Select.");
		}
		delay(100);

		/* Press start button to enter submenu / switch pid modes */
		if (startbutton())
		{
			LCD.clear(); LCD.home();
			MainMenuItem::Open(menuIndex);
		}

		/* Press stop button to exit menu */
		if (stopbutton())
		{
			delay(100);
			if (stopbutton())
			{
				LCD.clear(); LCD.home();
				LCD.print("Leaving menu");
				delay(500);
				// reset variables and counters
				count = 0;
				t = 1;
				last_error = 0;
				recent_error = 0;
				I_error = 0;
				LCD.clear();
				return;
			}
		}
	}
}