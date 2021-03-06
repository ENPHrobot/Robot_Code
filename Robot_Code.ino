#include <avr/EEPROM.h>
#include <phys253_TEST.h>
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
			turnForward(val, 130);
			break;
		case 10:
			//travel(val, FORWARDS);
			fullRun = (fullRun ? false : true);
			LCD.clear(); LCD.home();
			LCD.print(fullRun);
			delay(500);
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
		case 7:
			strategySelection();
			break;
		case 8:
			dropPetCtrl(val);
			LCD.clear(); LCD.home(); LCD.print("Turning...");
			delay(300);
			dropPetCtrl(STOP);
			break;
		case 9:
			qrdRead();
			break;
		case 4:
			travel(2, BACKWARDS);
			delay(300);
			turnForward(-15, 90);
			delay(300);
			if (isRedBoard) {
				travel(5, BACKWARDS);
			} else {
				travel(4, BACKWARDS);
			}
			delay(300);
			turnBack(-4, 150);
			delay(300);
			fastTravel(4, BACKWARDS, 140);
			getSixthPet();
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
MainMenuItem travelTest  = MainMenuItem("change mode");
MainMenuItem launchTest  = MainMenuItem("launchTest");
MainMenuItem armTest  = MainMenuItem("armTest");
MainMenuItem strategy = MainMenuItem("strategy");
MainMenuItem hand = MainMenuItem("hand motor");
MainMenuItem qrdTest = MainMenuItem("QRDs");
MainMenuItem parallelPark = MainMenuItem("Driver Test");
MainMenuItem mainMenu[]  = {Sensors, TapePID, IRPID, pivotTest, parallelPark, launchTest, armTest, strategy, hand, qrdTest, travelTest};

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
	fullRun = eeprom_read_word(FULLRUN_EEPROM);

	// set ports 8 to 15 as OUTPUT
	portMode(1, OUTPUT);
	// ensure relays/digital outputs are LOW on start.
	digitalWrite(LAUNCH_F, LOW);
	digitalWrite(HAND_UP, LOW);
	digitalWrite(HAND_DOWN, LOW);

	lastSpeedUp = 0;
	time_L = millis();
	time_R = millis();

	// set servo initial positions
	RCServo2.write(90);
	RCServo0.write(85);

	// default PID loop is QRD tape following
	pidfn = tapePID;

	int counter = 0;

	while (!startbutton()) {
		lowerArmPID();
		upperArmPID();
		if (counter == 100) {
			counter = 0;
			int knobvalue = knob(7) / 512;
			LCD.clear(); LCD.home();
			LCD.print("RC7 Press Start."); LCD.setCursor(0, 1);
			if ( knobvalue == 0) LCD.print("WHITEBOARD");
			else LCD.print("REDBOARD");
		}
		counter++;
	};
	// choose depending if going on red board or white board
	LCD.clear(); LCD.home();
	isRedBoard = knob(7) / 512 == 0 ? false : true;
	LCD.print( isRedBoard ? "REDBOARD SET" : "WHITEBOARD SET");
	delay(400);
	motor.stop_all();
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
	petProcess();
	pidfn();
}

/* Control Loops */
void tapePID() {

	encoderProcess();

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
		else if ( last_error <= 0)
			error = -4;
	}
	if ( !(error == last_error))
	{
		recent_error = last_error;
		to = t;
		t = 1;
	}

	P_error = q_pro_gain * error;
	D_error = q_diff_gain * ((float)(error - recent_error) / (float)(t + to)); // time is present within the differential gain
	net_error = P_error + D_error;

	// prevent adjusting errors from going over actual speed.
	net_error = constrain(net_error, -base_speed, base_speed);

	//if net error is positive, right_motor will be weaker, will turn to the right
	motor.speed(LEFT_MOTOR, base_speed + net_error);
	motor.speed(RIGHT_MOTOR, base_speed - net_error);

	if ( count == 100 ) {
		count = 0;
		LCD.clear(); LCD.home();
		// LCD.print(petCount);
		// LCD.print(" LQ:"); LCD.print(left_sensor);
		// LCD.print(" LM:"); LCD.print(base_speed + net_error);
		// LCD.setCursor(0, 1);
		// LCD.print("RQ:"); LCD.print(right_sensor);
		// LCD.print(" RM:"); LCD.print(base_speed - net_error);
		LCD.print("LE:"); LCD.print(encount_L); LCD.print(" RE:"); LCD.print(encount_R);
		// LCD.setCursor(0, 1); //LCD.print(s_L); LCD.print(" "); LCD.print(s_R); LCD.print(" ");
		// LCD.print("base:"); LCD.print(base_speed); LCD.print(" ");
		// LCD.print((s_L + s_R) / 2);
	}

	last_error = error;
	count++;
	t++;
}

