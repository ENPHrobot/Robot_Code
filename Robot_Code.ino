#include <avr/EEPROM.h>
#include <Interrupts.h>
#include <Servo.h>
#include <phys253.h>
#include <LiquidCrystal.h>
#include "Robot_Code.h"

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
		boolean f = true;
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
			travel(val, FORWARDS);
			break;
		case 5:
			delay(1000);
			LCD.clear(); LCD.home();
			LCD.print("R U SURE~");
			LCD.setCursor(0, 1); LCD.print("PRESS START");
			while (f) {
				if ( startbutton()) {
					f = false;
					launch(val);
				} else if (stopbutton()) {
					f = false;
				}
			}
			break;
		case 6:
			delay(250);
			armCal();
			LCD.clear(); LCD.home();
			LCD.print("Returning...");
			delay(300);
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
MenuItem HorThreshold = MenuItem("H-volt");
MenuItem menuItems[]      = {Speed, ProportionalGain, DerivativeGain, IntegralGain, ThresholdVoltage, HorThreshold};

uint16_t IRMenuItem::MenuItemCount = 0;
IRMenuItem IRProportionalGain = IRMenuItem("P-gain");
IRMenuItem IRDerivativeGain   = IRMenuItem("D-gain");
IRMenuItem IRIntegralGain     = IRMenuItem("I-gain");
IRMenuItem IRmenuItems[]      = {IRProportionalGain, IRDerivativeGain, IRIntegralGain};

uint16_t MainMenuItem::MenuItemCount = 0;
MainMenuItem Sensors     = MainMenuItem("Sensors");
MainMenuItem TapePID     = MainMenuItem("Tape PID");
MainMenuItem IRPID       = MainMenuItem("IR PID");
MainMenuItem pivotTest   = MainMenuItem("pivotTest");
MainMenuItem travelTest  = MainMenuItem("travelTest");
MainMenuItem launchTest  = MainMenuItem("launchTest");
MainMenuItem armTest  = MainMenuItem("armTest");
MainMenuItem mainMenu[]  = {Sensors, TapePID, IRPID, pivotTest, travelTest, launchTest, armTest};

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
	h_threshold = menuItems[5].Value;
	ir_pro_gain = IRmenuItems[0].Value;
	ir_diff_gain = IRmenuItems[1].Value;
	ir_int_gain = IRmenuItems[2].Value;

	// attach servos
	pinMode(SERVO_0_Pin, OUTPUT);
	pinMode(SERVO_1_Pin, OUTPUT);
	pinMode(SERVO_2_Pin, OUTPUT);
	RCServo0.attach(SERVO_0_Pin);
	RCServo1.attach(SERVO_1_Pin);
	RCServo2.attach(SERVO_2_Pin);

	// set ports 8 to 15 as OUTPUT
	portMode(1, OUTPUT);
	// ensure relays are LOW on start.
	digitalWrite(LAUNCH_F, LOW);

	// attach external interrupts on encoder pins
	enableExternalInterrupt(ENC_L, RISING);
	enableExternalInterrupt(ENC_R, RISING);
	/*attachISR(ENC_L, LE);
	attachISR(ENC_R, RE);*/

	// if testing overall speed
	attachISR(ENC_L, LES);
	attachISR(ENC_R, RES);
	time_L = millis();
	time_R = millis();
	lastSpeedUp = 0;

	// set servo initial positions
	RCServo2.write(90);
	RCServo0.write(75);

	// default PID loop is QRD tape following
	pidfn = tapePID;

	LCD.print("RC5"); LCD.setCursor(0, 1);
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

	speedControl();

	int left_sensor = analogRead(QRD_L);
	int right_sensor = analogRead(QRD_R);
	int error = 0;

	if (left_sensor > q_threshold && right_sensor > q_threshold)
		error = 0; // both sensors on black
	else if (left_sensor > q_threshold && right_sensor <= q_threshold)
		error = -1;	// left sensor on black
	else if (left_sensor <= q_threshold && right_sensor > q_threshold)
		error = 1; // right sensor on black
	else if (left_sensor <= q_threshold && right_sensor <= q_threshold)
	{
		// neither sensor on black. check last error to see which side we are on.
		if ( last_error > 0)
			error = 4;
		else if ( last_error < 0)
			error = -4;
	}

	if (checkPet()) {

		pauseDrive();
		LCD.clear(); LCD.home();
		petCount++;

		// TODO: pet pickup actions
		if (petCount == 1) {
			getFirstPet();
			// upon exit, apply correcting negative error so that robot returns to line
			while (!stopbutton()) {} // check getFirstPet
			error = -3;
		} else if (petCount == 2) {

			error = -3;
		} else if (petCount == 3) {
			// for pausing motors on the ramp.
			motor.speed(LEFT_MOTOR, 50);
			motor.speed(RIGHT_MOTOR, 50);

			error = 1;
		} else if (petCount == 4) {
			// TODO: implement more elegant switching to ir
			encount_L = 0;
			encount_R = 0;
			switchMode();
		}
		armCal();

		// speed control
		if (petCount == 2 ) {
			lastSpeedUp = millis();
		} else if (petCount == 3 ) {
			lastSpeedUp = millis();
		}

		LCD.clear(); LCD.home();
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
		/*LCD.print("LQ:"); LCD.print(left_sensor);
		LCD.print(" LM:"); LCD.print(base_speed + net_error);
		LCD.setCursor(0, 1);
		LCD.print("RQ:"); LCD.print(right_sensor);
		LCD.print(" RM:"); LCD.print(base_speed - net_error);*/
		LCD.print("LE:"); LCD.print(encount_L); LCD.print(" RE:"); LCD.print(encount_R);
		LCD.setCursor(0, 1); LCD.print(s_L); LCD.print(" "); LCD.print(s_R); LCD.print(" "); LCD.print((s_L + s_R) / 2);
	}

	last_error = error;
	count++;
	t++;
}