void irPID() {

	encoderProcess();

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

	// Limit max error
	net_error = constrain(net_error, -base_speed, base_speed);

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
void petProcess() {
	if (checkPet()) {
		pauseDrive();
		petCount++;

		if (petCount == 1) {

			if (fullRun) { // will just tape follow normally if strategy is last three
				getFirstPet();

				// decrease base speed for the turn
				base_speed = 65;

				// initial tape following conditions
				last_error = -1;
				recent_error = 0;
				t = 1;
				to = 0;

			}
		} else if (petCount == 3) {

			if (fullRun) {
				// for pausing motors on the ramp.
				motor.speed(LEFT_MOTOR, base_speed);
				motor.speed(RIGHT_MOTOR, base_speed);
				delay(100);

				turnForward(-2, 130);
				motor.speed(LEFT_MOTOR, 40);
				motor.speed(RIGHT_MOTOR, 40);

				if (petOnArm()) {
					placeSecondPet();
				}
				delay(300);
				getThirdPet();

				base_speed = 170;

				// reset tape following conditions
				last_error = 1;
				t = 1;
				to = 0;
				recent_error = 0;
			} else {
				base_speed = 170;
			}
			encount_L = 0;
			encount_R = 0;

		} else if (petCount == 4)  { //Slows down after detecting top of ramp
			lastSpeedUp = millis();

		} else if (petCount == LAST_TAPE_PET) {

			getFourthPet();

			encount_L = 0;
			encount_R = 0;
			switchMode();

		} else if (petCount == LAST_TAPE_PET + 1) { //Enters loop when over encoder count or petOnArm

			fastPivot(6, 140);
			delay(400);
			if (fourthPet) {
				launch(140);
				delay(200);
			}

			if (petOnArm()) {
				launchFifthPet();
			}
			// move upper arm down to avoid zipline
			RCServo0.write(90);
			delay(250);
			int c = 0;
			boolean flag = false;
			uint32_t timeStart = millis();
			while (!flag) {
				upperArmPID();
				uint16_t dt = millis() - timeStart;
				if (c == 0) {
					setUpperArm(550);
					c++;
				} else if (dt >= 1000 && c == 1) {
					flag = true;
				}
			}
			delay(200);
			if (isRedBoard) {
				fastPivot(-6, 100);
			} else {
				fastPivot(-5, 110);
			}

		} else if (petCount == LAST_TAPE_PET + 2) {
			// sixth pet parallel parking motion
			travel(2, BACKWARDS);
			delay(300);
			turnForward(-15, 90);
			delay(300);
			if (isRedBoard) {
				travel(5, BACKWARDS);
			} else {
				travel(5, BACKWARDS);
			}
			delay(300);
			turnBack(-4, 150);
			delay(300);
			fastTravel(4, BACKWARDS, 140);
			getSixthPet();
		} else if (petCount >= LAST_TAPE_PET + 3) {
			while (!stopbutton()) {
				LCD.clear(); LCD.home(); LCD.print("done");
			}
		}

		// speed control
		if ( petCount == 2 ) {
			lastSpeedUp = millis();
		}

	}

	// speed control
	if (petCount == 2 && millis() - lastSpeedUp > 1200) {
		base_speed = 170;
		q_pro_gain = 70;
		q_diff_gain = 15;
	} else if (petCount == 4 && millis() - lastSpeedUp > 150) {
		base_speed = 65;
	}
}

void switchMode() {
	pidfn = irPID;
}

// Set arm vertical height
void setUpperArm(int V) {
	upperArmV = V;
}

void setLowerArm(int V) {
	lowerArmV = V + 15;
}

// Keep arm vertically in place. Run in a while loop to PID the arm segments.
void upperArmPID() {
	int currentV = constrain(analogRead(UPPER_POT), 300, 740);
	int diff = currentV - upperArmV;
	if ( diff <= 25 && diff >= -25) {
		diff = 0;
	}
	if ( diff  > 0) diff = 255;
	else if (diff < 0) diff = -255;
	motor.speed(UPPER_ARM, diff);
}
void lowerArmPID() {
	int currentV = constrain(analogRead(LOWER_POT), 330, 645);
	int diff = currentV - lowerArmV;
	if (diff <= 22 && diff >= -22) {
		diff = 0;
	}
	if (diff  > 0) diff = 255;
	else if (diff < 0) diff = -255;
	motor.speed(LOWER_ARM, diff);
}

// Stop driving
void pauseDrive() {
	motor.stop(LEFT_MOTOR);
	motor.stop(RIGHT_MOTOR);
}

void hardStop() {
	motor.speed(LEFT_MOTOR, -base_speed);
	motor.speed(RIGHT_MOTOR, -base_speed);
	delay(50);
	pauseDrive();
}

// Stop arm movement
void pauseArms() {
	motor.stop(LOWER_ARM);
	motor.stop(UPPER_ARM);
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
	if (petCount < LAST_TAPE_PET - 2) {
		int e = analogRead(QRD_LINE);
		if ( e >= h_threshold && onTape == false) {
			onTape = true;
			return true;
		} else if ( e < q_threshold ) {
			onTape = false;
		}

	}
	else {
		// only start checking for end of ramp after six encoder counts have occurred since third pet tape
		if (petCount == LAST_TAPE_PET - 2) {
			int e = analogRead(QRD_LINE);
			if ( e >= h_threshold && onTape == false && encount_R >= 6) {
				onTape = true;
				return true;
			} else if ( e < q_threshold ) {
				onTape = false;
			}
		}
		else if (petCount == LAST_TAPE_PET - 1) {
			int e = analogRead(QRD_LINE);
			// for the fourth pet, the tape will only trigger when left encoder has surpassed
			// right encoder by 10 so we know that the turn has been made by the robot.
			if ( e >= h_threshold && onTape == false && (encount_L - encount_R) > 11) {
				onTape = true;
				return true;
			} else if ( e < q_threshold ) {
				onTape = false;
			}
		}
		else if (petCount == LAST_TAPE_PET) {
			if (checkRafterPet())
				return true;
		} else if ( petCount == LAST_TAPE_PET + 1) {
			if (checkBoxedPet()) return true;
		} else if (petCount == LAST_TAPE_PET + 2) {
			return true;
		}
	}
	return false;
}

// Checks if it is time to pick up the pet on the rafter in IR following
boolean checkRafterPet() {
	if ( encount_L >= ENC_RAFTER && encount_R >= ENC_RAFTER ) {
		return true;
	}
	return false;
}

// Check if robot has followed to the end, where the box is
boolean checkBoxedPet() {
	if (digitalRead(FRONT_SWITCH) == LOW) {
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
	int speed = STABLE_SPEED;

	motor.speed(RIGHT_MOTOR, counts > 0 ? -speed : speed);
	motor.speed(LEFT_MOTOR, counts > 0 ? speed : -speed );
	while (lflag == false || rflag == false) {
		encoderProcess();
		if (encount_L - pivotEncountStart_L >= pivotCount) {
			motor.stop(LEFT_MOTOR);
			lflag = true;
		}
		if (encount_R - pivotEncountStart_R >= pivotCount) {
			motor.stop(RIGHT_MOTOR);
			rflag = true;
		}
	}
	LCD.clear(); LCD.home();
	LCD.print("E:"); LCD.print(encount_L); LCD.print(" "); LCD.print(encount_R);
	LCD.setCursor(0, 1); LCD.print(pivotEncountStart_L); LCD.print(" "); LCD.print(pivotEncountStart_R);
}

// Pivot robot in direction d until tape is detected after timer has passed
void pivotToLine(int d, int timer) {
	int s = 68;
	if ( d == LEFT) {
		motor.speed(RIGHT_MOTOR, s + 20);
		motor.speed(LEFT_MOTOR, -s);
	} else if ( d == RIGHT) {
		motor.speed(RIGHT_MOTOR, -s - 20);
		motor.speed(LEFT_MOTOR, s);
	}
	s = 85;
	uint32_t start = millis();
	while (true) {
		if ((analogRead(QRD_L) >= q_threshold || analogRead(QRD_R) >= q_threshold) && millis() - start >= timer) {
			if (d == LEFT) {
				motor.speed(RIGHT_MOTOR, -s);
				motor.speed(LEFT_MOTOR, s);
				delay(120);
			} else if (d == RIGHT) {
				motor.speed(RIGHT_MOTOR, s);
				motor.speed(LEFT_MOTOR, -s);
				delay(120);
			}
			pauseDrive();
			return;
		}
	}
}

// A pivotToLine without hard stopping.
void pivotOnLine(int d, int timer, int delayTime) {
	if ( d == LEFT) {
		motor.speed(RIGHT_MOTOR, STABLE_SPEED + 20);
		motor.speed(LEFT_MOTOR, -STABLE_SPEED);
	} else if ( d == RIGHT) {
		motor.speed(RIGHT_MOTOR, -STABLE_SPEED - 20);
		motor.speed(LEFT_MOTOR, STABLE_SPEED);
	}
	uint32_t start = millis();
	while (true) {
		if ((analogRead(QRD_L) >= q_threshold && analogRead(QRD_R) >= q_threshold) && millis() - start >= timer) {
			delay(delayTime);
			pauseDrive();
			return;
		}
	}
}

// Pivot in direction until IR_sensor threshold value has been reached for IR_following
// Not Used.
void pivotToIR(int d, int threshold) {
	int reading = analogRead(IR_R);
	int lastreading = reading;
	if ( d == LEFT) {
		motor.speed(RIGHT_MOTOR, STABLE_SPEED + 20);  // Pivoting, can also use turn forward?
		motor.speed(LEFT_MOTOR, -STABLE_SPEED);
	} else if ( d == RIGHT) {
		motor.speed(RIGHT_MOTOR, -STABLE_SPEED - 20);
		motor.speed(LEFT_MOTOR, STABLE_SPEED);
	}

	while (true) {
		reading = analogRead(IR_R);
		if ( ((reading + lastreading) >> 1) >= threshold || analogRead(IR_L) >= threshold) {
			pauseDrive();
			return;
		}
		lastreading = reading;
	}
}

// Pivot robot at a speed s for a number of encoder counts
void fastPivot(int counts, int s) {
	// flags to check whether motors have stopped or not
	boolean lflag = false, rflag = false;
	// cache starting values
	int pivotCount = abs(counts);
	int pivotEncountStart_L = encount_L;
	int pivotEncountStart_R = encount_R;
	int speed = s;

	motor.speed(RIGHT_MOTOR, counts > 0 ? -speed : speed);
	motor.speed(LEFT_MOTOR, counts > 0 ? speed : -speed );
	while (lflag == false || rflag == false) {
		encoderProcess();
		if (encount_L - pivotEncountStart_L >= pivotCount) {
			motor.stop(LEFT_MOTOR);
			lflag = true;
		}
		if (encount_R - pivotEncountStart_R >= pivotCount) {
			motor.stop(RIGHT_MOTOR);
			rflag = true;
		}
	}
	LCD.clear(); LCD.home();
	LCD.print("E:"); LCD.print(encount_L); LCD.print(" "); LCD.print(encount_R);
	LCD.setCursor(0, 1); LCD.print(pivotEncountStart_L); LCD.print(" "); LCD.print(pivotEncountStart_R);
}

// Turns robot on one wheel until either IR sensors pass the threshold
// Not Used.
void turnToIR(int d, int threshold) {
	if (d == LEFT) {
		motor.stop(LEFT_MOTOR);
		motor.speed(RIGHT_MOTOR, STABLE_SPEED);
	} else if (d == RIGHT) {
		motor.stop(RIGHT_MOTOR);
		motor.speed(LEFT_MOTOR, STABLE_SPEED);
	}
	while (true) {
		if ((analogRead(IR_R) >= threshold || analogRead(IR_L) >= threshold)) {
			pauseDrive();
			return;
		}
	}
}

// Turn robot left (counts < 0) or right (counts > 0) for
// certain amount of encoder counts forward at speed s
void turnForward(int counts, int s) {
	int turnCount = abs(counts);
	int turnEncountStart_L = encount_L;
	int turnEncountStart_R = encount_R;
	time_R = millis();
	time_L = millis();
	if (counts < 0) {
		motor.stop(LEFT_MOTOR);
		motor.speed(RIGHT_MOTOR, s);
		while (true) {
			encoderProcess();
			if (encount_R - turnEncountStart_R >= turnCount ) {
				motor.stop(RIGHT_MOTOR);
				return;
			}
		}
	} else if (counts > 0) {
		motor.stop(RIGHT_MOTOR);
		motor.speed(LEFT_MOTOR, s);
		while (true) {
			encoderProcess();
			if (encount_L - turnEncountStart_L >= turnCount ) {
				motor.stop(LEFT_MOTOR);
				return;
			}
		}
	}
	LCD.clear(); LCD.home();
	LCD.print("E:"); LCD.print(encount_L); LCD.print(" "); LCD.print(encount_R);
	LCD.setCursor(0, 1); LCD.print(turnEncountStart_L); LCD.print(" "); LCD.print(turnEncountStart_R);
}

// Turn robot left (counts < 0) or right (counts > 0) for
// certain amount of encoder counts backward
void turnBack(int counts, int s) {
	int turnCount = abs(counts);
	int turnEncountStart_L = encount_L;
	int turnEncountStart_R = encount_R;
	int count = 0;
	uint32_t timeout = millis();
	uint16_t expiry = 600 * counts;

	if (counts < 0) {
		motor.speed(RIGHT_MOTOR, -s);
		motor.stop(LEFT_MOTOR);
		while (true) {
			encoderProcess();
			if ((encount_R - turnEncountStart_R >= turnCount)  && millis() - timeout < expiry) {
				motor.stop(RIGHT_MOTOR);
				return;
			}
		}
	} else if (counts > 0) {
		motor.stop(RIGHT_MOTOR);
		motor.speed(LEFT_MOTOR, -s);
		while (true) {
			encoderProcess();
			if ((encount_L - turnEncountStart_L >= turnCount) && millis() - timeout < expiry) {
				motor.stop(LEFT_MOTOR);
				return;
			}
		}
	}
	LCD.clear(); LCD.home();
	LCD.print("E:"); LCD.print(encount_L); LCD.print(" "); LCD.print(encount_R);
	LCD.setCursor(0, 1); LCD.print(turnEncountStart_L); LCD.print(" "); LCD.print(turnEncountStart_R);
}

// Travel in a direction d for a number of counts.
void travel(int counts, int d) {

	boolean lflag = false, rflag = false;
	int travelCount = counts;
	int travelEncountStart_L = encount_L;
	int travelEncountStart_R = encount_R;
	time_R = millis();
	time_L = millis();

	motor.speed(RIGHT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	motor.speed(LEFT_MOTOR, d == FORWARDS ? STABLE_SPEED : -STABLE_SPEED);
	while (lflag == false || rflag == false) {
		encoderProcess();
		if (encount_L - travelEncountStart_L >= travelCount) {
			motor.stop(LEFT_MOTOR);
			lflag = true;
		}
		if (encount_R - travelEncountStart_R >= travelCount) {
			motor.stop(RIGHT_MOTOR);
			rflag = true;
		}
	}
	LCD.clear(); LCD.home();
	LCD.print("E:"); LCD.print(encount_L); LCD.print(" "); LCD.print(encount_R);
	LCD.setCursor(0, 1); LCD.print(travelEncountStart_L); LCD.print(" "); LCD.print(travelEncountStart_R);
}

// Travel in direction d for a number of counts at speed s
void fastTravel(int counts, int d, int s) {

	boolean lflag = false, rflag = false;
	int travelEncountStart_L = encount_L;
	int travelEncountStart_R = encount_R;
	uint32_t timeout = millis();
	uint16_t expiry = 600 * counts;

	motor.speed(RIGHT_MOTOR, d == FORWARDS ? s : -s);
	motor.speed(LEFT_MOTOR, d == FORWARDS ? s : -s);
	while ((lflag == false || rflag == false) && millis() - timeout < expiry) {
		encoderProcess();
		if (encount_L - travelEncountStart_L >= counts) {
			motor.stop(LEFT_MOTOR);
			lflag = true;
		}
		if (encount_R - travelEncountStart_R >= counts) {
			motor.stop(RIGHT_MOTOR);
			rflag = true;
		}
	}
	pauseDrive();

	LCD.clear(); LCD.home();
	LCD.print("E:"); LCD.print(encount_L); LCD.print(" "); LCD.print(encount_R);
	LCD.setCursor(0, 1); LCD.print(travelEncountStart_L); LCD.print(" "); LCD.print(travelEncountStart_R);
}

// Travel in a direction d for a time t.
void timedTravel( uint32_t t, int d) {
	int speed = 100;
	motor.speed(RIGHT_MOTOR, d == FORWARDS ? speed : -speed);
	motor.speed(LEFT_MOTOR, d == FORWARDS ? speed : -speed);
	delay(t);
	pauseDrive();
}

// A drop pet function. Moves upper arm to maximum after moving magnet up to ensure pet falls.
void dropPet() {
	LCD.clear(); LCD.home(); LCD.print("DR- DR- DR-");
	LCD.setCursor(0, 1); LCD.print("DROP THE PET!");
	int duration = HAND_DURATION;
	uint32_t timeStart = millis();
	while (millis() - timeStart <= duration) {
		digitalWrite(HAND_UP, HIGH);
	}
	digitalWrite(HAND_UP, LOW);
	delay(200);
	timeStart = millis();
	setUpperArm(MAX_UPPER);
	while (millis() - timeStart <= 1000) {
		upperArmPID();
	}
	pauseArms();
	timeStart = millis();
	while (millis() - timeStart <= duration) {
		digitalWrite(HAND_DOWN, HIGH);
	}
	digitalWrite(HAND_DOWN, LOW);
}

// Direct control of the hand motor
void dropPetCtrl(int com) {
	switch (com) {
	case RAISE:
		digitalWrite(HAND_UP, HIGH);
		break;
	case LOWER:
		digitalWrite(HAND_DOWN, HIGH);
		break;
	case STOP:
		digitalWrite(HAND_UP, LOW);
		digitalWrite(HAND_DOWN, LOW);
		break;
	}
}

// Pivot the arm from and to
void pivotArm( int from, int to, int increment) {
	int p = from;
	if (from > to) {
		while ( p > to) {
			p -= increment;
			p = constrain(p, to, 185);
			RCServo0.write(p);
			delay(100);
		}
	} else if ( from < to) {
		while ( p < to ) {
			p += increment;
			p = constrain(p, 0, to);
			RCServo0.write(p);
			delay(100);
		}
	}
}

// Adjust pivot arm position in multiple attempts of pet
void adjustArm(int pivotPosition, int tries, int increment) {
	if (tries == 1) {
		RCServo0.write(pivotPosition - increment);
	} else if (tries == 2) {
		RCServo0.write(pivotPosition + increment);
	}
}

//Function to get first pet
void getFirstPet() {

	boolean flag = false, unsuccessful = false, found = false;
	int pivotPosition = 43; // was 50
	int pivotIncrement = 15;
	int c = 0, try_num = 0;
	int t1 = 800, t2 = t1, t3 = t2 + 800, t4 = t3 + 700, t5 = t4 + 900;

	RCServo0.write(pivotPosition);
	delay(200);
	setUpperArm(375);
	uint32_t timeStart = millis();

	while (!flag) {

		upperArmPID();
		lowerArmPID();

		uint16_t dt = millis() - timeStart;

		if ( dt >= t1 && c == 0 ) {
			// check if pet is somewhere near hand
			if (petOnArm()) {
				found = true;
				try_num = 0;
				pivotIncrement = 7;
			}
			c++;
		} else if ( dt >= t2 && c == 1 ) {
			setUpperArm(500);
			c++;
		} else if ( dt >= t3 && c == 2 ) {
			if (petOnArm()) {
				c++;
			} else if (try_num < 2) {
				adjustArm(pivotPosition, try_num, pivotIncrement);
				try_num++;
				c = (found == true) ? 1 : 0;
				setUpperArm(390);
				timeStart = millis();
			} else if (try_num >= 2 && !petOnArm()) {
				c = 4;
				unsuccessful = true;
				timeStart = millis() - t4;
			}
		} else if ( c == 3 ) {
			setUpperArm(MAX_UPPER);
			c++;
		} else if ( dt >= t4 && c == 4 ) {
			setLowerArm(MAX_LOWER);
			c++;
		} else if ( dt >= t5 && c == 5 ) {
			flag = true;
		}
	}
	LCD.clear(); LCD.home(); LCD.print("placing catapult");
	motor.stop_all(); // ensure motors are not being powered

	if (!unsuccessful) {
		placePetCatapult(pivotPosition);
		delay(250);
		if (isRedBoard) {
			pivotToLine(RIGHT, 1700);
		} else {
			pivotToLine(RIGHT, 1800);
		}
		// move arm out of catapult's way
		RCServo0.write(70);
		c = 0;
		flag = false;
		t1 = 500; t2 = t1 + 1000;
		timeStart = millis();
		while (!flag) {
			lowerArmPID();
			uint16_t dt = millis() - timeStart;
			if (dt >= t1 && c == 0) {
				// this lower arm height is also the height second pet is picked up from
				if (isRedBoard) {
					setLowerArm(550);
				} else {
					setLowerArm(530);
				}
				c++;
			} else if (dt >= t2 && c == 1) {
				flag = true;
			}
		}
		pauseArms(); // ensure arms are not powered

		delay(150);
		launch(75);
		pivotToLine(RIGHT, 1000);
		delay(150);

	} else {
		RCServo0.write(70);
	}

	// setArmSecondPet();

	// if (analogRead(QRD_L) >= q_threshold && analogRead(QRD_R) < q_threshold)
	// 	pivotOnLine(LEFT, 0, 0);
	// else if (analogRead(QRD_R) >= q_threshold && analogRead(QRD_L) < q_threshold)
	// 	pivotOnLine(RIGHT, 0, 0);
	// else if (analogRead(QRD_L) < q_threshold && analogRead(QRD_R) < q_threshold)
	// 	pivotOnLine(LEFT, 0, 0);
	pauseArms();
}

// Setting arm for second pet pickup
void setArmSecondPet() {
	int c = 0;
	int pivotPosition = 35;
	int t1 = 500, t2 = t1 + 1000, t3 = t2 + 1200;
	RCServo0.write(pivotPosition);
	uint32_t timeStart = millis();
	while (true) {
		lowerArmPID();
		upperArmPID();
		uint16_t dt = millis() - timeStart;
		if (dt >= t1 && c == 0) {
			setLowerArm(515);
			c++;
		} else if ( dt >= t2 && c == 1) {
			setUpperArm(395);
			c++;
		} else if (dt >= t3 && c == 2) {
			return;
		}
	}
	pauseArms();
}

void placeSecondPet() {
	int pivotTo = 116;
	int c = 1;
	int t1 = 1200, t2 = t1 + 1000, t3 = t2 + 800, t4 = t3 + 2000;
	int t5 = 1200, t6 = t5 + 1000;

	setLowerArm(610);

	uint32_t timeStart = millis();
	while (true) {
		upperArmPID();
		lowerArmPID();

		uint16_t dt = millis() - timeStart;

		if ( dt >= t1 && c == 1 ) {
			setUpperArm(MAX_UPPER);
			c++;
		} else if (dt >= t2 && c == 2) {
			RCServo0.write(pivotTo);
			c++;
		} else if ( dt >= t3 && c == 3) {
			setLowerArm(350);
			setUpperArm(510);
			c++;
		} else if ( dt >= t4 && c == 4) {
			pauseArms();
			dropPet();
			timeStart = millis();
			c++;
		} else if ( c == 5) {
			setLowerArm(600);
			c++;
		} else if ( dt >= t5 && c == 6) {
			setUpperArm(MAX_UPPER);
			c++;
		} else if ( dt >= t6 && c == 7) {
			return;
		}
	}
	pauseArms();
}

void getThirdPet() {
	boolean flag = false, unsuccessful = false, found = false;
	int pivotPosition = 36;
	int pivotIncrement = 13;
	int c = 1;
	int try_num = 0;
	int t1 = -1000, t2 = t1 + 1000, t3 = t2 + 1000, t4 = t3 + 800, t5 = t4 + 800, t6 = t5 + 1000;
	pivotArm(70, pivotPosition, 5);

	uint32_t timeStart = millis();
	while (!flag) {

		upperArmPID();
		lowerArmPID();
		uint16_t dt = millis() - timeStart;

		if ( dt >= t1 && c == 0 ) {
			// won't go in here
			setLowerArm(550);
			c++;
		} else if ( dt >= t2 && c == 1 ) {
			if (isRedBoard) {
				setUpperArm(350);
			} else {
				setUpperArm(370);
			}
			c++;
		} else if ( dt >= t3 && c == 2 ) {
			if (petOnArm()) {
				found = true;
				try_num = 0;
				pivotIncrement = 10;
			}
			c++;
		} else if (dt >= t3 && c == 3) {
			setUpperArm(500);
			c++;
		} else if ( dt >= t4 && c == 4) {
			if (petOnArm()) {
				c++;
			} else if (try_num < 2) {
				adjustArm(pivotPosition, try_num, pivotIncrement);
				setLowerArm(515);
				try_num++;
				c = (found == true) ? 3 : 2;
				setUpperArm(380);
				timeStart = millis() - t2;
			} else if (try_num >= 2 && !petOnArm()) {
				c = 5;
				unsuccessful = true;
				thirdPet = false;
				timeStart = millis() - t4;
			}
		} else if ( c == 5) {
			if (!unsuccessful)
				setUpperArm(MAX_UPPER);
			else if (unsuccessful)
				setUpperArm(600);
			c++;
		} else if ( dt >= t5 && c == 6 ) {
			setLowerArm(MAX_LOWER);
			c++;
		} else if ( dt >= t6 && c == 7 ) {
			flag = true;
		}
	}

	if (!unsuccessful) {
		// pivot arm to correct location
		RCServo0.write(110);

		flag = false;
		c = 0;
		t1 = 500; t2 = t1 + 1000; t3 = t2 + 1000; t4 = 2000;
		timeStart = millis();
		//move upper/lower arm to correct position for drop;
		while (!flag) {
			upperArmPID();
			lowerArmPID();

			uint16_t dt = millis() - timeStart;
			if ( dt >= t1 && c == 0 ) {
				setLowerArm(370);
				c++;
			} else if ( dt >= t2 && c == 1) {
				setUpperArm(520);
				c++;
			} else if ( dt >= t3 && c == 2) {
				pauseArms();
				dropPet();
				c++;
				timeStart = millis();
			} else if (c == 3) {
				RCServo0.write(80);
				setUpperArm(450);
				setLowerArm(600);
				c++;
			} else if ( dt >= t4 && c == 4 ) {
				flag = true;
			}
		}
	} else {
		flag = false;
		c = 0;
		t1 = 500; t2 = t1 + 1500;
		timeStart = millis();
		// move arm out of way
		while (!flag) {
			upperArmPID();
			lowerArmPID();

			uint16_t dt = millis() - timeStart;
			if ( dt >= t1 && c == 0 ) {
				setUpperArm(450);
				setLowerArm(600);
				c++;
				RCServo0.write(80);
			} else if ( dt >= t2 && c == 1 ) {
				flag = true;
			}
		}
	}
	pauseArms();
}

void getFourthPet() {
	boolean flag = false, unsuccessful = false, found = false;
	LCD.clear(); LCD.home();
	LCD.print("L:"); LCD.print(encount_L);
	LCD.print(" R:"); LCD.print(encount_R);
	int pivotPosition = 85, pivotIncrement = 15;
	int c = 0;
	int try_num = 0;
	int t1 = 500, t2 = t + 1000, t3 = t2 + 1000, t4 = t3 + 800, t5 = t4 + 1000, t6 = t5 + 1000;
	RCServo0.write(pivotPosition);

	uint32_t timeStart = millis();
	while (!flag) {
		upperArmPID();
		lowerArmPID();

		uint16_t dt = millis() - timeStart;

		if ( dt >= t1 && c == 0 ) {
			if (isRedBoard) {
				setLowerArm(500);
			} else {
				setLowerArm(530);
			}
			c++;
		} else if ( dt >= t2 && c == 1 ) {
			setUpperArm(390);
			c++;
		} else if ( dt >= t3 && c == 2 ) {
			if (petOnArm()) {
				found = true;
				try_num = 0;
				pivotIncrement = 8;
			}
			c++;
		} else if ( dt >= t3 && c == 3) {
			setUpperArm(500);
			setLowerArm(600);
			c++;
		} else if ( dt >= t4 && c == 4) {
			if (petOnArm()) {
				c++;
			} else if (try_num < 2) {
				try_num++;
				c = (found == true) ? 3 : 2;
				if (try_num == 0) {
					if (isRedBoard) {
						setLowerArm(460);
					} else {
						setLowerArm(490);
					}
				}
				setUpperArm(390);
				timeStart = millis() - t2;
			} else if (try_num >= 2 && !petOnArm()) {
				c = 5;
				unsuccessful = true;
				fourthPet = false;
				timeStart = millis() - t4;
			}
		} else if ( c == 5) {
			setUpperArm(MAX_UPPER);
			c++;
		} else if (dt >= t5 && c == 6) {
			pauseArms();
			setLowerArm(MAX_LOWER);
			c++;
		} else if ( dt >= t6 && c == 7) {
			flag = true;

		}
	}

	pauseArms();
	if (!unsuccessful) {
		placePetCatapult(pivotPosition);
		delay(200);
	}
	if (isRedBoard) {
		fastTravel(8, FORWARDS, 100); // with ir
	} else {
		fastTravel(9, FORWARDS, 100); // with ir
	}
	delay(300);
	turnForward(5, 100);
	RCServo0.write(40);

	// get fifth pet arm position
	c = 0;
	flag = false;
	t1 = 500; t2 = t1 + 1200, t3 = t2 + 1000;
	timeStart = millis();
	while (!flag) {
		lowerArmPID();
		upperArmPID();
		uint16_t dt = millis() - timeStart;

		if (dt >= t1 && c == 0) {
			// this lower arm height is also the height the fifth pet will be picked up from
			setLowerArm(529);
			c++;
		} else if (dt >= t2 && c == 1) {
			// this lower arm height is also the height second pet is picked up from
			if (isRedBoard) {
				setUpperArm(574);
			} else {
				setUpperArm(575);
			}
			c++;
		} else if ( dt >= t3 && c == 2) {
			flag = true;
		}
	}
	pauseArms(); // ensure arms are not powered
}

void launchFifthPet() {
	int c = 0;
	boolean flag = false;
	uint32_t timeStart = millis();
	while (!flag) {
		lowerArmPID();
		upperArmPID();
		uint16_t dt = millis() - timeStart;
		if (c == 0) {
			setLowerArm(MAX_LOWER);
			setUpperArm(MAX_UPPER);
			c++;
		} else if (dt >= 1500 && c == 1) {
			flag = true;
		}
	}

	// adjust lower/upper arm for catapult placing
	placePetCatapult(35);
	delay(200);
	// move arm out of catapult's way
	RCServo0.write(70);
	c = 0;
	flag = false;
	timeStart = millis();
	while (!flag) {
		lowerArmPID();
		unsigned int dt = millis() - timeStart;
		if (dt >= 500 && c == 0) {
			setLowerArm(550);
			c++;
		} else if (dt >= 1500 && c == 1) {
			flag = true;
		}
	}
	pauseArms(); // ensure arms are not powered

	delay(200);
	launch(140);
	delay(100);
}

void getSixthPet() {

	boolean flag = false, alreadyTried = false;
	int pivotPosition = 39;
	int pivotIncrement = 7;
	int c = 0;
	int try_num = 0;
	int lowerHeight = 580;
	int upperHeight = 420;
	int direction = LEFT;
	int t1 = 0, t2 = t1 + 700, t3 = t2 + 500, t4 = t3 + 400, t5 = t4 + 400, t6 = t5 + 1000, t7 = t6 + 1000, t8 = t7 + 1000;

	RCServo0.write(pivotPosition);
	delay(200);
	uint32_t timeStart = millis();
	while (!flag) {
		upperArmPID();
		lowerArmPID();

		uint16_t dt = millis() - timeStart;
		if ( dt >= t1 && c == 0 ) {
			setLowerArm(580);
			setUpperArm(423);
			c++;
		} else if ( dt >= t2 && c == 1 ) {
			setLowerArm(420);
			c++;
		} else if ( dt >= t3 && c == 2 ) {
			setLowerArm(580);
			c++;
		} else if ( dt >= t4 && c == 3) {
			setUpperArm(500);
			c++;
		} else if ( dt >= t5 && c == 4) {
			if (petOnArm()) {
				c++;
			} else if (try_num < 2) {
				if (pivotPosition >= 48) {
					direction = RIGHT;
					try_num++;
				} else if (pivotPosition < 30) {
					direction = LEFT;
					try_num++;
				}
				if ( direction == LEFT) {
					pivotPosition += pivotIncrement;
				} else if (direction == RIGHT) {
					pivotPosition -= pivotIncrement;
				}
				RCServo0.write(pivotPosition);
				c = 0;
				timeStart = millis();
			} else {
				c++;
			}
		} else if (c == 5) {
			setUpperArm(640);
			c++;
		} else if (dt >= t6 && c == 6) {
			if (petOnArm() || alreadyTried) {
				c++;
			} else {
				RCServo0.write(pivotPosition);
				setUpperArm(390);
				c = 4;
				alreadyTried = true;
				timeStart = millis() - t3;
			}
		} else if (dt >= t7 && c == 7) {
			setLowerArm(MAX_LOWER);
			setUpperArm(MAX_UPPER);
			c++;
		} else if (dt >= t8 && c == 8) {
			flag = true;
		}
	}

	fastTravel(4, BACKWARDS, 130);
	if (petOnArm()) {
		placeSixthPetCatapult(pivotPosition);
		delay(200);
		turnForward(-5, 140);
		delay(200);
		flag = false;
		c = 0;
		timeStart = millis();
		while (!flag) {
			lowerArmPID();
			upperArmPID();
			uint16_t dt = millis() - timeStart;
			if (dt >= 0 && c == 0) {
				setLowerArm(480);
				setUpperArm(520);
				c++;
				RCServo0.write(75);
			} else if ( dt >= 1000 && c == 1) {
				setUpperArm(450);
				c++;
			} else if ( dt >= 2000 && c == 2) {
				flag = true;
			}
		}
		pauseArms();
		delay(200);
		fastTravel(3, FORWARDS, 130);
		delay(200);
		turnForward(-12, 130);
		delay(200);
		motor.stop_all();

		flag = false;
		c = 0;
		delay(200);
		RCServo0.write(70);
		timeStart = millis();

		while (!flag) {
			lowerArmPID();
			uint16_t dt = millis() - timeStart;
			if (dt >= 0 && c == 0) {
				setLowerArm(480);
				c++;
			} else if ( dt >= 1000 && c == 1) {
				flag = true;
			}
		}
		delay(200);
		launch(140);
		motor.stop_all();
	}

}

// Place pet in catapult from pivot arm's position 'pivotFrom'
void placePetCatapult(int pivotFrom) {
	int c = 1;
	pauseArms();
	pivotArm(pivotFrom, 163, 8);
	delay(250);
	uint32_t timeStart = millis();
	while (true) {
		lowerArmPID();
		upperArmPID();

		uint32_t dt = millis() - timeStart;
		if ( dt >= 0 && c == 1 ) {
			setLowerArm(520);
			LCD.clear(); LCD.home();
			LCD.print("placing pet");
			digitalWrite(HAND_UP, HIGH);
			c++;
		} else if (dt >= HAND_DURATION && c == 2) {
			setLowerArm(640);
			setUpperArm(MAX_UPPER);
			digitalWrite(HAND_UP, LOW);
			c++;
		} else if ( c == 3) {
			c++;
			digitalWrite(HAND_DOWN, HIGH);
			timeStart = millis();
		} else if (dt >= HAND_DURATION && c == 4 ) {
			digitalWrite(HAND_DOWN, LOW);
			pauseArms(); // ensure arms stop moving
			return;
		}
	}
}

// A separate placePetCatapult for the sixth pet.
void placeSixthPetCatapult(int pivotFrom) {
	int c = 1;
	pauseArms();
	pivotArm(pivotFrom, 163, 8);
	delay(250);
	uint32_t timeStart = millis();
	while (true) {
		lowerArmPID();
		upperArmPID();

		uint32_t dt = millis() - timeStart;
		if ( dt >= 0 && c == 1 ) {
			setLowerArm(520);
			LCD.clear(); LCD.home();
			LCD.print("placing pet");
			digitalWrite(HAND_UP, HIGH);
			c++;
		} else if (dt >= HAND_DURATION && c == 2) {
			setLowerArm(MAX_LOWER);
			digitalWrite(HAND_UP, LOW);
			c++;
		} else if ( c == 3) {
			c++;
			digitalWrite(HAND_DOWN, HIGH);
			timeStart = millis();
		} else if (dt >= HAND_DURATION && c == 4 ) {
			digitalWrite(HAND_DOWN, LOW);
			pauseArms(); // ensure arms stop moving
			return;
		}
	}
}

// Testing mode for measuring QRD values in menu.
void qrdRead() {
	while (!stopbutton()) {
		LCD.clear(); LCD.home();
		LCD.print("LQ:"); LCD.print(analogRead(QRD_L)); LCD.print(" RQ:"); LCD.print(analogRead(QRD_R));
		LCD.setCursor(0, 1); LCD.print("HQ:"); LCD.print(analogRead(QRD_LINE));
		delay(200);

	}
	LCD.clear(); LCD.home();
	LCD.print("Returning...");
	delay(500);
}

// Arm calibration test code.
void armCal() {
	int a;
	int s = 90;
	int c = 0;

	while (!stopbutton()) {
		// temporary arm calibration code
		int selection = map(knob(6), 0 , 1023, 0, 3);

		if (selection == 0) {
			a = map(knob(7), 0 , 1023, 0 , 184);
		} else if (selection == 1) {
			a = map(knob(7), 0, 1023, 350, MAX_LOWER); // lower arm
		} else if ( selection == 2) {
			a = map(knob(7), 0 , 1023, 300, MAX_UPPER + 5); // higher arm
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
			dropPetCtrl(RAISE);
			delay(HAND_DURATION);
			dropPetCtrl(STOP);
			delay(100);
			dropPetCtrl(LOWER);
			delay(HAND_DURATION);
			dropPetCtrl(STOP);
		}

		// move arm
		upperArmPID();
		lowerArmPID();
		c++;
	}
}

/* ISRs */

// Encoder ISRs.
void encoderProcess() {
	if (digitalRead(1) == HIGH && !ecL) {
		encount_L++;
		time_L = millis();
		ecL = true;
	} else if (digitalRead(1) == LOW) {
		ecL = false;
	}
	if (digitalRead(2) == HIGH && !ecR) {
		encount_R++;
		time_R = millis();
		ecR = true;
	} else if (digitalRead(2) == LOW) {
		ecR = false;
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

void strategySelection()
{
	delay(500);
	while (!stopbutton())
	{
		LCD.clear(); LCD.home();
		int selection = map(knob(6), 0 , 1023, 0, 2);

		LCD.print("Strategy: ");
		LCD.print(fullRun ? "Full" : "Top");

		LCD.setCursor(0, 1);

		if (selection == 0) {
			LCD.print("Full?");
		} else if (selection == 1) {
			LCD.print("Top?");
		}

		if (startbutton()) {
			delay(100);

			if (selection == 0) {
				fullRun = true;
				eeprom_write_word(FULLRUN_EEPROM, true);
			} else if (selection == 1) {
				fullRun = false;
				eeprom_write_word(FULLRUN_EEPROM, false);
			}
		}
		delay(100);
	}
	LCD.clear(); LCD.home(); LCD.print("Returning...");
	delay(600);
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
			//val = (knob(7) >> 3) - 64;
			val = (knob(7) >> 2 ) - 128;
			LCD.print(val); LCD.print("?");
		} else if ( menuIndex == 10) {
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
		} else if (menuIndex == 8) {
			LCD.print("Hand Control");
			LCD.setCursor(0, 1);
			val = map(knob(7), 0 , 1023, 7, 9);
			LCD.print(val == 7 ? "LOWER?" : "RAISE?");
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