void irPID() {

	processfn();

	int left_sensor = analogRead(IR_L);
	int right_sensor = analogRead(IR_R);
	int error = right_sensor - left_sensor;
	int average = (left_sensor + right_sensor) >> 3;

	P_error = (ir_pro_gain) * error;
	D_error = ir_diff_gain * (error - last_error);
	I_error += ir_int_gain * error;
	net_error = static_cast<int32_t>(P_error + D_error + I_error) >> 4;
	Serial.print(average); Serial.print(" ");
	Serial.println(net_error);
	//Serial.print(D_error); Serial.print(" ");

	// Limit max error
	net_error = constrain(net_error, -base_speed, base_speed);

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

/* Helper Functions */

void switchMode() {
	pidfn = irPID;
}

// Set arm vertical height
void setUpperArm(int V) {
	upperArmV = V;
}

void setLowerArm(int V) {
	lowerArmV = V;
}

// Keep arm vertically in place. Should be run along with PID.
void upperArmPID() {
	int currentV = constrain(analogRead(UPPER_POT), 300, 740);
	int diff = currentV - upperArmV;
	if ( diff <= 10 && diff >= -10) {
		diff = 0;
	}
	diff = 2 * diff;
	diff = constrain(diff, -255, 255);
	motor.speed(UPPER_ARM, diff);
}

void lowerArmPID() {
	int currentV = constrain(analogRead(LOWER_POT), 350, 600);
	int diff = currentV - lowerArmV;
	if (diff <= 10 && diff >= -10) {
		diff = 0;
	}
	diff = 3 * (diff);
	diff = constrain(diff, -255, 255);
	motor.speed(LOWER_ARM, diff);
}

// Stop driving
void pauseDrive() {
	motor.stop(LEFT_MOTOR);
	motor.stop(RIGHT_MOTOR);
}

// Power the catapult for a time ms.
void launch(int ms) {
	// start catapult motion (relay on)
	digitalWrite(LAUNCH_F, HIGH);
	delay(ms);
	// stop catapult motion (relay off)
	digitalWrite(LAUNCH_F, LOW);
}

// Checks for the horizontal line that signals a pet to pick up.
boolean checkPet() {
	int e = analogRead(QRD_LINE);
	if ( e > h_threshold && onTape == false) {
		onTape = true;
		return true;
	} else if ( e < q_threshold ) {
		onTape = false;
	}
	return false;
}

// Checks if it is time to pick up the pet on the rafter in IR following
boolean checkRafterPet() {
	// TODO: needs tuning
	if ( encount_L >= ENC_RAFTER && encount_R >= ENC_RAFTER ) {
		return true;
	}
	return false;
}

// Check if robot has followed to the end, where the box is
boolean checkBoxedPet() {
	if (digitalRead(FRONT_SWITCH) == HIGH) {
		return true;
	}
	return false;
}

// Check if switch on arm is activated
boolean petOnArm() {
	if (digitalRead(HAND_SWITCH) == LOW) {
		return true;
	}
	return false;
}

// Pivot the robot for a specified number of encoder
// counts on both motors.
void pivot(int counts) {
	// flags to check whether motors have stopped or not
	boolean lflag = false, rflag = false;
	// cache starting values
	int pivotCount = abs(counts);
	int pivotEncountStart_L = encount_L;
	int pivotEncountStart_R = encount_R;
	lastPivotTime = millis();

	motor.speed(RIGHT_MOTOR, counts > 0 ? -STABLE_SPEED : STABLE_SPEED);
	motor.speed(LEFT_MOTOR, counts > 0 ? STABLE_SPEED : -STABLE_SPEED);
	while (lflag == false || rflag == false) {
		if (encount_L - pivotEncountStart_L >= pivotCount) {
			motor.stop(LEFT_MOTOR);
			lflag = true;
		}
		if (encount_R - pivotEncountStart_R >= pivotCount) {
			motor.stop(RIGHT_MOTOR);
			rflag = true;
		}
	}

}

// Turn robot left (counts < 0) or right (counts > 0) for
// certain amount of encoder counts forward
void turnForward(int counts) {
	boolean flag = false;
	int turnCount = abs(counts);
	int turnEncountStart_L = encount_L;
	int turnEncountStart_R = encount_R;

	if (counts < 0) {
		motor.stop(LEFT_MOTOR);
		motor.speed(RIGHT_MOTOR, STABLE_SPEED);
		while (flag == false) {
			if (encount_R - turnEncountStart_R >= turnCount) {
				motor.stop(RIGHT_MOTOR);
				flag = true;
			}
		}
	} else if (counts > 0) {
		motor.stop(RIGHT_MOTOR);
		motor.speed(LEFT_MOTOR, STABLE_SPEED);
		while (flag == false) {
			if (encount_L - turnEncountStart_L >= turnCount) {
				motor.stop(LEFT_MOTOR);
				flag = true;
			}
		}
	}
}

// Turn robot left (counts < 0) or right (counts > 0) for
// certain amount of encoder counts backward
void turnBack(int counts) {
	boolean flag = false;
	int turnCount = abs(counts);
	int turnEncountStart_L = encount_L;
	int turnEncountStart_R = encount_R;

	if (counts < 0) {
		motor.speed(RIGHT_MOTOR, -STABLE_SPEED);
		motor.stop(LEFT_MOTOR);
		while (flag == false) {
			if (encount_R - turnEncountStart_R >= turnCount) {
				motor.stop(RIGHT_MOTOR);
				flag = true;
			}
		}
	} else if (counts > 0) {
		motor.stop(RIGHT_MOTOR);
		motor.speed(LEFT_MOTOR, -STABLE_SPEED);
		while (flag == false) {
			if (encount_L - turnEncountStart_L >= turnCount) {
				motor.stop(LEFT_MOTOR);
				flag = true;
			}
		}
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
	pauseDrive();
}

// Travel in a direction d for a number of counts.
void travel(int counts, int d) {

	boolean lflag = false, rflag = false;
	int travelCount = counts;
	int travelEncountStart_L = encount_L;
	int travelEncountStart_R = encount_R;
	lastTravelTime = millis();

	// TODO: one motor may need a power offset to travel straight
	motor.speed(RIGHT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	motor.speed(LEFT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	while (lflag == false || rflag == false) {
		if (encount_L - travelEncountStart_L >= travelCount) {
			motor.stop(LEFT_MOTOR);
			lflag = true;
		}
		if (encount_R - travelEncountStart_R >= travelCount) {
			motor.stop(RIGHT_MOTOR);
			rflag = true;
		}
	}
}

// Travel in a direction d for a time t.
void timedTravel( uint32_t t, int d) {
	motor.speed(RIGHT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	motor.speed(LEFT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	delay(t);
	pauseDrive();
}

// Changes base speed depending on how fast encoder counts are triggered.
void speedControl() {
	// TODO: see if reducing speed up rate is required
	if (petCount == 2 && millis() - lastSpeedUp > 800) {
		//base_speed = constrain(base_speed + 10, 0, 255);
		base_speed = 180;
		//lastSpeedUp = millis();
	} else if (petCount == 3 && millis() - lastSpeedUp > 1650) {
		base_speed = menuItems[0].Value;
		/*// reset ISRs and processfn
		attachISR(ENC_L, LE);
		attachISR(ENC_R, RE);
		processfn = empty;*/
	}
}

// processfn for first IR segment
void rafterProcess() {
	if (checkRafterPet()) {
		pauseDrive();
		int a1;
		int a2;
		petCount++;
		// TODO: rafter pet pickup here
		while (stopbutton()) {
			armCal();
		}
		processfn = buriedProcess;
	}
}

// processfn for second IR segment
void buriedProcess() {
	if (checkBoxedPet()) {
		pauseDrive();
		int a1;
		int a2;
		petCount++;
		// TODO: buried pet pickup here
		while (stopbutton()) {
			armCal();
		}
	}
}

//Function to get first pet
void getFirstPet() {
	boolean flag = false;
	uint32_t timeStart = millis();
	int c = 0;

	// first stage pickup - pick up pet; checks if pet is picked up,
	// if not, pick up pet again
	RCServo0.write(25);
	delay(200);
	while (!flag) {
		upperArmPID();
		lowerArmPID();

		unsigned int dt = millis() - timeStart;
		// TODO: may be able to set lower upper arm at same time as pivot
		if ( dt >= 1000 && c == 0 ) {
			setLowerArm(600);
			c++;
		} else if ( dt >= 3000 && c == 1 ) {
			setUpperArm(400);
			c++;
		} else if ( dt >= 4000 && c == 2 ) {
			setUpperArm(740);
			c++;
		} else if ( dt >= 6000 ) {
			if (petOnArm()) {
				flag = true;
				delay(1000);
			} else {
				c = 1;
				timeStart = millis();
			}
		}
	}
	placePetCatapult(25);
}

// Place pet in catapult from pivot arm's position 'pivot'
void placePetCatapult(int pivot) {
	int pivotIncrement = 10;
	boolean flag = false;
	int c = 0;

	while ( pivot < 165 ) {
		pivot += pivotIncrement;
		pivot = constrain(pivot, 25, 165);
		RCServo0.write(pivot);
		delay(150);
	}

	delay(1000);
	uint32_t timeStart = millis();
	while (!flag) {
		lowerArmPID();
		upperArmPID();

		unsigned int dt = millis() - timeStart;
		if ( dt >= 1000 && c == 0 ) {
			setLowerArm(500);
			c++;
		}
		else if ( dt >= 2000) {
			RCServo2.write(0);
			delay(100);
			RCServo2.write(90);
			flag = true;
		}
	}
	delay(1000);
	RCServo0.write(65);
}

// testing arm calibration code
void armCal() {
	int a;
	int s = 90; // temp
	int c = 0;

	while (!stopbutton()) {
		// temporary arm calibration code
		int selection = map(knob(6), 0 , 1023, 0, 2);

		if (selection == 0) {
			a = map(knob(7), 0 , 1023, 0 , 184);
		} else if (selection == 1) {
			a = map(knob(7), 0, 1023, 350, 600); // lower arm
		} else if ( selection == 2) {
			a = map(knob(7), 0 , 1023, 300, 740); // higher arm
		}

		if ( c >= 100) {
			c = 0;
			LCD.clear(); LCD.home();
			if (selection == 0)
				LCD.print("PIVOT ARM:");
			else if (selection == 1) {
				LCD.print("LOWER ARM: "); LCD.print(analogRead(LOWER_POT));
			}
			else if (selection == 2) {
				LCD.print("UPPER ARM:"); LCD.print(analogRead(UPPER_POT));
			}

			LCD.setCursor(0, 1); LCD.print(a); LCD.print("? S:");
			if (selection == 0)
				LCD.print(s);
			else if (selection == 1)
				LCD.print(lowerArmV);
			else if (selection == 2)
				LCD.print(upperArmV);
		}

		if (startbutton()) {
			delay(200);
			if (selection == 0) {
				s = a;
				RCServo0.write(s);
			} else if (selection == 1) {
				setLowerArm(a);
			} else if (selection == 2) {
				setUpperArm(a);
			}
		}

		if (digitalRead(FRONT_SWITCH) == LOW) {
			RCServo2.write(0);
			delay(500);
			RCServo2.write(90);
		}

		// move arm
		upperArmPID();
		lowerArmPID();
		c++;
	}
}

/* ISRs */

// Encoder ISRs. S == speed
void LE() {
	encount_L++;
}

void RE() {
	encount_R++;
}

void LES() {
	encount_L++;
	int ct = millis() - time_L;
	// filter out speeds less than 10 ms
	s_L = ct > 10 ? ct : s_L;
	time_L = millis();
}

void RES() {
	encount_R++;
	int ct = millis() - time_R;
	s_R = ct > 10 ? ct : s_R;
	time_R = millis();
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
		LCD.print("Set to ");
		if (menuIndex == 0 || menuIndex == 1 || menuIndex == 2) {
			LCD.print(knob(7) >> 2);
		} else {
			LCD.print(knob(7));
		}

		LCD.print("?");

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
			} else if (menuIndex == 1 || menuIndex == 2) {
				val = val >> 2;
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
				h_threshold = menuItems[5].Value;
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
				LCD.setCursor(0, 1); LCD.print("HQ:"); LCD.print(analogRead(QRD_LINE));
			}  else if (mode == "ir") {
				LCD.print("L:"); LCD.print(analogRead(IR_L)); LCD.print(" R:"); LCD.print(analogRead(IR_R));
			} else {
				LCD.print("Error: no mode");
			}
			modeIndex = (knob(7) * (sizeof(modes) / sizeof(*modes))) >> 10; // sizeof(a)/sizeof(*a) gives length of array
			LCD.setCursor(9, 1);
			LCD.print(modes[modeIndex]); LCD.print("?");
		} else if (menuIndex == 3) {
			// pivot test menu option
			LCD.print(mainMenu[menuIndex].Name);
			LCD.setCursor(0, 1);
			val = (knob(7) >> 2) - 128;
			LCD.print(val); LCD.print("?");
		} else if ( menuIndex == 4) {
			// travel test menu option
			LCD.print(mainMenu[menuIndex].Name);
			LCD.setCursor(0, 1);
			val = map(knob(7), 0, 1023, 0, 3069);
			LCD.print(val); LCD.print("?");
		} else if (menuIndex == 5) {
			// launch catapult test menu option
			LCD.print(mainMenu[menuIndex].Name);
			LCD.setCursor(0, 1);
			val = knob(7) >> 1;
			LCD.print(val); LCD.print("?");
		} else {
			// generic submenu handling
			LCD.print(mainMenu[menuIndex].Name);
			LCD.setCursor(0, 1);
			LCD.print("Start to Select.");
		}

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
				base_speed = menuItems[0].Value;
				onTape = false;
				petCount = 0;
				count = 0;
				t = 1;
				last_error = 0;
				recent_error = 0;
				I_error = 0;
				LCD.clear();
				return;
			}
		}
		delay(150);
	}
}

